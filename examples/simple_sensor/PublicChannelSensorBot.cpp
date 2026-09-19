#include "PublicChannelSensorBot.h"

#include <ctype.h>
#include <string.h>

PublicChannelSensorBot::PublicChannelSensorBot()
    : _next_recent(0), _next_reply(0), _reply_count(0) {
  static const uint8_t PUBLIC_SECRET[16] = {
    0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
    0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72,
  };
  memset(&_channel, 0, sizeof(_channel));
  memset(_recent_commands, 0, sizeof(_recent_commands));
  memset(_reply_times, 0, sizeof(_reply_times));
  memcpy(_channel.secret, PUBLIC_SECRET, sizeof(PUBLIC_SECRET));
  mesh::Utils::sha256(_channel.hash, sizeof(_channel.hash),
                      _channel.secret, sizeof(PUBLIC_SECRET));
}

bool PublicChannelSensorBot::equalsIgnoreCase(const char* lhs, const char* rhs) {
  while (*lhs && *rhs) {
    if (tolower((unsigned char)*lhs++) != tolower((unsigned char)*rhs++)) return false;
  }
  return *lhs == 0 && *rhs == 0;
}

int PublicChannelSensorBot::findChannel(const uint8_t* hash, mesh::GroupChannel channels[],
                                        int max_matches) const {
  if (!hash || !channels || max_matches < 1 || hash[0] != _channel.hash[0]) return 0;
  channels[0] = _channel;
  return 1;
}

bool PublicChannelSensorBot::accept(uint8_t type, uint8_t* data, size_t len,
                                    uint32_t now_millis, uint8_t& metric_mask) {
  metric_mask = 0;
  if (type != PAYLOAD_TYPE_GRP_TXT || !data || len < 6 || len >= MAX_PACKET_PAYLOAD) return false;
  if ((data[4] >> 2) != 0) return false;

  data[len] = 0;
  char* text = reinterpret_cast<char*>(&data[5]);
  char* body = strstr(text, ": ");
  if (!body) return false;
  body += 2;
  while (*body == ' ') body++;

  static const char COMMAND[] = "!hillvue";
  if (strncasecmp(body, COMMAND, sizeof(COMMAND) - 1) != 0) return false;
  char* argument = body + sizeof(COMMAND) - 1;
  if (*argument != 0 && !isspace((unsigned char)*argument)) return false;
  while (isspace((unsigned char)*argument)) argument++;

  char* end = argument + strlen(argument);
  while (end > argument && isspace((unsigned char)end[-1])) *--end = 0;
  if (*argument == 0 || equalsIgnoreCase(argument, "all")) {
    metric_mask = METRIC_ALL;
  } else if (equalsIgnoreCase(argument, "ping")) {
    metric_mask = REQUEST_PING;
  } else if (equalsIgnoreCase(argument, "path")) {
    metric_mask = REQUEST_PATH;
  } else if (equalsIgnoreCase(argument, "trace")) {
    metric_mask = REQUEST_TRACE;
  } else {
    char* token = argument;
    while (*token) {
      char* separator = token;
      while (*separator && !isspace((unsigned char)*separator)) separator++;
      if (*separator) *separator++ = 0;

      if (equalsIgnoreCase(token, "ping") || equalsIgnoreCase(token, "path") ||
          equalsIgnoreCase(token, "trace")) {
        return false;  // Standalone requests cannot be combined with measurements.
      } else if (equalsIgnoreCase(token, "all")) {
        metric_mask = METRIC_ALL;
      } else {
        switch (tolower((unsigned char)token[0])) {
          case 't': metric_mask |= METRIC_TEMPERATURE; break;
          case 'h': metric_mask |= METRIC_HUMIDITY; break;
          case 'p': metric_mask |= METRIC_PRESSURE; break;
          case 'a': metric_mask |= METRIC_AIR_QUALITY; break;
          case 'v': metric_mask |= METRIC_VOLTAGE; break;
          default: return false;
        }
      }

      token = separator;
      while (isspace((unsigned char)*token)) token++;
    }
  }

  uint32_t command_hash;
  mesh::Utils::sha256(reinterpret_cast<uint8_t*>(&command_hash), sizeof(command_hash),
                      data, len);
  if (command_hash == 0) command_hash = 1;
  for (int i = 0; i < RECENT_COMMANDS; i++)
    if (_recent_commands[i] == command_hash) return false;

  // The next slot is the oldest accepted reply once the window is full.
  if (_reply_count == MAX_REPLIES_PER_WINDOW &&
      (uint32_t)(now_millis - _reply_times[_next_reply]) < REPLY_WINDOW_MILLIS) return false;

  _recent_commands[_next_recent] = command_hash;
  _next_recent = (_next_recent + 1) % RECENT_COMMANDS;
  _reply_times[_next_reply] = now_millis;
  _next_reply = (_next_reply + 1) % MAX_REPLIES_PER_WINDOW;
  if (_reply_count < MAX_REPLIES_PER_WINDOW) _reply_count++;
  return true;
}
