#pragma once
// RingBuffer -- lock-free single-producer/single-consumer framed byte ring.
//
// RULESET.md rule 10: a radio/promiscuous callback (and any ISR) may not
// allocate, sort, or touch std::string. It may only copy raw bytes somewhere a
// later loop() pass can parse them. This is that somewhere.
//
// Shape: variable-length frames in one fixed byte array. Storage is an in-object
// member array, so a RingBuffer placed in an Activity costs exactly its declared
// size and never touches the heap -- no allocation to fail, nothing to fragment,
// and the bytes live in DRAM where an ISR is allowed to read them.
//
// Concurrency: exactly one producer thread (the callback) and one consumer
// thread (loop()). head_ is written only by the producer, tail_ only by the
// consumer, so the two never contend and no lock is needed -- which is the
// point, because rule 12 forbids taking a semaphore from an ISR.
//
// Overflow drops the *incoming* frame rather than overwriting unread ones, and
// counts it. A recon app that reports "1,204 captured / 37 dropped" is telling
// the truth; one that silently overwrites is not.
//
// Usage:
//   RingBuffer<4096> frames;                       // in the Activity, not the heap
//   void IRAM_ATTR onPacket(void* buf, uint16_t n) // producer: copy and leave
//       { frames.push(static_cast<const uint8_t*>(buf), n); }
//   void loop() {                                  // consumer: parse at leisure
//     uint8_t frame[256];
//     while (const uint16_t n = frames.pop(frame, sizeof(frame))) { parse(frame, n); }
//   }

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Capacity must be a power of two: the mask turns the wrap into an AND instead
// of a divide, which matters in a callback that runs per packet.
template <uint32_t Capacity>
class RingBuffer {
  static_assert(Capacity >= 64, "RingBuffer too small to hold a useful frame");
  static_assert((Capacity & (Capacity - 1)) == 0, "RingBuffer capacity must be a power of two");

 public:
  // Two length bytes precede every payload.
  static constexpr uint32_t kHeaderBytes = 2;
  // Largest payload that can ever fit, even in a completely empty buffer.
  static constexpr uint16_t kMaxFrameBytes = static_cast<uint16_t>(Capacity - kHeaderBytes);

  RingBuffer() = default;
  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  // Producer side. Safe to call from an ISR or a radio callback: no allocation,
  // no lock, no flash reads, bounded work (one memcpy of len bytes).
  // Returns false and counts a drop if the frame does not fit; never blocks.
  bool push(const uint8_t* data, const uint16_t len) {
    if (data == nullptr || len == 0 || len > kMaxFrameBytes) {
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    const uint32_t head = head_.load(std::memory_order_relaxed);
    // acquire pairs with the consumer's release store of tail_, so the space it
    // freed is visible here before we write into it.
    const uint32_t tail = tail_.load(std::memory_order_acquire);
    const uint32_t needed = kHeaderBytes + len;
    // Counters are monotonic; unsigned wrap makes this difference correct even
    // after 2^32 bytes, and it removes the classic full-vs-empty ambiguity.
    if (Capacity - (head - tail) < needed) {
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    uint8_t header[kHeaderBytes] = {static_cast<uint8_t>(len & 0xFF), static_cast<uint8_t>(len >> 8)};
    writeAt(head, header, kHeaderBytes);
    writeAt(head + kHeaderBytes, data, len);
    // release: the payload above must land before the consumer can see the
    // advanced head and start reading it.
    head_.store(head + needed, std::memory_order_release);
    return true;
  }

  // Consumer side, from loop(). Copies the oldest frame out and retires it.
  // Returns the payload byte count, or 0 when the buffer is empty.
  // A frame larger than maxLen cannot be delivered intact, so it is retired and
  // counted as a drop rather than half-copied -- a truncated packet parsed as a
  // whole one is worse than a missing one.
  uint16_t pop(uint8_t* out, const uint16_t maxLen) {
    const uint32_t tail = tail_.load(std::memory_order_relaxed);
    const uint32_t head = head_.load(std::memory_order_acquire);
    if (head == tail) return 0;
    uint8_t header[kHeaderBytes];
    readAt(tail, header, kHeaderBytes);
    const uint16_t len = static_cast<uint16_t>(header[0] | (header[1] << 8));
    const uint32_t next = tail + kHeaderBytes + len;
    if (out == nullptr || len > maxLen) {
      tail_.store(next, std::memory_order_release);
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return 0;
    }
    readAt(tail + kHeaderBytes, out, len);
    // release: the read above must complete before the producer may reuse these
    // bytes, which it can as soon as it sees the advanced tail.
    tail_.store(next, std::memory_order_release);
    return len;
  }

  // Payload size of the next frame without retiring it, or 0 when empty. Lets a
  // consumer size its scratch buffer before committing to a pop().
  uint16_t peekFrameLength() const {
    const uint32_t tail = tail_.load(std::memory_order_relaxed);
    if (head_.load(std::memory_order_acquire) == tail) return 0;
    uint8_t header[kHeaderBytes];
    readAt(tail, header, kHeaderBytes);
    return static_cast<uint16_t>(header[0] | (header[1] << 8));
  }

  bool empty() const { return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_relaxed); }

  // Bytes in flight, frame headers included.
  uint32_t used() const { return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_relaxed); }

  static constexpr uint32_t capacity() { return Capacity; }

  // Frames lost to a full buffer or an undersized read. Surface this in the UI:
  // a capture that dropped frames must say so.
  uint32_t droppedFrames() const { return dropped_.load(std::memory_order_relaxed); }

  // Consumer-side reset. Only safe while the producer is stopped (radio released,
  // callback detached) -- it moves tail_ past frames the producer may still be
  // writing behind.
  void reset() {
    tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
    dropped_.store(0, std::memory_order_relaxed);
  }

 private:
  static constexpr uint32_t kMask = Capacity - 1;

  // Both halves split the copy at the wrap point so a frame straddling the end
  // of the array is contiguous to callers.
  void writeAt(const uint32_t pos, const uint8_t* src, const uint32_t len) {
    const uint32_t start = pos & kMask;
    const uint32_t firstChunk = (len < Capacity - start) ? len : Capacity - start;
    std::memcpy(&storage_[start], src, firstChunk);
    if (len > firstChunk) std::memcpy(&storage_[0], src + firstChunk, len - firstChunk);
  }

  void readAt(const uint32_t pos, uint8_t* dst, const uint32_t len) const {
    const uint32_t start = pos & kMask;
    const uint32_t firstChunk = (len < Capacity - start) ? len : Capacity - start;
    std::memcpy(dst, &storage_[start], firstChunk);
    if (len > firstChunk) std::memcpy(dst + firstChunk, &storage_[0], len - firstChunk);
  }

  uint8_t storage_[Capacity] = {};
  std::atomic<uint32_t> head_{0};
  std::atomic<uint32_t> tail_{0};
  std::atomic<uint32_t> dropped_{0};
};
