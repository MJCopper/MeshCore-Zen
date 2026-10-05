#pragma once

#include "PetMeshRewards.h"
#include <helpers/ui/ZenDisplayDriver.h>
#include <stdio.h>

namespace zen { namespace pet {

struct PetRewardsView {
  static void render(ZenDisplayDriver& d, int y, int bottom,
                     const PetMeshRewards& rewards, bool details) {
    const auto& p = rewards.progress();
    const int step = d.lineStep();
    char rows[4][32];
    if (details) {
      snprintf(rows[0],sizeof(rows[0]),"Two-way: %s",p.conversation?"Done":"Not yet");
      snprintf(rows[1],sizeof(rows[1]),"Favourite: %s",p.favourite?"Done":"Not yet");
      snprintf(rows[2],sizeof(rows[2]),"Pending %uXP %uB",p.pending_xp,p.pending_bond);
      snprintf(rows[3],sizeof(rows[3]),"%s; Bond max 5",rewards.synchronized()?"Local":"24h");
    } else {
      snprintf(rows[0],sizeof(rows[0]),"DM %u/20 XP",p.dm?20:0);
      snprintf(rows[1],sizeof(rows[1]),"Channel/Room %u/15",p.shared?15:0);
      snprintf(rows[2],sizeof(rows[2]),"Bond %u/5",p.bond);
      snprintf(rows[3],sizeof(rows[3]),"Enter: Details");
    }
    for (int i=0;i<4 && y+i*step+d.getLineHeight()<=bottom;++i)
      d.drawTextLeftAlign(0,y+i*step,rows[i]);
  }
};

} } // namespace zen::pet
