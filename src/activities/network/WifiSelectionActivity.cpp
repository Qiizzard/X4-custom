#include "WifiSelectionActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>
#include <Logging.h>
#include <RadioManager.h>

#include <algorithm>
#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "SdCardFontSystem.h"
#include "WifiCredentialStore.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {

constexpr fui::ActionId ACTION_ROW = 1;

std::string getDisplayMacAddress() {
  uint8_t mac[6] = {};

  RADIO.stationMac(mac);

  char macStr[64];
  snprintf(macStr, sizeof(macStr), "%s %02x-%02x-%02x-%02x-%02x-%02x", tr(STR_MAC_ADDRESS), mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
  return std::string(macStr);
}

}  // namespace

WifiSelectionActivity::WifiSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             const bool autoConnect, const bool useReaderButtonHints,
                                             const char* parentRadioOwner)
    : Activity("WifiSelection", renderer, mappedInput),
      allowAutoConnect(autoConnect),
      useReaderButtonHints(useReaderButtonHints),
      parentRadioOwner(parentRadioOwner),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void WifiSelectionActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<WifiSelectionActivity*>(user);
  if (self->state != WifiSelectionState::NETWORK_LIST) return;
  if (event.value < 0 || event.value >= static_cast<int16_t>(self->networks.size())) return;
  self->selectedNetworkIndex = static_cast<size_t>(event.value);
  // Long-press a saved network to forget it (mirrors the Left-button hold in loop()).
  if (event.longPress) {
    if (self->networks[self->selectedNetworkIndex].hasSavedPassword) {
      self->selectedSSID = self->networks[self->selectedNetworkIndex].ssid;
      self->state = WifiSelectionState::FORGET_PROMPT;
      self->forgetPromptSelection = 0;  // Default to "Cancel"
      self->app.clearTapFlash();
      self->requestUpdate();
    }
    return;
  }
  // Selection leaves this screen (password entry / connecting); a lingering
  // flash would gray an unrelated row.
  self->app.clearTapFlash();
  self->selectNetwork(static_cast<int>(self->selectedNetworkIndex));
}

bool WifiSelectionActivity::hasRadioAccess() const {
  return parentRadioOwner && RADIO.mode() == RadioManager::Mode::WifiStation && RADIO.owner() == parentRadioOwner;
}

bool WifiSelectionActivity::requireRadioAccess() {
  if (!radioAccessDenied && hasRadioAccess()) return true;
  if (!radioAccessDenied) {
    LOG_ERR("WIFI", "Picker radio authorization unavailable");
    radioAccessDenied = true;
    state = WifiSelectionState::CONNECTION_FAILED;
    connectionError = tr(STR_RADIO_BUSY_OR_UNAVAILABLE);
    requestUpdate();
  }
  return false;
}

void WifiSelectionActivity::onEnter() {
  Activity::onEnter();
  sdFontSystem.releaseLoadedFont(renderer);
  if (!requireRadioAccess()) return;

  // Reset state
  selectedNetworkIndex = 0;
  networks.clear();
  realNetworkCount = 0;
  state = WifiSelectionState::SCANNING;
  selectedSSID.clear();
  connectedIP.clear();
  connectionError.clear();
  enteredPassword.clear();
  usedSavedPassword = false;
  tearDownWifiOnExit = true;
  savePromptSelection = 0;
  forgetPromptSelection = 0;
  autoConnecting = false;
  lastConnectionStatusLogTime = 0;
  lastLoggedWifiStatus = -1;
  manualNetworkListRequested = false;
  autoAttemptedSsids.clear();
  const size_t savedCredentialCount = WIFI_STORE.getCredentialCount();
  autoAttemptedSsids.reserve(savedCredentialCount);

  // Cache MAC address for display
  cachedMacAddress = getDisplayMacAddress();

  uiReady = false;
  visibleRows = 1;
  topIndex = 0;
  app.setTheme(uiThemeTokens(uiTarget));
  app.on(ACTION_ROW, &WifiSelectionActivity::onRowEvent, this);
  app.setScreen(&WifiSelectionActivity::listScreen, this);

  // Trigger first update to show scanning message
  requestUpdate();

  // Attempt to auto-connect to known networks. Try the last successful
  // network first for speed, then scan and try any visible saved networks by
  // signal strength. The user can interrupt this and show the scan result.
  if (allowAutoConnect && savedCredentialCount != 0) {
    const std::string lastSsid = WIFI_STORE.getLastConnectedSsid();
    if (!lastSsid.empty()) {
      const auto cred = WIFI_STORE.findCredential(lastSsid);
      if (cred && tryAutoConnectCredential(*cred)) {
        return;
      }
    }

    startWifiScan(true);
    return;
  }

  // Fallback to scanning
  startWifiScan();
}

void WifiSelectionActivity::onExit() {
  Activity::onExit();
  if (radioAccessDenied || !hasRadioAccess()) return;

  // Stop any ongoing WiFi scan
  RADIO.clearPickerScan(parentRadioOwner);

  // Success leaves the parent connected. Cancellation disconnects while the
  // parent retains its reservation and performs final shutdown.
  if (tearDownWifiOnExit) {
    RADIO.setPickerEventLogging(parentRadioOwner, false);
    RADIO.disconnectPicker(parentRadioOwner, true);
  }

  LOG_DBG("WIFI", "Free heap at onExit end: %d bytes", ESP.getFreeHeap());
}

void WifiSelectionActivity::startWifiScan(const bool autoScan) {
  if (!requireRadioAccess()) return;
  autoConnecting = autoScan;
  manualNetworkListRequested = false;
  topIndex = 0;
  state = WifiSelectionState::SCANNING;
  networks.clear();
  requestUpdate();

  const int scanStartResult = RADIO.startPickerScan(parentRadioOwner);
  scanStartFailed = scanStartResult == RadioManager::kScanFailed;
  LOG_INF("WIFI", "WiFi scan requested (result=%d)", scanStartResult);
}

void WifiSelectionActivity::processWifiScanResults() {
  const int scanResult = scanStartFailed ? RadioManager::kScanFailed : RADIO.pickerScanCount(parentRadioOwner);

  if (scanResult == RadioManager::kScanRunning) {
    // Scan still in progress
    return;
  }

  if (scanResult == RadioManager::kScanFailed) {
    LOG_INF("WIFI", "WiFi scan failed");
    networks.clear();
    realNetworkCount = 0;
    appendHiddenNetworkEntry();
    autoConnecting = false;
    manualNetworkListRequested = false;
    state = WifiSelectionState::NETWORK_LIST;
    selectedNetworkIndex = 0;
    requestUpdate();
    return;
  }

  LOG_INF("WIFI", "WiFi scan complete: retainedResults=%d", scanResult);

  // Scan complete, process results: deduplicate in-place, keeping strongest signal
  networks.clear();
  networks.reserve(static_cast<size_t>(scanResult) + 1);  // Includes the hidden-network action.
  int hiddenNetworks = 0;
  int duplicateNetworks = 0;

  for (int i = 0; i < scanResult; i++) {
    RadioManager::ScanResult entry{};
    if (!RADIO.pickerScanResult(parentRadioOwner, static_cast<size_t>(i), entry)) continue;
    const char* ssid = entry.ssid;
    const int32_t rssi = entry.rssi;
    // Skip hidden networks (empty SSID)
    if (ssid[0] == '\0') {
      hiddenNetworks++;
      continue;
    }

    auto it =
        std::find_if(networks.begin(), networks.end(), [&ssid](const WifiNetworkInfo& n) { return n.ssid == ssid; });
    if (it != networks.end()) {
      duplicateNetworks++;
    }
    if (it == networks.end()) {
      WifiNetworkInfo network;
      network.ssid = ssid;
      network.rssi = rssi;
      network.isEncrypted = entry.encrypted;
      network.hasSavedPassword = WIFI_STORE.hasSavedCredential(network.ssid);
      networks.push_back(std::move(network));
    } else if (rssi > it->rssi) {
      it->rssi = rssi;
      it->isEncrypted = entry.encrypted;
    }
  }

  // Sort: saved-password networks first, then by signal strength (strongest first)
  std::sort(networks.begin(), networks.end(), [](const WifiNetworkInfo& a, const WifiNetworkInfo& b) {
    if (a.hasSavedPassword != b.hasSavedPassword) {
      return a.hasSavedPassword;
    }
    return a.rssi > b.rssi;
  });

  realNetworkCount = networks.size();
  appendHiddenNetworkEntry();

  RADIO.clearPickerScan(parentRadioOwner);
  LOG_INF("WIFI", "WiFi scan usable networks=%zu hidden=%d duplicates=%d", realNetworkCount, hiddenNetworks,
          duplicateNetworks);

  if (autoConnecting && !manualNetworkListRequested && tryNextSavedNetworkFromScan()) {
    return;
  }

  autoConnecting = false;
  manualNetworkListRequested = false;
  state = WifiSelectionState::NETWORK_LIST;
  selectedNetworkIndex = 0;
  requestUpdate();
}

void WifiSelectionActivity::appendHiddenNetworkEntry() {
  // Synthetic list entry that lets the user type an SSID that is not broadcast.
  // ESP32 can join hidden APs as long as the SSID is supplied to WiFi.begin().
  WifiNetworkInfo placeholder;
  placeholder.rssi = 0;
  placeholder.isEncrypted = true;  // Treated as encrypted; an empty password still connects open APs
  placeholder.hasSavedPassword = false;
  placeholder.isHiddenPlaceholder = true;
  networks.push_back(std::move(placeholder));
}

void WifiSelectionActivity::selectNetwork(const int index) {
  if (index < 0 || index >= static_cast<int>(networks.size())) {
    return;
  }

  const auto& network = networks[index];

  // Synthetic "Add hidden network..." entry: prompt the user to type the SSID first
  if (network.isHiddenPlaceholder) {
    promptHiddenSsid();
    return;
  }

  selectedSSID = network.ssid;
  selectedRequiresPassword = network.isEncrypted;
  usedSavedPassword = false;
  enteredPassword.clear();
  autoConnecting = false;

  // Check if we have saved credentials for this network
  const auto savedCred = WIFI_STORE.findCredential(selectedSSID);
  if (savedCred && !savedCred->password.empty()) {
    // Use saved password - connect directly
    enteredPassword = savedCred->password;
    usedSavedPassword = true;
    LOG_INF("WIFI", "Selected network: ssid=%s encrypted=%d saved=1 rssi=%d", selectedSSID.c_str(),
            selectedRequiresPassword, network.rssi);
    attemptConnection();
    return;
  }

  if (selectedRequiresPassword) {
    promptPasswordEntry();
  } else {
    // Connect directly for open networks
    LOG_INF("WIFI", "Selected open network: ssid=%s rssi=%d", selectedSSID.c_str(), network.rssi);
    attemptConnection();
  }
}

void WifiSelectionActivity::promptPasswordEntry() {
  // Show password entry
  state = WifiSelectionState::PASSWORD_ENTRY;
  // Don't allow screen updates while changing activity
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ENTER_WIFI_PASSWORD),
                                                                 "",  // No initial text
                                                                 64,  // Max password length
                                                                 InputType::Password),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             state = WifiSelectionState::NETWORK_LIST;
                           } else {
                             enteredPassword = std::get<KeyboardResult>(result.data).text;
                             // state will be updated in next loop iteration
                           }
                         });
}

void WifiSelectionActivity::promptHiddenSsid() {
  selectedSSID.clear();
  selectedRequiresPassword = true;  // Hidden networks are usually encrypted; empty password still joins open APs
  usedSavedPassword = false;
  enteredPassword.clear();
  autoConnecting = false;

  // Suppress rendering during the activity transition (see render()).
  state = WifiSelectionState::HIDDEN_SSID_ENTRY;
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ENTER_WIFI_SSID),
                                                                 "",  // No initial text
                                                                 32,  // Max SSID length (IEEE 802.11: 32 bytes)
                                                                 InputType::Text),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             state = WifiSelectionState::NETWORK_LIST;
                             return;
                           }
                           selectedSSID = std::get<KeyboardResult>(result.data).text;
                           if (selectedSSID.empty()) {
                             state = WifiSelectionState::NETWORK_LIST;
                           }
                           // Otherwise stay in HIDDEN_SSID_ENTRY; loop() continues the flow.
                         });
}

bool WifiSelectionActivity::hasAttemptedAutoSsid(const std::string& ssid) const {
  return std::find(autoAttemptedSsids.begin(), autoAttemptedSsids.end(), ssid) != autoAttemptedSsids.end();
}

bool WifiSelectionActivity::tryAutoConnectCredential(const WifiCredential& cred) {
  if (hasAttemptedAutoSsid(cred.ssid)) {
    return false;
  }

  LOG_DBG("WIFI", "Attempting saved network: %s", cred.ssid.c_str());
  autoAttemptedSsids.push_back(cred.ssid);
  selectedSSID = cred.ssid;
  enteredPassword = cred.password;
  selectedRequiresPassword = !cred.password.empty();
  usedSavedPassword = true;
  autoConnecting = true;
  manualNetworkListRequested = false;
  attemptConnection();
  requestUpdate();
  return true;
}

bool WifiSelectionActivity::tryNextSavedNetworkFromScan() {
  for (const auto& network : networks) {
    if (!network.hasSavedPassword || hasAttemptedAutoSsid(network.ssid)) {
      continue;
    }

    const auto cred = WIFI_STORE.findCredential(network.ssid);
    if (cred && tryAutoConnectCredential(*cred)) {
      return true;
    }
  }
  return false;
}

void WifiSelectionActivity::handleAutoConnectFailure() {
  LOG_DBG("WIFI", "Saved network failed: %s", selectedSSID.c_str());
  RADIO.disconnectPicker(parentRadioOwner);

  if (!networks.empty()) {
    if (tryNextSavedNetworkFromScan()) {
      return;
    }
    autoConnecting = false;
    state = WifiSelectionState::NETWORK_LIST;
    selectedNetworkIndex = 0;
    requestUpdate();
    return;
  }

  startWifiScan(true);
}

void WifiSelectionActivity::showNetworkListFromAutoConnect() {
  RADIO.disconnectPicker(parentRadioOwner);
  autoConnecting = false;
  manualNetworkListRequested = true;

  if (networks.empty()) {
    startWifiScan(false);
    return;
  }

  state = WifiSelectionState::NETWORK_LIST;
  selectedNetworkIndex = 0;
  requestUpdate();
}

void WifiSelectionActivity::attemptConnection() {
  if (!requireRadioAccess()) return;
  state = autoConnecting ? WifiSelectionState::AUTO_CONNECTING : WifiSelectionState::CONNECTING;
  connectionStartTime = millis();
  connectedIP.clear();
  connectionError.clear();
  lastConnectionStatusLogTime = 0;
  lastLoggedWifiStatus = -1;
  requestUpdate();

  LOG_INF("WIFI", "Connecting to ssid=%s auto=%d saved=%d encrypted=%d passProvided=%d heap=%u maxAlloc=%u",
          selectedSSID.c_str(), autoConnecting, usedSavedPassword, selectedRequiresPassword, !enteredPassword.empty(),
          ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  if (!RADIO.preparePickerConnection(parentRadioOwner)) {
    connectionError = tr(STR_RADIO_BUSY_OR_UNAVAILABLE);
    state = WifiSelectionState::CONNECTION_FAILED;
    requestUpdate();
    return;
  }

  const char* password = selectedRequiresPassword && !enteredPassword.empty() ? enteredPassword.c_str() : nullptr;
  const int beginStatus = RADIO.beginPickerConnection(parentRadioOwner, selectedSSID.c_str(), password);
  LOG_INF("WIFI", "Connection begin returned status=%d", beginStatus);
}

void WifiSelectionActivity::checkConnectionStatus() {
  if (state != WifiSelectionState::CONNECTING && state != WifiSelectionState::AUTO_CONNECTING) {
    return;
  }

  RadioManager::PickerStatus snapshot;
  if (!RADIO.pickerStatus(parentRadioOwner, snapshot)) {
    connectionError = tr(STR_RADIO_BUSY_OR_UNAVAILABLE);
    state = WifiSelectionState::CONNECTION_FAILED;
    requestUpdate();
    return;
  }
  const int status = snapshot.code;
  const unsigned long now = millis();

  if (lastLoggedWifiStatus != static_cast<int>(status) ||
      now - lastConnectionStatusLogTime >= CONNECTION_STATUS_LOG_INTERVAL_MS) {
    LOG_INF("WIFI", "Connection poll: elapsed=%lums status=%d/%s rssi=%d", now - connectionStartTime,
            static_cast<int>(status), snapshot.name, snapshot.rssi);
    lastLoggedWifiStatus = static_cast<int>(status);
    lastConnectionStatusLogTime = now;
  }

  if (snapshot.connected) {
    // Successfully connected
    const auto& ip = snapshot.ip;
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    connectedIP = ipStr;
    autoConnecting = false;
    RADIO.setPickerEventLogging(parentRadioOwner, false);
    LOG_INF("WIFI", "Connected to ssid=%s ip=%s rssi=%d", selectedSSID.c_str(), connectedIP.c_str(), snapshot.rssi);

#if defined(ENABLE_SERIAL_LOG) && LOG_LEVEL >= 2
    const auto& connectedBssid = snapshot.bssid;
    LOG_DBG("WIFI", "Connected BSSID: %02x:%02x:%02x:%02x:%02x:%02x, channel: %d, RSSI: %d dBm",
            static_cast<unsigned>(connectedBssid[0]), static_cast<unsigned>(connectedBssid[1]),
            static_cast<unsigned>(connectedBssid[2]), static_cast<unsigned>(connectedBssid[3]),
            static_cast<unsigned>(connectedBssid[4]), static_cast<unsigned>(connectedBssid[5]), snapshot.channel,
            snapshot.rssi);
#endif

    // Sync RTC from NTP on the first successful WiFi connection only. Users can force a re-sync from
    // Settings > System > Device > Sync Date/Time Now.
    if (halClock.isAvailable() && (!SETTINGS.clockHasBeenSynced || !SETTINGS.clockDateHasBeenSynced)) {
      if (halClock.syncFromNTP()) {
        SETTINGS.clockHasBeenSynced = 1;
        SETTINGS.clockDateHasBeenSynced = 1;
        SETTINGS.saveToFile();
      }
    }

    // Save this as the last connected network - SD card operations need lock as
    // we use SPI for both
    {
      RenderLock lock(*this);
      WIFI_STORE.setLastConnectedSsid(selectedSSID);
    }

    // If we entered a new password, ask if user wants to save it
    // Otherwise, immediately complete so parent can start web server
    if (!usedSavedPassword && !enteredPassword.empty()) {
      state = WifiSelectionState::SAVE_PROMPT;
      savePromptSelection = 0;  // Default to "Yes"
      requestUpdate();
    } else {
      // Using saved password or open network - complete immediately
      if (allowAutoConnect) {
        LOG_DBG("WIFI",
                "Connected with saved/open credentials, "
                "completing immediately");
        onComplete(true);
      } else {
        state = WifiSelectionState::CONNECTED;
        requestUpdate();
      }
    }
    return;
  }

  if (snapshot.failed) {
    connectionError = tr(STR_ERROR_GENERAL_FAILURE);
    if (snapshot.networkNotFound) {
      connectionError = tr(STR_ERROR_NETWORK_NOT_FOUND);
    }
    LOG_INF("WIFI", "Connection failed: ssid=%s status=%d/%s elapsed=%lums", selectedSSID.c_str(),
            static_cast<int>(status), snapshot.name, now - connectionStartTime);
    RADIO.logPickerDisconnectReason(parentRadioOwner);
    RADIO.setPickerEventLogging(parentRadioOwner, false);
    if (autoConnecting) {
      handleAutoConnectFailure();
      return;
    }
    state = WifiSelectionState::CONNECTION_FAILED;
    requestUpdate();
    return;
  }

  // Check for timeout
  const unsigned long timeoutMs = autoConnecting ? AUTO_CONNECTION_TIMEOUT_MS : CONNECTION_TIMEOUT_MS;
  if (millis() - connectionStartTime > timeoutMs) {
    RADIO.disconnectPicker(parentRadioOwner);
    connectionError = tr(STR_ERROR_CONNECTION_TIMEOUT);
    LOG_INF("WIFI", "Connection timed out: ssid=%s elapsed=%lums lastStatus=%d/%s", selectedSSID.c_str(),
            millis() - connectionStartTime, static_cast<int>(status), snapshot.name);
    RADIO.logPickerDisconnectReason(parentRadioOwner);
    RADIO.setPickerEventLogging(parentRadioOwner, false);
    if (autoConnecting) {
      handleAutoConnectFailure();
      return;
    }
    state = WifiSelectionState::CONNECTION_FAILED;
    requestUpdate();
    return;
  }
}

void WifiSelectionActivity::loop() {
  if (!requireRadioAccess()) {
    if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
        mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      onComplete(false);
    }
    return;
  }
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    switch (state) {
      case WifiSelectionState::SCANNING:
        RADIO.setPickerEventLogging(parentRadioOwner, false);
        RADIO.clearPickerScan(parentRadioOwner);
        onComplete(false);
        return;
      case WifiSelectionState::CONNECTING:
      case WifiSelectionState::AUTO_CONNECTING:
        RADIO.setPickerEventLogging(parentRadioOwner, false);
        RADIO.disconnectPicker(parentRadioOwner);
        onComplete(false);
        return;
      case WifiSelectionState::SAVE_PROMPT:
      case WifiSelectionState::CONNECTED:
        onComplete(true);
        return;
      case WifiSelectionState::FORGET_PROMPT:
        startWifiScan();
        return;
      case WifiSelectionState::CONNECTION_FAILED:
        if (autoConnecting || usedSavedPassword) {
          autoConnecting = false;
          state = WifiSelectionState::FORGET_PROMPT;
          forgetPromptSelection = 0;
        } else {
          state = WifiSelectionState::NETWORK_LIST;
        }
        requestUpdate();
        return;
      case WifiSelectionState::NETWORK_LIST:
        onComplete(false);
        return;
      default:
        break;
    }
  }

  if ((state == WifiSelectionState::SCANNING || state == WifiSelectionState::CONNECTING ||
       state == WifiSelectionState::AUTO_CONNECTING) &&
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    RADIO.setPickerEventLogging(parentRadioOwner, false);
    if (state == WifiSelectionState::SCANNING) {
      RADIO.clearPickerScan(parentRadioOwner);
    } else {
      RADIO.disconnectPicker(parentRadioOwner);
    }
    mappedInput.suppressNextBackRelease();
    onComplete(false);
    return;
  }

  // Check scan progress
  if (state == WifiSelectionState::SCANNING) {
    if (autoConnecting && mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      autoConnecting = false;
      manualNetworkListRequested = true;
      requestUpdate();
    }
    processWifiScanResults();
    return;
  }

  // Check connection progress
  if (state == WifiSelectionState::CONNECTING || state == WifiSelectionState::AUTO_CONNECTING) {
    if (state == WifiSelectionState::AUTO_CONNECTING) {
      if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        showNetworkListFromAutoConnect();
        return;
      }
    }
    checkConnectionStatus();
    return;
  }

  // Reached once the hidden-network SSID has been entered (and was non-empty).
  if (state == WifiSelectionState::HIDDEN_SSID_ENTRY) {
    const auto savedCred = WIFI_STORE.findCredential(selectedSSID);
    if (savedCred && !savedCred->password.empty()) {
      // We already know this hidden network - connect with the saved password
      enteredPassword = savedCred->password;
      usedSavedPassword = true;
      attemptConnection();
    } else {
      // Prompt for the password (empty password connects to open hidden APs)
      promptPasswordEntry();
    }
    return;
  }

  if (state == WifiSelectionState::PASSWORD_ENTRY) {
    // Reach here once password entry finished in subactivity
    attemptConnection();
    return;
  }

  // Handle save prompt state
  if (state == WifiSelectionState::SAVE_PROMPT) {
    {
      const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
      const auto height = renderer.getLineHeight(UI_10_FONT_ID);
      const int buttonY = screen.y + (screen.height - height * 3) / 2 + 80;
      constexpr int buttonWidth = 60;
      constexpr int buttonSpacing = 30;
      const int startX = screen.x + (screen.width - (buttonWidth * 2 + buttonSpacing)) / 2;
      int touchedOption = -1;
      const auto touch = mappedInput.colTouch(touchedOption, startX - 8, buttonWidth + buttonSpacing, 2, buttonY - 8,
                                              buttonY + height + 8, buttonWidth + 16);
      if (touch == MappedInputManager::RowTouch::Down) {
        if (savePromptSelection != touchedOption) {
          savePromptSelection = touchedOption;
          requestUpdate();
        }
        return;
      }
      if (touch == MappedInputManager::RowTouch::Tap) {
        savePromptSelection = touchedOption;
        if (savePromptSelection == 0) {
          RenderLock lock(*this);
          WIFI_STORE.addCredential(selectedSSID, enteredPassword);
        }
        onComplete(true);
        return;
      }
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
        mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      if (savePromptSelection > 0) {
        savePromptSelection--;
        requestUpdate();
      }
    } else if (mappedInput.wasPressed(MappedInputManager::Button::Down) ||
               mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      if (savePromptSelection < 1) {
        savePromptSelection++;
        requestUpdate();
      }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (savePromptSelection == 0) {
        // User chose "Yes" - save the password
        RenderLock lock(*this);
        WIFI_STORE.addCredential(selectedSSID, enteredPassword);
      }
      // Complete - parent will start web server
      onComplete(true);
    } else if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      // Skip saving, complete anyway
      mappedInput.suppressNextBackRelease();
      onComplete(true);
    }
    return;
  }

  // Handle forget prompt state (connection failed with saved credentials)
  if (state == WifiSelectionState::FORGET_PROMPT) {
    {
      const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
      const auto height = renderer.getLineHeight(UI_10_FONT_ID);
      const int buttonY = screen.y + (screen.height - height * 3) / 2 + 80;
      constexpr int buttonWidth = 120;
      constexpr int buttonSpacing = 30;
      const int startX = screen.x + (screen.width - (buttonWidth * 2 + buttonSpacing)) / 2;
      int touchedOption = -1;
      const auto touch = mappedInput.colTouch(touchedOption, startX - 8, buttonWidth + buttonSpacing, 2, buttonY - 8,
                                              buttonY + height + 8, buttonWidth + 16);
      if (touch == MappedInputManager::RowTouch::Down) {
        if (forgetPromptSelection != touchedOption) {
          forgetPromptSelection = touchedOption;
          requestUpdate();
        }
        return;
      }
      if (touch == MappedInputManager::RowTouch::Tap) {
        forgetPromptSelection = touchedOption;
        if (forgetPromptSelection == 1) {
          RenderLock lock(*this);
          WIFI_STORE.removeCredential(selectedSSID);
          const auto network = find_if(networks.begin(), networks.end(),
                                       [this](const WifiNetworkInfo& net) { return net.ssid == selectedSSID; });
          if (network != networks.end()) {
            network->hasSavedPassword = false;
          }
        }
        startWifiScan();
        return;
      }
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
        mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      if (forgetPromptSelection > 0) {
        forgetPromptSelection--;
        requestUpdate();
      }
    } else if (mappedInput.wasPressed(MappedInputManager::Button::Down) ||
               mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      if (forgetPromptSelection < 1) {
        forgetPromptSelection++;
        requestUpdate();
      }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (forgetPromptSelection == 1) {
        RenderLock lock(*this);
        // User chose "Forget network" - forget the network
        WIFI_STORE.removeCredential(selectedSSID);
        // Update the network list to reflect the change
        const auto network = std::find_if(networks.begin(), networks.end(),
                                          [this](const WifiNetworkInfo& net) { return net.ssid == selectedSSID; });
        if (network != networks.end()) {
          network->hasSavedPassword = false;
        }
      }
      // Go back to network list (whether Cancel or Forget network was selected)
      startWifiScan();
    } else if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      // Skip forgetting, go back to network list
      startWifiScan();
    }
    return;
  }

  if (state == WifiSelectionState::CONNECTED) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      mappedInput.suppressNextBackRelease();
      onComplete(true);
      return;
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      onComplete(true);
    }
    return;
  }

  // Handle connection failed state
  if (state == WifiSelectionState::CONNECTION_FAILED) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      // If we were auto-connecting or using a saved credential, offer to forget
      // the network
      if (autoConnecting || usedSavedPassword) {
        autoConnecting = false;
        state = WifiSelectionState::FORGET_PROMPT;
        forgetPromptSelection = 0;  // Default to "Cancel"
      } else {
        // Go back to network list on failure for non-saved credentials
        state = WifiSelectionState::NETWORK_LIST;
      }
      requestUpdate();
      return;
    }
  }

  // Handle network list state
  if (state == WifiSelectionState::NETWORK_LIST) {
    // Check for Back button to exit (cancel)
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      mappedInput.suppressNextBackRelease();
      onComplete(false);
      return;
    }

    // Check for Confirm button to select network or rescan
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (!networks.empty()) {
        selectNetwork(selectedNetworkIndex);
      } else {
        startWifiScan();
      }
      return;
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      startWifiScan();
      return;
    }

    const bool leftPressed = mappedInput.wasPressed(MappedInputManager::Button::Left);
    if (leftPressed) {
      const bool hasSavedPassword = !networks.empty() && networks[selectedNetworkIndex].hasSavedPassword;
      if (hasSavedPassword) {
        selectedSSID = networks[selectedNetworkIndex].ssid;
        state = WifiSelectionState::FORGET_PROMPT;
        forgetPromptSelection = 0;  // Default to "Cancel"
        requestUpdate();
        return;
      }
    }

    // Touch goes through the FreeInkApp: render() registered the row hit
    // rects; route the snapshot and let onRowEvent dispatch.
    if (uiReady) {
      const fui::InputSnapshot snap = touchSnapshotFrom(mappedInput);
      if (snap.touchPressed || snap.touchReleased) {
        const auto event = app.route(snap);
        if (app.invalidated()) requestUpdate();
        if (event) return;  // dispatched to onRowEvent
      }
    }

    if (!networks.empty()) {
      // Swipes scroll the viewport; the selection stays put and button
      // navigation pulls the view back to it.
      const auto swipe = mappedInput.wasSwipe();
      if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
        const int delta = swipe == MappedInputManager::SwipeDir::Up ? visibleRows : -visibleRows;
        const int next = scrollListBy(topIndex, delta, visibleRows, static_cast<int>(networks.size()));
        if (next != topIndex) {
          topIndex = next;
          requestUpdate();
        }
        return;
      }
    }

    const auto moveSelection = [this](const int index) {
      selectedNetworkIndex = static_cast<size_t>(index);
      topIndex = followListSelection(static_cast<int>(selectedNetworkIndex), topIndex, visibleRows,
                                     static_cast<int>(networks.size()));
      requestUpdate();
    };
    buttonNavigator.onNext(
        [this, &moveSelection] { moveSelection(ButtonNavigator::nextIndex(selectedNetworkIndex, networks.size())); });
    buttonNavigator.onPrevious([this, &moveSelection] {
      moveSelection(ButtonNavigator::previousIndex(selectedNetworkIndex, networks.size()));
    });
  }
}

std::string WifiSelectionActivity::getSignalStrengthIndicator(const int32_t rssi) const {
  // Convert RSSI to signal bars representation
  if (rssi >= -50) {
    return "||||";  // Excellent
  }
  if (rssi >= -60) {
    return " |||";  // Good
  }
  if (rssi >= -70) {
    return "  ||";  // Fair
  }
  return "   |";  // Very weak
}

void WifiSelectionActivity::render(RenderLock&&) {
  // Don't render if we're in a keyboard-entry state - we're just transitioning
  // from the keyboard subactivity back to the main activity
  if (state == WifiSelectionState::PASSWORD_ENTRY || state == WifiSelectionState::HIDDEN_SSID_ENTRY) {
    return;
  }

  renderer.clearScreen();

  auto& theme = UITheme::getInstance();
  auto metrics = theme.getMetrics();
  Rect screen = theme.getScreenSafeArea(renderer, true, false);

  // Draw header
  // Translated labels can exceed the English byte count in UTF-8 (Arabic is
  // currently about 37 bytes), so leave room without allocating on the heap.
  char countStr[64];
  snprintf(countStr, sizeof(countStr), tr(STR_NETWORKS_FOUND), realNetworkCount);
  const Rect header{screen.x, screen.y + metrics.topPadding, screen.width,
                    TouchHeaderBackButton::height(metrics, mappedInput)};
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, uiTarget, header, tr(STR_WIFI_NETWORKS), false, 150, countStr);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_WIFI_NETWORKS), countStr);
  }
  GUI.drawSubHeader(renderer,
                    Rect{screen.x, screen.y + metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput),
                         screen.width, metrics.tabBarHeight},
                    cachedMacAddress.c_str());

  switch (state) {
    case WifiSelectionState::AUTO_CONNECTING:
      renderConnecting(&screen, &metrics);
      break;
    case WifiSelectionState::SCANNING:
      renderConnecting(&screen, &metrics);  // Reuse connecting screen with different message
      break;
    case WifiSelectionState::NETWORK_LIST:
      renderNetworkList(&screen, &metrics);
      break;
    case WifiSelectionState::HIDDEN_SSID_ENTRY:
      // Transitioning to/from the SSID keyboard subactivity - nothing to draw
      break;
    case WifiSelectionState::CONNECTING:
      renderConnecting(&screen, &metrics);
      break;
    case WifiSelectionState::CONNECTED:
      renderConnected(&screen, &metrics);
      break;
    case WifiSelectionState::SAVE_PROMPT:
      renderSavePrompt(&screen, &metrics);
      break;
    case WifiSelectionState::CONNECTION_FAILED:
      renderConnectionFailed(&screen, &metrics);
      break;
    case WifiSelectionState::FORGET_PROMPT:
      renderForgetPrompt(&screen, &metrics);
      break;
    case WifiSelectionState::PASSWORD_ENTRY:
      break;  // Handled by early return above
  }

  renderer.displayBuffer(screenTransitionRefresh.modeFor(static_cast<uint8_t>(state)));
}

void WifiSelectionActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<WifiSelectionActivity*>(user)->buildListScreen(screen);
}

void WifiSelectionActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  // Content below the header + MAC sub-band, above the legend line.
  screen.setContentMargin(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) +
                           metrics.tabBarHeight + metrics.verticalSpacing),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height) + metrics.verticalSpacing * 2),
      static_cast<int16_t>(safe.x)});

  if (networks.empty()) {
    screen.centeredText(tr(STR_NO_NETWORKS), screen.theme().bodyText);
    return;
  }

  // Per-render owned status strings ("+ * ||||"); items point into them for
  // the draw only.
  std::vector<std::string> statuses(networks.size());
  std::vector<fui::ListItem> items;
  items.reserve(networks.size());
  for (size_t i = 0; i < networks.size(); i++) {
    const auto& network = networks[i];
    if (!network.isHiddenPlaceholder) {
      statuses[i] = std::string(network.hasSavedPassword ? "+ " : "") + (network.isEncrypted ? "* " : "") +
                    getSignalStrengthIndicator(network.rssi);
    }
    fui::ListItem item;
    item.label = network.isHiddenPlaceholder ? tr(STR_ADD_HIDDEN_NETWORK) : network.ssid.c_str();
    if (!statuses[i].empty()) item.value = statuses[i].c_str();
    item.actionValue = static_cast<int16_t>(i);
    items.push_back(item);
  }

  fui::ListProps props;
  props.items = items.data();
  props.count = static_cast<uint16_t>(items.size());
  props.selectedIndex = static_cast<int16_t>(selectedNetworkIndex);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  props.valueInset = 8;  // air between the signal bars and the row edge
  const auto rows = configureUiList(props, screen.theme(), screen.body());
  visibleRows = rows > 0 ? rows : 1;
  topIndex = scrollListBy(topIndex, 0, visibleRows, static_cast<int>(networks.size()));  // clamp to range
  props.topIndex = static_cast<uint16_t>(topIndex);
  screen.list(props);
}

void WifiSelectionActivity::renderNetworkList(const Rect* screen, const ThemeMetrics* metrics) {
  uiReady = false;
  app.render();
  uiReady = true;
  if (networks.empty()) {
    // Below the centered "no networks" line the app drew.
    const auto height = renderer.getLineHeight(UI_10_FONT_ID);
    const auto top = screen->y + (screen->height - height) / 2;
    UITheme::drawCenteredText(renderer, *screen, SMALL_FONT_ID, top + height + 10, tr(STR_PRESS_OK_SCAN));
  }

  GUI.drawHelpText(renderer,
                   Rect{screen->x, screen->y + screen->height - metrics->contentSidePadding - 15, screen->width, 20},
                   tr(STR_NETWORK_LEGEND));

  const bool hasSavedPassword = !networks.empty() && networks[selectedNetworkIndex].hasSavedPassword;
  const char* forgetLabel = hasSavedPassword ? tr(STR_FORGET_BUTTON) : "";

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONNECT), forgetLabel, tr(STR_RETRY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, useReaderButtonHints);
}

void WifiSelectionActivity::renderConnecting(const Rect* screen, const ThemeMetrics* metrics) const {
  const auto height = renderer.getLineHeight(UI_10_FONT_ID);
  const auto top = screen->y + (screen->height - height) / 2;

  if (state == WifiSelectionState::SCANNING) {
    UITheme::drawCenteredText(renderer, *screen, UI_10_FONT_ID, top,
                              autoConnecting ? tr(STR_FINDING_SAVED_WIFI) : tr(STR_SCANNING));
    if (autoConnecting) {
      const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_SHOW_NETWORKS), "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, useReaderButtonHints);
    }
  } else {
    UITheme::drawCenteredText(renderer, *screen, UI_12_FONT_ID, top - 40,
                              autoConnecting ? tr(STR_CONNECTING_SAVED_WIFI) : tr(STR_CONNECTING), true,
                              EpdFontFamily::BOLD);

    const std::string ssidInfo = std::string(tr(STR_TO_PREFIX)) + selectedSSID;
    const int textWidth = std::max(1, screen->width - metrics->contentSidePadding * 2);
    const auto ssidLines = renderer.wrappedText(UI_10_FONT_ID, ssidInfo.c_str(), textWidth, 3);
    const int ssidBlockHeight = static_cast<int>(ssidLines.size()) * height;
    int ssidY = top - ssidBlockHeight / 2 + height / 2;
    for (const auto& line : ssidLines) {
      UITheme::drawCenteredText(renderer, *screen, UI_10_FONT_ID, ssidY, line.c_str());
      ssidY += height;
    }
    if (autoConnecting) {
      const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_SHOW_NETWORKS), "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, useReaderButtonHints);
    }
  }

  if (!autoConnecting) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, useReaderButtonHints);
  }
}

void WifiSelectionActivity::renderConnected(const Rect* screen, const ThemeMetrics* metrics) const {
  const auto height = renderer.getLineHeight(UI_10_FONT_ID);
  const auto top = screen->y + (screen->height - height * 4) / 2;

  UITheme::drawCenteredText(renderer, *screen, UI_12_FONT_ID, top - 30, tr(STR_CONNECTED), true, EpdFontFamily::BOLD);

  std::string ssidInfo = std::string(tr(STR_NETWORK_PREFIX)) + selectedSSID;
  if (ssidInfo.length() > 28) {
    ssidInfo.replace(25, ssidInfo.length() - 25, "...");
  }
  UITheme::drawCenteredText(renderer, *screen, UI_10_FONT_ID, top + 10, ssidInfo.c_str());

  const std::string ipInfo = std::string(tr(STR_IP_ADDRESS_PREFIX)) + connectedIP;
  UITheme::drawCenteredText(renderer, *screen, UI_10_FONT_ID, top + 40, ipInfo.c_str());

  // Use centralized button hints
  const auto labels = mappedInput.mapLabels("", tr(STR_DONE), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, useReaderButtonHints);
}

void WifiSelectionActivity::renderSavePrompt(const Rect* screen, const ThemeMetrics* metrics) const {
  const auto height = renderer.getLineHeight(UI_10_FONT_ID);
  const auto top = screen->y + (screen->height - height * 3) / 2;

  UITheme::drawCenteredText(renderer, *screen, UI_12_FONT_ID, top - 40, tr(STR_CONNECTED), true, EpdFontFamily::BOLD);

  std::string ssidInfo = std::string(tr(STR_NETWORK_PREFIX)) + selectedSSID;
  if (ssidInfo.length() > 28) {
    ssidInfo.replace(25, ssidInfo.length() - 25, "...");
  }
  UITheme::drawCenteredText(renderer, *screen, UI_10_FONT_ID, top, ssidInfo.c_str());

  UITheme::drawCenteredText(renderer, *screen, UI_10_FONT_ID, top + 40, tr(STR_SAVE_PASSWORD));

  // Draw Yes/No buttons
  const int buttonY = top + 80;
  constexpr int buttonWidth = 60;
  constexpr int buttonSpacing = 30;
  constexpr int totalWidth = buttonWidth * 2 + buttonSpacing;
  const int startX = screen->x + (screen->width - totalWidth) / 2;

  // Draw "Yes" button
  if (savePromptSelection == 0) {
    std::string text = "[" + std::string(tr(STR_YES)) + "]";
    renderer.drawText(UI_10_FONT_ID, startX, buttonY, text.c_str());
  } else {
    renderer.drawText(UI_10_FONT_ID, startX + 4, buttonY, tr(STR_YES));
  }

  // Draw "No" button
  if (savePromptSelection == 1) {
    std::string text = "[" + std::string(tr(STR_NO)) + "]";
    renderer.drawText(UI_10_FONT_ID, startX + buttonWidth + buttonSpacing, buttonY, text.c_str());
  } else {
    renderer.drawText(UI_10_FONT_ID, startX + buttonWidth + buttonSpacing + 4, buttonY, tr(STR_NO));
  }

  // Use centralized button hints
  const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, useReaderButtonHints);
}

void WifiSelectionActivity::renderConnectionFailed(const Rect* screen, const ThemeMetrics* metrics) const {
  const auto height = renderer.getLineHeight(UI_10_FONT_ID);
  const int messageWidth = screen->width - metrics->contentSidePadding * 2;
  const auto errorLines = renderer.wrappedText(UI_10_FONT_ID, connectionError.c_str(), messageWidth, 3);
  const auto top = screen->y + (screen->height - height * (1 + errorLines.size())) / 2;

  UITheme::drawCenteredText(renderer, *screen, UI_12_FONT_ID, top - 20, tr(STR_CONNECTION_FAILED), true,
                            EpdFontFamily::BOLD);
  int errorY = top + height + 10;
  for (const auto& line : errorLines) {
    UITheme::drawCenteredText(renderer, *screen, UI_10_FONT_ID, errorY, line.c_str());
    errorY += height;
  }

  // Use centralized button hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DONE), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, useReaderButtonHints);
}

void WifiSelectionActivity::renderForgetPrompt(const Rect* screen, const ThemeMetrics* metrics) const {
  const auto height = renderer.getLineHeight(UI_10_FONT_ID);
  const auto top = screen->y + (screen->height - height * 3) / 2;

  UITheme::drawCenteredText(renderer, *screen, UI_12_FONT_ID, top - 40, tr(STR_FORGET_NETWORK), true,
                            EpdFontFamily::BOLD);

  std::string ssidInfo = std::string(tr(STR_NETWORK_PREFIX)) + selectedSSID;
  if (ssidInfo.length() > 28) {
    ssidInfo.replace(25, ssidInfo.length() - 25, "...");
  }
  UITheme::drawCenteredText(renderer, *screen, UI_10_FONT_ID, top, ssidInfo.c_str());

  UITheme::drawCenteredText(renderer, *screen, UI_10_FONT_ID, top + 40, tr(STR_FORGET_AND_REMOVE));

  // Draw Cancel/Forget network buttons
  const int buttonY = top + 80;
  constexpr int buttonWidth = 120;
  constexpr int buttonSpacing = 30;
  constexpr int totalWidth = buttonWidth * 2 + buttonSpacing;
  const int startX = screen->x + (screen->width - totalWidth) / 2;

  // Draw "Cancel" button
  if (forgetPromptSelection == 0) {
    std::string text = "[" + std::string(tr(STR_CANCEL)) + "]";
    renderer.drawText(UI_10_FONT_ID, startX, buttonY, text.c_str());
  } else {
    renderer.drawText(UI_10_FONT_ID, startX + 4, buttonY, tr(STR_CANCEL));
  }

  // Draw "Forget network" button
  if (forgetPromptSelection == 1) {
    std::string text = "[" + std::string(tr(STR_FORGET_BUTTON)) + "]";
    renderer.drawText(UI_10_FONT_ID, startX + buttonWidth + buttonSpacing, buttonY, text.c_str());
  } else {
    renderer.drawText(UI_10_FONT_ID, startX + buttonWidth + buttonSpacing + 4, buttonY, tr(STR_FORGET_BUTTON));
  }

  // Use centralized button hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4, useReaderButtonHints);
}

void WifiSelectionActivity::onComplete(const bool requestedConnected) {
  const bool connected = requestedConnected && requireRadioAccess();
  tearDownWifiOnExit = !connected;
  ActivityResult result;
  result.isCancelled = !connected;
  if (connected) {
    result.data = WifiResult{true, selectedSSID, connectedIP};
  }
  setResult(std::move(result));
  finish();
}
