#include "HalBulletinServer.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>
#include <RadioManager.h>
#include <strings.h>

#include <cstdio>
#include <cstring>
#ifndef SIMULATOR
#include <fcntl.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>

#include <cerrno>
#endif

bool HalBulletinServer::append(const char* text, bool escape) {
  for (; *text; ++text) {
    const char* replacement = nullptr;
    if (escape) {
      switch (*text) {
        case '&':
          replacement = "&amp;";
          break;
        case '<':
          replacement = "&lt;";
          break;
        case '>':
          replacement = "&gt;";
          break;
        case '"':
          replacement = "&quot;";
          break;
        case '\'':
          replacement = "&#39;";
          break;
      }
    }
    const size_t size = replacement ? strlen(replacement) : 1;
    if (responseSize_ + size >= sizeof(response_)) return false;
    memcpy(response_ + responseSize_, replacement ? replacement : text, size);
    responseSize_ += size;
  }
  response_[responseSize_] = 0;
  return true;
}
void HalBulletinServer::respond(bool valid) {
  responseSize_ = 0;
  const char* type = "text/plain; charset=utf-8";
  if (valid && route_ == 1) {
    type = "text/html; charset=utf-8";
    valid = append("<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'><h1>") &&
            append(tr(STR_BOARD_APP), true) && append("</h1><p>") && append(tr(STR_BOARD_WEB_NOTE), true) &&
            append("</p><form id=f><input id=m maxlength=200 required><button>") && append(tr(STR_BOARD_POST), true) &&
            append(
                "</button></form><p id=e></p><pre id=p style='white-space:pre-wrap'></pre><script>"
                "const "
                "f=document.getElementById('f'),m=document.getElementById('m'),p=document.getElementById('p'),e="
                "document.getElementById('e');"
                "async function load(){try{let r=await fetch('/messages');p.textContent=await "
                "r.text()}catch(x){e.textContent=x.message}}"
                "f.onsubmit=async ev=>{ev.preventDefault();try{let r=await "
                "fetch('/post',{method:'POST',headers:{'X-X4-Board':'1'},body:m.value});"
                "e.textContent=await "
                "r.text();if(r.ok){m.value='';load()}}catch(x){e.textContent=x.message}};load();setInterval(load,5000)<"
                "/script>");
  } else if (valid && route_ == 2) {
    for (unsigned i = 0; valid && i < count_; ++i) {
      const unsigned index = ((count_ == 16 ? next_ : 0) + i) % 16;
      valid = append(posts_[index]) && append("\n\n");
    }
  } else if (valid && route_ == 3) {
    for (size_t i = 0; i < wanted_; ++i) {
      const unsigned char c = request_[bodyAt_ + i];
      if (c < 32 || c > 126) {
        valid = false;
        break;
      }
    }
    if (valid) {
      memcpy(posts_[next_], request_ + bodyAt_, wanted_);
      posts_[next_][wanted_] = 0;
      next_ = (next_ + 1) % 16;
      if (count_ < 16) ++count_;
      ++revision_;
      valid = append(tr(STR_BOARD_POSTED));
    }
  } else
    valid = false;
  if (!valid) {
    responseSize_ = 0;
    append(tr(STR_BOARD_HTTP_ERROR));
    type = "text/plain; charset=utf-8";
  }
  headerSize_ = snprintf(header_, sizeof(header_),
                         "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\nCache-Control: "
                         "no-store\r\nX-Content-Type-Options: nosniff\r\n\r\n",
                         valid ? "200 OK" : "400 Bad Request", type, unsigned(responseSize_));
  if (headerSize_ >= sizeof(header_)) {
    closeClient();
    return;
  }
  sent_ = 0;
  replying_ = true;
}
bool HalBulletinServer::parse() {
  char* end = strstr(request_, "\r\n\r\n");
  if (!end) return false;
  bodyAt_ = size_t(end - request_) + 4;
  if (bodyAt_ > 1024) {
    respond(false);
    return false;
  }
  char* lineEnd = strstr(request_, "\r\n");
  *lineEnd = 0;
  if (!strcmp(request_, "GET / HTTP/1.1"))
    route_ = 1;
  else if (!strcmp(request_, "GET /messages HTTP/1.1"))
    route_ = 2;
  else if (!strcmp(request_, "POST /post HTTP/1.1"))
    route_ = 3;
  else {
    respond(false);
    return false;
  }
  bool host = false, length = false, guard = false, valid = true;
  for (char* line = lineEnd + 2; line < end; line = lineEnd + 2) {
    lineEnd = strstr(line, "\r\n");
    if (!lineEnd || lineEnd > end) {
      valid = false;
      break;
    }
    *lineEnd = 0;
    char* colon = strchr(line, ':');
    if (!colon || line[0] == ' ' || line[0] == '\t') {
      valid = false;
      break;
    }
    *colon = 0;
    char* value = colon + 1;
    while (*value == ' ' || *value == '\t') ++value;
    if (!strcasecmp(line, "Host")) {
      if (host || (strcmp(value, host_) &&
                   !(strncmp(value, host_, strlen(host_)) == 0 && !strcmp(value + strlen(host_), ":80"))))
        valid = false;
      host = true;
    } else if (!strcasecmp(line, "Content-Length")) {
      if (length || !*value) valid = false;
      length = true;
      wanted_ = 0;
      for (; *value; ++value) {
        if (*value < '0' || *value > '9' || wanted_ > 200) {
          valid = false;
          break;
        }
        wanted_ = wanted_ * 10 + unsigned(*value - '0');
      }
      if (wanted_ > 200) valid = false;
    } else if (!strcasecmp(line, "Transfer-Encoding") || !strcasecmp(line, "Expect"))
      valid = false;
    else if (!strcasecmp(line, "X-X4-Board")) {
      if (guard || strcmp(value, "1")) valid = false;
      guard = true;
    }
  }
  valid = valid && host && (route_ == 3 ? guard && length && wanted_ > 0 : wanted_ == 0);
  if (!valid) {
    respond(false);
    return false;
  }
  parsed_ = true;
  return true;
}

#ifdef SIMULATOR
bool HalBulletinServer::start(const char*) {
  LOG_ERR("Board", "HTTP board unavailable in simulator");
  return false;
}
void HalBulletinServer::closeClient() {}
void HalBulletinServer::stop() {}
void HalBulletinServer::poll() {}
#else
void HalBulletinServer::closeClient() {
  if (client_ >= 0) {
    ::close(client_);
    client_ = -1;
  }
  used_ = bodyAt_ = wanted_ = responseSize_ = headerSize_ = sent_ = 0;
  parsed_ = replying_ = false;
  route_ = 0;
}
void HalBulletinServer::stop() {
  closeClient();
  if (listener_ >= 0) {
    ::close(listener_);
    listener_ = -1;
  }
  memset(posts_, 0, sizeof(posts_));
  count_ = next_ = 0;
}
bool HalBulletinServer::start(const char* owner) {
  stop();
  uint8_t address[4];
  if (!RADIO.accessPointAddress(owner, address)) {
    LOG_ERR("Board", "Owned AP required");
    return false;
  }
  snprintf(host_, sizeof(host_), "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
  listener_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener_ < 0) {
    LOG_ERR("Board", "socket failed");
    return false;
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(80);
  addr.sin_addr.s_addr =
      htonl(uint32_t(address[0]) << 24 | uint32_t(address[1]) << 16 | uint32_t(address[2]) << 8 | address[3]);
  const int flags = fcntl(listener_, F_GETFL, 0);
  if (flags < 0 || fcntl(listener_, F_SETFL, flags | O_NONBLOCK) < 0 ||
      bind(listener_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || listen(listener_, 1) < 0) {
    LOG_ERR("Board", "listen setup failed");
    stop();
    return false;
  }
  return true;
}
void HalBulletinServer::poll() {
  if (listener_ < 0) return;
  if (client_ < 0) {
    client_ = accept(listener_, nullptr, nullptr);
    if (client_ < 0) return;
    const int flags = fcntl(client_, F_GETFL, 0);
    if (flags < 0 || fcntl(client_, F_SETFL, flags | O_NONBLOCK) < 0) {
      LOG_ERR("Board", "client setup failed");
      closeClient();
      return;
    }
    began_ = millis();
    request_[0] = 0;
  }
  if (uint32_t(millis() - began_) >= 5000) {
    closeClient();
    return;
  }
  if (replying_) {
    const bool sendingHeader = sent_ < headerSize_;
    const size_t offset = sendingHeader ? sent_ : sent_ - headerSize_;
    const size_t remaining = sendingHeader ? headerSize_ - offset : responseSize_ - offset;
    if (!remaining) {
      closeClient();
      return;
    }
    const int n = send(client_, (sendingHeader ? header_ : response_) + offset, remaining > 512 ? 512 : remaining, 0);
    if (n > 0)
      sent_ += n;
    else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
      closeClient();
    return;
  }
  size_t room = sizeof(request_) - 1 - used_;
  if (room > 512) room = 512;
  if (!room) {
    respond(false);
    return;
  }
  const int n = recv(client_, request_ + used_, room, 0);
  if (n == 0) {
    closeClient();
    return;
  }
  if (n < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) closeClient();
    return;
  }
  if (memchr(request_ + used_, 0, n)) {
    respond(false);
    return;
  }
  used_ += n;
  request_[used_] = 0;
  if (!parsed_ && !parse()) {
    if (!replying_ && used_ >= 1024) respond(false);
    return;
  }
  if (used_ > bodyAt_ + wanted_) {
    respond(false);
    return;
  }
  if (used_ == bodyAt_ + wanted_) respond(true);
}
#endif
