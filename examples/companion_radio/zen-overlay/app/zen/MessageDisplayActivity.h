#pragma once
#include "DisplayActivity.h"
#include "MessageModels.h"
namespace zen {
struct MessageDisplayActivity {
  static void direct(DisplayActivity& activity,unsigned slot,const DirectMessageRecord& e) {
    activity.observe({DisplayActivity::DM_FIRST+slot,e.activity_seq},
      e.outgoing && e.delivery_origin==DELIVERY_ORIGIN_ZEN && e.ack_status==DELIVERY_PENDING,
      e.ack_deadline_ms+5000);
  }
  static void channel(DisplayActivity& activity,unsigned slot,const ChannelMessageRecord& e,
                      bool transport_pending) {
    activity.observe({DisplayActivity::CHANNEL_FIRST+slot,e.relay_seq},
      e.local_send && e.relay_status==DELIVERY_PENDING && transport_pending);
  }
};
}
