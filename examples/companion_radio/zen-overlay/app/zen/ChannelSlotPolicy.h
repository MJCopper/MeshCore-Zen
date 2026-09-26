#pragma once

#include <cstring>
#include <cstdint>

namespace zen {

struct ChannelSlotPolicy {
  static bool isEmpty(const uint8_t secret[32]) {
    for (int i = 0; i < 32; i++) if (secret[i]) return false;
    return true;
  }

  template <typename Details, typename Host>
  static int firstFree(Host& host, int count) {
    Details channel;
    for (int i = 0; i < count; i++) {
      if (host.getChannel(i, channel) && isEmpty(channel.channel.secret)) return i;
    }
    return -1;
  }

  template <typename Details, typename Host>
  static int duplicate(Host& host, int count, int exclude, const uint8_t secret[32]) {
    if (isEmpty(secret)) return -1;
    Details channel;
    for (int i = 0; i < count; i++) {
      if (i != exclude && host.getChannel(i, channel) &&
          std::memcmp(channel.channel.secret, secret, 32) == 0) return i;
    }
    return -1;
  }

  template <typename Details, typename Set, typename Save>
  static bool saveWithRollback(int idx, const Details& next, const Details& previous,
                               Set set, Save save) {
    if (!set(idx, next)) return false;
    if (save()) return true;
    set(idx, previous);  // The old file remains authoritative on a failed write.
    return false;
  }
};

} // namespace zen
