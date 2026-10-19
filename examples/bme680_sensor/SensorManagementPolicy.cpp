#include "SensorManagementPolicy.h"
#include <stdio.h>

ClientInfo* SensorManagementPolicy::login(ClientACL& clients, const mesh::Identity& sender,
                                         const uint8_t* secret, uint32_t timestamp, uint32_t now,
                                         const char* password, const char* admin_password,
                                         bool& persist) {
  persist = false;
  bool admin = password[0] && strcmp(password, admin_password) == 0;
  bool known = clients.getClient(sender.pub_key, PUB_KEY_SIZE) != NULL;
  ClientInfo* client = clients.putClientPreservingAdmins(sender, 0);
  if (!client || timestamp <= client->last_timestamp) return NULL;
  client->last_timestamp = timestamp;
  client->last_activity = now;
  if (admin) {
    client->permissions = (client->permissions & ~PERM_ACL_ROLE_MASK) | PERM_ACL_ADMIN;
    client->permissions |= 0xC0;  // Standard low/high-priority alert permissions.
  } else if (password[0] || !known) {
    client->permissions = PERM_ACL_READ_ONLY;
  }
  memcpy(client->shared_secret, secret, PUB_KEY_SIZE);
  persist = admin;
  return client;
}

int SensorManagementPolicy::ownerInfo(const char* version, const char* name, const char* owner,
                                     uint8_t* reply, size_t capacity) {
  if (!reply || !capacity) return 0;
  int length = snprintf((char*)reply, capacity, "%s\n%s\n%s", version, name, owner);
  if (length < 0) return 0;
  return length < (int)capacity ? length : capacity - 1;
}
