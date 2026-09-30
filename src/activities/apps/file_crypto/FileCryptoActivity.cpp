#include "FileCryptoActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/apps/secure_vault/SecretEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr const char* inbox = "/crossink/to-encrypt";
constexpr const char* encrypted = "/crossink/encrypted";
constexpr const char* exported = "/crossink/decrypted";
bool safeName(const char* name) {
  const size_t n = strnlen(name, 65);
  if (!n || n == 65 || name[0] == '.') return false;
  for (size_t i = 0; i < n; ++i) {
    const char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
          c == '.'))
      return false;
  }
  return true;
}
}  // namespace
FileCryptoActivity::~FileCryptoActivity() { wipe(); }
void FileCryptoActivity::wipe() {
  securestore::secureZero(plain, sizeof(plain));
  securestore::secureZero(blob, sizeof(blob));
  securestore::secureZero(entry, sizeof(entry));
  securestore::secureZero(key, sizeof(key));
}
void FileCryptoActivity::fail() {
  LOG_ERR("FileCrypto", "File/crypto operation failed; clearing secret buffers");
  wipe();
  state = State::Error;
  requestUpdate();
}
void FileCryptoActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}
void FileCryptoActivity::onExit() {
  wipe();
  renderer.clearScreen();
  Activity::onExit();
}
void FileCryptoActivity::scan() {
  count = selected = 0;
  limited = false;
  const char* root = decrypt ? encrypted : inbox;
  if (!Storage.ensureDirectoryExists(root)) {
    fail();
    return;
  }
  auto dir = Storage.open(root);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    fail();
    return;
  }
  bool ok = true;
  unsigned inspected = 0;
  while (count < 16 && inspected < 512) {
    auto file = dir.openNextFile();
    if (!file) {
      if (file.allocationFailed()) ok = false;
      break;
    }
    ++inspected;
    char name[66]{};  // One extra byte detects filenames too long for the retained 65-byte slot.
    file.getName(name, sizeof(name));
    const size_t size = file.fileSize();
    const bool eligible = !file.isDirectory() && safeName(name) &&
                          (decrypt ? size >= securestore::kHeaderBytes && size <= sizeof(blob) : size <= sizeof(plain));
    if (!file.close()) {
      ok = false;
      break;
    }
    if (eligible) memcpy(names[count++], name, strlen(name) + 1);
  }
  limited = count == 16 || inspected == 512;
  if (!dir.close()) ok = false;
  if (!ok) {
    fail();
    return;
  }
  state = State::Files;
  requestUpdate();
}
void FileCryptoActivity::ask(bool repeat) {
  auto child = makeUniqueNoThrow<SecretEntryActivity>(
      renderer, mappedInput, repeat ? StrId::STR_VAULT_CONFIRM_KEY : StrId::STR_VAULT_KEY, entry, sizeof(entry), 8);
  if (!child) {
    LOG_ERR("FileCrypto", "Secret input allocation failed");
    fail();
    return;
  }
  startActivityForResult(std::move(child), [this, repeat](const ActivityResult& result) {
    RenderLock lock;
    if (result.isCancelled) {
      wipe();
      state = State::Files;
      requestUpdate();
      return;
    }
    if (!repeat) {
      memcpy(key, entry, sizeof(key));
      securestore::secureZero(entry, sizeof(entry));
      if (!decrypt) {
        ask(true);
        return;
      }
    } else {
      unsigned difference = 0;
      for (unsigned i = 0; i < sizeof(key); ++i) difference |= key[i] ^ entry[i];
      if (difference) {
        fail();
        return;
      }
    }
    const bool ok = process();
    wipe();
    if (!ok) {
      fail();
      return;
    }
    state = State::Done;
    requestUpdate();
  });
}
bool FileCryptoActivity::process() {
  char source[96];
  snprintf(source, sizeof(source), "%s/%s", decrypt ? encrypted : inbox, names[selected]);
  auto input = Storage.open(source);
  if (!input) return false;
  const size_t size = input.fileSize();
  uint8_t* data = decrypt ? blob : plain;
  bool ok = !input.isDirectory() && size <= (decrypt ? sizeof(blob) : sizeof(plain)) &&
            (!decrypt || size >= securestore::kHeaderBytes);
  if (ok && size) ok = input.read(data, size) == size;
  if (!input.close()) ok = false;
  if (!ok) return false;
  size_t outputSize = 0;
  securestore::Status status;
  if (decrypt)
    status = securestore::decrypt(blob, size, key, plain, sizeof(plain), &outputSize);
  else
    status = securestore::encrypt(plain, size, key, blob, sizeof(blob), &outputSize);
  if (status != securestore::Status::Ok) {
    LOG_ERR("FileCrypto", "Crypto failed: %s", securestore::statusName(status));
    return false;
  }
  const char* root = decrypt ? exported : encrypted;
  if (!Storage.ensureDirectoryExists(root)) return false;
  for (unsigned i = 0; i < 100; ++i) {
    snprintf(destination, sizeof(destination), "%s/file-%02u.bin", root, i);
    if (Storage.exists(destination)) continue;
    auto output = Storage.open(destination, O_WRITE | O_CREAT | O_EXCL);
    if (!output) return false;
    ok = !outputSize || output.write(decrypt ? plain : blob, outputSize) == outputSize;
    if (ok) ok = output.sync();
    if (!output.close()) ok = false;
    if (!ok && !Storage.remove(destination)) LOG_ERR("FileCrypto", "Partial output remains: %s", destination);
    return ok;
  }
  return false;
}
void FileCryptoActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    if (state == State::Mode) {
      finish();
      return;
    }
    RenderLock lock(*this);
    wipe();
    state = State::Mode;
    requestUpdate();
    return;
  }
  RenderLock lock(*this);
  const bool left = mappedInput.wasPressed(MappedInputManager::Button::Left);
  const bool right = mappedInput.wasPressed(MappedInputManager::Button::Right);
  const bool confirm = mappedInput.wasPressed(MappedInputManager::Button::Confirm);
  if (state == State::Mode) {
    if (left || right) decrypt = !decrypt;
    if (confirm) scan();
  } else if (state == State::Files && count) {
    if (left) selected = (selected + count - 1) % count;
    if (right) selected = (selected + 1) % count;
    if (confirm) ask(false);
  }
  if (left || right || confirm) requestUpdate();
}
void FileCryptoActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  GUI.drawHeader(renderer, header, tr(STR_FILE_CRYPTO_APP));
  const Rect area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = area.y + line;
  auto draw = [&](const char* text) {
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, y, text);
    y += line;
  };
  draw(decrypt ? tr(STR_FILE_CRYPTO_DECRYPT) : tr(STR_FILE_CRYPTO_ENCRYPT));
  draw(tr(STR_FILE_CRYPTO_BOUND));
  if (state == State::Error)
    draw(tr(STR_FILE_CRYPTO_ERROR));
  else if (state == State::Done) {
    draw(tr(STR_FILE_CRYPTO_SAVED));
    draw(destination);
  } else {
    draw(decrypt ? tr(STR_FILE_CRYPTO_ENCRYPTED_FOLDER) : tr(STR_FILE_CRYPTO_INBOX));
    if (state == State::Files) {
      draw(count ? names[selected] : tr(STR_FILE_CRYPTO_EMPTY));
      if (limited) draw(tr(STR_FILE_CRYPTO_LIMITED));
    }
    draw(tr(STR_FILE_CRYPTO_CONTROLS));
  }
  draw(tr(STR_FILE_CRYPTO_ORIGINALS));
  draw(tr(STR_FILE_CRYPTO_EXPORTS));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
