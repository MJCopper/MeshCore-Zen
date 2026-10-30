#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Storage is independent of baseline NodePrefs and its serializers.
class DoublerSettingsStorage {
public:
  static constexpr size_t MAX_BYTES = 256;
  enum ReadResult { MISSING, READ_OK, READ_ERROR };
  virtual ~DoublerSettingsStorage() = default;
  virtual ReadResult read(char* buffer, size_t capacity, size_t& length) = 0;
  // Replace must preserve the previous file on failure (atomic rename).
  virtual bool replace(const char* buffer, size_t length) = 0;
};

class DoublerSettings {
public:
  enum Status { DEFAULTED, VALID, CORRUPT, UNSUPPORTED, STORAGE_ERROR };

private:
  uint8_t mode = 0;
  Status status = DEFAULTED;

  static bool whitespace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
  static const char* skipSpace(const char* p) { while (whitespace(*p)) p++; return p; }

  struct Reader {
    const char* data;
    size_t length, position = 0;
    Reader(const char* d, size_t n) : data(d), length(n) {}
    int read() { return position < length ? (uint8_t)data[position++] : -1; }
    size_t readBytes(char* destination, size_t count) {
      size_t n = 0;
      while (n < count && position < length) destination[n++] = data[position++];
      return n;
    }
  };

public:
  uint8_t getMode() const { return mode; }
  Status getStatus() const { return status; }

  Status load(DoublerSettingsStorage& storage) {
    char buffer[DoublerSettingsStorage::MAX_BYTES];
    size_t length = 0;
    auto result = storage.read(buffer, sizeof(buffer), length);
    mode = 0;  // invalid/unreadable/unknown settings fail off, never auto-rewrite
    if (result == DoublerSettingsStorage::MISSING) {
      mode = 0;
      return status = DEFAULTED;
    }
    if (result != DoublerSettingsStorage::READ_OK || length > sizeof(buffer)) return status = STORAGE_ERROR;
    Reader reader(buffer, length);
    JsonDocument document;
    auto error = deserializeJson(document, reader, DeserializationOption::NestingLimit(4));
    if (error == DeserializationError::NoMemory || error == DeserializationError::TooDeep)
      return status = STORAGE_ERROR;
    if (error || !document.is<JsonObject>()) return status = CORRUPT;
    while (reader.position < length) {
      if (!whitespace(buffer[reader.position++])) return status = CORRUPT;
    }
    if (!document["schema"].is<uint32_t>()) return status = CORRUPT;
    if (document["schema"].as<uint32_t>() != 1) return status = UNSUPPORTED;
    for (JsonPair field : document.as<JsonObject>()) {
      if (strcmp(field.key().c_str(), "schema") && strcmp(field.key().c_str(), "multi_flood"))
        return status = UNSUPPORTED;
    }
    if (!document["multi_flood"].is<uint8_t>() || document["multi_flood"].as<uint8_t>() > 2)
      return status = CORRUPT;
    mode = document["multi_flood"].as<uint8_t>();
    return status = VALID;
  }

  // Called only via the repeater's existing serial/authenticated-admin path.
  // Return false for unrelated commands so the baseline CLI can handle them.
  bool handleCommand(const char* command, char* reply, DoublerSettingsStorage& storage) {
    constexpr size_t keyLength = sizeof("get multi.flood") - 1;
    bool get = strncmp(command, "get multi.flood", keyLength) == 0;
    bool set = strncmp(command, "set multi.flood", keyLength) == 0;
    if ((!get && !set) || (command[keyLength] && !whitespace(command[keyLength]))) return false;
    const char* argument = skipSpace(command + keyLength);
    if (get) {
      if (*argument) strcpy(reply, "Err - usage: get multi.flood");
      else snprintf(reply, 32, "> %u", (unsigned)mode);
      return true;
    }
    if (*argument < '0' || *argument > '2' || *skipSpace(argument + 1)) {
      strcpy(reply, "Err - expected 0, 1 or 2");
      return true;
    }
    if (status == UNSUPPORTED || status == STORAGE_ERROR) {
      strcpy(reply, "Err - multi.flood settings unavailable; file preserved");
      return true;
    }
    uint8_t requested = *argument - '0';
    if (requested != mode || status == CORRUPT) {
      char buffer[48];
      int length = snprintf(buffer, sizeof(buffer), "{\"schema\":1,\"multi_flood\":%u}\n", (unsigned)requested);
      if (!storage.replace(buffer, length)) {
        strcpy(reply, "Err - unable to save multi.flood");
        return true;
      }
      mode = requested;
      status = VALID;
    }
    strcpy(reply, "OK");
    return true;
  }
};
