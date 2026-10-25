#include "PublicChannelSensorBot.h"

#include <ctype.h>
#include <helpers/UTF8Helpers.h>
#include <string.h>

PublicChannelSensorBot::PublicChannelSensorBot(const char* channel_name)
    : _next_recent(0), _next_reply(0), _reply_count(0) {
  static const uint8_t PUBLIC_SECRET[16] = {
    0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
    0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72,
  };
  memset(&_channel, 0, sizeof(_channel));
  memset(_recent_commands, 0, sizeof(_recent_commands));
  memset(_reply_times, 0, sizeof(_reply_times));
  if (channel_name && channel_name[0] == '#') {
    mesh::Utils::sha256(_channel.secret, sizeof(PUBLIC_SECRET),
                        reinterpret_cast<const uint8_t*>(channel_name), strlen(channel_name));
  } else {
    memcpy(_channel.secret, PUBLIC_SECRET, sizeof(PUBLIC_SECRET));
  }
  mesh::Utils::sha256(_channel.hash, sizeof(_channel.hash),
                      _channel.secret, sizeof(PUBLIC_SECRET));
  memset(&_public_channel, 0, sizeof(_public_channel));
  memcpy(_public_channel.secret, PUBLIC_SECRET, sizeof(PUBLIC_SECRET));
  mesh::Utils::sha256(_public_channel.hash, sizeof(_public_channel.hash),
                      _public_channel.secret, sizeof(PUBLIC_SECRET));
}

bool PublicChannelSensorBot::equalsIgnoreCase(const char* lhs, const char* rhs) {
  while (*lhs && *rhs) {
    if (tolower((unsigned char)*lhs++) != tolower((unsigned char)*rhs++)) return false;
  }
  return *lhs == 0 && *rhs == 0;
}

int PublicChannelSensorBot::findChannel(const uint8_t* hash, mesh::GroupChannel channels[],
                                        int max_matches) const {
  if (!hash || !channels || max_matches < 1) return 0;
  int count = 0;
  if (hash[0] == _channel.hash[0]) channels[count++] = _channel;
  if (count < max_matches && hash[0] == _public_channel.hash[0] &&
      memcmp(_channel.secret, _public_channel.secret, 16) != 0)
    channels[count++] = _public_channel;
  return count;
}

int PublicChannelSensorBot::channelIndex(const mesh::GroupChannel& source) const {
  if (memcmp(source.secret, _channel.secret, 16) == 0) return 0;
  if (memcmp(source.secret, _public_channel.secret, 16) == 0) return 1;
  return -1;
}

bool PublicChannelSensorBot::accept(uint8_t type, uint8_t* data, size_t len,
                                    uint32_t now_millis, bool trace_busy,
                                    uint8_t& metric_mask, bool& busy_trace,
                                    uint8_t channel_index, uint8_t* trace_prefix, bool* targeted,
                                    TraceAction* trace_action, char* requester_name,
                                    size_t requester_name_size) {
  metric_mask = 0;
  busy_trace = false;
  if (targeted) *targeted = false;
  if (requester_name && requester_name_size) requester_name[0] = 0;
  uint8_t prefix[2]{};
  bool has_target = false;
  TraceAction action = TRACE_START;
  if (trace_action) *trace_action = action;
  if (type != PAYLOAD_TYPE_GRP_TXT || !data || len < 6 || len >= MAX_PACKET_PAYLOAD) return false;
  if ((data[4] >> 2) != 0) return false;

  data[len] = 0;
  char* text = reinterpret_cast<char*>(&data[5]);
  char* body = strstr(text, ": ");
  if (!body) return false;
  if (requester_name && requester_name_size) {
    char normalized[MAX_PACKET_PAYLOAD]{};
    size_t source_length = (size_t)(body - text);
    size_t valid_length = mesh::validUtf8PrefixLength(text, source_length);
    size_t used = 0;
    bool pending_space = false;
    for (size_t i = 0; i < valid_length; ++i) {
      unsigned char ch = (unsigned char)text[i];
      if (ch <= 0x20 || ch == 0x7F) {
        pending_space = used != 0;
        continue;
      }
      if (pending_space && used < sizeof(normalized) - 1) normalized[used++] = ' ';
      pending_space = false;
      if (used < sizeof(normalized) - 1) normalized[used++] = (char)ch;
    }
    size_t copy_length = mesh::validUtf8PrefixLength(normalized, used);
    if (copy_length >= requester_name_size) {
      copy_length = mesh::validUtf8PrefixLength(normalized, requester_name_size - 1);
    }
    if (copy_length) memcpy(requester_name, normalized, copy_length);
    requester_name[copy_length] = 0;
  }
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
  } else if (strncasecmp(argument, "trace", 5) == 0 &&
             (argument[5] == 0 || isspace((unsigned char)argument[5]))) {
    char* destination = argument + 5;
    while (isspace((unsigned char)*destination)) destination++;
    if (equalsIgnoreCase(destination, "status")) action = TRACE_STATUS;
    else if (equalsIgnoreCase(destination, "cancel")) action = TRACE_CANCEL;
    else if (*destination) {
      if (strlen(destination) != 4) return false;
      for (uint8_t i = 0; i < 4; i++) {
        unsigned char c = tolower((unsigned char)destination[i]);
        if (!isxdigit(c)) return false;
        prefix[i / 2] = (prefix[i / 2] << 4) | (c <= '9' ? c - '0' : c - 'a' + 10);
      }
      has_target = true;
    }
    metric_mask = REQUEST_TRACE;
  } else {
    char* token = argument;
    while (*token) {
      char* separator = token;
      while (*separator && !isspace((unsigned char)*separator)) separator++;
      if (*separator) *separator++ = 0;

      if (strncasecmp(token, "trace", 5) == 0) return false;
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

  if (trace_action) *trace_action = action;
  if (targeted) *targeted = has_target;
  if (trace_prefix && has_target) memcpy(trace_prefix, prefix, sizeof(prefix));
  if (metric_mask == REQUEST_TRACE && action == TRACE_START && trace_busy) {
    busy_trace = true;
    return true;
  }

  uint32_t command_hash;
  mesh::Utils::sha256(reinterpret_cast<uint8_t*>(&command_hash), sizeof(command_hash),
                      data, len, &channel_index, sizeof(channel_index));
  if (command_hash == 0) command_hash = 1;
  for (int i = 0; i < RECENT_COMMANDS; i++)
    if (_recent_commands[i] == command_hash) return false;

  // An active operation must always be cancellable, even when normal command
  // slots are exhausted. One cancel ack per rate-limited start remains bounded.
  if (action == TRACE_CANCEL && trace_busy) {
    _recent_commands[_next_recent] = command_hash;
    _next_recent = (_next_recent + 1) % RECENT_COMMANDS;
    return true;
  }

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
