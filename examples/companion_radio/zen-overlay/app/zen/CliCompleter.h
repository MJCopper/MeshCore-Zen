#pragma once

#include "T9Predictor.h"

namespace zen {

// Console-only, flash-resident vocabulary. Ordered by expected usefulness,
// not scraped prose frequency. Source: https://docs.meshcore.io/cli_commands/
// Reviewed 2026-09-06. Tokens are suggestions, not capability declarations:
// commands remain subject to the remote firmware, hardware and permissions.
class CliCompleter {
  static const char* const* words() {
    static const char* const WORDS[] = {
      "get", "set", "radio", "name", "tx", "ver", "board", "clock",
      "advert", "on", "off", "repeat", "region", "gps", "sensor",
      "owner.info", "advert.interval", "flood.advert.interval",
      "txdelay", "rxdelay", "direct.txdelay", "flood.max", "dutycycle",
      "path.hash.mode", "loop.detect", "multi.acks", "radio.rxgain",
      "flood.max.unscoped", "flood.max.advert", "guest.password",
      "allow.read.only", "password", "setperm", "public.key", "role",
      "freq", "lat", "lon", "time", "sync", "setloc", "none", "share", "prefs",
      "minimal", "moderate", "strict", "save", "load", "allowf", "denyf",
      "home", "default", "put", "def", "remove", "list", "allowed", "denied",
      "neighbors", "neighbor.remove", "discover.neighbors", "advert.zerohop",
      "tempradio", "powersaving", "start", "stop", "ota", "clear", "stats",
      "log", "reboot", "poweroff", "shutdown", "clkreboot",
      "adc.multiplier", "int.thresh", "agc.reset.interval", "cad",
      "radio.fem.rxgain", "radio.fem.txgain", "af",
      "bridge.type", "bridge.enabled", "bridge.delay", "bridge.source",
      "bridge.baud", "bridge.channel", "bridge.secret", "bootloader.ver",
      "pwrmgt.support", "pwrmgt.source", "pwrmgt.bootreason", "pwrmgt.bootmv",
      "eth.status", "prv.key", "erase", "acl", "stats-core", "stats-radio", "stats-packets",
      nullptr
    };
    return WORDS;
  }
  static char lower(char c) { return c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c; }
  static bool prefixMatch(const char* word, const char* prefix, size_t length) {
    for (size_t i = 0; i < length; i++)
      if (!word[i] || lower(word[i]) != lower(prefix[i])) return false;
    return true;
  }
  static bool append(const char* word, char out[][WordCompleter::MAX_WORD_LEN],
                     uint8_t& count, uint8_t max_results, size_t max_bytes) {
    size_t length = strlen(word);
    if (count >= max_results || length >= WordCompleter::MAX_WORD_LEN || length > max_bytes) return false;
    for (uint8_t i = 0; i < count; i++) if (!strcmp(word, out[i])) return false;
    memcpy(out[count++], word, length + 1);
    return true;
  }

public:
  static size_t dictionarySize() {
    size_t n = 0;
    while (words()[n]) n++;
    return n;
  }
  static const char* wordAt(size_t index) { return index < dictionarySize() ? words()[index] : nullptr; }
  static bool tokenChar(char c) {
    c = lower(c);
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '.' || c == '_' || c == '-' || c == '\'';
  }
  static WordCompleter::WordRange currentToken(const char* text, size_t len, size_t cursor) {
    if (cursor > len) cursor = len;
    size_t start = cursor, end = cursor;
    while (start && tokenChar(text[start - 1])) start--;
    while (end < len && tokenChar(text[end])) end++;
    return {start, end};
  }

  static uint8_t suggestT9(const char* digits, size_t length,
                           char out[][WordCompleter::MAX_WORD_LEN], uint8_t max_results,
                           size_t max_bytes = WordCompleter::MAX_WORD_LEN - 1) {
    if (!digits || !length || !out || !max_results) return 0;
    if (max_results > WordCompleter::MAX_SUGGESTIONS) max_results = WordCompleter::MAX_SUGGESTIONS;
    uint8_t count = 0;
    // Exact CLI words, then longer CLI completions, then conversational T9.
    // Dots/hyphens consume no digit, just like apostrophes in chat prediction.
    for (int pass = 0; pass < 2 && count < max_results; pass++) {
      for (size_t i = 0; words()[i] && count < max_results; i++) {
        const char* word = words()[i];
        if ((T9Predictor::digitLength(word) == length) != (pass == 0) ||
            !T9Predictor::matches(word, digits, length)) continue;
        append(word, out, count, max_results, max_bytes);
      }
    }
    if (count < max_results) {
      char chat[WordCompleter::MAX_SUGGESTIONS][WordCompleter::MAX_WORD_LEN];
      uint8_t n = T9Predictor::suggest(digits, length, chat, WordCompleter::MAX_SUGGESTIONS, max_bytes);
      for (uint8_t i = 0; i < n && count < max_results; i++) append(chat[i], out, count, max_results, max_bytes);
    }
    return count;
  }

  static uint8_t suggest(const char* prefix, size_t length,
                         char out[][WordCompleter::MAX_WORD_LEN], uint8_t max_results,
                         size_t max_bytes = WordCompleter::MAX_WORD_LEN - 1) {
    if (!prefix || !length || !out || !max_results) return 0;
    if (max_results > WordCompleter::MAX_SUGGESTIONS) max_results = WordCompleter::MAX_SUGGESTIONS;
    uint8_t count = 0;
    for (size_t i = 0; words()[i] && count < max_results; i++) {
      const char* word = words()[i];
      if (strlen(word) > length && prefixMatch(word, prefix, length))
        append(word, out, count, max_results, max_bytes);
    }
    // Walk the existing dictionary directly: no second copy, and capacity
    // filtering cannot hide a shorter candidate behind eight longer matches.
    for (size_t i = 0; i < WordCompleter::dictionarySize() && count < max_results; i++) {
      const char* word = WordCompleter::wordAt(i);
      if (strlen(word) > length && prefixMatch(word, prefix, length))
        append(word, out, count, max_results, max_bytes);
    }
    return count;
  }
};

} // namespace zen
