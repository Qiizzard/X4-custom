// RingBuffer -- the SPSC primitive RULESET.md rule 10 makes mandatory for every
// radio/ISR callback. It is the one place a dropped frame is allowed to happen,
// so its drop accounting has to be exactly right.
#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <numeric>
#include <thread>
#include <vector>

#include "RingBuffer.h"

namespace {

// Fill a frame with a recognisable pattern so a wrap that copies the wrong half
// shows up as wrong bytes, not just a wrong length.
std::vector<uint8_t> pattern(const uint16_t len, const uint8_t seed) {
  std::vector<uint8_t> v(len);
  for (uint16_t i = 0; i < len; ++i) v[i] = static_cast<uint8_t>(seed + i);
  return v;
}

TEST(RingBuffer, EmptyOnConstruction) {
  RingBuffer<256> rb;
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ(rb.used(), 0u);
  EXPECT_EQ(rb.droppedFrames(), 0u);
  uint8_t out[64];
  EXPECT_EQ(rb.pop(out, sizeof(out)), 0u);
}

TEST(RingBuffer, RoundTripsOneFrame) {
  RingBuffer<256> rb;
  const auto in = pattern(32, 0x10);
  ASSERT_TRUE(rb.push(in.data(), 32));
  EXPECT_FALSE(rb.empty());
  EXPECT_EQ(rb.peekFrameLength(), 32u);

  uint8_t out[64] = {};
  ASSERT_EQ(rb.pop(out, sizeof(out)), 32u);
  EXPECT_EQ(std::vector<uint8_t>(out, out + 32), in);
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ(rb.droppedFrames(), 0u);
}

TEST(RingBuffer, PreservesFrameOrderAndBoundaries) {
  RingBuffer<1024> rb;
  for (uint8_t i = 0; i < 8; ++i) {
    const auto in = pattern(static_cast<uint16_t>(10 + i), static_cast<uint8_t>(i * 7));
    ASSERT_TRUE(rb.push(in.data(), static_cast<uint16_t>(10 + i))) << "push " << int(i);
  }
  for (uint8_t i = 0; i < 8; ++i) {
    uint8_t out[64] = {};
    const uint16_t n = rb.pop(out, sizeof(out));
    ASSERT_EQ(n, 10u + i) << "frame " << int(i) << " came back the wrong length";
    EXPECT_EQ(std::vector<uint8_t>(out, out + n), pattern(n, static_cast<uint8_t>(i * 7)));
  }
  EXPECT_TRUE(rb.empty());
}

// The wrap is where a framed ring gets subtly wrong: a frame that straddles the
// end of the array must still read back contiguous.
TEST(RingBuffer, FramesSurviveTheWrap) {
  RingBuffer<128> rb;
  uint8_t out[64] = {};
  // Push/pop enough to walk the write cursor well past the end of the array.
  for (int cycle = 0; cycle < 40; ++cycle) {
    const auto in = pattern(29, static_cast<uint8_t>(cycle));
    ASSERT_TRUE(rb.push(in.data(), 29)) << "cycle " << cycle;
    ASSERT_EQ(rb.pop(out, sizeof(out)), 29u) << "cycle " << cycle;
    EXPECT_EQ(std::vector<uint8_t>(out, out + 29), in) << "cycle " << cycle << " wrapped badly";
  }
  EXPECT_EQ(rb.droppedFrames(), 0u);
}

// Overflow must drop the incoming frame, never corrupt the ones already queued.
// A capture app reports these counts, so they have to be honest.
TEST(RingBuffer, DropsIncomingFrameWhenFullAndKeepsQueuedOnes) {
  RingBuffer<128> rb;
  const auto in = pattern(40, 0xA0);
  ASSERT_TRUE(rb.push(in.data(), 40));
  ASSERT_TRUE(rb.push(in.data(), 40));
  ASSERT_TRUE(rb.push(in.data(), 40));  // 3 * 42 = 126 of 128 bytes used
  EXPECT_EQ(rb.used(), 126u);

  EXPECT_FALSE(rb.push(in.data(), 40));  // no room: the newcomer is dropped
  EXPECT_EQ(rb.droppedFrames(), 1u);
  EXPECT_EQ(rb.used(), 126u);

  // The three already queued are untouched.
  for (int i = 0; i < 3; ++i) {
    uint8_t out[64] = {};
    ASSERT_EQ(rb.pop(out, sizeof(out)), 40u) << "queued frame " << i << " was corrupted by the drop";
    EXPECT_EQ(std::vector<uint8_t>(out, out + 40), in);
  }
  EXPECT_TRUE(rb.empty());
}

TEST(RingBuffer, RecoversCapacityAfterDraining) {
  RingBuffer<128> rb;
  const auto in = pattern(40, 0x5A);
  for (int i = 0; i < 3; ++i) ASSERT_TRUE(rb.push(in.data(), 40));
  EXPECT_FALSE(rb.push(in.data(), 40));

  uint8_t out[64] = {};
  ASSERT_EQ(rb.pop(out, sizeof(out)), 40u);
  EXPECT_TRUE(rb.push(in.data(), 40)) << "space freed by pop() was not reused";
}

TEST(RingBuffer, RejectsOversizedAndEmptyPushes) {
  RingBuffer<128> rb;
  const auto in = pattern(64, 1);
  EXPECT_FALSE(rb.push(in.data(), 0));
  EXPECT_FALSE(rb.push(nullptr, 8));
  EXPECT_FALSE(rb.push(in.data(), RingBuffer<128>::kMaxFrameBytes + 1));
  EXPECT_EQ(rb.droppedFrames(), 3u);
  EXPECT_TRUE(rb.empty());
}

// A frame too big for the caller's scratch buffer is retired whole, not
// truncated: half a packet parsed as a whole one is worse than a missing one.
TEST(RingBuffer, RetiresRatherThanTruncatesAnOversizedFrame) {
  RingBuffer<256> rb;
  const auto big = pattern(80, 0x33);
  const auto small = pattern(8, 0x44);
  ASSERT_TRUE(rb.push(big.data(), 80));
  ASSERT_TRUE(rb.push(small.data(), 8));

  uint8_t out[16] = {};
  EXPECT_EQ(rb.pop(out, sizeof(out)), 0u);  // 80 does not fit in 16
  EXPECT_EQ(rb.droppedFrames(), 1u);
  // The next frame is still readable -- the stream stays in sync.
  ASSERT_EQ(rb.pop(out, sizeof(out)), 8u);
  EXPECT_EQ(std::vector<uint8_t>(out, out + 8), small);
}

TEST(RingBuffer, ResetDiscardsPendingFramesAndCounters) {
  RingBuffer<128> rb;
  const auto in = pattern(40, 0x77);
  ASSERT_TRUE(rb.push(in.data(), 40));
  ASSERT_TRUE(rb.push(in.data(), 40));
  ASSERT_TRUE(rb.push(in.data(), 40));
  EXPECT_FALSE(rb.push(in.data(), 40));

  rb.reset();
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ(rb.used(), 0u);
  EXPECT_EQ(rb.droppedFrames(), 0u);
  EXPECT_TRUE(rb.push(in.data(), 40));
}

// The counters are monotonic uint32 and rely on unsigned wrap being correct.
// Walking many megabytes through a small ring exercises that.
TEST(RingBuffer, SurvivesFarMoreBytesThanItsCapacity) {
  RingBuffer<256> rb;
  uint8_t out[64] = {};
  uint64_t delivered = 0;
  for (int i = 0; i < 200000; ++i) {
    const auto in = pattern(50, static_cast<uint8_t>(i));
    ASSERT_TRUE(rb.push(in.data(), 50)) << "iteration " << i;
    ASSERT_EQ(rb.pop(out, sizeof(out)), 50u) << "iteration " << i;
    ASSERT_EQ(std::vector<uint8_t>(out, out + 50), in) << "iteration " << i;
    delivered += 50;
  }
  EXPECT_EQ(delivered, 10000000u);
  EXPECT_EQ(rb.droppedFrames(), 0u);
}

// The real shape of rule 10: a producer that never blocks (stands in for the
// promiscuous callback) and a consumer draining it (stands in for loop()).
// Every frame that reports pushed must come back byte-identical and in order.
TEST(RingBuffer, ConcurrentProducerConsumerLosesNothingItClaimedToAccept) {
  RingBuffer<2048> rb;
  constexpr int kFrames = 100000;
  std::atomic<bool> producerDone{false};
  std::vector<uint8_t> accepted;  // seeds the producer said it took, in order
  accepted.reserve(kFrames);

  std::thread producer([&] {
    for (int i = 0; i < kFrames; ++i) {
      const uint16_t len = static_cast<uint16_t>(16 + (i % 48));
      const auto in = pattern(len, static_cast<uint8_t>(i));
      if (rb.push(in.data(), len)) accepted.push_back(static_cast<uint8_t>(i));
      // No retry, no blocking: a callback that spins would stall the radio.
    }
    producerDone.store(true, std::memory_order_release);
  });

  std::vector<uint8_t> received;
  received.reserve(kFrames);
  std::thread consumer([&] {
    uint8_t out[128] = {};
    for (;;) {
      const uint16_t n = rb.pop(out, sizeof(out));
      if (n == 0) {
        if (producerDone.load(std::memory_order_acquire) && rb.empty()) break;
        continue;
      }
      // Recover the seed from the first byte and verify the payload matches it.
      const uint8_t seed = out[0];
      EXPECT_EQ(std::vector<uint8_t>(out, out + n), pattern(n, seed)) << "torn frame";
      received.push_back(seed);
    }
  });

  producer.join();
  consumer.join();

  // Frames may be dropped under pressure, but never reordered, torn, or
  // invented -- and the drop count must explain every missing frame.
  EXPECT_EQ(received, accepted) << "a frame the producer accepted was lost or reordered";
  EXPECT_EQ(accepted.size() + rb.droppedFrames(), static_cast<size_t>(kFrames));
}

}  // namespace
