#include <gtest/gtest.h>

#include "../../examples/companion_radio/zen-overlay/app/zen/NotificationPolicy.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/NotificationCoordinator.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/NotificationProfiles.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/NotificationEligibility.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/NotificationPopupState.h"

using zen::NotificationPolicy;
using ScreenWake = NotificationPolicy::ScreenWake;

TEST(NotificationPolicy, ModeFollowsExistingSavedBuzzerChoices) {
  EXPECT_EQ(NotificationPolicy::ON, NotificationPolicy::mode(false, false));
  EXPECT_EQ(NotificationPolicy::OFF, NotificationPolicy::mode(true, false));
  EXPECT_EQ(NotificationPolicy::AUTO, NotificationPolicy::mode(false, true));
}

TEST(NotificationPolicy, OnWakesEvenWithClientConnected) {
  auto result = NotificationPolicy::decide(
      true, false, true, NotificationPolicy::ON, true, ScreenWake::ON, true);
  EXPECT_TRUE(result.record_unread);
  EXPECT_TRUE(result.show_visual);
  EXPECT_TRUE(result.wake_screen);
  EXPECT_TRUE(result.play_sound);
}

TEST(NotificationPolicy, OffAndConnectedAutoKeepUnreadAndVisiblePopup) {
  for (auto mode : { NotificationPolicy::OFF, NotificationPolicy::AUTO }) {
    auto result = NotificationPolicy::decide(
        true, false, true, mode, true, ScreenWake::ON, true);
    EXPECT_TRUE(result.record_unread);
    EXPECT_TRUE(result.show_visual);
    EXPECT_FALSE(result.wake_screen);
    EXPECT_FALSE(result.play_sound);
    EXPECT_FALSE(result.vibrate);
  }
}

TEST(NotificationPolicy, ScreenWakeAndQuietTimeAreIndependent) {
  auto no_wake = NotificationPolicy::decide(
      true, false, true, NotificationPolicy::ON, false, ScreenWake::OFF, true);
  EXPECT_TRUE(no_wake.show_visual);
  EXPECT_FALSE(no_wake.wake_screen);
  EXPECT_TRUE(no_wake.play_sound);

  auto quiet = NotificationPolicy::decide(
      true, true, true, NotificationPolicy::AUTO, false, ScreenWake::ON, true);
  EXPECT_TRUE(quiet.wake_screen);
  EXPECT_FALSE(quiet.play_sound);
  EXPECT_FALSE(quiet.vibrate);
}

TEST(NotificationCoordinator, ChildEligibilityRejectsEveryOutputAndUnread) {
  zen::NotificationEvent event;
  event.type = zen::NotificationType::DIRECT_MESSAGE;
  event.eligible = false;
  event.record_unread = true;
  event.visual = true;
  zen::NotificationContext context;
  context.screen_wake = ScreenWake::ALWAYS;
  auto result = zen::NotificationCoordinator::decide(event, context);
  EXPECT_FALSE(result.record_unread);
  EXPECT_FALSE(result.show_visual);
  EXPECT_FALSE(result.wake_screen);
  EXPECT_FALSE(result.play_sound);
  EXPECT_FALSE(result.vibrate);
}

TEST(NotificationCoordinator, UrgentWarningUsesWakeSettingButRemainsSilent) {
  zen::NotificationEvent event;
  event.type = zen::NotificationType::WARNING;
  event.visual = true;
  event.audible = false;
  event.quiet_affected = false;
  event.urgent_wake = true;
  zen::NotificationContext context;
  context.mode = NotificationPolicy::OFF;
  context.screen_wake = ScreenWake::ON;
  auto result = zen::NotificationCoordinator::decide(event, context);
  EXPECT_TRUE(result.show_visual);
  EXPECT_TRUE(result.wake_screen);
  EXPECT_FALSE(result.play_sound);
}

TEST(NotificationCoordinator, EmergencyDoesNotOverrideSilence) {
  zen::NotificationEvent event;
  event.type = zen::NotificationType::DIRECT_MESSAGE;
  event.record_unread = true;
  event.visual = true;
  zen::NotificationContext context;
  context.emergency = true;
  context.silent = true;
  auto result = zen::NotificationCoordinator::decide(event, context);
  EXPECT_TRUE(result.record_unread);
  EXPECT_TRUE(result.show_visual);
  EXPECT_FALSE(result.play_sound);
  EXPECT_FALSE(result.vibrate);
}

TEST(NotificationCoordinator, LowPowerDoesNotRewriteNotificationPreferences) {
  auto event = zen::NotificationProfiles::event(
      zen::NotificationType::DIRECT_MESSAGE);
  zen::NotificationContext context;
  context.low_power = true;
  context.mode = NotificationPolicy::OFF;
  context.screen_wake = ScreenWake::ALWAYS;
  auto result = zen::NotificationCoordinator::decide(event, context);
  EXPECT_TRUE(result.record_unread);
  EXPECT_TRUE(result.show_visual);
  EXPECT_TRUE(result.wake_screen);
  EXPECT_FALSE(result.play_sound);
}

TEST(NotificationCoordinator, PopupPriorityProtectsWarningsAndErrors) {
  EXPECT_LT(zen::NotificationCoordinator::popupPriority(
                zen::NotificationType::DIRECT_MESSAGE),
            zen::NotificationCoordinator::popupPriority(
                zen::NotificationType::WARNING));
  EXPECT_LT(zen::NotificationCoordinator::popupPriority(
                zen::NotificationType::WARNING),
            zen::NotificationCoordinator::popupPriority(
                zen::NotificationType::ERROR));
}

TEST(NotificationProfiles, DefinesAllOutputsByEventClass) {
  auto message = zen::NotificationProfiles::event(
      zen::NotificationType::DIRECT_MESSAGE);
  EXPECT_TRUE(message.record_unread);
  EXPECT_TRUE(message.visual);
  EXPECT_TRUE(message.audible);

  auto advert = zen::NotificationProfiles::event(
      zen::NotificationType::ADVERT_LOCAL);
  EXPECT_FALSE(advert.record_unread);
  EXPECT_FALSE(advert.visual);
  EXPECT_TRUE(advert.audible);

  auto warning = zen::NotificationProfiles::event(
      zen::NotificationType::WARNING);
  EXPECT_TRUE(warning.visual);
  EXPECT_FALSE(warning.audible);
  EXPECT_TRUE(warning.urgent_wake);
}

TEST(NotificationEligibility, ChildModeSuppressesAdvertPresentation) {
  EXPECT_TRUE(zen::NotificationEligibility::advert(false));
  EXPECT_FALSE(zen::NotificationEligibility::advert(true));
}

TEST(NotificationPopupState, ProtectsHigherPriorityUntilExpiry) {
  zen::NotificationPopupState popup;
  EXPECT_TRUE(popup.accept(3, 100, 0));
  EXPECT_FALSE(popup.accept(1, 200, 1000));
  EXPECT_TRUE(popup.accept(1, 1000, 1000));
  EXPECT_EQ(1, popup.priority());
  popup.clear();
  EXPECT_EQ(0, popup.priority());
}

TEST(NotificationWakeController, NotificationWakeExpiresInFiveSeconds) {
  zen::NotificationWakeController wake;
  auto sleeping = wake.present(false, true, 1000, 60000);
  EXPECT_TRUE(sleeping.turn_on);
  EXPECT_TRUE(wake.active());
  EXPECT_EQ(6000u, sleeping.deadline);

  auto repeated = wake.present(true, false, 2000, 60000);
  EXPECT_FALSE(repeated.turn_on);
  EXPECT_EQ(7000u, repeated.deadline);
  wake.interaction();
  auto user_owned = wake.present(true, false, 3000, 60000);
  EXPECT_EQ(63000u, user_owned.deadline);
}

TEST(NotificationPolicy, AdvertsAndAcknowledgementsNeverRequestWake) {
  auto result = NotificationPolicy::decide(
      true, false, true, NotificationPolicy::ON, false, ScreenWake::ALWAYS, false);
  EXPECT_FALSE(result.show_visual);
  EXPECT_FALSE(result.wake_screen);
  EXPECT_TRUE(result.play_sound);
}

TEST(NotificationPolicy, ManualDndSharesQuietTimeSoundAndIconPolicy) {
  bool scheduled_quiet = false;
  bool manual_dnd = true;
  bool quiet = scheduled_quiet || manual_dnd;
  auto decision = NotificationPolicy::decide(
      true, quiet, true, NotificationPolicy::ON, false, ScreenWake::ON, true);
  EXPECT_TRUE(decision.record_unread);
  EXPECT_TRUE(decision.wake_screen);
  EXPECT_FALSE(decision.play_sound);
  EXPECT_TRUE(NotificationPolicy::audioMuted(NotificationPolicy::ON, false, quiet));

  manual_dnd = false;
  EXPECT_FALSE(NotificationPolicy::audioMuted(NotificationPolicy::ON, false,
                                               scheduled_quiet || manual_dnd));
  scheduled_quiet = true;
  EXPECT_TRUE(NotificationPolicy::audioMuted(NotificationPolicy::ON, false,
                                              scheduled_quiet || manual_dnd));
  EXPECT_TRUE(NotificationPolicy::audioMuted(NotificationPolicy::AUTO, true, false));
}

TEST(NotificationPolicy, MutedSourceKeepsUnreadWithoutAnyLocalAlert) {
  auto result = NotificationPolicy::decide(
      true, false, true, NotificationPolicy::ON, false, ScreenWake::ALWAYS, true,
      NotificationPolicy::MUTED);
  EXPECT_TRUE(result.record_unread);
  EXPECT_FALSE(result.show_visual);
  EXPECT_FALSE(result.wake_screen);
  EXPECT_FALSE(result.play_sound);
  EXPECT_FALSE(result.vibrate);
}

TEST(NotificationPolicy, LocalSourceOverridesConnectedAutoOnly) {
  auto local = NotificationPolicy::decide(
      true, false, true, NotificationPolicy::AUTO, true, ScreenWake::ON, true,
      NotificationPolicy::LOCAL);
  EXPECT_TRUE(local.show_visual);
  EXPECT_TRUE(local.wake_screen);
  EXPECT_TRUE(local.play_sound);

  auto globally_off = NotificationPolicy::decide(
      true, false, true, NotificationPolicy::OFF, true, ScreenWake::ON, true,
      NotificationPolicy::LOCAL);
  EXPECT_TRUE(globally_off.record_unread);
  EXPECT_TRUE(globally_off.show_visual);
  EXPECT_FALSE(globally_off.wake_screen);
  EXPECT_FALSE(globally_off.play_sound);

  auto quiet = NotificationPolicy::decide(
      true, true, true, NotificationPolicy::AUTO, true, ScreenWake::ON, true,
      NotificationPolicy::LOCAL);
  EXPECT_TRUE(quiet.wake_screen);
  EXPECT_FALSE(quiet.play_sound);
}

TEST(NotificationPolicy, NewContactVisualAlertFollowsWakeAndQuietRules) {
  auto visual_only = NotificationPolicy::decide(
      true, true, true, NotificationPolicy::ON, false, ScreenWake::OFF, true);
  EXPECT_TRUE(visual_only.show_visual);
  EXPECT_FALSE(visual_only.wake_screen);
  EXPECT_FALSE(visual_only.play_sound);

  auto blocked = NotificationPolicy::decide(
      false, false, true, NotificationPolicy::ON, false, ScreenWake::ALWAYS, true);
  EXPECT_FALSE(blocked.present());
}

TEST(NotificationPolicy, AlwaysWakesDespiteModeAndQuietTime) {
  for (auto mode : { NotificationPolicy::OFF, NotificationPolicy::AUTO }) {
    auto result = NotificationPolicy::decide(
        true, true, true, mode, true, ScreenWake::ALWAYS, true);
    EXPECT_TRUE(result.show_visual);
    EXPECT_TRUE(result.wake_screen);
    EXPECT_FALSE(result.play_sound);
  }
}

TEST(NotificationPolicy, WakeValuePreservesExistingSettings) {
  EXPECT_EQ(ScreenWake::OFF, NotificationPolicy::screenWake(0));
  EXPECT_EQ(ScreenWake::ON, NotificationPolicy::screenWake(1));
  EXPECT_EQ(ScreenWake::ALWAYS, NotificationPolicy::screenWake(2));
  EXPECT_EQ(ScreenWake::ON, NotificationPolicy::screenWake(255));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
