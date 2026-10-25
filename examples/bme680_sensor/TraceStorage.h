#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace trace_storage {
inline uint32_t checksum(const void* bytes, size_t size, uint32_t hash = 2166136261UL) {
  auto p = static_cast<const uint8_t*>(bytes);
  while (size--) hash = (hash ^ *p++) * 16777619UL;
  return hash;
}
inline File open(FILESYSTEM* fs, const char* path, bool write) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return fs->open(path, write ? FILE_O_WRITE : FILE_O_READ);
#else
  return fs->open(path, write ? "w" : "r");
#endif
}
inline bool read(FILESYSTEM* fs, const char* path, void* data, size_t size) {
  if (!fs) return false;
  auto f = open(fs, path, false);
  bool ok = f && f.size() == size && f.read((uint8_t*)data, size) == size;
  f.close(); return ok;
}
inline bool write(FILESYSTEM* fs, const char* path, const void* data, size_t size) {
  if (!fs) return false;
  char temp[64]; snprintf(temp, sizeof(temp), "%s.tmp", path);
  fs->remove(temp);
  auto f = open(fs, temp, true);
  bool ok = f && f.write((const uint8_t*)data, size) == size;
  f.close();
  if (ok) ok = fs->rename(temp, path);
  if (!ok) fs->remove(temp);
  return ok;
}
}
