#pragma once

#include <stddef.h>
#include <stdint.h>

// Formats at most two channel messages and holds delayed second parts.
// The command rate limit is enforced separately by PublicChannelSensorBot.
class PublicResponseQueue {
public:
  static const size_t MAX_PART_LENGTH = 160;
  static const uint8_t MAX_PENDING = 4;

  PublicResponseQueue();

  // Returns true if a second part is needed. max_body includes the part label.
  static bool split(const char* response, size_t max_body,
                    char first[MAX_PART_LENGTH + 1],
                    char second[MAX_PART_LENGTH + 1]);
  bool scheduleAfterFirst(const char* second, const void* first_packet, uint8_t channel_index = 0);
  void onFirstSent(const void* first_packet, uint32_t now_millis);
  void onFirstFailed(const void* first_packet);
  bool schedule(const char* second, uint32_t due_at, uint8_t channel_index = 0);
  bool takeDue(uint32_t now_millis, char part[MAX_PART_LENGTH + 1], uint8_t* channel_index = NULL);

private:
  struct Pending {
    char text[MAX_PART_LENGTH + 1];
    uint32_t due_at;
    const void* first_packet;
    uint8_t channel_index;
    bool waiting_first;
    bool active;
  };
  Pending _pending[MAX_PENDING];
};
