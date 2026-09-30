#pragma once
#include <cstddef>
#include <cstdint>

// App-owned bounded HTTP board. Socket APIs remain behind the HAL.
class HalBulletinServer {
 public:
  HalBulletinServer() = default;
  HalBulletinServer(const HalBulletinServer&) = delete;
  HalBulletinServer& operator=(const HalBulletinServer&) = delete;
  ~HalBulletinServer() { stop(); }
  bool start(const char* owner, bool drop = false);
  void stop();
  void poll();
  unsigned count() const { return count_; }
  uint32_t revision() const { return revision_; }

 private:
  int listener_ = -1, client_ = -1;
  char host_[32]{}, request_[5121]{}, response_[4096]{}, header_[192]{};
  char posts_[16][201]{};
  size_t used_ = 0, bodyAt_ = 0, wanted_ = 0, responseSize_ = 0, headerSize_ = 0, sent_ = 0;
  unsigned route_ = 0, next_ = 0, count_ = 0;
  uint32_t began_ = 0, revision_ = 0;
  bool parsed_ = false, replying_ = false, drop_ = false;
  unsigned fileIndex_ = 0;
  void closeClient();
  bool parse();
  void respond(bool valid);
  bool append(const char* text, bool escape = false);
};
