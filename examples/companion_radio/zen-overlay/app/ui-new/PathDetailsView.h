#pragma once

#include "../zen/PathDetails.h"

// Shared, read-only overlay for Node List and Direct Messages. No route probe,
// flash write or companion-protocol change is performed while it is open.
class PathDetailsView {
  uint8_t _key[PUB_KEY_SIZE]{};
  int _scroll = 0;
  zen::PathAttemptSnapshot _attempt{};

  bool contact(ContactInfo& out) const {
    for (int i = 0; i < the_mesh.getNumContacts(); i++) {
      if (the_mesh.getContactByIdx(i, out) &&
          memcmp(out.id.pub_key, _key, PUB_KEY_SIZE) == 0) return true;
    }
    return false;
  }

  void hopLabel(char* out, size_t size, const uint8_t* hash, uint8_t width) const {
    int matches = 0;
    char name[32]{};
    ContactInfo candidate;
    for (int i = 0; i < the_mesh.getNumContacts(); i++) {
      if (!the_mesh.getContactByIdx(i, candidate) ||
          memcmp(candidate.id.pub_key, hash, width)) continue;
      if (++matches > 1) break;
      snprintf(name, sizeof(name), "%s", candidate.name);
    }
    if (matches == 1 && name[0]) {
      snprintf(out, size, "%s", name);
      return;
    }
    char* p = out;
    size_t left = size;
    for (uint8_t i = 0; i < width && left > 2; i++) {
      int n = snprintf(p, left, "%02X", hash[i]);
      p += n;
      left -= n;
    }
  }

public:
  bool active = false;
  const uint8_t* key() const { return _key; }
  void setAttempt(const zen::PathAttemptSnapshot& attempt) { _attempt = attempt; }

  void open(const uint8_t* key, const zen::PathAttemptSnapshot& attempt = {}) {
    memcpy(_key, key, sizeof(_key));
    _attempt = attempt;
    _scroll = 0;
    active = true;
  }

  bool handleInput(char c) {
    if (!active) return false;
    if (c == KEY_CANCEL) active = false;
    else if (c == KEY_DOWN) _scroll++;
    else if (c == KEY_UP && _scroll > 0) _scroll--;
    return true;
  }

  int render(ZenDisplayDriver& display) {
    display.setTextSize(1);
    display.drawCenteredHeader("Path Details");
    ContactInfo target;
    if (!contact(target)) {
      display.drawTextCentered(display.width() / 2, display.listStart(), "Contact unavailable");
      return 1000;
    }
    zen::PathShape shape = zen::pathShape(target.out_path_len, sizeof(target.out_path));
    int visible = (display.height() - display.listStart()) / display.lineStep();
    if (visible < 1) visible = 1;
    int total = 5 + (shape.valid && shape.known ? shape.hops : 0);
    if (_scroll > total - visible) _scroll = total > visible ? total - visible : 0;
    int end = _scroll + visible;
    if (end > total) end = total;
    for (int row = _scroll; row < end; row++) {
      char line[56]{};
      if (row == 0) snprintf(line, sizeof(line), "%s", target.name);
      else if (row == 1) {
        if (!shape.valid) snprintf(line, sizeof(line), "Route: Invalid");
        else if (!shape.known) snprintf(line, sizeof(line), "Route: Unknown (flood)");
        else if (!shape.hops) snprintf(line, sizeof(line), "Route: Direct (0 hops)");
        else snprintf(line, sizeof(line), "Route: Learned (%u hops)", shape.hops);
      } else if (row == 2) {
        if (shape.valid && shape.known)
          snprintf(line, sizeof(line), "Hash: %u byte%s", shape.hash_bytes,
                   shape.hash_bytes == 1 ? "" : "s");
        else snprintf(line, sizeof(line), "Hash: --");
      } else if (row == 3) {
        const char* route = _attempt.route == DELIVERY_ROUTE_PATH ? "Path" :
                            _attempt.route == DELIVERY_ROUTE_DIRECT ? "Direct" :
                            _attempt.route == DELIVERY_ROUTE_FLOOD ? "Flood" : "None";
        const char* result = _attempt.result == ACK_OK ? "OK" :
                             _attempt.result == ACK_FAIL ? "Failed" :
                             _attempt.result == ACK_PENDING ? "Waiting" : "--";
        if (_attempt.tries)
          snprintf(line, sizeof(line), "%s %s; %u sent", route, result, _attempt.tries);
        else snprintf(line, sizeof(line), "Last: No local send");
      } else if (row == 4) {
        if (_attempt.fallback_from_path)
          snprintf(line, sizeof(line), "Route no ACK: %u hops", _attempt.fallback_hops);
        else snprintf(line, sizeof(line), "Learned, not live-tested");
      } else {
        uint8_t hop = (uint8_t)(row - 5);
        char label[40]{};
        hopLabel(label, sizeof(label), target.out_path + hop * shape.hash_bytes,
                 shape.hash_bytes);
        snprintf(line, sizeof(line), "%u: %s", hop + 1, label);
      }
      display.drawTextEllipsized(0, display.listStart() +
          (row - _scroll) * display.lineStep(), display.width() - 2, line, false);
    }
    return 1000;
  }
};
