#pragma once
#include <cstddef>
#include <cstdint>

namespace NetworkToolRequest {
struct Result {
  char body[1025] = {};
  size_t bytes = 0;
  int status = 0;
  uint32_t elapsedMs = 0;
  bool truncated = false;
};
// Explicit one-shot GET (nullptr body) or text/plain POST (at most 512 bytes).
// No credentials, redirect following, persistence, or certificate bypass.
bool send(const char* owner, const char* url, const char* postBody, Result& result);
}  // namespace NetworkToolRequest
