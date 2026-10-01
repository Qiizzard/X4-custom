#include "PasswordManagerActivity.h"

#include <Arduino.h>
#include <HalClock.h>
#include <HalClockSync.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>
#include <SecureStore.h>

#include <algorithm>
#include <cstring>

#include "EncryptedVaultFile.h"
#include "SecretEntryActivity.h"
#include "WifiQrPayload.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr unsigned long kIdleMs = 60000, kRevealMs = 10000;
}  // namespace
const char* PasswordManagerActivity::path() const {
  if (decoy) return "/crossink/vaults/decoy.bin";
  return passwords() ? "/crossink/vaults/passwords.bin" : "/crossink/vaults/totp.bin";
}
const char* PasswordManagerActivity::nextPath() const {
  if (decoy) return "/crossink/vaults/decoy.next";
  return passwords() ? "/crossink/vaults/passwords.next" : "/crossink/vaults/totp.next";
}
const char* PasswordManagerActivity::previousPath() const {
  if (decoy) return "/crossink/vaults/decoy.previous";
  return passwords() ? "/crossink/vaults/passwords.previous" : "/crossink/vaults/totp.previous";
}
bool PasswordManagerActivity::decoyFilesReady() const {
  return Storage.exists("/crossink/vaults/decoy.bin") && !Storage.exists("/crossink/vaults/decoy.next");
}
bool PasswordManagerActivity::unlockTotp() {
  if (passwords()) return true;
  for (size_t i = 0; i < records.size(); ++i) {
    const auto* record = records.at(i);
    if (record->username[0] || !Totp::validSeed(record->password)) {
      LOG_ERR("TOTP", "Invalid authenticated account record");
      return false;
    }
  }
  return totp.open();
}
void PasswordManagerActivity::refreshWifiQr() {
  codeValid = false;
  securestore::secureZero(encoded, sizeof(encoded));
  securestore::secureZero(scratch, sizeof(scratch));
  if (!reveal) return;
  const auto* record = records.at(selected);
  // Reuse existing vault buffers; v9-L fits the maximum 208-byte payload.
  char* payload = reinterpret_cast<char*>(scratch);
  if (record && buildWifiQrPayload(record->username, record->password, wifiAuth, payload, sizeof(scratch)) &&
      qrcode_getBufferSize(9) <= sizeof(encoded) && qrcode_initText(&qr, encoded, 9, ECC_LOW, payload) == 0)
    codeValid = true;
  else {
    LOG_ERR("WiFiQR", "Invalid record or QR encoding failed");
    securestore::secureZero(encoded, sizeof(encoded));
  }
  securestore::secureZero(scratch, sizeof(scratch));
}

void PasswordManagerActivity::refreshCode() {
  if (passwords() || screen != Screen::Detail || !reveal) return;
  uint64_t seconds = 0;
  if (!HalClockSync::getSyncedUnixTime(seconds)) {
    const bool changed = codeValid;
    codeValid = false;
    securestore::secureZero(code, sizeof(code));
    securestore::secureZero(qrModules, sizeof(qrModules));
    if (changed) requestUpdate();
    return;
  }
  if (codeValid && codeCounter == seconds / 30) return;
  const auto* record = records.at(selected);
  if (!record || !totp.generate(record->password, seconds, code)) {
    fail();
    return;
  }
  codeCounter = seconds / 30;
  codeValid = true;
  if (mode == Mode::TotpQr) {
    if (qrcode_getBufferSize(1) > sizeof(qrModules) || qrcode_initText(&qr, qrModules, 1, ECC_LOW, code) != 0) {
      fail();
      return;
    }
  }
  requestUpdate();
}
PasswordManagerActivity::~PasswordManagerActivity() { wipe(); }
void PasswordManagerActivity::wipe() {
  totp.close();
  codeValid = false;
  securestore::secureZero(code, sizeof(code));
  securestore::secureZero(qrModules, sizeof(qrModules));
  records.lock();
  securestore::secureZero(&draft, sizeof(draft));
  securestore::secureZero(encoded, sizeof(encoded));
  securestore::secureZero(scratch, sizeof(scratch));
  securestore::secureZero(passphrase, sizeof(passphrase));
  securestore::secureZero(entry, sizeof(entry));
  reveal = false;
  wifiAuth = 0;
  decoy = mode == Mode::DecoySetup;
  selected = 0;
}
void PasswordManagerActivity::fail() {
  LOG_ERR("Passwords", "Vault operation failed; locking without automatic recovery");
  wipe();
  screen = Screen::Error;
  requestUpdate();
}
void PasswordManagerActivity::onEnter() {
  Activity::onEnter();
  wipe();
  existing = Storage.exists(path());
  if (mode == Mode::WifiQr && !existing) {
    fail();
    return;
  }
  if ((mode == Mode::Duress || mode == Mode::DecoySetup) &&
      (!Storage.exists("/crossink/vaults/passwords.bin") || Storage.exists("/crossink/vaults/passwords.next"))) {
    fail();
    return;
  }
  // Never interpret a missing primary with recovery artifacts as a new vault.
  if (Storage.exists(nextPath()) || (!existing && Storage.exists(previousPath()))) {
    fail();
    return;
  }
  screen = Screen::Locked;
  lastInput = millis();
  requestUpdate();
}
void PasswordManagerActivity::onExit() {
  wipe();
  renderer.clearScreen();  // erase the shared RAM framebuffer before leaving
  Activity::onExit();
}
void PasswordManagerActivity::ask(Input step) {
  inputStep = step;
  reveal = false;
  StrId prompt = StrId::STR_VAULT_KEY;
  size_t capacity = sizeof(entry), minimum = 8;
  if (step == Input::AuthorizeDecoy) prompt = StrId::STR_VAULT_AUTHORIZE_DECOY;
  if (step == Input::ConfirmKey) prompt = StrId::STR_VAULT_CONFIRM_KEY;
  if (step == Input::Title) {
    prompt = StrId::STR_VAULT_TITLE;
    capacity = sizeof(draft.title);
    minimum = 1;
  } else if (step == Input::Username) {
    prompt = StrId::STR_USERNAME;
    capacity = sizeof(draft.username);
    minimum = 0;
  } else if (step == Input::Password) {
    prompt = passwords() ? StrId::STR_PASSWORD : StrId::STR_TOTP_SEED;
    capacity = sizeof(draft.password);
    minimum = 1;
  }
  // Small child with a fixed 65-byte draft; the parent's entry buffer outlives it.
  auto child = makeUniqueNoThrow<SecretEntryActivity>(renderer, mappedInput, prompt, entry, capacity, minimum);
  if (!child) {
    LOG_ERR("Passwords", "Secret input allocation failed (%u bytes)", unsigned(sizeof(SecretEntryActivity)));
    fail();
    return;
  }
  startActivityForResult(std::move(child), [this](const ActivityResult& result) {
    RenderLock lock;
    accept(result.isCancelled);
  });
}
void PasswordManagerActivity::accept(bool cancelled) {
  lastInput = millis();
  if (cancelled) {
    wipe();  // cancellation or child timeout locks the whole vault
    screen = Screen::Locked;
    return;
  }
  switch (inputStep) {
    case Input::AuthorizeDecoy: {
      size_t length = 0;
      const auto loaded = vaultfile::load("/crossink/vaults/passwords.bin", entry, encoded, sizeof(encoded), &length,
                                          scratch, sizeof(scratch));
      securestore::secureZero(entry, sizeof(entry));
      const bool ok = loaded.status == vaultfile::Status::Ok && records.decode(encoded, length);
      securestore::secureZero(encoded, sizeof(encoded));
      records.lock();
      if (!ok) {
        fail();
        return;
      }
      ask(Input::Create);
      return;
    }
    case Input::Create:
      memcpy(passphrase, entry, sizeof(passphrase));
      securestore::secureZero(entry, sizeof(entry));
      ask(Input::ConfirmKey);
      return;
    case Input::ConfirmKey: {
      unsigned difference = 0;
      for (size_t i = 0; i < sizeof(entry); ++i) difference |= entry[i] ^ passphrase[i];
      securestore::secureZero(entry, sizeof(entry));
      if (!difference && mode == Mode::DecoySetup) {
        size_t length = 0;
        const auto checked = vaultfile::load("/crossink/vaults/passwords.bin", passphrase, encoded, sizeof(encoded),
                                             &length, scratch, sizeof(scratch));
        securestore::secureZero(encoded, sizeof(encoded));
        // Only a genuine authentication mismatch permits a separate decoy key.
        if (checked.status != vaultfile::Status::CryptoError || checked.crypto != securestore::Status::AuthFailed)
          difference = 1;
      }
      if (difference || !save(true)) {
        fail();
        return;
      }
      existing = true;
      if (!unlockTotp()) {
        fail();
        return;
      }
      lastInput = millis();  // Do not count synchronous KDF work as user idle.
      screen = Screen::List;
      return;
    }
    case Input::Unlock: {
      memcpy(passphrase, entry, sizeof(passphrase));
      securestore::secureZero(entry, sizeof(entry));
      size_t length = 0;
      decoy = mode == Mode::DecoySetup;
      auto loaded = vaultfile::load(path(), passphrase, encoded, sizeof(encoded), &length, scratch, sizeof(scratch));
      if (mode == Mode::Duress && loaded.status == vaultfile::Status::CryptoError &&
          loaded.crypto == securestore::Status::AuthFailed && decoyFilesReady()) {
        // AuthFailed can mean a wrong key or tampering. Format/I/O failures never select the decoy.
        decoy = true;
        loaded = vaultfile::load(path(), passphrase, encoded, sizeof(encoded), &length, scratch, sizeof(scratch));
      }
      bool ok = loaded.status == vaultfile::Status::Ok;
      if (ok && !passwords()) {
        ok = length == sizeof(encoded) && memcmp(encoded, "TVR1", 4) == 0;
        if (ok) encoded[0] = 'P';  // only after AEAD verification and domain check
      }
      ok = ok && records.decode(encoded, length) && unlockTotp();
      securestore::secureZero(encoded, sizeof(encoded));
      if (!ok) {
        fail();
        return;
      }
      lastInput = millis();  // Do not count synchronous KDF work as user idle.
      screen = Screen::List;
      return;
    }
    case Input::Title:
      memcpy(draft.title, entry, sizeof(draft.title));
      securestore::secureZero(entry, sizeof(entry));
      ask(passwords() ? Input::Username : Input::Password);
      return;
    case Input::Username:
      memcpy(draft.username, entry, sizeof(draft.username));
      securestore::secureZero(entry, sizeof(entry));
      ask(Input::Password);
      return;
    case Input::Password:
      if (!passwords() && !Totp::validSeed(entry)) {
        fail();
        return;
      }
      memcpy(draft.password, entry, sizeof(draft.password));
      securestore::secureZero(entry, sizeof(entry));
      screen = Screen::Save;
      return;
  }
}
bool PasswordManagerActivity::save(bool creating) {
  if (mode == Mode::WifiQr) {
    LOG_ERR("WiFiQR", "Read-only mode cannot save vault records");
    return false;
  }
  if (!records.encode(encoded, sizeof(encoded))) return false;
  if (!passwords()) encoded[0] = 'T';  // authenticated TOTP domain tag
  bool ok = Storage.exists("/crossink/vaults") || Storage.mkdir("/crossink/vaults");
  if (ok && !Storage.exists(nextPath())) {
    const auto written = vaultfile::create(creating ? path() : nextPath(), passphrase, encoded, sizeof(encoded),
                                           scratch, sizeof(scratch));
    ok = written.status == vaultfile::Status::Ok;
    if (ok && !creating) {
      // Prior primary remains intact until the new encrypted file is synced/closed.
      // Keep one encrypted backup. Any failure stops; no automatic rollback.
      if (Storage.exists(previousPath())) ok = Storage.remove(previousPath());
      if (ok) ok = Storage.rename(path(), previousPath());
      if (ok) ok = Storage.rename(nextPath(), path());
    }
  } else
    ok = false;
  securestore::secureZero(encoded, sizeof(encoded));
  lastInput = millis();  // KDF is synchronous; do not count its time as user idle
  return ok;
}
void PasswordManagerActivity::loop() {
  RenderLock lock;
  if (screen != Screen::Locked && screen != Screen::Error && millis() - lastInput >= kIdleMs) {
    wipe();
    screen = Screen::Locked;
    requestUpdate();
    return;
  }
  if (!passwords() && millis() - lastClockPoll >= 250) {
    lastClockPoll = millis();
    refreshCode();
  }
  if (reveal && millis() - revealedAt >= kRevealMs) {
    reveal = false;
    codeValid = false;
    securestore::secureZero(code, sizeof(code));
    securestore::secureZero(qrModules, sizeof(qrModules));
    if (mode == Mode::WifiQr) {
      securestore::secureZero(encoded, sizeof(encoded));
      securestore::secureZero(scratch, sizeof(scratch));
    }
    requestUpdate();
  }
  using Button = MappedInputManager::Button;
  const bool back = mappedInput.wasReleased(Button::Back), confirm = mappedInput.wasReleased(Button::Confirm);
  const bool up = mappedInput.wasReleased(Button::Up), down = mappedInput.wasReleased(Button::Down);
  const bool edit = mappedInput.wasReleased(Button::PageForward), remove = mappedInput.wasReleased(Button::PageBack);
  const bool left = mappedInput.wasReleased(Button::Left), right = mappedInput.wasReleased(Button::Right);
  if (!(back || confirm || up || down || edit || remove || left || right)) return;
  lastInput = millis();
  if (back) {
    if (screen == Screen::Locked || screen == Screen::Error)
      finish();
    else {
      wipe();
      screen = Screen::Locked;
    }
  } else if (screen == Screen::Locked && confirm) {
    ask(existing ? Input::Unlock : mode == Mode::DecoySetup ? Input::AuthorizeDecoy : Input::Create);
  } else if (screen == Screen::List) {
    const size_t total =
        records.size() + (mode != Mode::WifiQr && records.size() < PasswordRecords::kMaxRecords ? 1 : 0);
    if (!total) return;
    if (up) selected = (selected + total - 1) % total;
    if (down) selected = (selected + 1) % total;
    if (confirm) {
      if (selected == records.size())
        ask(Input::Title);
      else {
        codeValid = false;
        screen = Screen::Detail;
      }
    }
  } else if (screen == Screen::Detail) {
    if (confirm) {
      reveal = !reveal;
      revealedAt = millis();
      if (mode == Mode::WifiQr)
        refreshWifiQr();
      else
        refreshCode();
    }
    if (mode == Mode::WifiQr && (left || right)) {
      wifiAuth = (wifiAuth + (right ? 1 : 2)) % 3;
      reveal = false;
      refreshWifiQr();
    }
    if (edit && mode != Mode::WifiQr) ask(Input::Title);  // replacement wizard, never prefill secret input
    if (remove && mode != Mode::WifiQr) {
      reveal = false;
      screen = Screen::Delete;
    }
    if (up || down) {
      reveal = false;
      if (mode == Mode::WifiQr) refreshWifiQr();
      screen = Screen::List;
    }
  } else if ((screen == Screen::Save || screen == Screen::Delete) && confirm) {
    const bool changed = screen == Screen::Delete ? records.erase(selected)
                                                  : records.put(selected, draft.title, draft.username, draft.password);
    securestore::secureZero(&draft, sizeof(draft));
    if (!changed || !save(false))
      fail();
    else {
      selected = 0;
      screen = Screen::List;
    }
  }
  requestUpdate();
}
void PasswordManagerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& m = UITheme::getInstance().getMetrics();
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  GUI.drawHeader(renderer, Rect{ml, mt + m.topPadding, renderer.getScreenWidth() - ml - mr, m.headerHeight},
                 mode == Mode::WifiQr          ? tr(STR_WIFI_QR_APP)
                 : passwords()                 ? tr(STR_VAULT_APP)
                 : mode == Mode::Authenticator ? tr(STR_TOTP_APP)
                                               : tr(STR_TOTP_QR_APP));
  int y = mt + m.topPadding + m.headerHeight + 20;
  if (screen == Screen::Locked || screen == Screen::Error) {
    if (mode == Mode::WifiQr) renderer.drawCenteredText(SMALL_FONT_ID, y + 85, tr(STR_WIFI_QR_SOURCE));
    renderer.drawCenteredText(UI_10_FONT_ID, y,
                              screen == Screen::Error ? tr(STR_VAULT_ERROR)
                              : existing              ? tr(STR_VAULT_UNLOCK)
                                                      : tr(STR_VAULT_CREATE));
    renderer.drawCenteredText(SMALL_FONT_ID, y + 45, tr(STR_VAULT_WIP));
    if (mode == Mode::Duress || mode == Mode::DecoySetup) {
      renderer.drawCenteredText(SMALL_FONT_ID, y + 85,
                                mode == Mode::DecoySetup ? tr(STR_VAULT_DECOY_SETUP_NOTE) : tr(STR_VAULT_DURESS_NOTE));
      renderer.drawCenteredText(SMALL_FONT_ID, y + 115, tr(STR_VAULT_DURESS_LIMIT));
    }
  } else if (screen == Screen::Save || screen == Screen::Delete) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, screen == Screen::Save ? tr(STR_VAULT_SAVE) : tr(STR_VAULT_DELETE));
  } else if (screen == Screen::Detail) {
    const auto* record = records.at(selected);
    if (record) {
      renderer.drawCenteredText(UI_10_FONT_ID, y, record->title);
      if (mode == Mode::WifiQr) {
        renderer.drawCenteredText(SMALL_FONT_ID, y + 30, tr(STR_WIFI_QR_CONTROLS));
        renderer.drawCenteredText(SMALL_FONT_ID, y + 55,
                                  wifiAuth == 0   ? tr(STR_WIFI_QR_WPA)
                                  : wifiAuth == 1 ? tr(STR_WIFI_QR_WEP)
                                                  : tr(STR_WIFI_QR_OPEN));
        if (!reveal || !codeValid)
          renderer.drawCenteredText(UI_10_FONT_ID, y + 100, !reveal ? tr(STR_VAULT_MASKED) : tr(STR_WIFI_QR_INVALID));
        else {
          const int top = y + 80;
          const int width = renderer.getScreenWidth() - ml - mr;
          const int height = renderer.getScreenHeight() - mb - m.buttonHintsHeight - top;
          const int scale = std::min(width, height) / 61;  // v9: 53 modules + quiet zone
          if (scale > 0) {
            const int x = ml + (width - 53 * scale) / 2;
            for (uint8_t row = 0; row < 53; ++row)
              for (uint8_t col = 0; col < 53; ++col)
                if (qrcode_getModule(&qr, col, row))
                  renderer.fillRect(x + col * scale, top + (row + 4) * scale, scale, scale, true);
          }
        }
      } else if (passwords()) {
        renderer.drawCenteredText(UI_10_FONT_ID, y + 40, record->username);
        renderer.drawCenteredText(UI_10_FONT_ID, y + 80, reveal ? record->password : tr(STR_VAULT_MASKED));
        renderer.drawCenteredText(SMALL_FONT_ID, y + 130, tr(STR_VAULT_ACTIONS));
      } else {
        renderer.drawCenteredText(UI_12_FONT_ID, y + 40,
                                  !reveal     ? tr(STR_VAULT_MASKED)
                                  : codeValid ? code
                                              : tr(STR_TOTP_SYNC));
        renderer.drawCenteredText(SMALL_FONT_ID, y + 75, tr(STR_VAULT_ACTIONS));
        if (mode == Mode::Authenticator) renderer.drawCenteredText(SMALL_FONT_ID, y + 115, tr(STR_TOTP_PARAMETERS));
        if (mode == Mode::TotpQr && reveal && codeValid) {
          const int top = y + 100;
          const int width = renderer.getScreenWidth() - ml - mr;
          const int height = renderer.getScreenHeight() - mb - m.buttonHintsHeight - top;
          const int scale = std::min(width, height) / 29;  // 21 modules + four-module quiet zone each side
          if (scale > 0) {
            const int x = ml + (width - 21 * scale) / 2;
            const int startY = top + 4 * scale;
            for (uint8_t row = 0; row < 21; ++row)
              for (uint8_t col = 0; col < 21; ++col)
                if (qrcode_getModule(&qr, col, row))
                  renderer.fillRect(x + col * scale, startY + row * scale, scale, scale, true);
          }
        }
      }
    }
  } else {
    const size_t rows = std::max(1, (renderer.getScreenHeight() - mb - m.buttonHintsHeight - y) / 40);
    const size_t first = selected / rows * rows;
    if (mode == Mode::WifiQr && !records.size()) renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_WIFI_QR_SOURCE));
    const size_t total =
        records.size() + (mode != Mode::WifiQr && records.size() < PasswordRecords::kMaxRecords ? 1 : 0);
    for (size_t i = first; i < std::min(first + rows, total); ++i) {
      renderer.drawText(UI_10_FONT_ID, ml + 10, y, i == selected ? ">" : "");
      renderer.drawText(UI_10_FONT_ID, ml + 35, y, i < records.size() ? records.at(i)->title : tr(STR_VAULT_ADD));
      y += 40;
    }
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
