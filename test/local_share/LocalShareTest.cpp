#include <Arduino.h>
#include <HalBulletinServer.h>
#include <HalStorage.h>
#include <gtest/gtest.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>

#include <chrono>
#include <thread>
#undef bind
namespace {
class Share : public testing::Test {
 protected:
  HalBulletinServer server;
  void SetUp() override {
    Storage = {};
    testMillis = 0;
    ASSERT_TRUE(server.start("test_owner"));
  }
  void TearDown() override {
    server.stop();
    EXPECT_EQ(Storage.handles, 0u);
  }
  int connectClient() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(testPort);
    if (fd < 0 || connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
      ADD_FAILURE() << "loopback connect: " << strerror(errno);
      if (fd >= 0) close(fd);
      return -1;
    }
    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
  }
  std::string request(const std::string& bytes, size_t fragment = 0) {
    const int fd = connectClient();
    if (fd < 0) return {};
    for (size_t sent = 0; sent < bytes.size();) {
      const size_t n = std::min(fragment ? fragment : bytes.size(), bytes.size() - sent);
      const auto result = send(fd, bytes.data() + sent, n, 0);
      if (result <= 0) {
        ADD_FAILURE() << "client send";
        break;
      }
      sent += size_t(result);
      server.poll();
    }
    std::string output;
    for (unsigned i = 0; i < 5000; ++i) {
      server.poll();
      char buffer[512];
      const auto n = recv(fd, buffer, sizeof(buffer), 0);
      if (n == 0) {
        close(fd);
        return output;
      }
      if (n > 0)
        output.append(buffer, size_t(n));
      else
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    ADD_FAILURE() << "response timeout";
    close(fd);
    return output;
  }
  std::string post(const std::string& path, const std::string& body) {
    return "POST " + path +
           " HTTP/1.1\r\nHost: 127.0.0.1\r\nX-X4-Board: 1\r\nContent-Length: " + std::to_string(body.size()) +
           "\r\n\r\n" + body;
  }
  void good(const std::string& result) { EXPECT_EQ(result.find("HTTP/1.1 200 OK\r\n"), 0u); }
  void bad(const std::string& result) { EXPECT_EQ(result.find("HTTP/1.1 400 Bad Request\r\n"), 0u); }
};
TEST_F(Share, FragmentedPostRoundTripAndRingLimit) {
  good(request(post("/post", "hello"), 3));
  auto result = request("GET /messages HTTP/1.1\r\nHost: 127.0.0.1:80\r\n\r\n");
  good(result);
  EXPECT_NE(result.find("hello\n\n"), std::string::npos);
  for (unsigned i = 0; i < 17; ++i) good(request(post("/post", "message " + std::to_string(i))));
  EXPECT_EQ(server.count(), 16u);
  result = request("GET /messages HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
  EXPECT_EQ(result.find("hello"), std::string::npos);
  EXPECT_EQ(result.find("message 0\n"), std::string::npos);
  EXPECT_NE(result.find("message 16\n"), std::string::npos);
}
TEST_F(Share, RejectsHostFramingAndMutationGuardErrors) {
  for (const std::string& headers :
       {"Host: evil.example\r\n", "Host: 127.0.0.1\r\nHost: 127.0.0.1\r\n", "",
        "Host: 127.0.0.1\r\nTransfer-Encoding: chunked\r\n", "Host: 127.0.0.1\r\nExpect: 100-continue\r\n"})
    bad(request("GET / HTTP/1.1\r\n" + headers + "\r\n"));
  bad(request("POST /post HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 1\r\n\r\nx"));
  bad(request(
      "POST /post HTTP/1.1\r\nHost: 127.0.0.1\r\nX-X4-Board: 1\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx"));
  bad(request(post("/post", std::string(201, 'x'))));
  bad(request(post("/post", "a\nb")));
  EXPECT_EQ(server.count(), 0u);
}
TEST_F(Share, DropBinaryEmptyExclusiveLimitAndExactPaths) {
  ASSERT_TRUE(server.start("test_owner", true));
  std::string binary(4096, '\0');
  for (size_t i = 0; i < binary.size(); ++i) binary[i] = char(i % 256);
  good(request(post("/upload", binary), 311));
  auto result = request("GET /file-00.bin HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
  good(result);
  const auto at = result.find("\r\n\r\n");
  ASSERT_NE(at, std::string::npos);
  EXPECT_EQ(result.substr(at + 4), binary);
  good(request(post("/upload", "")));
  EXPECT_EQ(Storage.files.at("/crossink/drop/file-01.bin").size(), 0u);
  bad(request("GET /../file-00.bin HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"));
  bad(request("GET /file-32.bin HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"));
  bad(request(post("/upload", std::string(4097, 'x'))));
  for (unsigned i = 2; i < 32; ++i) good(request(post("/upload", "x")));
  bad(request(post("/upload", "overflow")));
  EXPECT_EQ(Storage.files.size(), 32u);
  EXPECT_EQ(Storage.files.at("/crossink/drop/file-00.bin"), (std::vector<uint8_t>(binary.begin(), binary.end())));
}
TEST_F(Share, DropIoFailureRemovesPartialFile) {
  ASSERT_TRUE(server.start("test_owner", true));
  for (auto fault : {FakeStorage::Fault::Write, FakeStorage::Fault::Sync, FakeStorage::Fault::Close}) {
    Storage.fault = fault;
    bad(request(post("/upload", "bytes")));
    EXPECT_TRUE(Storage.files.empty());
  }
}
TEST_F(Share, RejectsUnownedStartAndTimesOutPartialRequest) {
  EXPECT_FALSE(server.start("wrong_owner"));
  ASSERT_TRUE(server.start("test_owner"));
  const int fd = connectClient();
  ASSERT_GE(fd, 0);
  ASSERT_EQ(send(fd, "GET / HTTP/1.1\r\n", 16, 0), 16);
  for (unsigned i = 0; i < 10; ++i) {
    server.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  testMillis = 5000;
  bool ended = false;
  for (unsigned i = 0; i < 100; ++i) {
    server.poll();
    char byte;
    if (recv(fd, &byte, 1, 0) == 0) {
      ended = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_TRUE(ended);
  close(fd);
  good(request("GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"));
}
}  // namespace
