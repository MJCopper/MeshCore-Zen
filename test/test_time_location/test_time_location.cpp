#define FIRMWARE_ZEN_BUILD 1

#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/TimeLocationCoordinator.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/LocalTimeService.h"

class FakeGpsSensor : public zen::GpsService {
public:
  Status runtime;
  Event events[4]{};
  uint8_t event_count = 0;
  uint8_t event_index = 0;
  uint8_t wake_count = 0;

  Status runtimeStatus() const override { return runtime; }
  bool takeEvent(Event& event) override {
    if (event_index >= event_count) return false;
    event = events[event_index++];
    return true;
  }
  void onUserDisplayWake() override { wake_count++; }
  bool getAdaptiveRetry(uint32_t&) const override { return false; }
  bool applyConfiguration(const Configuration&) override { return true; }
  bool setPowerClaim(bool, Purpose) override { return true; }
};

static zen::GpsService::Status fix(
    zen::GpsService::Purpose purpose, uint32_t session,
    bool active, int32_t lat, int32_t lon) {
  zen::GpsService::Status status;
  status.available = true;
  status.configured = true;
  status.receiver_active = active;
  status.fix_valid = active;
  status.quality_good = active;
  status.purpose = purpose;
  status.session_id = session;
  status.latitude = lat;
  status.longitude = lon;
  status.hdop = 10;
  status.course = 90000;
  status.speed = 2000;
  return status;
}

TEST(TimeLocationCoordinator, ScheduledSessionsProduceTravelCourse) {
  zen::TimeLocationCoordinator coordinator;
  coordinator.begin(1, false, true, 0);
  auto first = fix(zen::GpsService::GPS_SCHEDULED, 1, true, -33860000, 151200000);
  coordinator.tick(1, true, true, true, 300, first, true, 1000);
  first.receiver_active = false;
  first.fix_valid = false;
  coordinator.tick(1, true, false, true, 300, first, true, 7000);

  auto second = fix(zen::GpsService::GPS_SCHEDULED, 2, true, -33860000, 151200300);
  coordinator.tick(1, true, true, true, 300, second, true, 8000);
  second.receiver_active = false;
  second.fix_valid = false;
  coordinator.tick(1, true, false, true, 300, second, true, 14000);

  long course = LONG_MIN;
  EXPECT_EQ(coordinator.course(course), zen::GpsCourse::TRAVEL);
  EXPECT_NE(course, LONG_MIN);
}

TEST(TimeLocationCoordinator, TimeSyncSessionsDoNotCreateTravelCourse) {
  zen::TimeLocationCoordinator coordinator;
  coordinator.begin(1, true, false, 0);
  auto first = fix(zen::GpsService::GPS_TIME_SYNC, 1, true, -33860000, 151200000);
  coordinator.tick(1, false, true, true, 300, first, true, 1000);
  first.receiver_active = false;
  first.fix_valid = false;
  coordinator.tick(1, false, false, true, 300, first, true, 2000);
  auto second = fix(zen::GpsService::GPS_TIME_SYNC, 2, true, -33860000, 151200300);
  coordinator.tick(1, false, true, true, 300, second, true, 3000);
  second.receiver_active = false;
  second.fix_valid = false;
  coordinator.tick(1, false, false, true, 300, second, true, 4000);
  long course = LONG_MIN;
  EXPECT_EQ(coordinator.course(course), zen::GpsCourse::NONE);
}

TEST(TimeLocationCoordinator, ExplicitSyncSourceSurvivesCompletion) {
  zen::TimeLocationCoordinator coordinator;
  coordinator.begin(4, true, false, 0);
  coordinator.noteSynchronized(zen::TimeSyncSource::COMPANION, 5, 1000);
  zen::GpsService::Status gps;
  coordinator.tick(5, false, false, true, 0, gps, true, 1000);
  EXPECT_FALSE(coordinator.syncPending());
  EXPECT_EQ(coordinator.status().sync_source, zen::TimeSyncSource::COMPANION);
}

TEST(TimeLocationCoordinator, ConsumesLifecycleEventsAndKeepsTerminalResult) {
  zen::TimeLocationCoordinator coordinator;
  coordinator.begin(1, false, true, 0);
  FakeGpsSensor sensor;
  sensor.runtime = fix(zen::GpsService::GPS_SCHEDULED, 7, false,
                       -33860000, 151200000);
  sensor.runtime.fix_valid = false;
  sensor.events[0] = {zen::GpsService::GPS_EVENT_STARTED,
                      zen::GpsService::GPS_SCHEDULED, 0, 7};
  sensor.events[1] = {zen::GpsService::GPS_EVENT_TIMEOUT,
                      zen::GpsService::GPS_SCHEDULED, 0, 7};
  sensor.events[2] = {zen::GpsService::GPS_EVENT_STOPPED,
                      zen::GpsService::GPS_SCHEDULED, 0, 7};
  sensor.event_count = 3;

  coordinator.tick(1, true, false, true, 300, &sensor, true, 1000);

  EXPECT_EQ(coordinator.status().gps_event_count, 3u);
  EXPECT_EQ(coordinator.status().last_gps_event.type,
            zen::GpsService::GPS_EVENT_TIMEOUT);
  EXPECT_EQ(coordinator.status().last_gps_event.session_id, 7u);
  EXPECT_EQ(coordinator.status().gps.session_id, 7u);
}

TEST(TimeLocationCoordinator, DelegatesUserWakeToSensorPolicy) {
  zen::TimeLocationCoordinator coordinator;
  FakeGpsSensor sensor;
  coordinator.onUserWake(&sensor);
  EXPECT_EQ(sensor.wake_count, 1u);
}

TEST(TimeLocationCoordinator, InfersGpsSyncFromActiveReceiver) {
  zen::TimeLocationCoordinator coordinator;
  coordinator.begin(10, true, false, 0);
  FakeGpsSensor sensor;
  sensor.runtime = fix(zen::GpsService::GPS_TIME_SYNC, 1, true,
                       -33860000, 151200000);

  auto actions = coordinator.tick(11, false, true, true, 0, &sensor,
                                  false, 1000);

  EXPECT_FALSE(coordinator.syncPending());
  EXPECT_TRUE(actions.release_gps_time);
  EXPECT_EQ(coordinator.status().sync_source, zen::TimeSyncSource::GPS);
}

TEST(TimeLocationCoordinator, DefersHourlyGpsRetryWhenPowerPolicyBlocksIt) {
  zen::TimeLocationCoordinator coordinator;
  coordinator.begin(1, true, false, 0);
  FakeGpsSensor sensor;

  auto timeout = coordinator.tick(1, false, true, true, 0, &sensor, true,
                                  zen::BootTimeSync::GPS_TIMEOUT_MS);
  EXPECT_TRUE(timeout.release_gps_time);

  auto blocked = coordinator.tick(1, false, false, false, 0, &sensor, true,
                                  zen::BootTimeSync::RETRY_INTERVAL_MS);
  EXPECT_FALSE(blocked.request_gps_time);

  auto allowed = coordinator.tick(1, false, false, true, 0, &sensor, true,
                                  zen::BootTimeSync::RETRY_INTERVAL_MS + 1);
  EXPECT_TRUE(allowed.request_gps_time);
}

TEST(LocalTimeService, UsesSameDstAwareOffsetAndMinuteOfDay) {
  ZenPrefs prefs{};
  prefs.timezone_mode = zen::TimezonePolicy::CITY;
  prefs.timezone_city = zen::TimezonePolicy::DEFAULT_CITY;
  zen::LocalTimeService service;
  service.bind(&prefs);
  uint32_t summer = zen::TimezonePolicy::utcDate(2026, 1, 1, 0, 0);
  EXPECT_EQ(service.offsetMinutes(summer), 660);
  EXPECT_TRUE(service.daylightTime(summer));
  EXPECT_EQ(service.minuteOfDay(summer), 11u * 60u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
