#include <Arduino.h>
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <array>
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetPage.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetPopupInput.h"

class PetDisplay : public ZenDisplayDriver {
public:
  struct Text { int y; std::string value; };
  std::vector<Text> text;
  int cursor_y = 0;
  int cursor_x = 0;
  int scale = 1;
  bool eink = false;
  bool valid = true;
  PetDisplay(int w=128,int h=64,int s=1) : ZenDisplayDriver(w,h), scale(s) {}
  bool isOn() override { return true; }
  bool isEink() override { return eink; }
  void turnOn() override {}
  void turnOff() override {}
  void clear() override {}
  void startFrame(ColorVal = 0) override { text.clear(); valid = true; }
  void setTextSize(int) override {}
  void setColor(ColorVal) override {}
  void setCursor(int x, int y) override { cursor_x = x; cursor_y = y; }
  void print(const char* value) override {
    text.push_back({cursor_y,value});
    if(cursor_x<0 || cursor_y<0 || cursor_y+8*scale>height() || cursor_x+strlen(value)*6*scale>width()) {
      ADD_FAILURE() << value << " y=" << cursor_y << " scale=" << scale;
      valid=false;
    }
  }
  void bounds(int x,int y,int w,int h) {
    if(x<0 || y<0 || w<0 || h<0 || x+w>width() || y+h>height()) valid=false;
  }
  void fillRect(int x,int y,int w,int h) override { bounds(x,y,w,h); }
  void drawRect(int x,int y,int w,int h) override { bounds(x,y,w,h); }
  void drawXbm(int,int,const uint8_t*,int,int) override {}
  uint16_t getTextWidth(const char* value) override { return strlen(value)*6*scale; }
  int getCharWidth() const override { return 6*scale; }
  int getLineHeight() const override { return 8*scale; }
  void endFrame() override {}
  bool contains(const char* value) const {
    for(const auto& row:text) if(row.value==value) return true;
    return false;
  }
};
static bool key(zen::pet::Page& page,char c) {
  return page.input(c,'e','b','h','u','d');
}
TEST(PetUI, ScheduledSleepAndWakeOverride) {
  using zen::pet::PetSleep;
  EXPECT_TRUE(PetSleep::scheduled(true,23*3600,22*60,7*60));
  EXPECT_TRUE(PetSleep::scheduled(true,6*3600,22*60,7*60));
  EXPECT_FALSE(PetSleep::scheduled(true,7*3600,22*60,7*60));
  EXPECT_FALSE(PetSleep::scheduled(false,23*3600,22*60,7*60));
  EXPECT_FALSE(PetSleep::scheduled(true,0,0,0));
  PetSleep sleep;
  uint32_t now = UINT32_MAX-100;
  EXPECT_TRUE(sleep.sleeping(now,true,true));
  sleep.wake(now);
  EXPECT_FALSE(sleep.sleeping(now+1799999UL,true,true));
  EXPECT_TRUE(sleep.sleeping(now+1800000UL,true,true));
  sleep.wake(0);
  sleep.sleeping(1,false,true);
  EXPECT_TRUE(sleep.sleeping(2,true,true));
  sleep.wake(2);
  sleep.sleeping(3,true,false);
  EXPECT_TRUE(sleep.sleeping(4,true,true));
}
TEST(PetUI, WakeMenuRespectsLowPowerAndExpires) {
  zen::pet::Page page;
  PetDisplay display;
  g_mock_millis = 100;
  page.update(100,true,true,true);
  key(page,'e'); key(page,'u');
  page.render(display,12,63);
  EXPECT_TRUE(display.contains("Wake Up"));
  key(page,'e');
  display.startFrame(); page.render(display,12,63);
  EXPECT_TRUE(display.contains("Wake Up"));
  EXPECT_FALSE(display.contains("Low Power"));
  zen::pet::PetNotifications::Alert alert;
  ASSERT_TRUE(page.takeAlert(100,alert)); EXPECT_TRUE(alert.action);
  EXPECT_STREQ("Low Power",alert.text);
  page.update(100,true,true,false);
  key(page,'e');
  display.startFrame(); page.render(display,12,63);
  EXPECT_FALSE(display.contains("Sprout Sleep"));
  page.update(1800100,true,true,false);
  key(page,'e'); key(page,'u');
  display.startFrame(); page.render(display,12,63);
  EXPECT_TRUE(display.contains("Wake Up"));
}
TEST(PetUI, DailyRewardsScrollIntoViewAndDetailsNavigate) {
  zen::pet::Page page; PetDisplay d;
  page.update(0,true,false,false);
  ASSERT_TRUE(key(page,'e'));
  for(int i=0;i<4;++i) ASSERT_TRUE(key(page,'d'));
  page.render(d,24,63);
  EXPECT_TRUE(d.contains("Daily Rewards")); EXPECT_TRUE(d.valid);
  d.startFrame(); ASSERT_TRUE(key(page,'e')); page.render(d,24,63);
  EXPECT_TRUE(d.contains("DM 0/20 XP")); EXPECT_TRUE(d.contains("Bond 0/5"));
  EXPECT_TRUE(d.valid);
  d.startFrame(); ASSERT_TRUE(key(page,'e')); page.render(d,24,63);
  EXPECT_TRUE(d.contains("Two-way: Not yet")); EXPECT_TRUE(d.valid);
  d.startFrame(); ASSERT_TRUE(key(page,'b')); page.render(d,24,63);
  EXPECT_TRUE(d.contains("DM 0/20 XP"));
  d.startFrame(); ASSERT_TRUE(key(page,'b')); page.render(d,24,63);
  EXPECT_TRUE(d.contains("Daily Rewards")); EXPECT_TRUE(d.valid);
}
TEST(PetUI, CareLabelsStayStableAndFeedbackIsVisualOnly) {
  zen::pet::Page page; PetDisplay d;
  g_mock_millis=0; page.update(0,true,false,false); key(page,'e'); key(page,'e');
  page.render(d,12,63);
  EXPECT_TRUE(d.contains("Feed")); EXPECT_FALSE(d.contains("Fed"));
  zen::pet::PetNotifications::Alert alert;
  ASSERT_TRUE(page.takeAlert(0,alert)); EXPECT_STREQ("Fed",alert.text);
  EXPECT_TRUE(alert.action); EXPECT_EQ(nullptr,zen::pet::PetNotifications::melody(alert.sound));
  EXPECT_FALSE(page.takeAlert(1,alert));
  page.update(2,true,true,false); key(page,'d'); key(page,'e');
  d.startFrame(); page.render(d,12,63);
  EXPECT_TRUE(d.contains("Train")); EXPECT_FALSE(d.contains("Sleeping"));
  ASSERT_TRUE(page.takeAlert(2,alert,true)); EXPECT_STREQ("Sleeping",alert.text);
  EXPECT_TRUE(alert.action);
}
TEST(PetUI, ManualCancellationUsesPopupWithoutChangingTrainLabel) {
  zen::pet::Page page; PetDisplay d;
  g_mock_millis=0; page.update(0,true,false,false);
  key(page,'e'); key(page,'d'); key(page,'e'); key(page,'b');
  page.render(d,12,63);
  EXPECT_TRUE(d.contains("Train")); EXPECT_FALSE(d.contains("Training cancelled"));
  zen::pet::PetNotifications::Alert alert;
  ASSERT_TRUE(page.takeAlert(0,alert)); EXPECT_STREQ("Training cancelled",alert.text);
  EXPECT_TRUE(alert.action); EXPECT_FALSE(page.takeAlert(1,alert));
}
TEST(PetUI, HighestPendingRewardTotalsFitDetails) {
  zen::pet::PetMeshRewards rewards; PetDisplay d;
  for(uint32_t day=0;day<2100;++day) {
    rewards.update(day*86400000UL,true,true,false,false,0);
    uint8_t key[32]={1};
    rewards.started(zen::pet::PetMeshRewards::DM,key,day+1,0,day+1,true);
    rewards.completed(day+1,zen::pet::PetMeshRewards::DM,true,true);
    rewards.received(key,day+1,false,true,true);
  }
  EXPECT_EQ(zen::pet::Evolution::xp(zen::pet::Evolution::LEVELS),rewards.progress().pending_xp);
  EXPECT_EQ(100,rewards.progress().pending_bond);
  zen::pet::PetRewardsView::render(d,24,63,rewards,true);
  EXPECT_TRUE(d.valid);
  PetDisplay eink(250,128,2);
  zen::pet::PetRewardsView::render(eink,48,127,rewards,true);
  EXPECT_TRUE(eink.valid);
}
TEST(PetUI, RewardsDoNotApplyWhileSleeping) {
  zen::pet::Page page; PetDisplay d; uint8_t key[32]={1};
  page.update(0,true,true,false);
  page.rewards().started(zen::pet::PetMeshRewards::DM,key,1,0,1,true);
  page.rewards().completed(1,zen::pet::PetMeshRewards::DM,true,false);
  page.update(1000,true,true,false);
  EXPECT_EQ(20,page.rewards().progress().pending_xp);
  page.update(2000,true,false,false);
  EXPECT_EQ(0,page.rewards().progress().pending_xp);
  page.render(d,24,63); EXPECT_TRUE(d.contains("XP 20"));
  zen::pet::PetNotifications::Alert alert;
  ASSERT_TRUE(page.takeAlert(2000,alert));
  EXPECT_STREQ("Pet: +20 XP",alert.text);
}
TEST(PetUI, BonusAlertsReportActualAmountsAndStopAtCaps) {
  zen::pet::Page page; uint8_t key[32]={1};
  zen::pet::PetNotifications::Alert alert;
  for(uint32_t day=0;day<2060;++day) {
    uint32_t now=day*86400000UL;
    page.update(now,true,false,false);
    page.rewards().started(zen::pet::PetMeshRewards::DM,key,day+1,0,day+1,true);
    page.rewards().completed(day+1,zen::pet::PetMeshRewards::DM,true,true);
    page.update(now+1,true,false,false);
    bool presented=page.takeAlert(now+1,alert);
    if(day<2053) ASSERT_TRUE(presented);
    else EXPECT_FALSE(presented);
    if(day==33) EXPECT_STREQ("Pet: +20 XP, +1 Bond",alert.text);
    if(day==34) EXPECT_STREQ("Pet: +20 XP",alert.text);
    if(day==7) {
      EXPECT_EQ(zen::pet::PetNotifications::READY,alert.sound);
      EXPECT_NE(nullptr,strstr(alert.text,"Evolve ready"));
    }
    // Normal UI updates also present the independent pending hunger alert.
    zen::pet::PetNotifications::Alert hunger;
    if(page.takeAlert(now+5001,hunger))
      EXPECT_TRUE(hunger.sound==zen::pet::PetNotifications::HUNGRY ||
                  hunger.sound==zen::pet::PetNotifications::VERY_HUNGRY);
  }
}
static uint32_t gameSeed(zen::pet::PetTrainingGames::Game wanted) {
  using Games=zen::pet::PetTrainingGames;
  for(uint32_t seed=1;seed<10000;++seed) {
    Games game; game.begin(0,seed,false);
    if(game.game()==wanted) return seed;
  }
  return 0;
}
TEST(PetUI, AllTrainingGamePhasesFitOLEDAndEink) {
  using Games=zen::pet::PetTrainingGames;
  for(int scale:{1,2}) for(int id=0;id<Games::COUNT;++id) {
    PetDisplay d(scale==1?128:250,64*scale,scale);
    Games game; uint32_t now=0;
    game.begin(now,gameSeed((Games::Game)id),scale==2);
    for(int phase=0;phase<4;++phase) {
      d.startFrame();
      zen::pet::PetTrainingView::render(d,24*scale,64*scale-1,game,now);
      EXPECT_TRUE(d.valid) << id << ":" << phase << ":" << scale;
      if(phase==0) game.input(Games::ENTER,now);
      if(phase==1) while(game.previewing()) { now+=game.interval(); game.tick(now); }
      if(phase==2) { now+=60001; game.tick(now); }
    }
  }
}
TEST(PetUI, TimingTargetsAndTurnLabelsFitBothDisplays) {
  using Games=zen::pet::PetTrainingGames;
  for(int scale:{1,2}) for(uint32_t seed=1;seed<=50;++seed) {
    Games game; uint32_t now=0; game.begin(0,seed,scale==2);
    if(game.game()!=Games::TIMING) continue;
    PetDisplay d(scale==1?128:250,64*scale,scale);
    game.input(Games::ENTER,now);
    for(uint8_t turn=0;turn<3;++turn) {
      d.startFrame(); zen::pet::PetTrainingView::render(d,24*scale,64*scale-1,game,now);
      char label[16]; snprintf(label,sizeof(label),"Turn %u/3",turn+1);
      EXPECT_TRUE(d.contains(label)); EXPECT_TRUE(d.valid);
      while(!game.inZone()) { now+=game.interval(); game.tick(now); }
      d.startFrame(); zen::pet::PetTrainingView::render(d,24*scale,64*scale-1,game,now);
      EXPECT_TRUE(d.valid); game.input(Games::ENTER,now);
    }
    EXPECT_EQ(Games::WON,game.phase());
  }
}
TEST(PetUI, FiveFoodLanesFitBothDisplays) {
  using Games=zen::pet::PetTrainingGames;
  for(int scale:{1,2}) for(uint32_t seed=1;seed<=50;++seed) {
    Games game; game.begin(0,seed,scale==2);
    if(game.game()!=Games::FOOD) continue;
    PetDisplay d(scale==1?128:250,64*scale,scale);
    game.input(Games::ENTER,0);
    game.input(Games::LEFT,0); game.input(Games::LEFT,0);
    for(int lane=0;lane<5;++lane) {
      d.startFrame(); zen::pet::PetTrainingView::render(d,24*scale,64*scale-1,game,0);
      EXPECT_TRUE(d.valid); EXPECT_EQ(lane,game.selection());
      game.input(Games::RIGHT,0);
    }
  }
}
TEST(PetUI, ChangedShapesRenderThreeSolidCoveredSquaresOnBothDisplays) {
  using Games=zen::pet::PetTrainingGames;
  struct Squares : PetDisplay {
    unsigned count=0;
    Squares(int s) : PetDisplay(s==1?128:250,64*s,s) {}
    void fillRect(int x,int y,int w,int h) override {
      if(w==7*scale && h==7*scale) ++count;
      PetDisplay::fillRect(x,y,w,h);
    }
  };
  for(int scale:{1,2}) {
    Games game; game.begin(0,gameSeed(Games::CHANGED_SHAPE),scale==2);
    game.input(Games::ENTER,0); uint32_t now=game.interval(); game.tick(now);
    ASSERT_EQ(Games::COVERED,game.phase()); Squares d(scale);
    EXPECT_EQ(3000,zen::pet::PetTrainingView::render(d,24*scale,64*scale-1,game,now));
    EXPECT_EQ(3u,d.count); EXPECT_TRUE(d.valid);
  }
}
TEST(PetUI, CorrectArrowInputsReplaceOnlyTheirBoxesAndRetryClearsThem) {
  using Games=zen::pet::PetTrainingGames;
  struct Arrows : PetDisplay {
    unsigned boxes[3]={}, pixels[3]={};
    Arrows(int s) : PetDisplay(s==1?128:250,64*s,s) {}
    void startFrame(ColorVal color=0) override {
      PetDisplay::startFrame(color);
      for(int i=0;i<3;++i) boxes[i]=pixels[i]=0;
    }
    void fillRect(int x,int y,int w,int h) override {
      PetDisplay::fillRect(x,y,w,h);
      if(w==scale && h==scale) ++pixels[x*3/width()];
    }
    void drawRect(int x,int y,int w,int h) override {
      PetDisplay::drawRect(x,y,w,h);
      if(w==6*scale && h==6*scale) ++boxes[x*3/width()];
    }
  };
  for(int scale:{1,2}) {
    Games game; uint32_t now=0; game.begin(0,gameSeed(Games::ARROWS),scale==2);
    uint8_t original[3]={game.arrow(0),game.arrow(1),game.arrow(2)};
    game.input(Games::ENTER,now); now+=game.interval(); game.tick(now);
    Arrows d(scale);
    auto check=[&](uint8_t completed) {
      d.startFrame(); zen::pet::PetTrainingView::render(d,24*scale,64*scale-1,game,now);
      EXPECT_TRUE(d.valid);
      for(uint8_t i=0;i<3;++i) {
        EXPECT_EQ(i<completed?0u:1u,d.boxes[i]);
        EXPECT_EQ(i<completed,d.pixels[i]>0);
      }
    };
    check(0);
    for(uint8_t i=0;i<2;++i) {
      game.input((Games::Action)(Games::UP+game.arrow(i)),now); check(i+1);
    }
    game.input((Games::Action)(Games::UP+(game.arrow(2)+1)%4),now);
    ASSERT_EQ(Games::FAILED,game.phase());
    game.input(Games::ENTER,now); game.input(Games::ENTER,now);
    for(uint8_t i=0;i<3;++i) EXPECT_EQ(original[i],game.arrow(i));
    now+=game.interval(); game.tick(now); check(0);
    for(uint8_t i=0;i<3;++i) {
      game.input((Games::Action)(Games::UP+game.arrow(i)),now); check(i+1);
    }
    EXPECT_EQ(Games::WON,game.phase());
  }
}
TEST(PetUI, EachGameAwardsTrainingOnceAndOnlyAfterSuccess) {
  using Games=zen::pet::PetTrainingGames;
  for(int id=0;id<Games::COUNT;++id) {
    g_mock_millis=0; zen::pet::Page page; PetDisplay d; Games model;
    page.update(0,true,false,false);
    key(page,'e'); key(page,'d');
    uint32_t seed=gameSeed((Games::Game)id);
    page.input('e','e','b','h','u','d','l','r','[',']',seed);
    model.begin(0,seed,false);
    ASSERT_TRUE(page.trainingActive()); EXPECT_EQ(Games::INSTRUCTIONS,model.phase());
    EXPECT_TRUE(page.gameDisplay(0,true).hold);
    uint32_t now=0;
    auto action=[&](Games::Action a) {
      static const char KEYS[]={'?','u','r','d','l','e','b'};
      g_mock_millis=now; model.input(a,now); key(page,KEYS[a]);
    };
    auto tick=[&]() {
      now+=model.interval(); g_mock_millis=now;
      model.tick(now); page.update(now,true,false,false);
    };
    action(Games::ENTER);
    while(model.previewing()) tick();
    if(id==Games::ARROWS) {
      for(int i=0;i<3;++i) action((Games::Action)(Games::UP+model.arrow(i)));
    } else if(id==Games::TIMING) {
      for(int hit=0;hit<3;++hit) {
        while(!model.inZone()) tick();
        action(Games::ENTER);
      }
    } else if(id==Games::FOOD) {
      while(model.phase()==Games::PLAY) {
        while(model.selection()!=model.target())
          action(model.selection()<model.target()?Games::RIGHT:Games::LEFT);
        tick();
      }
    } else {
      while(model.selection()!=model.target())
        action(model.selection()<model.target()?Games::RIGHT:Games::LEFT);
      action(Games::ENTER);
      if(model.phase()==Games::ANSWER) tick();
    }
    EXPECT_EQ(Games::WON,model.phase()); EXPECT_FALSE(page.trainingActive());
    EXPECT_TRUE(page.gameDisplay(now,true).hold);
    EXPECT_TRUE(page.gameDisplay(now+4999,true).hold);
    EXPECT_TRUE(page.gameDisplay(now+5000,true).released);
    zen::pet::PetNotifications::Alert alert;
    ASSERT_TRUE(page.takeAlert(now,alert,true)); EXPECT_STREQ("Won!",alert.text);
    page.update(now+1,true,false,false);
    EXPECT_FALSE(page.takeAlert(now+5000,alert));
    key(page,'h'); page.render(d,24,63);
    EXPECT_TRUE(d.contains("Sprout L1")); EXPECT_TRUE(d.contains("XP 20/160"));
    EXPECT_TRUE(d.contains("Energy 80 Food 3"));
    EXPECT_TRUE(d.contains("Full 60 Bond 2/20"));
    key(page,'e'); key(page,'e'); key(page,'d'); key(page,'e');
    EXPECT_FALSE(page.trainingActive()); // Cooldown starts on successful completion.
    const uint32_t offsets[]={1,59999,60000,60001,240000,299999};
    const char* rests[]={"Rest 5 minutes","Rest 5 minutes","Rest 4 minutes",
                         "Rest 4 minutes","Rest 1 minute","Rest 1 minute"};
    for(unsigned i=0;i<6;++i) {
      g_mock_millis=now+offsets[i]; page.update(g_mock_millis,true,false,false);
      key(page,'e'); ASSERT_TRUE(page.takeAlert(g_mock_millis,alert));
      EXPECT_TRUE(alert.action); EXPECT_STREQ(rests[i],alert.text);
      EXPECT_FALSE(page.trainingActive());
    }
    g_mock_millis=now+300000; page.update(g_mock_millis,true,false,false);
    key(page,'e'); EXPECT_TRUE(page.trainingActive());
  }
}
TEST(PetUI, EveryBoxShuffleFrameAndAnswerFitBothDisplays) {
  using Games=zen::pet::PetTrainingGames;
  for(int scale:{1,2}) for(uint32_t seed=1;seed<=50;++seed) {
    Games game; uint32_t now=0; game.begin(0,seed,scale==2);
    if(game.game()!=Games::BOXES) continue;
    PetDisplay d(scale==1?128:250,64*scale,scale);
    game.input(Games::ENTER,now);
    auto frame=[&]() {
      d.startFrame(); zen::pet::PetTrainingView::render(d,24*scale,64*scale-1,game,now);
      EXPECT_TRUE(d.valid) << game.phase() << " stage " << game.stage();
    };
    while(game.previewing()) { frame(); now+=game.interval(); game.tick(now); }
    frame(); EXPECT_TRUE(d.contains("Where is your pet?"));
    game.input(Games::ENTER,now); frame(); EXPECT_EQ(Games::ANSWER,game.phase());
    EXPECT_TRUE(d.contains(game.selection()==game.target()?"Found your pet!":"Your pet was here"));
  }
}
TEST(PetUI, CancellingBoxAnswerRevealDoesNotAwardTraining) {
  using Games=zen::pet::PetTrainingGames;
  zen::pet::Page page; Games model; uint32_t now=0;
  g_mock_millis=0; page.update(0,true,false,false); key(page,'e'); key(page,'d');
  uint32_t seed=gameSeed(Games::BOXES);
  page.input('e','e','b','h','u','d','l','r','[',']',seed);
  model.begin(0,seed,false); model.input(Games::ENTER,0); key(page,'e');
  while(model.previewing()) {
    now+=model.interval(); g_mock_millis=now; model.tick(now); page.update(now,true,false,false);
  }
  while(model.selection()!=model.target()) {
    bool right=model.selection()<model.target();
    model.input(right?Games::RIGHT:Games::LEFT,now); key(page,right?'r':'l');
  }
  key(page,'e'); ASSERT_TRUE(page.trainingActive());
  zen::pet::PetNotifications::Alert alert; EXPECT_FALSE(page.takeAlert(now,alert));
  key(page,'b'); EXPECT_FALSE(page.trainingActive());
  key(page,'b'); key(page,'h'); PetDisplay d; page.render(d,24,63);
  EXPECT_TRUE(d.contains("Sprout L1")); EXPECT_TRUE(d.contains("XP 0/160")); EXPECT_TRUE(d.contains("Energy 100 Food 3"));
}
TEST(PetUI, CancellingOrLosingVisibilityNeverChargesTraining) {
  for(int reason=0;reason<5;++reason) {
    g_mock_millis=0; zen::pet::Page page; PetDisplay d;
    page.update(0,true,false,false); key(page,'e'); key(page,'d'); key(page,'e');
    ASSERT_TRUE(page.trainingActive());
    if(reason==0) { key(page,'b'); key(page,'b'); }
    else page.update(1000,reason!=1,reason==2,reason==3,false,0,reason!=4);
    EXPECT_FALSE(page.trainingActive());
    zen::pet::PetNotifications::Alert alert;
    EXPECT_FALSE(page.takeAlert(1000,alert));
    key(page,'h'); page.render(d,24,63);
    EXPECT_TRUE(d.contains("Sprout L1")); EXPECT_TRUE(d.contains("XP 0/160")); EXPECT_TRUE(d.contains("Energy 100 Food 3"));
  }
}
TEST(PetUI, GameDisplayInstructionsIdleAndSafetyOverrides) {
  for(int safety=0;safety<5;++safety) {
    g_mock_millis=0; zen::pet::Page page;
    page.update(0,true,false,false); key(page,'e'); key(page,'d'); key(page,'e');
    ASSERT_TRUE(page.trainingActive()); EXPECT_TRUE(page.gameDisplay(0,true).hold);
    EXPECT_TRUE(page.gameDisplay(59999,true).hold);
    EXPECT_TRUE(page.gameDisplay(60000,true).expired);
    if(safety==0)page.cancelTraining();
    if(safety==1)page.update(60001,false,false,false);
    if(safety==2)page.update(60001,true,true,false);
    if(safety==3)page.update(60001,true,false,true);
    EXPECT_FALSE(page.gameDisplay(60001,safety!=4).hold);
    EXPECT_FALSE(page.gameDisplay(60002,safety!=4).expired);
  }
}
TEST(PetUI, LossNotifiesOnceAndPopupPreservesRetry) {
  using Games=zen::pet::PetTrainingGames;
  g_mock_millis=0; zen::pet::Page page;
  page.update(0,true,false,false); key(page,'e'); key(page,'d');
  page.input('e','e','b','h','u','d','l','r','[',']',gameSeed(Games::TIMING));
  key(page,'e'); key(page,'e'); // Marker starts outside the hit zone.
  ASSERT_TRUE(page.trainingFailed());
  EXPECT_TRUE(page.gameDisplay(0,true).hold);
  EXPECT_TRUE(page.gameDisplay(4999,true).hold);
  EXPECT_TRUE(page.gameDisplay(5000,true).released);
  zen::pet::PetNotifications::Alert alert;
  ASSERT_TRUE(page.takeAlert(0,alert)); EXPECT_STREQ("Lost!",alert.text);
  page.update(1000,true,false,false,false,0,false); // Popup covers the game.
  EXPECT_TRUE(page.trainingFailed());
  EXPECT_FALSE(page.takeAlert(5000,alert));
  g_mock_millis=5000; key(page,'e'); // Retry.
  key(page,'e'); key(page,'e');
  ASSERT_TRUE(page.takeAlert(5000,alert)); EXPECT_STREQ("Lost!",alert.text);
  EXPECT_FALSE(page.takeAlert(10000,alert));
  key(page,'e'); EXPECT_FALSE(page.trainingActive());
}
TEST(PetUI, GameplayTimeoutReportsLostButBackCancellationDoesNot) {
  g_mock_millis=0; zen::pet::Page page;
  page.update(0,true,false,false); key(page,'e'); key(page,'d'); key(page,'e'); key(page,'e');
  g_mock_millis=45000;
  page.update(45000,true,false,false);
  zen::pet::PetNotifications::Alert alert;
  ASSERT_TRUE(page.takeAlert(45000,alert)); EXPECT_STREQ("Lost!",alert.text);
  EXPECT_FALSE(page.takeAlert(50000,alert));
}
TEST(PetUI, CardKBPreviousNextAliasesAreTrainingDirectionsNotCarouselNavigation) {
  using Games=zen::pet::PetTrainingGames;
  g_mock_millis=0; zen::pet::Page page; Games model;
  uint32_t seed=gameSeed(Games::CHANGED_SHAPE), now=0;
  page.update(0,true,false,false); key(page,'e'); key(page,'d');
  page.input('e','e','b','h','u','d','l','r','[',']',seed);
  model.begin(0,seed,false); model.input(Games::ENTER,0); key(page,'e');
  while(model.previewing()) {
    now+=model.interval(); g_mock_millis=now; model.tick(now); page.update(now,true,false,false);
  }
  while(model.selection()!=model.target()) { model.input(Games::RIGHT,now); key(page,']'); }
  key(page,'e'); EXPECT_FALSE(page.trainingActive());
  PetDisplay d; key(page,'h'); page.render(d,24,63);
  EXPECT_TRUE(d.contains("Sprout L1")); EXPECT_TRUE(d.contains("XP 20/160"));
}
TEST(PetUI, AllFormsDetailsPosesAndEvolutionPreviewsFitBothDisplays) {
  using zen::pet::Evolution;
  g_mock_millis=0;
  for(int scale:{1,2}) for(uint8_t id=0;id<Evolution::FORMS;++id) {
    zen::pet::Engine engine; engine.update(0,true,false,false); engine.bonus(65535,255);
    uint8_t level=Evolution::level(id), index=id-Evolution::offset(level);
    while(engine.level()<level) {
      uint8_t current=engine.level();
      uint8_t choice=(current&1)?(index>>(level/2-1-current/2))&1:0;
      ASSERT_EQ(zen::pet::Engine::OK,engine.evolve(choice));
    }
    ASSERT_EQ(id,engine.state().form);
    PetDisplay d(scale==1?128:250,64*scale,scale);
    zen::pet::PetPersonality::Presentation look;
    for(uint8_t pose=0;pose<=zen::pet::PetPersonality::REST;++pose) {
      look.pose=(zen::pet::PetPersonality::Pose)pose;
      d.startFrame(); zen::pet::Renderer::render(d,24*scale,64*scale-1,engine,0,0,look);
      EXPECT_TRUE(d.valid) << unsigned(id) << " pose " << unsigned(pose);
    }
    d.startFrame(); zen::pet::Renderer::render(d,24*scale,64*scale-1,engine,2,0,look);
    EXPECT_TRUE(d.valid) << unsigned(id);
    if(level==12) EXPECT_TRUE(d.contains("Final form XP 41050"));
    else for(uint8_t choice=0;choice<engine.choices();++choice) {
      d.startFrame(); zen::pet::Renderer::render(d,24*scale,64*scale-1,engine,3,choice,look);
      EXPECT_TRUE(d.valid) << unsigned(id) << " choice " << unsigned(choice);
    }
    engine.update(1,true,true,false);
    d.startFrame(); zen::pet::Renderer::render(d,24*scale,64*scale-1,engine,0,0,look);
    EXPECT_TRUE(d.valid) << unsigned(id) << " sleep";
  }
}
TEST(PetUI, LowBatteryDetailsScrollWithoutLosingCareValues) {
  for(int scale:{1,2}) {
    zen::pet::Page page;
    page.update(0,true,false,false,true,0,true,20);
    key(page,'h'); PetDisplay d(scale==1?128:250,64*scale,scale);
    page.render(d,24*scale,64*scale-1);
    EXPECT_TRUE(d.contains("Sprout L1")); EXPECT_TRUE(d.valid);
    key(page,'d'); key(page,'d'); key(page,'d');
    d.startFrame(); page.render(d,24*scale,64*scale-1);
    EXPECT_TRUE(d.contains("Charge to reduce")); EXPECT_TRUE(d.contains("hunger"));
    EXPECT_TRUE(d.contains("Full 70 Bond 0/20")); EXPECT_TRUE(d.valid);
    key(page,'u'); key(page,'u'); key(page,'u');
    d.startFrame(); page.render(d,24*scale,64*scale-1);
    EXPECT_TRUE(d.contains("Sprout L1")); EXPECT_TRUE(d.valid);
    key(page,'d'); page.update(1,true,false,false,true,0,true,100);
    key(page,'u');
    d.startFrame(); page.render(d,24*scale,64*scale-1);
    EXPECT_TRUE(d.contains("Sprout L1")); EXPECT_FALSE(d.contains("Charge to reduce"));
    EXPECT_TRUE(d.valid);
  }
}
TEST(PetUI, ToastDismissalRetainsDetailsAndRequiresFreshEnter) {
  using Gate=zen::pet::PetPopupInput;
  for(char dismiss:{'e','b'}) {
    zen::pet::Page page; page.update(0,true,false,false);
    key(page,'h'); ASSERT_TRUE(page.menuOpen());
    unsigned dispatched=0;
    for(char c:{dismiss,'e','e'}) {
      auto action=Gate::action(page.menuOpen(),page.trainingActive(),true,c,'e','b');
      if(action==Gate::DISMISS) break; // Production drains the queued burst.
      if(action==Gate::PASS) { ++dispatched; key(page,c); }
    }
    EXPECT_EQ(0u,dispatched); EXPECT_TRUE(page.menuOpen());
    PetDisplay d; page.render(d,24,63); EXPECT_TRUE(d.contains("Sprout L1"));
    EXPECT_EQ(Gate::PASS,Gate::action(true,false,false,'e','e','b'));
    key(page,'e'); EXPECT_FALSE(page.menuOpen());
  }
}
TEST(PetUI, ToastBlocksNavigationAndPreservesSelectedCareAction) {
  using Gate=zen::pet::PetPopupInput;
  zen::pet::Page page; page.update(0,true,false,false);
  key(page,'e'); key(page,'d'); // Train selected.
  for(char c:{'u','d','l','r','h'})
    EXPECT_EQ(Gate::BLOCK,Gate::action(true,false,true,c,'e','b'));
  EXPECT_EQ(Gate::DISMISS,Gate::action(true,false,true,'e','e','b'));
  // Expiry/dismissal does not touch Page. Fresh Enter still starts training.
  key(page,'e'); EXPECT_TRUE(page.trainingActive());
  EXPECT_EQ(Gate::PASS,Gate::action(true,true,true,'b','e','b'));
  EXPECT_EQ(Gate::PASS,Gate::action(false,false,true,'e','e','b'));
  key(page,'b'); EXPECT_FALSE(page.trainingActive());
}
TEST(PetUI, EveryPersonalityPoseQuirkAndBubbleFitsAllFormsOnBothDisplays) {
  using P=zen::pet::PetPersonality;
  class PortraitDisplay : public PetDisplay {
  public:
    using PetDisplay::PetDisplay;
    void fillRect(int x,int y,int w,int h) override {
      EXPECT_GE(x,width()/2+2); PetDisplay::fillRect(x,y,w,h);
    }
    void drawRect(int x,int y,int w,int h) override {
      EXPECT_GE(x,width()/2+2); PetDisplay::drawRect(x,y,w,h);
    }
  };
  const char* phrases[]={"You're back!","That was close!","Snack?","Much better!","Ready!","Welcome back"};
  for(int scale:{1,2}) {
    PortraitDisplay d(scale==1?128:250,64*scale,scale); d.eink=scale==2;
    for(unsigned id=0;id<zen::pet::Evolution::FORMS;++id)
      for(unsigned pose=0;pose<=P::REST;++pose)
        for(unsigned quirk=0;quirk<=P::STRETCH;++quirk) for(unsigned frame=0;frame<4;++frame) {
          P::Presentation look; look.pose=(P::Pose)pose; look.quirk=(P::Quirk)quirk;
          look.frame=frame; look.phrase=phrases[(id+pose+quirk)%6]; look.active=true;
          for(int offset=0;offset<45;++offset) for(bool bubble:{false,true}) {
            look.offset_x=offset%9-4; look.offset_y=offset/9-2;
            auto layout=zen::pet::PetPortraitLayout::calculate(d.width(),d.getLineHeight(),
                18*scale,64*scale-1,zen::pet::form(id).size);
            if(abs(look.offset_x)>layout.range_x || abs(look.offset_y)>layout.range_y ||
                (d.eink && (look.offset_x || look.offset_y)))continue;
            look.phrase=bubble?phrases[(id+pose+quirk)%6]:nullptr;
            d.startFrame(); zen::pet::PetPersonalityView::render(d,18*scale,64*scale-1,zen::pet::form(id),look);
            EXPECT_TRUE(d.valid) << id << ':' << pose << ':' << quirk << ':' << frame << ':' << scale << ':' << offset;
          }
        }
    P::Presentation look; look.phrase="You're back!";
    d.startFrame(); zen::pet::PetPersonalityView::render(d,24*scale,64*scale-1,zen::pet::form(0),look);
    EXPECT_FALSE(d.text.empty()); EXPECT_TRUE(d.valid);
  }
}
TEST(PetUI, PortraitLayoutPreservesSizeAndReservesEveryAnimation) {
  for(unsigned id=0;id<zen::pet::Evolution::FORMS;++id) {
    auto p=zen::pet::PetPortraitLayout::calculate(128,8,18,63,zen::pet::form(id).size);
    EXPECT_EQ(zen::pet::form(id).size,p.size);
    EXPECT_LE(p.range_x,4); EXPECT_LE(p.range_y,2);
    EXPECT_GE(p.x-p.range_x-1,p.left);
    EXPECT_GE(p.y-p.range_y-2,p.top);
    EXPECT_LE(p.x+p.range_x+p.size+7,p.right);
    EXPECT_LE(p.y+p.range_y+p.size,p.bottom);
    EXPECT_EQ(12,p.bubbleHeight(8,1)); EXPECT_EQ(20,p.bubbleHeight(8,2));
  }
  auto tight=zen::pet::PetPortraitLayout::calculate(128,8,24,63,32);
  EXPECT_EQ(26,tight.size); EXPECT_EQ(0,tight.range_y);
}

TEST(PetUI, ShuffleMovesWholePortraitButNotBubbleAndEinkIgnoresOffsets) {
  using P=zen::pet::PetPersonality;
  class TraceDisplay : public PetDisplay {
  public:
    std::vector<std::array<int,4>> fills,boxes;
    ColorVal color=0;
    std::vector<ColorVal> colors;
    void setColor(ColorVal c) override { color=c; }
    void fillRect(int x,int y,int w,int h) override {
      fills.push_back({x,y,w,h}); colors.push_back(color); PetDisplay::fillRect(x,y,w,h);
    }
    void drawRect(int x,int y,int w,int h) override {
      boxes.push_back({x,y,w,h}); PetDisplay::drawRect(x,y,w,h);
    }
  };
  for(bool eink:{false,true}) for(bool bubble:{false,true}) {
    TraceDisplay d; d.eink=eink; P::Presentation p;
    p.active=true; p.pose=P::PROUD; p.phrase=bubble?"Snack?":nullptr;
    zen::pet::PetPersonalityView::render(d,24,63,zen::pet::form(0),p);
    auto fills=d.fills,boxes=d.boxes; auto text=d.text;
    if(bubble) {
      EXPECT_EQ(ZenDisplayDriver::DARK,d.colors[d.colors.size()-2]);
      EXPECT_EQ(ZenDisplayDriver::LIGHT,d.colors.back());
      d.fills.clear(); d.boxes.clear(); d.startFrame(); p.phrase=nullptr;
      zen::pet::PetPersonalityView::render(d,24,63,zen::pet::form(0),p);
      ASSERT_EQ(fills.size()-2,d.fills.size());
      for(unsigned i=0;i<d.fills.size();++i) EXPECT_EQ(fills[i],d.fills[i]);
      p.phrase="Snack?";
    }
    d.fills.clear(); d.boxes.clear(); d.startFrame(); p.offset_x=1; p.offset_y=-1;
    zen::pet::PetPersonalityView::render(d,24,63,zen::pet::form(0),p);
    ASSERT_EQ(fills.size(),d.fills.size()); EXPECT_EQ(boxes,d.boxes);
    ASSERT_EQ(text.size(),d.text.size());
    for(unsigned i=0;i<text.size();++i) {
      EXPECT_EQ(text[i].y,d.text[i].y); EXPECT_EQ(text[i].value,d.text[i].value);
    }
    for(unsigned i=0;i<fills.size();++i) {
      bool shifted=!eink && !(bubble && i>=fills.size()-2); // Opaque bubble and tail are stationary.
      EXPECT_EQ(fills[i][0]+(shifted?1:0),d.fills[i][0]);
      EXPECT_EQ(fills[i][1]-(shifted?1:0),d.fills[i][1]);
      EXPECT_EQ(fills[i][2],d.fills[i][2]); EXPECT_EQ(fills[i][3],d.fills[i][3]);
    }
  }
}
TEST(PetUI, PersonalityObservesSuccessfulFeedButNotRefusalsAndDetailsShowsNature) {
  g_mock_millis=0; zen::pet::Page page; page.update(0,true,false,false);
  auto nature=page.personality().temperament();
  key(page,'e'); key(page,'e');
  EXPECT_EQ(zen::pet::PetPersonality::HAPPY,page.personality().presentation(0).pose);
  g_mock_millis=4000; page.update(4000,true,false,false);
  key(page,'e'); // Already full; no new happy reaction.
  EXPECT_EQ(zen::pet::PetPersonality::NEUTRAL,page.personality().presentation(4000).pose);
  key(page,'b'); key(page,'h'); PetDisplay d; page.render(d,24,63);
  key(page,'d'); d.startFrame(); page.render(d,24,63);
  EXPECT_TRUE(d.contains((std::string("Nature ")+page.personality().name()).c_str()));
  page.update(5000,false,false,false); page.update(6000,true,false,false);
  EXPECT_EQ(nature,page.personality().temperament());
}
TEST(PetUI, PersonalityDoesNotLeakIntoMenuGamesSleepOrWakeOverride) {
  g_mock_millis=0; zen::pet::Page page;
  page.update(0,true,false,false,true,0,true,100,false,0,true);
  EXPECT_EQ(zen::pet::PetPersonality::SLEEPY,page.personality().presentation(0).pose);
  page.update(1,true,true,false,true,0,true,100,false,1,true);
  key(page,'e'); key(page,'u'); key(page,'e'); // Wake Up.
  page.update(2,true,true,false,true,0,true,100,false,2,true);
  EXPECT_NE(zen::pet::PetPersonality::SLEEPY,page.personality().presentation(2).pose);
  EXPECT_EQ(nullptr,page.personality().presentation(2).phrase);
  page.hidePresentation(3); EXPECT_FALSE(page.personality().presentation(3).active);
}
int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
