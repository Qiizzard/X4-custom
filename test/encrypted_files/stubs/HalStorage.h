#pragma once
#include <fcntl.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
constexpr int O_WRITE = O_WRONLY;
struct FakeStorage {
  enum class Fault { None, Open, Read, Write, Sync, Close } fault = Fault::None;
  std::map<std::string, std::vector<uint8_t>> files;
  bool ready() const { return true; }
  bool ensureDirectoryExists(const char*) { return true; }
  bool removeFails = false;
  unsigned handles = 0;
  bool exists(const char* path) const { return files.count(path); }
  bool remove(const char* path) { return !removeFails && files.erase(path); }
  class File {
    FakeStorage* storage = nullptr;
    std::string path;
    size_t position = 0;

   public:
    File() = default;
    File(FakeStorage& s, const char* p) : storage(&s), path(p) { ++s.handles; }
    File(const File&) = delete;
    File(File&& other) noexcept : storage(other.storage), path(std::move(other.path)), position(other.position) {
      other.storage = nullptr;
    }
    ~File() {
      if (storage) close();
    }
    explicit operator bool() const { return storage != nullptr; }
    bool isDirectory() const { return false; }
    size_t fileSize() const { return storage->files.at(path).size(); }
    uint64_t fileSize64() const { return fileSize(); }
    bool seekSet(size_t offset) {
      if (offset > fileSize()) return false;
      position = offset;
      return true;
    }
    size_t read(void* out, size_t n) {
      auto& bytes = storage->files.at(path);
      n = std::min(n, bytes.size() - position);
      if (storage->fault == Fault::Read && n) --n;
      memcpy(out, bytes.data() + position, n);
      position += n;
      return n;
    }
    size_t write(const void* in, size_t n) {
      if (storage->fault == Fault::Write && n) --n;
      auto& bytes = storage->files.at(path);
      bytes.resize(position + n);
      memcpy(bytes.data() + position, in, n);
      position += n;
      return n;
    }
    bool sync() { return storage->fault != Fault::Sync; }
    bool close() {
      if (!storage) return true;
      const bool ok = storage->fault != Fault::Close;
      --storage->handles;
      storage = nullptr;
      return ok;
    }
  };
  File open(const char* path, int flags = 0) {
    if (fault == Fault::Open || ((flags & O_EXCL) && exists(path))) return {};
    if (!exists(path)) {
      if (!(flags & O_CREAT)) return {};
      files[path] = {};
    }
    return File(*this, path);
  }
};
using HalFile = FakeStorage::File;
inline FakeStorage Storage;
