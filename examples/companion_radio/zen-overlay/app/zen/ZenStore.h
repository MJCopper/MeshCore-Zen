#pragma once

#include <Arduino.h>
#include "../ZenPrefs.h"
#include "ZenPrefsCodec.h"

namespace zen {

// Persistence boundary for Zen-owned state. This class never receives the
// MeshCore DataStore and cannot name or modify baseline files.
class ZenStore {
public:
  enum class Failure : uint8_t {
    NONE, FILESYSTEM_UNAVAILABLE, WRITE_LOCKED, ENCODE_FAILED,
    TEMP_WRITE_FAILED, TEMP_VERIFY_FAILED, RENAME_FAILED,
    BACKUP_FAILED, PROMOTION_FAILED, PROMOTION_VERIFY_FAILED
  };

private:
  FILESYSTEM* _fs = nullptr;
  uint32_t _generation = 0;
  uint64_t _fingerprint = 0;
  bool _fingerprint_valid = false;
  bool _write_locked = false;
  bool _recovery_needed = false;
  Failure _failure = Failure::NONE;
  uint8_t _buffer[ZenPrefsCodec::MAX_ENCODED_SIZE]{};

  static File openRead(FILESYSTEM* fs, const char* path) {
#if defined(RP2040_PLATFORM)
    return fs->open(path, "r");
#else
    return fs->open(path);
#endif
  }

  static File openWrite(FILESYSTEM* fs, const char* path) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
    fs->remove(path);
    return fs->open(path, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
    return fs->open(path, "w");
#else
    return fs->open(path, "w", true);
#endif
  }

  bool writeAndVerify(const char* path, const uint8_t* data, size_t size,
                      uint32_t generation) {
    File file = openWrite(_fs, path);
    bool ok = file && file.write(data, size) == size;
    if (file) file.close();
    if (!ok) { _failure = Failure::TEMP_WRITE_FAILED; return false; }

    File check = openRead(_fs, path);
    size_t checked_size = check ? (size_t)check.size() : 0;
    bool checked = checked_size == size &&
                   check.read(_buffer, size) == (int)size;
    if (check) check.close();
    uint32_t checked_generation = 0;
    bool verified = checked && ZenPrefsCodec::inspect(
      _buffer, size, checked_generation) && checked_generation == generation;
    if (!verified) _failure = Failure::TEMP_VERIFY_FAILED;
    return verified;
  }

  bool rawMatches(const char* path, const uint8_t* data, size_t size) {
    if (!_fs || !data || size > sizeof(_buffer)) return false;
    File check = openRead(_fs, path);
    bool ok = check && (size_t)check.size() == size &&
              check.read(_buffer, size) == (int)size &&
              memcmp(_buffer, data, size) == 0;
    if (check) check.close();
    return ok;
  }

  bool envelopeMatches(const char* path, size_t size, uint32_t generation) {
    File check = openRead(_fs, path);
    bool read_ok = check && (size_t)check.size() == size &&
                   check.read(_buffer, size) == (int)size;
    if (check) check.close();
    uint32_t found = 0;
    return read_ok && ZenPrefsCodec::inspect(_buffer, size, found) &&
           found == generation;
  }

  bool loadCandidate(const char* path, ZenPrefs& prefs, bool primary) {
    File file = openRead(_fs, path);
    if (!file) return false;
    size_t size = (size_t)file.size();
    bool read_ok = size <= sizeof(_buffer) &&
                   file.read(_buffer, size) == (int)size;
    file.close();
    if (read_ok && ZenPrefsCodec::hasNewerVersion(_buffer, size)) {
      if (primary) _write_locked = true;
      return false;
    }
    uint32_t generation = 0;
    if (!read_ok || !ZenPrefsCodec::decode(prefs, _buffer, size, &generation))
      return false;
    _generation = generation;
    _fingerprint = ZenPrefsCodec::fingerprint(
        prefs, _buffer, sizeof(_buffer));
    _fingerprint_valid = _fingerprint != 0;
    return _fingerprint_valid;
  }

public:
  static constexpr const char* PREFS_PATH = "/zen_prefs";
  static constexpr const char* PREFS_TMP_PATH = "/zen_prefs.tmp";
  static constexpr const char* PREFS_BAK_PATH = "/zen_prefs.bak";

  void begin(FILESYSTEM* external, FILESYSTEM* fallback) {
    _fs = external ? external : fallback;
  }

  bool load(ZenPrefs& prefs) {
    if (!_fs) return false;
    _write_locked = false;
    _recovery_needed = false;
    if (loadCandidate(PREFS_PATH, prefs, true)) return true;
    if (_write_locked) return false;

    // Recover an interrupted promotion from the verified staging record first,
    // then from the previous committed record. A subsequent save promotes it.
    if (loadCandidate(PREFS_TMP_PATH, prefs, false) ||
        loadCandidate(PREFS_BAK_PATH, prefs, false)) {
      _fingerprint_valid = false;
      _recovery_needed = true;
      return true;
    }
    return false;
  }

  bool save(const ZenPrefs& prefs) {
    _failure = Failure::NONE;
    if (!_fs) { _failure = Failure::FILESYSTEM_UNAVAILABLE; return false; }
    if (_write_locked) { _failure = Failure::WRITE_LOCKED; return false; }
    uint64_t fingerprint = ZenPrefsCodec::fingerprint(
        prefs, _buffer, sizeof(_buffer));
    if (!fingerprint) { _failure = Failure::ENCODE_FAILED; return false; }

    uint32_t generation = _generation + 1;
    size_t size = ZenPrefsCodec::encode(
        prefs, generation, _buffer, sizeof(_buffer));
    if (!size) { _failure = Failure::ENCODE_FAILED; return false; }
    if (!replaceVerified(PREFS_TMP_PATH, PREFS_PATH, PREFS_BAK_PATH,
                         _buffer, size, generation)) return false;
    _generation = generation;
    _fingerprint = fingerprint;
    _fingerprint_valid = true;
    return true;
  }

  File openRead(const char* path) { return openRead(_fs, path); }
  File openWrite(const char* path) { return openWrite(_fs, path); }
  bool remove(const char* path) { return _fs && _fs->remove(path); }
  bool writeLocked() const { return _write_locked; }
  bool recoveryNeeded() const { return _recovery_needed; }
  Failure failure() const { return _failure; }
  uint32_t generation() const { return _generation; }

  bool replaceVerified(const char* temporary, const char* final_path,
                       const char* backup, const uint8_t* data, size_t size,
                       uint32_t generation = 0) {
    if (!_fs || !temporary || !final_path || !backup || !data ||
        size > sizeof(_buffer)) return false;

    bool staged = generation
        ? writeAndVerify(temporary, data, size, generation)
        : ([&]() {
            File file = openWrite(_fs, temporary);
            bool ok = file && file.write(data, size) == size;
            if (file) file.close();
            return ok && rawMatches(temporary, data, size);
          })();
    if (!staged) { _fs->remove(temporary); return false; }

    auto finalMatches = [&]() {
      return generation ? envelopeMatches(final_path, size, generation)
                        : rawMatches(final_path, data, size);
    };
    bool first_rename_ok = _fs->rename(temporary, final_path);
    if (first_rename_ok && finalMatches()) {
      _fs->remove(backup);
      _recovery_needed = false;
      return true;
    }

    // A successful rename has already promoted the new record. If its
    // verification read is inconclusive, keep both candidates for boot-time
    // recovery instead of moving the new record into the backup path.
    if (first_rename_ok) {
      _failure = Failure::PROMOTION_VERIFY_FAILED;
      _recovery_needed = true;
      return false;
    }

    // QSPI LittleFS may not replace an existing destination. Move the known
    // good old file aside first, then promote the already-verified temporary.
    _fs->remove(backup);
    File existing = openRead(_fs, final_path);
    bool had_existing = (bool)existing;
    if (existing) existing.close();
    if (had_existing && !_fs->rename(final_path, backup)) {
      _failure = Failure::BACKUP_FAILED;
      return false;
    }

    bool second_rename_ok = _fs->rename(temporary, final_path);
    if (second_rename_ok && finalMatches()) {
      _fs->remove(backup);
      _recovery_needed = false;
      return true;
    }

    if (second_rename_ok) {
      _failure = Failure::PROMOTION_VERIFY_FAILED;
      _recovery_needed = true;
      return false;
    }

    // Leave the previous valid record in service whenever promotion fails.
    _fs->remove(final_path);
    if (had_existing) _fs->rename(backup, final_path);
    _failure = Failure::PROMOTION_FAILED;
    return false;
  }
};

} // namespace zen
