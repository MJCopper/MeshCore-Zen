#pragma once
#include "ClientACL.h"

// Shared stock repeater login semantics. Trace clients use the anonymous login
// protocol to introduce their full key; this helper never selects a send route.
inline ClientInfo* authenticateRepeaterClient(ClientACL& acl,const mesh::Identity& sender,
    const uint8_t* secret,uint32_t timestamp,uint32_t activity,const char* supplied,
    const char* admin_password,const char* guest_password,bool& persist) {
  persist=false;
  ClientInfo* client=supplied[0]==0?acl.getClient(sender.pub_key,PUB_KEY_SIZE):nullptr;
  if(!client) {
    uint8_t role;
    if(!strcmp(supplied,admin_password)) role=PERM_ACL_ADMIN;
    else if(!strcmp(supplied,guest_password)) role=PERM_ACL_GUEST;
    else return nullptr;
    client=acl.putClient(sender,0);
    if(!client || timestamp<=client->last_timestamp) return nullptr;
    client->last_timestamp=timestamp; client->last_activity=activity;
    client->permissions=(client->permissions & ~3)|role;
    memcpy(client->shared_secret,secret,PUB_KEY_SIZE);
    persist=role!=PERM_ACL_GUEST;
  }
  return client;
}
inline uint8_t encodeRepeaterLoginReply(uint8_t* output,const ClientInfo& client,
                                       uint32_t server_time,const uint8_t nonce[4],uint8_t version) {
  memcpy(output,&server_time,4); output[4]=0; output[5]=0;
  output[6]=client.isAdmin()?1:0; output[7]=client.permissions;
  memcpy(output+8,nonce,4); output[12]=version; return 13;
}
inline bool isFreshRepeaterRequest(const ClientInfo& client,uint32_t timestamp) {
  return timestamp>client.last_timestamp;
}
inline void commitRepeaterRequest(ClientInfo& client,uint32_t timestamp,uint32_t activity) {
  client.last_timestamp=timestamp; client.last_activity=activity;
}
