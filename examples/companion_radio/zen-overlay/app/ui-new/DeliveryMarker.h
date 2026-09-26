#pragma once

#include <helpers/ui/ZenDisplayDriver.h>
#include "icons.h"

inline char deliveryRouteLabel(uint8_t route) {
  if (route == DELIVERY_ROUTE_DIRECT) return 'D';
  if (route == DELIVERY_ROUTE_PATH) return 'P';
  if (route == DELIVERY_ROUTE_FLOOD) return 'F';
  return '\0';
}

inline int deliveryMarkerWidth(ZenDisplayDriver& d, uint8_t state, uint8_t route,
                               int transmission) {
  if (route == DELIVERY_ROUTE_RELAY) {
    if (state == ACK_NONE) return 0;
    if (state == ACK_FAIL) return ICON_CROSS.w * miniIconScale(d);
    char count[5]; snprintf(count, sizeof(count), "%d", transmission);
    return d.getTextWidth(count) +
           (state == ACK_OK ? ICON_CHECK.w * miniIconScale(d) : 0);
  }
  char label = deliveryRouteLabel(route);
  if (state == ACK_NONE || !label) return 0;
  if (state == ACK_FAIL) return ICON_CROSS.w * miniIconScale(d);
  char buf[8];
  if (state == ACK_PENDING) snprintf(buf, sizeof(buf), "%c%d", label, transmission);
  else snprintf(buf, sizeof(buf), "%c", label);
  int width = d.getTextWidth(buf);
  if (state == ACK_OK) width += ICON_CHECK.w * miniIconScale(d);
  return width;
}

inline void drawDeliveryMarker(ZenDisplayDriver& d, int x, int y, uint8_t state,
                               uint8_t route, int transmission) {
  if (route == DELIVERY_ROUTE_RELAY) {
    if (state == ACK_NONE) return;
    if (state == ACK_FAIL) {
      miniIconDraw(d, x, y, ICON_CROSS);
      return;
    }
    char count[5]; snprintf(count, sizeof(count), "%d", transmission);
    d.setCursor(x, y);
    d.print(count);
    if (state == ACK_OK)
      miniIconDraw(d, x + d.getTextWidth(count), y, ICON_CHECK);
    return;
  }
  char label = deliveryRouteLabel(route);
  if (state == ACK_NONE || !label) return;
  if (state == ACK_FAIL) {
    miniIconDraw(d, x, y, ICON_CROSS);
    return;
  }
  char buf[8];
  if (state == ACK_PENDING) snprintf(buf, sizeof(buf), "%c%d", label, transmission);
  else snprintf(buf, sizeof(buf), "%c", label);
  d.setCursor(x, y);
  d.print(buf);
  if (state == ACK_OK)
    miniIconDraw(d, x + d.getTextWidth(buf), y, ICON_CHECK);
}
