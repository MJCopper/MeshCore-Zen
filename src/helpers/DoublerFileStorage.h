#pragma once

#include "DoublerSettings.h"

// Filesystem adapter for the opt-in nRF52 builds. No baseline file is touched.
// The backend must support atomic rename-over-existing, as LittleFS does.
template<class FS, class WriteMode = uint8_t>
class DoublerFileStorage : public DoublerSettingsStorage {
  FS& fs;
  WriteMode writeMode;

  ReadResult readFile(const char* path, char* buffer, size_t capacity, size_t& length) {
    length = 0;
    if (!fs.exists(path)) return MISSING;
    auto file = fs.open(path);
    if (!file) return READ_ERROR;
    size_t expected = file.size();
    if (expected > capacity) { file.close(); return READ_ERROR; }
    size_t actual = file.read((uint8_t*)buffer, expected);
    file.close();
    if (actual != expected) return READ_ERROR;
    length = actual;
    return READ_OK;
  }

public:
  DoublerFileStorage(FS& filesystem, WriteMode write_mode) : fs(filesystem), writeMode(write_mode) {}
  ReadResult read(char* buffer, size_t capacity, size_t& length) override {
    return readFile("/doubler.json", buffer, capacity, length);
  }
  bool replace(const char* buffer, size_t length) override {
    if (length > MAX_BYTES) return false;
    if (fs.exists("/doubler.tmp") && !fs.remove("/doubler.tmp")) return false;
    auto file = fs.open("/doubler.tmp", writeMode);
    if (!file) return false;
    size_t written = file.write((const uint8_t*)buffer, length);
    file.close();
    char verification[MAX_BYTES];
    size_t verifiedLength = 0;
    bool valid = written == length
        && readFile("/doubler.tmp", verification, sizeof(verification), verifiedLength) == READ_OK
        && verifiedLength == length && memcmp(buffer, verification, length) == 0;
    if (!valid || !fs.rename("/doubler.tmp", "/doubler.json")) {
      fs.remove("/doubler.tmp");
      return false;
    }
    return true;
  }
};
