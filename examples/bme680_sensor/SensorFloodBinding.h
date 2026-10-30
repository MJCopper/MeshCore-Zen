#pragma once
#include "SensorScopeBinding.h"
#include <helpers/FloodTextDoubler.h>
#include <helpers/DoublerContext.h>
#include <helpers/DoublerFileStorage.h>

// Sensor-local policy; shared Dispatcher defaults remain inactive in other roles.
template<class Runtime>
class SensorFloodBinding : public SensorScopeBinding<Runtime> {
  DoublerSettings settings;
  DoublerContext context;
  FILESYSTEM* filesystem=nullptr;
  mesh::Packet* forwarded[32]{};
  bool tracking_failed=false;
  struct Owner { mesh::Packet* packet=nullptr; uint32_t epoch=0,deadline=0; bool cancelled=false; } owners[32];
  Owner* owner(const mesh::Packet* p) {
    for(auto& o:owners) if(o.packet==p) return &o;
    return nullptr;
  }
  bool forgetForwarded(mesh::Packet* p) {
    for(auto& f:forwarded) if(f==p) { f=nullptr; return true; }
    return false;
  }
  bool ownerAllows(const mesh::Packet* p,uint32_t now) {
    auto o=owner(p); return !o || (!o->cancelled && (int32_t)(o->deadline-now)>0);
  }
  void syncContext() {
    auto scope=this->captureDefaultScope();
    context.packetContext(floods,this->self_id.pub_key,floodHashMode(),scope.key);
  }
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  using Storage=DoublerFileStorage<FILESYSTEM>;
  Storage storage() { return Storage(*filesystem,FILE_O_WRITE); }
#else
  using Storage=DoublerFileStorage<FILESYSTEM,const char*>;
  Storage storage() { return Storage(*filesystem,"w"); }
#endif
protected:
  virtual uint8_t floodHashMode() { return 0; }
  virtual void onFloodTxStarted(mesh::Packet*,uint32_t,bool) { }
  virtual void onFloodTxComplete(mesh::Packet*,int,bool) { }
  virtual void onFloodTxFailed(mesh::Packet*,int,bool) { }
  void onRadioApplied(float freq,float bw,uint8_t sf,uint8_t cr) override {
    context.radioApplied(floods,freq,bw,sf,cr);
  }
  void onPacketContextChanged() override { syncContext(); }
  void onTxStarted(mesh::Packet* p,uint32_t now) override {
    syncContext(); bool repeat=floods.isRepeat(p); floods.onTransmitStarted(p);
    if(repeat) ++floodStats.transmitted;
    onFloodTxStarted(p,now,repeat);
  }
  void logTx(mesh::Packet* p,int length) override { onFloodTxComplete(p,length,floods.isRepeat(p)); }
  void logTxFail(mesh::Packet* p,int length) override {
    bool repeat=floods.isRepeat(p); if(repeat) ++floodStats.failed;
    onFloodTxFailed(p,length,repeat);
  }
  bool retainAfterTransmit(mesh::Packet* p,uint32_t now) override {
    syncContext(); bool candidate=floods.eligible(p),repeat=floods.isRepeat(p);
    if(forgetForwarded(p) || tracking_failed || !ownerAllows(p,now)) {
      if(candidate && !repeat) ++floodStats.skipped;
      return false;
    }
    bool retain=floods.onTransmitComplete(p,*this->_mgr,*this->getRNG(),now);
    if(retain) ++floodStats.retained;
    else if(candidate && !repeat) ++floodStats.skipped;
    return retain;
  }
  void onPacketReleased(mesh::Packet* p) override {
    forgetForwarded(p); floods.forget(p);
    if(auto o=owner(p)) *o=Owner{};
  }
  mesh::DispatcherAction onRecvPacket(mesh::Packet* p) override {
    auto action=SensorScopeBinding<Runtime>::onRecvPacket(p);
    if(action!=ACTION_RELEASE && action!=ACTION_MANUAL_HOLD) {
      bool recorded=false;
      for(auto f:forwarded) if(f==p) recorded=true;
      if(!recorded) {
        for(auto& f:forwarded) if(!f) { f=p; recorded=true; break; }
        if(!recorded) tracking_failed=true;
      }
    }
    return action;
  }
  void serviceOptionalPackets(uint32_t now,const mesh::Packet* active) override {
    syncContext();
    for(auto& o:owners) if(o.packet && !ownerAllows(o.packet,now)) floods.cancel(o.packet);
    bool cancelled=false;
    while(auto p=floods.takeExpired(now,active,&cancelled)) {
      if(cancelled) ++floodStats.cancelled; else ++floodStats.expired;
      Runtime::releasePacket(p);
    }
  }
  mesh::Packet* getOptionalPacket(uint32_t now) override { syncContext(); return floods.nextReady(now); }
  void deferOptionalPackets(uint32_t now) override { floods.deferReady(now); }
  bool allowPacketStart(mesh::Packet* p,uint32_t now) override {
    syncContext(); if(!floods.isRepeat(p)) return true;
    if(this->_radio->isReceiving()) { ++floodStats.skipped; return false; }
    auto o=owner(p); uint64_t watchdog=0;
    if(o) watchdog=uint64_t(this->_radio->getEstAirtimeFor(p->getRawLength()))*3/2+1000;
    now=(uint32_t)this->_ms->getMillis();
    if(!ownerAllows(p,now) || (o && watchdog>uint32_t(o->deadline-now)) || !floods.canStart(p,now)) {
      ++floodStats.skipped; return false;
    }
    return true;
  }
public:
  using SensorScopeBinding<Runtime>::SensorScopeBinding;
  FloodTextDoubler floods{true};
  struct Counters { uint32_t retained=0,transmitted=0,expired=0,cancelled=0,skipped=0,failed=0; } floodStats;
  DoublerSettings::Status beginFlood(FILESYSTEM* fs) {
    filesystem=fs;
    if(!fs) return DoublerSettings::STORAGE_ERROR;
    auto adapter=storage(); auto status=settings.load(adapter);
    floods.setMode(settings.getMode()); syncContext(); return status;
  }
  bool floodCommand(const char* command,char* reply) {
    if(!strcmp(command,"get multi.flood.stats")) {
      snprintf(reply,160,"> retained=%lu tx=%lu expired=%lu cancelled=%lu skipped=%lu failed=%lu pending=%u",
        (unsigned long)floodStats.retained,(unsigned long)floodStats.transmitted,(unsigned long)floodStats.expired,
        (unsigned long)floodStats.cancelled,(unsigned long)floodStats.skipped,(unsigned long)floodStats.failed,floods.pendingCount());
      return true;
    }
    if(!filesystem) return false;
    auto adapter=storage();
    if(!settings.handleCommand(command,reply,adapter)) return false;
    floods.setMode(settings.getMode()); return true;
  }
  void tagTracePacket(mesh::Packet* p,uint32_t epoch,uint32_t deadline) override {
    if(!p) return;
    auto o=owner(p);
    if(!o) for(auto& available:owners) if(!available.packet) { o=&available; break; }
    if(o) { o->packet=p; o->epoch=epoch; o->deadline=deadline; o->cancelled=false; }
    else { tracking_failed=true; ++floodStats.skipped; }
  }
  void cancelTraceCopies(uint32_t epoch) override {
    for(auto& o:owners) if(o.packet && o.epoch==epoch) { o.cancelled=true; floods.cancel(o.packet); }
  }
  bool hasOptionalFloodWork() const { return floods.pendingCount()!=0; }
};
