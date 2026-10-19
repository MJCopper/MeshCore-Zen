#pragma once
#ifdef __cplusplus
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Stream.h"

class File : public Stream {
  std::shared_ptr<std::vector<uint8_t>> _bytes;
  size_t _position = 0;
  size_t* _writes = nullptr;
public:
  File() = default;
  File(std::shared_ptr<std::vector<uint8_t>> bytes, size_t* writes)
      : _bytes(bytes), _writes(writes) { }
  explicit operator bool() const { return bool(_bytes); }
  size_t size() const { return _bytes ? _bytes->size() : 0; }
  int available() override { return size() - _position; }
  int read() override { return available() ? (*_bytes)[_position++] : -1; }
  size_t read(uint8_t* dest, size_t length) { return readBytes(dest, length); }
  using Stream::write;
  size_t write(const uint8_t* data, size_t length) override {
    if (!_bytes) return 0;
    _bytes->insert(_bytes->end(), data, data + length);
    if (_writes) ++*_writes;
    return length;
  }
  void close() { _bytes.reset(); }
};

class MemoryFileSystem {
public:
  std::map<std::string, std::shared_ptr<std::vector<uint8_t>>> files;
  size_t writes = 0;
  bool fail_write = false;
  bool fail_rename = false;
  File open(const char* name, const char* mode = "r", bool = false) {
    if (mode[0] == 'w') {
      if (fail_write) return File();
      files[name] = std::make_shared<std::vector<uint8_t>>();
    }
    auto it = files.find(name);
    return it == files.end() ? File() : File(it->second, &writes);
  }
  bool exists(const char* name) const { return files.count(name); }
  bool remove(const char* name) { return files.erase(name); }
  bool rename(const char* from, const char* to) {
    if (fail_rename || !exists(from)) return false;
    files[to] = files[from];
    files.erase(from);
    return true;
  }
  void mkdir(const char*) { }
};
#endif
