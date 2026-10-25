#pragma once
#include "SensorBotService.h"
#include <helpers/TransportKeyStore.h>

// The production scope adapter is also instantiated against the native runtime.
// Runtime owns the region map and exposes only value capture and explicit send.
template<class Runtime>
class SensorScopeBinding : public Runtime, public SensorBotTransport {
  SensorBotService* _discovery_service = NULL;
  int _base_peers = 0;
  uint8_t _peer_slots[3]{}, _extra_peers = 0;
protected:
  int searchPeersByHash(const uint8_t* hash) override {
    _base_peers = Runtime::searchPeersByHash(hash); _extra_peers = 0;
    if (_discovery_service) for (uint8_t slot = 0; slot < 3; slot++) {
      auto key = _discovery_service->discoveryPeer(slot);
      if (key && mesh::Identity(key).isHashMatch(hash)) _peer_slots[_extra_peers++] = slot;
    }
    // The baseline's bounded ACL search plus at most three transient peers.
    return _base_peers + _extra_peers;
  }
  void getPeerSharedSecret(uint8_t* secret, int index) override {
    if (index < _base_peers) { Runtime::getPeerSharedSecret(secret, index); return; }
    this->self_id.calcSharedSecret(secret, _discovery_service->discoveryPeer(_peer_slots[index - _base_peers]));
  }
  void onPeerDataRecv(mesh::Packet* packet, uint8_t type, int index, const uint8_t* secret,
                       uint8_t* data, size_t length) override {
    if (type == PAYLOAD_TYPE_RESPONSE && _discovery_service) {
      for (uint8_t slot=0;slot<3;slot++) {
        auto key=_discovery_service->discoveryPeer(slot); if(!key) continue;
        uint8_t expected[32]; this->self_id.calcSharedSecret(expected,key);
        if(!memcmp(expected,secret,32) && _discovery_service->onLoginResponse(key,data,length,packet)) return;
      }
    }
    if (index < _base_peers) Runtime::onPeerDataRecv(packet, type, index, secret, data, length);
  }
  bool onPeerPathRecv(mesh::Packet* packet, int index, const uint8_t* secret, uint8_t* path,
                      uint8_t encoded, uint8_t extra_type, uint8_t* extra, uint8_t length) override {
    if (extra_type == PAYLOAD_TYPE_RESPONSE && _discovery_service) {
      // The sensor has no other outbound RESPONSE/PATH request owner. Unowned
      // replies must never install an ACL route, even after peer-ring eviction.
      for (uint8_t slot = 0; slot < 3; slot++) {
        auto key = _discovery_service->discoveryPeer(slot);
        if (!key) continue;
        uint8_t expected[32]; this->self_id.calcSharedSecret(expected, key);
        if (memcmp(expected, secret, sizeof(expected)) == 0 &&
            (_discovery_service->onLoginResponse(key,extra,length,packet,path,encoded) ||
             _discovery_service->onDiscoveryPath(key, packet, path, encoded, extra, length))) break;
      }
      return false;  // no reciprocal PATH and no saved management-route mutation
    }
    return index < _base_peers && Runtime::onPeerPathRecv(packet, index, secret, path, encoded,
                                                          extra_type, extra, length);
  }
public:
  using Runtime::Runtime;
  void bindDiscovery(SensorBotService* service) override { _discovery_service = service; }
  mesh::Packet* queueDiscovery(const uint8_t* key, uint32_t tag,
                               const SensorReplyContext& context) override {
    uint8_t data[13]{}; memcpy(data, &tag, 4);
    data[4] = 3; data[5] = (uint8_t)~1;  // base telemetry, MeshCore path discovery
    this->rng().random(data + 9, 4);
    uint8_t secret[32]; this->self_id.calcSharedSecret(secret, key);
    auto packet = this->createDatagram(PAYLOAD_TYPE_REQ, mesh::Identity(key), secret, data, sizeof(data));
    if (!packet) return NULL;
    sendReply(packet, 0, context);
    return this->isQueuedPacket(packet) ? packet : NULL;
  }
  const uint8_t* identityKey() const override { return this->self_id.pub_key; }
  mesh::Packet* queueGuestLogin(const uint8_t* key,uint32_t tag,const SensorReplyContext& context) override {
    uint8_t data[4]; memcpy(data,&tag,4);
    uint8_t secret[32]; this->self_id.calcSharedSecret(secret,key);
    auto packet=this->createAnonDatagram(PAYLOAD_TYPE_ANON_REQ,this->self_id,mesh::Identity(key),secret,data,sizeof(data));
    if(!packet) return nullptr;
    sendReply(packet,0,context);
    return this->isQueuedPacket(packet)?packet:nullptr;
  }
  mesh::PacketScope requestScope(const mesh::Packet& packet) override {
    return this->captureRequestScope(packet);
  }
  SensorReplyContext replyContext(const mesh::Packet& packet, uint8_t channel,
                                  const mesh::PacketScope& scope) override {
    SensorReplyContext context(channel);
    auto key = this->captureReplyScope(packet, scope);
    memcpy(context.scope_key, key.key, sizeof(key.key));
    context.hash_size = packet.getPathHashSize();
    return context;
  }
  void sendReply(mesh::Packet* packet, uint32_t delay, const SensorReplyContext& context) override {
    TransportKey scope{};
    memcpy(scope.key, context.scope_key, sizeof(scope.key));
    this->sendCapturedReply(scope, packet, delay, context.hash_size);
  }
};
