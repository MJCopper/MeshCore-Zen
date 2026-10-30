#pragma once

#include <Mesh.h>
#include <helpers/TxtDataHelpers.h>
#include "Bme680Telemetry.h"
#include "PublicChannelSensorBot.h"
#include "PublicResponseQueue.h"
#include "RepeaterNameCache.h"
#include "RepeaterTraceProbe.h"
#include "TraceReportDelivery.h"
#include "TraceRouteHistory.h"
#include "TraceDiagnostics.h"
#include "TraceDiscovery.h"
#include "TraceRequestClock.h"
#include "TraceGuestPolicy.h"
#include "TraceAdvertRouteCache.h"
#include "TraceRequestAcknowledgement.h"
#include "TraceTransactionBudget.h"

class SensorBotService;

// Radio ownership stays in the MeshCore adapter; service tests supply this interface.
class SensorBotTransport {
public:
  virtual void tagTracePacket(mesh::Packet*,uint32_t,uint32_t) { }
  virtual void cancelTraceCopies(uint32_t) { }
  virtual void bindDiscovery(SensorBotService* service) = 0;
  virtual mesh::Packet* queueDiscovery(const uint8_t* key, uint32_t tag, const SensorReplyContext& context) = 0;
  virtual mesh::Packet* queueGuestLogin(const uint8_t* key, uint32_t tag, const SensorReplyContext& context) = 0;
  virtual const uint8_t* identityKey() const = 0;
  virtual uint32_t wallTime() { return 0; }
  virtual bool trustedWallTime() { return false; }
  virtual uint32_t stableRadioProfile() { return radioProfile(); }
  virtual bool stableRadioConfiguration() { return true; }
  virtual SensorReplyContext controlContext(const SensorReplyContext& request) { return request; }
  virtual uint32_t now() = 0;
  virtual uint32_t uniqueTime() = 0;
  virtual const char* nodeName() = 0;
  virtual mesh::RNG& rng() = 0;
  virtual uint32_t estimateAirtime(uint8_t length) = 0;
  virtual void describeTraceRadio(char* dest,size_t size) { snprintf(dest,size,"PHY unavailable"); }
  virtual uint32_t radioProfile() { return 0; }
  // RadioLib's Boolean error cannot rule out an emitted frame after a failed SPI command.
  virtual bool failedStartDefinitelyNotSent() const { return false; }
  virtual mesh::Packet* createTrace(uint32_t tag, uint32_t auth, uint8_t flags) = 0;
  virtual mesh::Packet* createGroupDatagram(uint8_t type, const mesh::GroupChannel& channel,
                                           const uint8_t* data, size_t length) = 0;
  virtual mesh::PacketScope requestScope(const mesh::Packet& packet) = 0;
  virtual SensorReplyContext replyContext(const mesh::Packet& packet, uint8_t channel,
                                          const mesh::PacketScope& scope) = 0;
  virtual void sendReply(mesh::Packet* packet, uint32_t delay, const SensorReplyContext& context) = 0;
  virtual void sendDirect(mesh::Packet* packet, const uint8_t* path, uint8_t length,
                          uint32_t delay = 0) = 0;
  virtual void releasePacket(mesh::Packet* packet) = 0;
  virtual bool isQueuedPacket(const mesh::Packet* packet) const = 0;
  virtual bool cancelQueuedPacket(mesh::Packet* packet) = 0;
};

class SensorBotService {
public:
  // Settling can be suppressed by exact-boundary characterization tests only.
  SensorBotService(SensorBotTransport& transport, Bme680Telemetry& telemetry, bool settle = true)
      : _transport(transport), _telemetry(telemetry), _settle_enabled(settle) { _transport.bindDiscovery(this); }
  bool tracePolicyCommand(const char* command, char* reply);
  bool onLoginResponse(const uint8_t* key, const uint8_t* data, size_t length, const mesh::Packet* packet = nullptr,
                       const uint8_t* path = nullptr, uint8_t encoded = 0);
  const TraceTransactionBudget& timing() const { return _budget; }
  uint32_t traceDeadline() const { return _deadline; }
  const TraceDiagnostics& diagnostics() const { return trace_diagnostics; }

  void begin(FILESYSTEM* fs);

  // Poll the runtime radio between beforeRadioLoop() and loop(); report recovery
  // must allow Dispatcher to deliver completion/watchdog callbacks first.
  void loop();
  void beforeRadioLoop();

  void onTxStarted(mesh::Packet* packet, uint32_t now_millis);
  void onOptionalTxStarted(uint32_t now) { sampleQueues(now); _radio_tx_active=true; sampleQueues(now); }
  void onOptionalTxFinished(uint32_t now) { _radio_tx_active=false; sampleQueues(now); }

  void logTx(mesh::Packet* packet, int);

  void logTxFail(mesh::Packet* packet, int);
  void observeRoute(const mesh::Packet* packet);

  void sendPublicResponse(const char* response, const SensorReplyContext& context, uint32_t extra_delay_millis = 0,
                          const char* separator = NULL);

  void onAdvertRecv(mesh::Packet*, const mesh::Identity& id, uint32_t timestamp,
                    const uint8_t* app_data, size_t app_data_len);

  void onTraceRecv(mesh::Packet* packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                   const uint8_t* path_snrs, const uint8_t* path_hashes, uint8_t path_len);

  const uint8_t* discoveryPeer(uint8_t index) const { return discovery.peer(index); }
  bool onDiscoveryPath(const uint8_t* key, mesh::Packet* packet, const uint8_t* path,
                       uint8_t encoded, const uint8_t* extra, uint8_t length);

  int searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches);

  void onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel&,
                       uint8_t* data, size_t len);

private:
  SensorBotTransport& _transport;
  Bme680Telemetry& _telemetry;
  PublicChannelSensorBot public_bot;
  PublicResponseQueue public_replies;
  RepeaterNameCache repeater_names;
  RepeaterTraceProbe trace_probe;
  TraceRouteCandidates trace_routes;
  TraceReportDelivery trace_report;
  TraceRouteHistory trace_history;
  TraceDiagnostics trace_diagnostics;
  TraceDiscovery discovery;
  TraceRequestClock request_clock;
  TraceGuestPolicy guest_policy;
  TraceAdvertRouteCache advert_routes;
  SensorReplyContext control_context;
  const mesh::Packet* _control_packets[5]{};
  bool _advert_route = false, _attempt_advert[2]{};
  bool useAdvertRoute(uint32_t now);
  TraceRequestAcknowledgement trace_ack;
  TraceTransactionBudget _budget;
  uint32_t _airtime[MAX_TRANS_UNIT+1]{},_air_profile=0;
  char _trace_radio[96]{};
  bool _radio_tx_active=false;
  uint32_t _accepted_at=0,_queue_sample=0,_work_queue_used=0,_report_queue_used=0;
  bool _count_work_queue=false,_count_report_queue=false;
  TraceRoute _login_route{};
  bool _login_route_pending=false;
  bool cancelQueued(mesh::Packet* packet);
  void refreshAirtime();
  uint32_t cachedAirtime(uint8_t length) const { return _airtime[length]; }
  uint32_t workRemaining(uint32_t now) const { return _budget.work(now,_deadline); }
  void sampleQueues(uint32_t now);
  uint32_t _trace_epoch=0;
  uint32_t _deadline = 0;
  bool _transaction = false, _prelude = false;
  bool _targeted = false;
  uint32_t _profile = 0, _settle_at = 0, _settle_delay = 0, _quiet = 0, _next_queue_at = 0;
  uint32_t _trace_profile = 0, _probe_not_before = 0;
  uint8_t _local_failures = 0;
  bool _settling = false, _settle_enabled;
  uint32_t next_trace_tag = 0;
  bool busy_trace_reported = false;
  SensorReplyContext trace_context;
  mesh::PacketScope trace_scope;

  bool traceBusy() const { return _prelude || _login_route_pending || discovery.pending() || trace_probe.isActive() || trace_report.active(); }
  bool beginAcknowledgement(uint32_t now, const char* destination = nullptr, const char* fallback = nullptr, const uint8_t* target_key = nullptr);
  void endTraceWork(uint32_t now);
  void pumpAcknowledgement(uint32_t now);
  bool expireTransaction(uint32_t now);
  bool allowPhase(uint32_t now, uint64_t needed);
  const uint8_t* historyTarget() const { return _targeted ? discovery.key() : NULL; }
  void formatTraceStatus(char* response, size_t size);
  void pumpDiscovery(uint32_t now);
  void discoveryFailure(const char* text);
  void startDiscovered(const TraceRoute& route, uint32_t now);

  enum QueueTraceResult { TRACE_QUEUED, TRACE_NO_PACKET, TRACE_QUEUE_REJECTED };
  QueueTraceResult queueTrace(uint32_t delay_millis = 0);
  mesh::Packet* createPublicPart(const char* response, const SensorReplyContext& context, bool prefix = true);
  bool sendPublicPart(const char* response, const SensorReplyContext& context, uint32_t delay_millis);
  void sendTraceResult(const RepeaterTraceProbe::Result& result);
  void finishTrace(const RepeaterTraceProbe::Result& result);
  void closeTrace(bool preserve_deadline = false);
  void queueTraceFailure();
  void traceIdentifiers(uint32_t& tag, uint32_t& auth);
  void admitTrace(uint32_t now, uint32_t delay = 0);
  void pumpTraceReport(uint32_t now, bool queue_early = false);
};

