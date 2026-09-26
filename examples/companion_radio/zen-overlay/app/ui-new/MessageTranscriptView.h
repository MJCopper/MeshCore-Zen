#pragma once

#include <helpers/ui/ZenDisplayDriver.h>
#include "FullscreenMsgView.h"
#include "DeliveryMarker.h"

// Presentation-neutral message data supplied by MessagesScreen's DM, room and
// channel adapters. Keeping extraction outside the view lets every history use
// exactly the same wrapping, scrolling and drawing implementation.
struct TranscriptMessage {
  char sender[33];
  char reply_to[32];
  char body[256];
  char age[6];
  uint8_t delivery;  // AckState value: 0 none, 1 pending, 2 delivered, 3 failed
  uint8_t route;     // DeliveryRoute used by the latest transmission
  uint8_t sends;
};

struct TranscriptRenderResult {
  int newest_visible;
  int oldest_visible;
  int visible_count;
  int total_lines;
};

class MessageTranscriptView {
  int _scroll_lines = 0;       // zero = newest content at the bottom
  uint8_t _pending_added = 0;  // new newest entries awaiting the next layout

  static int wrappedLines(ZenDisplayDriver& d, const char* body, int width) {
    int n = FullscreenMsgView::wrapLines(d, body, width, s_wrap_lines,
                                         MSG_WRAP_LINES_MAX);
    return n > 0 ? n : 1;
  }

public:
  void reset() { _scroll_lines = 0; _pending_added = 0; }
  void messageAdded() { if (_pending_added < 255) _pending_added++; }
  void scrollOlder() { _scroll_lines++; }
  void scrollNewer() { if (_scroll_lines > 0) _scroll_lines--; }
  bool atNewest() const { return _scroll_lines == 0; }

  template <class Provider>
  TranscriptRenderResult render(ZenDisplayDriver& d, int count, int top_y,
                                Provider provide) {
    TranscriptRenderResult result{-1, -1, 0, 0};
    const int lh = d.getLineHeight();
    int view_h = d.height() - top_y;
    int visible_lines = view_h / lh;
    if (visible_lines < 1) visible_lines = 1;
    const int view_top = d.height() - visible_lines * lh;

    auto totalLines = [&](int reserve) {
      int total = 0;
      TranscriptMessage msg;
      for (int i = 0; i < count; i++) {
        if (!provide(i, msg)) continue;
        total += 1 + (msg.reply_to[0] ? 1 : 0) +
                 wrappedLines(d, msg.body, d.width() - 6 - reserve);
      }
      return total;
    };

    int total = totalLines(0);
    int reserve = total > visible_lines ? scrollIndicatorColWidth(d) : 0;
    if (reserve) total = totalLines(reserve);
    result.total_lines = total;

    // Preserve the same content when new entries arrive while the user is
    // reading older history. This uses the actual wrapped height of each new
    // entry and still works when the fixed-size history ring simultaneously
    // evicts an old entry (where total height may not grow at all).
    if (_scroll_lines > 0 && _pending_added > 0) {
      TranscriptMessage added;
      int n = _pending_added < count ? _pending_added : count;
      for (int i = 0; i < n; i++)
        if (provide(i, added))
          _scroll_lines += 1 + (added.reply_to[0] ? 1 : 0) +
                           wrappedLines(d, added.body, d.width() - 6 - reserve);
    }
    _pending_added = 0;
    int max_scroll = total > visible_lines ? total - visible_lines : 0;
    if (_scroll_lines > max_scroll) _scroll_lines = max_scroll;

    int item_bottom = d.height() + _scroll_lines * lh;
    TranscriptMessage msg;
    for (int item = 0; item < count && item_bottom > view_top; item++) {
      if (!provide(item, msg)) continue;
      int body_lines = FullscreenMsgView::wrapLines(
          d, msg.body, d.width() - 6 - reserve, s_wrap_lines, MSG_WRAP_LINES_MAX);
      if (body_lines < 1) { s_wrap_lines[0][0] = '\0'; body_lines = 1; }
      int reply_lines = msg.reply_to[0] ? 1 : 0;
      int rows = 1 + reply_lines + body_lines;
      int item_top = item_bottom - rows * lh;
      bool visible = false;

      if (item_top >= view_top && item_top + lh <= d.height()) {
        int age_w = msg.age[0] ? d.getTextWidth(msg.age) + 3 : 0;
        int aw = deliveryMarkerWidth(d, msg.delivery, msg.route, msg.sends);
        int name_w = d.width() - reserve - 3 - age_w - (aw ? aw + 3 : 0);
        if (name_w < 4) name_w = 4;
        int badge_w = d.getTextWidth(msg.sender) + 4;
        if (badge_w > name_w) badge_w = name_w;
        d.setColor(ZenDisplayDriver::LIGHT);
        d.fillRect(1, item_top, badge_w, lh);
        d.setColor(ZenDisplayDriver::DARK);
        d.drawTextEllipsized(3, item_top + 1, badge_w - 4, msg.sender);
        d.setColor(ZenDisplayDriver::LIGHT);
        if (aw) drawDeliveryMarker(d, 1 + badge_w + 3, item_top + 1,
                                   msg.delivery, msg.route, msg.sends);
        if (msg.age[0]) {
          d.setCursor(d.width() - reserve - age_w, item_top + 1);
          d.print(msg.age);
        }
        visible = true;
      }

      if (reply_lines) {
        int y = item_top + lh;
        if (y >= view_top && y + lh <= d.height()) {
          char to_line[38];
          snprintf(to_line, sizeof(to_line), "To: %s", msg.reply_to);
          d.setColor(ZenDisplayDriver::LIGHT);
          d.drawTextEllipsized(2, y, d.width() - 4 - reserve, to_line);
          visible = true;
        }
      }

      for (int line = 0; line < body_lines; line++) {
        int y = item_top + (line + 1 + reply_lines) * lh;
        if (y < view_top || y + lh > d.height()) continue;
        d.setColor(ZenDisplayDriver::LIGHT);
        d.setCursor(2, y);
        d.print(s_wrap_lines[line]);
        visible = true;
      }
      int rule_y = item_bottom - 1;
      if (rule_y >= view_top && rule_y < d.height()) {
        d.setColor(ZenDisplayDriver::LIGHT);
        d.fillRect(0, rule_y, d.width() - reserve, 1);
      }

      if (visible) {
        if (result.newest_visible < 0) result.newest_visible = item;
        result.oldest_visible = item;
        result.visible_count++;
      }
      item_bottom = item_top;
    }

    if (reserve) {
      long span = (long)(total - visible_lines) * lh;
      long from_top = span - (long)_scroll_lines * lh;
      if (from_top < 0) from_top = 0;
      drawScrollIndicatorPx(d, view_top, visible_lines * lh,
                            total * lh, visible_lines * lh, from_top);
    }
    return result;
  }
};
