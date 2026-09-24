#include "NetworkToolRequest.h"

#include <Arduino.h>
#include <Logging.h>
#include <Memory.h>
#include <RadioManager.h>

#include <cstring>
#ifndef SIMULATOR
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#endif

namespace NetworkToolRequest {
bool send(const char* owner, const char* url, const char* postBody, Result& out) {
  out.body[0] = 0;
  out.bytes = 0;
  out.status = 0;
  out.elapsedMs = 0;
  out.truncated = false;
#ifdef SIMULATOR
  LOG_ERR("NHTTP", "HTTP tool unavailable in simulator");
  return false;
#else
  const size_t urlLength = url ? strnlen(url, 257) : 0;
  const size_t postLength = postBody ? strnlen(postBody, 513) : 0;
  if (!RADIO.stationConnected(owner) || !urlLength || urlLength > 256 || postLength > 512) {
    LOG_ERR("NHTTP", "Request requires owned station and bounded input");
    return false;
  }
  const char* authority = nullptr;
  if (strncmp(url, "https://", 8) == 0)
    authority = url + 8;
  else if (strncmp(url, "http://", 7) == 0)
    authority = url + 7;
  if (!authority || !*authority || *authority == '/' || *authority == '?' || *authority == '#') {
    LOG_ERR("NHTTP", "Invalid HTTP URL");
    return false;
  }
  bool inAuthority = true;
  for (const char* c = url; *c; ++c) {
    if (static_cast<unsigned char>(*c) <= 32 || static_cast<unsigned char>(*c) >= 127) {
      LOG_ERR("NHTTP", "URL must use printable ASCII without spaces");
      return false;
    }
    if (c < authority) continue;
    if (*c == '/' || *c == '?' || *c == '#') inAuthority = false;
    if (inAuthority && (*c == '@' || *c == '%')) {
      LOG_ERR("NHTTP", "Embedded credentials/encoded authority unsupported");
      return false;
    }
  }
  // SDK config is larger than the preferred 256-byte stack budget. Allocate one
  // sizeof(config) cold-path object, then release after the SDK copies its fields.
  auto config = makeUniqueNoThrow<esp_http_client_config_t>();
  if (!config) {
    LOG_ERR("NHTTP", "Config allocation failed");
    return false;
  }
  *config = {};
  config->url = url;
  config->method = postBody ? HTTP_METHOD_POST : HTTP_METHOD_GET;
  config->timeout_ms = 5000;
  config->buffer_size = 1024;
  config->buffer_size_tx = 512;
  config->crt_bundle_attach = esp_crt_bundle_attach;
  config->disable_auto_redirect = true;
  config->keep_alive_enable = false;
  esp_http_client_handle_t client = esp_http_client_init(config.get());
  config.reset();
  if (!client) {
    LOG_ERR("NHTTP", "Client allocation failed");
    return false;
  }
  const uint32_t start = millis();
  bool ok = true;
  if (postBody) ok = esp_http_client_set_header(client, "Content-Type", "text/plain; charset=utf-8") == ESP_OK;
  if (ok) ok = esp_http_client_open(client, int(postLength)) == ESP_OK;
  size_t sent = 0;
  while (ok && sent < postLength) {
    if (!RADIO.stationConnected(owner) || millis() - start >= 10000) {
      ok = false;
      break;
    }
    const int n = esp_http_client_write(client, postBody + sent, int(postLength - sent));
    if (n <= 0)
      ok = false;
    else
      sent += size_t(n);
  }
  if (ok) ok = esp_http_client_fetch_headers(client) >= 0;
  if (ok) {
    out.status = esp_http_client_get_status_code(client);
    const uint32_t bodyStart = millis();
    while (out.bytes < sizeof(out.body) - 1 && !esp_http_client_is_complete_data_received(client)) {
      if (!RADIO.stationConnected(owner) || millis() - bodyStart >= 10000) {
        ok = false;
        break;
      }
      const int n = esp_http_client_read(client, out.body + out.bytes, int(sizeof(out.body) - 1 - out.bytes));
      if (n < 0) {
        ok = false;
        break;
      }
      if (n == 0) {
        ok = esp_http_client_is_complete_data_received(client);
        break;
      }
      out.bytes += size_t(n);
    }
    out.truncated = out.bytes == sizeof(out.body) - 1 && !esp_http_client_is_complete_data_received(client);
    // This tool displays a plain text preview, not a binary download or renderer.
    for (size_t i = 0; i < out.bytes; ++i) {
      const unsigned char c = out.body[i];
      if (c < 32)
        out.body[i] = ' ';
      else if (c >= 127)
        out.body[i] = '?';
    }
    out.body[out.bytes] = 0;
  }
  out.elapsedMs = millis() - start;
  if (esp_http_client_cleanup(client) != ESP_OK) {
    LOG_ERR("NHTTP", "Client cleanup failed");
    ok = false;
  }
  if (!ok || !RADIO.stationConnected(owner)) {
    LOG_ERR("NHTTP", "Request failed, incomplete, or station disconnected");
    return false;
  }
  return true;
#endif
}
}  // namespace NetworkToolRequest
