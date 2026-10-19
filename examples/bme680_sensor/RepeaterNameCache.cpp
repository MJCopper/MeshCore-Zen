#include "RepeaterNameCache.h"

#include <helpers/AdvertDataHelpers.h>
#include <helpers/UTF8Helpers.h>
#include <string.h>

static const char* CACHE_FILE = "/repeaters";
static const char* TEMP_FILE = "/repeaters.tmp";
static const uint32_t CACHE_MAGIC = 0x31504552;  // REP1

static File openRead(FILESYSTEM* fs, const char* path) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return fs->open(path, FILE_O_READ);
#else
  return fs->open(path, "r");
#endif
}

static File openWrite(FILESYSTEM* fs, const char* path) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return fs->open(path, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return fs->open(path, "w");
#else
  return fs->open(path, "w", true);
#endif
}

struct CacheHeader {
  uint32_t magic;
  uint32_t checksum;
  uint8_t count;
  uint8_t next_replace;
  uint8_t reserved[2];
};

static uint32_t checksumBytes(uint32_t hash, const uint8_t* data, size_t len) {
  while (len--) {
    hash ^= *data++;
    hash *= 16777619UL;
  }
  return hash;
}

RepeaterNameCache::RepeaterNameCache()
    : _fs(NULL), _count(0), _next_replace(0), _dirty_at(0),
      _last_save_at(0), _dirty(false), _has_saved(false) {
  memset(_entries, 0, sizeof(_entries));
}

void RepeaterNameCache::begin(FILESYSTEM* fs) {
  _fs = fs;
  _has_saved = load();
  _last_save_at = millis();
}

bool RepeaterNameCache::load() {
  if (!_fs) return false;
  File file = openRead(_fs, CACHE_FILE);
  if (!file) return false;

  CacheHeader header;
  bool valid = file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) == sizeof(header) &&
               header.magic == CACHE_MAGIC && header.count <= MAX_REPEATERS &&
               header.next_replace < MAX_REPEATERS &&
               file.size() == sizeof(header) + header.count * sizeof(Entry);
  uint32_t hash = 2166136261UL;
  if (valid) {
    hash = checksumBytes(hash, &header.count, 1);
    hash = checksumBytes(hash, &header.next_replace, 1);
    for (uint8_t i = 0; i < header.count; i++) {
      if (file.read(reinterpret_cast<uint8_t*>(&_entries[i]), sizeof(Entry)) != sizeof(Entry) ||
          _entries[i].name[sizeof(_entries[i].name) - 1] != 0) {
        valid = false;
        break;
      }
      hash = checksumBytes(hash, reinterpret_cast<const uint8_t*>(&_entries[i]), sizeof(Entry));
    }
  }
  file.close();
  if (!valid || hash != header.checksum) {
    _count = _next_replace = 0;
    return false;
  }
  _count = header.count;
  _next_replace = header.next_replace;
  return true;
}

bool RepeaterNameCache::save() {
  if (!_fs) return false;
  CacheHeader header = { CACHE_MAGIC, 2166136261UL, _count, _next_replace, { 0, 0 } };
  header.checksum = checksumBytes(header.checksum, &header.count, 1);
  header.checksum = checksumBytes(header.checksum, &header.next_replace, 1);
  for (uint8_t i = 0; i < _count; i++)
    header.checksum = checksumBytes(header.checksum,
                                    reinterpret_cast<const uint8_t*>(&_entries[i]), sizeof(Entry));

  _fs->remove(TEMP_FILE);
  File file = openWrite(_fs, TEMP_FILE);
  if (!file) return false;
  bool ok = file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header);
  for (uint8_t i = 0; ok && i < _count; i++)
    ok = file.write(reinterpret_cast<const uint8_t*>(&_entries[i]), sizeof(Entry)) == sizeof(Entry);
  file.close();
  if (ok) ok = _fs->rename(TEMP_FILE, CACHE_FILE);
  if (!ok) _fs->remove(TEMP_FILE);
  return ok;
}

void RepeaterNameCache::loop(uint32_t now_millis) {
  if (!_dirty || (uint32_t)(now_millis - _dirty_at) < SAVE_DELAY_MILLIS ||
      (_has_saved && (uint32_t)(now_millis - _last_save_at) < MIN_SAVE_INTERVAL_MILLIS)) return;

  bool saved = save();
  _last_save_at = now_millis;  // throttle retries if flash is unavailable
  _has_saved = true;
  if (saved) _dirty = false;
}

void RepeaterNameCache::onAdvert(const mesh::Identity& id, uint32_t timestamp,
                                 const uint8_t* app_data, size_t app_data_len,
                                 uint32_t now_millis) {
  if (!app_data || app_data_len == 0 || app_data_len > MAX_ADVERT_DATA_SIZE) return;
  AdvertDataParser parser(app_data, app_data_len);
  if (!parser.isValid() || !parser.hasName() || parser.getType() != ADV_TYPE_REPEATER) return;

  uint8_t index = 0;
  while (index < _count && memcmp(_entries[index].pub_key, id.pub_key, PUB_KEY_SIZE) != 0) index++;
  bool is_new = index == _count;
  if (!is_new && timestamp <= _entries[index].last_advert_timestamp) return;
  if (is_new) {
    if (_count < MAX_REPEATERS) {
      _count++;
    } else {
      index = _next_replace;
      _next_replace = (_next_replace + 1) % MAX_REPEATERS;
    }
    memcpy(_entries[index].pub_key, id.pub_key, PUB_KEY_SIZE);
  }
  _entries[index].last_advert_timestamp = timestamp;

  char name[32];
  strncpy(name, parser.getName(), sizeof(name) - 1);
  name[sizeof(name) - 1] = 0;
  for (char* p = name; *p; p++)
    if ((unsigned char)*p < 0x20 || (unsigned char)*p == 0x7F) *p = ' ';
  if (!is_new && strcmp(_entries[index].name, name) == 0) return;  // No flash write.
  strncpy(_entries[index].name, name, sizeof(_entries[index].name));
  if (!_dirty) _dirty_at = now_millis;
  _dirty = true;
}

const char* RepeaterNameCache::findName(const uint8_t* hash, uint8_t hash_size) const {
  const char* match = NULL;
  for (uint8_t i = 0; i < _count; i++) {
    if (memcmp(_entries[i].pub_key, hash, hash_size) == 0) {
      if (match) return NULL;  // A short hash matches more than one repeater.
      match = _entries[i].name;
    }
  }
  return match;
}

void RepeaterNameCache::formatLabel(const uint8_t* hash, uint8_t hash_size,
                                    char* dest, size_t size) const {
  if (!dest || size == 0) return;
  dest[0] = 0;
  if (!hash || hash_size == 0 || hash_size > 3) return;

  const char* name = findName(hash, hash_size);
  if (name) {
    size_t len = mesh::validUtf8PrefixLength(name, size - 1);
    memcpy(dest, name, len);
    dest[len] = 0;
  } else if (size >= hash_size * 2 + 1) {
    for (uint8_t i = 0; i < hash_size; i++)
      snprintf(&dest[i * 2], size - i * 2, "%02X", hash[i]);
  }
}

void RepeaterNameCache::formatPath(const mesh::Packet* packet, char* dest, size_t size) const {
  if (!dest || size == 0) return;
  dest[0] = 0;
  if (!packet || !packet->isRouteFlood()) {
    snprintf(dest, size, "Path unavailable (direct route)");
    return;
  }

  uint8_t count = packet->getPathHashCount();
  uint8_t hash_size = packet->getPathHashSize();
  if (count == 0) {
    snprintf(dest, size, "Path: direct (0 repeaters)");
    return;
  }

  int used = snprintf(dest, size, "Path (%u repeaters): ", count);
  for (uint8_t i = 0; i < count && used > 0 && (size_t)used < size; i++) {
    const uint8_t* hash = &packet->path[i * hash_size];
    char label[33];
    formatLabel(hash, hash_size, label, sizeof(label));
    size_t needed = strlen(label) + (i ? 3 : 0);
    if ((size_t)used + needed >= size) {
      snprintf(&dest[used], size - used, "...");
      break;
    }
    used += snprintf(&dest[used], size - used, "%s%s", i ? " > " : "", label);
  }
}
