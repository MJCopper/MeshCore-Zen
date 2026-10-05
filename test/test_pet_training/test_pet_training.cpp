#include <gtest/gtest.h>
#define CHANGE 1
#define round(x) arduino_round_macro(x)
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetTrainingGames.h"
#undef CHANGE
#undef round
using Games=zen::pet::PetTrainingGames;

static uint32_t seedFor(Games::Game wanted) {
  for(uint32_t seed=1;seed<10000;++seed) {
    Games game; game.begin(0,seed,false);
    if(game.game()==wanted) return seed;
  }
  return 0;
}
static void start(Games& game,uint32_t& now) {
  game.input(Games::ENTER,now);
  while(game.previewing()) { now+=game.interval(); game.tick(now); }
}
static void solve(Games& game,uint32_t& now) {
  start(game,now);
  if(game.game()==Games::ARROWS) {
    for(int i=0;i<3;++i) game.input((Games::Action)(Games::UP+game.arrow(i)),now);
  } else if(game.game()==Games::TIMING) {
    for(int hit=0;hit<3;++hit) {
      while(!game.inZone()) { now+=game.interval(); game.tick(now); }
      game.input(Games::ENTER,now);
    }
  } else if(game.game()==Games::FOOD) {
    while(game.phase()==Games::PLAY) {
      while(game.selection()!=game.target())
        game.input(game.selection()<game.target()?Games::RIGHT:Games::LEFT,now);
      now+=game.interval(); game.tick(now);
    }
  } else {
    while(game.selection()!=game.target())
      game.input(game.selection()<game.target()?Games::RIGHT:Games::LEFT,now);
    game.input(Games::ENTER,now);
    if(game.phase()==Games::ANSWER) { now+=game.interval(); game.tick(now); }
  }
}
TEST(PetTraining, AllFiveGamesCanBeCompletedOnBothDisplays) {
  for(bool slow:{false,true}) for(int id=0;id<Games::COUNT;++id) {
    Games game; uint32_t now=0;
    uint32_t seed=seedFor((Games::Game)id); ASSERT_NE(0u,seed);
    game.begin(now,seed,slow); EXPECT_EQ(id,game.game());
    EXPECT_EQ(Games::INSTRUCTIONS,game.phase()); solve(game,now);
    EXPECT_EQ(Games::WON,game.phase()) << id;
    EXPECT_LT(now,slow?60000u:45000u);
  }
}
TEST(PetTraining, TimingTargetsShrinkMoveAndRetainTheirSequenceOnRetry) {
  unsigned checked=0; bool placements[5]={};
  for(uint32_t seed=1;seed<=1000;++seed) {
    Games game; uint32_t now=0; game.begin(now,seed,false);
    if(game.game()!=Games::TIMING) continue;
    ++checked; uint8_t starts[3];
    for(uint8_t turn=0;turn<3;++turn) {
      starts[turn]=game.timingStart(turn);
      EXPECT_LE(starts[turn]+Games::timingWidth(turn),9);
      if(turn) EXPECT_NE(starts[turn-1],starts[turn]);
    }
    placements[starts[0]]=true;
    start(game,now); EXPECT_EQ(5,game.zoneWidth()); EXPECT_FALSE(game.inZone());
    while(!game.inZone()) { now+=game.interval(); game.tick(now); }
    game.input(Games::ENTER,now);
    EXPECT_EQ(1,game.progress()); EXPECT_EQ(3,game.zoneWidth());
    EXPECT_EQ(starts[1],game.zoneStart()); EXPECT_FALSE(game.inZone());
    game.input(Games::ENTER,now); EXPECT_EQ(Games::FAILED,game.phase());
    game.input(Games::ENTER,now); start(game,now);
    EXPECT_EQ(0,game.progress()); EXPECT_EQ(5,game.zoneWidth());
    for(uint8_t turn=0;turn<3;++turn) EXPECT_EQ(starts[turn],game.timingStart(turn));
    game.cancel(); game.begin(now,seed+1,true);
    for(uint8_t turn=0;turn<3;++turn) EXPECT_EQ(starts[turn],game.timingStart(turn));
    solve(game,now); EXPECT_EQ(Games::WON,game.phase());
  }
  EXPECT_GT(checked,100u);
  for(bool seen:placements) EXPECT_TRUE(seen);
}
TEST(PetTraining, TimingUsesFiveThreeTwoPositionsAndResetsOutsideEveryTurn) {
  for(bool slow:{false,true}) {
    Games game; uint32_t now=0; game.begin(now,seedFor(Games::TIMING),slow); start(game,now);
    const uint8_t widths[]={5,3,2};
    for(uint8_t turn=0;turn<3;++turn) {
      EXPECT_EQ(turn,game.progress()); EXPECT_EQ(widths[turn],game.zoneWidth());
      EXPECT_FALSE(game.inZone());
      EXPECT_EQ(slow?700u:160u,game.interval());
      while(!game.inZone()) { now+=game.interval(); game.tick(now); }
      game.input(Games::ENTER,now);
      EXPECT_EQ(turn+1,game.progress());
    }
    EXPECT_EQ(Games::WON,game.phase());
  }
}
TEST(PetTraining, NewGamesDoNotImmediatelyRepeatAndCancellationDoesNotReroll) {
  Games game; game.begin(0,123,false); auto previous=game.game();
  game.cancel(); game.begin(100,456,false); EXPECT_EQ(previous,game.game());
  for(uint32_t i=0;i<100;++i) {
    game.finish(); game.begin(i*100,i+789,false);
    EXPECT_NE(previous,game.game()); previous=game.game();
  }
}
TEST(PetTraining, OneRetryUsesTheSameChallengeThenCloses) {
  Games game; uint32_t now=0;
  game.begin(now,seedFor(Games::ARROWS),false);
  uint8_t first=game.arrow(0); start(game,now);
  game.input((Games::Action)(Games::UP+(first+1)%4),now);
  EXPECT_EQ(Games::FAILED,game.phase()); EXPECT_FALSE(game.retryUsed());
  game.input(Games::ENTER,now);
  EXPECT_EQ(Games::INSTRUCTIONS,game.phase()); EXPECT_TRUE(game.retryUsed());
  EXPECT_EQ(first,game.arrow(0)); start(game,now);
  game.input((Games::Action)(Games::UP+(first+1)%4),now);
  EXPECT_EQ(Games::FAILED,game.phase()); game.input(Games::ENTER,now);
  EXPECT_FALSE(game.active());
}
TEST(PetTraining, ArrowMistakesAndTimingOutsideZoneFail) {
  for(auto id:{Games::ARROWS,Games::TIMING}) {
    Games game; uint32_t now=0; game.begin(now,seedFor(id),false); start(game,now);
    game.input(id==Games::ARROWS?(Games::Action)(Games::UP+(game.arrow(0)+1)%4):Games::ENTER,now);
    EXPECT_EQ(Games::FAILED,game.phase());
  }
}
TEST(PetTraining, BoxAndChangeWrongSelectionFail) {
  for(auto id:{Games::BOXES,Games::CHANGED_SHAPE}) {
    Games game; uint32_t now=0; game.begin(now,seedFor(id),false); start(game,now);
    while(game.selection()==game.target())
      game.input(game.selection()==2?Games::LEFT:Games::RIGHT,now);
    game.input(Games::ENTER,now);
    if(id==Games::BOXES) {
      EXPECT_EQ(Games::ANSWER,game.phase()); now+=game.interval(); game.tick(now);
    }
    EXPECT_EQ(Games::FAILED,game.phase());
  }
}
TEST(PetTraining, BoxesRevealHidePreviewEachSwapAndPauseBeforeChoosing) {
  for(bool slow:{false,true}) {
    Games game; uint32_t now=0; game.begin(0,seedFor(Games::BOXES),slow);
    uint8_t initial=game.target(); // target is assigned on start.
    game.input(Games::ENTER,now); initial=game.target();
    EXPECT_EQ(Games::SHOW,game.phase()); EXPECT_EQ(3000u,game.interval());
    now+=3000; game.tick(now); EXPECT_EQ(Games::BOX_HIDE,game.phase());
    now+=1000; game.tick(now); ASSERT_EQ(Games::BOX_HINT,game.phase());
    uint8_t expected=initial;
    for(uint8_t swap=0;swap<3;++swap) {
      EXPECT_EQ(swap,game.stage()); EXPECT_EQ(expected,game.target());
      uint8_t a=game.swap(swap*2), b=game.swap(swap*2+1);
      game.input(Games::LEFT,now); game.input(Games::ENTER,now);
      EXPECT_EQ(1,game.selection()); EXPECT_EQ(Games::BOX_HINT,game.phase());
      now+=game.interval(); game.tick(now);
      if(!slow) {
        ASSERT_EQ(Games::BOX_MOVE,game.phase());
        for(uint8_t frame=0;frame<4;++frame) {
          EXPECT_EQ(frame,game.boxFrame()); EXPECT_EQ(expected,game.target());
          now+=game.interval(); game.tick(now);
        }
      }
      if(expected==a) expected=b; else if(expected==b) expected=a;
      ASSERT_EQ(Games::BOX_PAUSE,game.phase()); EXPECT_EQ(expected,game.target());
      now+=game.interval(); game.tick(now);
      EXPECT_EQ(swap==2?Games::PLAY:Games::BOX_HINT,game.phase());
    }
    for(int i=0;i<5;++i) game.input(Games::LEFT,now);
    EXPECT_EQ(0,game.selection());
    for(int i=0;i<5;++i) game.input(Games::RIGHT,now);
    EXPECT_EQ(2,game.selection());
    while(game.selection()!=game.target()) game.input(Games::LEFT,now);
    game.input(Games::ENTER,now); ASSERT_EQ(Games::ANSWER,game.phase());
    game.input(Games::RIGHT,now); EXPECT_EQ(expected,game.selection());
    game.tick(now+game.interval()-1); EXPECT_EQ(Games::ANSWER,game.phase());
    now+=game.interval(); game.tick(now); EXPECT_EQ(Games::WON,game.phase());
  }
}
TEST(PetTraining, BoxRetryPreservesHidingPlaceAndSwapsAndRevealCanCancel) {
  Games game; uint32_t now=0; game.begin(0,seedFor(Games::BOXES),false);
  uint8_t swaps[6]; for(uint8_t i=0;i<6;++i) swaps[i]=game.swap(i);
  game.input(Games::ENTER,now); uint8_t initial=game.target();
  while(game.previewing()) { now+=game.interval(); game.tick(now); }
  while(game.selection()==game.target())
    game.input(game.selection()==2?Games::LEFT:Games::RIGHT,now);
  game.input(Games::ENTER,now); now+=game.interval(); game.tick(now);
  ASSERT_EQ(Games::FAILED,game.phase()); game.input(Games::ENTER,now); game.input(Games::ENTER,now);
  EXPECT_EQ(initial,game.target());
  for(uint8_t i=0;i<6;++i) EXPECT_EQ(swaps[i],game.swap(i));
  while(game.previewing()) { now+=game.interval(); game.tick(now); }
  game.input(Games::ENTER,now); ASSERT_EQ(Games::ANSWER,game.phase());
  game.input(Games::BACK,now); EXPECT_FALSE(game.active());
}
TEST(PetTraining, FoodMissesEventuallyFailWithoutFakeCatches) {
  Games game; uint32_t now=0; game.begin(now,seedFor(Games::FOOD),false); start(game,now);
  while(game.phase()==Games::PLAY) {
    if(game.selection()==game.target())
      game.input(game.selection()==Games::FOOD_LANES-1?Games::LEFT:Games::RIGHT,now);
    now+=game.interval(); game.tick(now);
  }
  EXPECT_EQ(Games::FAILED,game.phase()); EXPECT_EQ(0,game.progress());
}
TEST(PetTraining, FoodStartsInCentreAndStopsAtFiveLaneEdges) {
  Games game; uint32_t now=0; game.begin(0,seedFor(Games::FOOD),false); start(game,now);
  EXPECT_EQ(2,game.selection());
  for(int i=0;i<8;++i) game.input(Games::LEFT,now);
  EXPECT_EQ(0,game.selection());
  for(uint8_t lane=1;lane<5;++lane) {
    game.input(Games::RIGHT,now); EXPECT_EQ(lane,game.selection());
  }
  game.input(Games::RIGHT,now); EXPECT_EQ(4,game.selection());
  game.input(Games::LEFT,now); EXPECT_EQ(3,game.selection());
}
TEST(PetTraining, FoodUsesEveryLaneAndRetainsAllFiveDropsOnRetry) {
  bool seen[5]={}; unsigned checked=0;
  for(uint32_t seed=1;seed<=1000;++seed) {
    Games game; uint32_t now=0; game.begin(0,seed,false);
    if(game.game()!=Games::FOOD) continue;
    ++checked; start(game,now); uint8_t sequence[5];
    for(uint8_t drop=0;drop<5;++drop) {
      sequence[drop]=game.target(); ASSERT_LT(game.target(),5); seen[game.target()]=true;
      if(game.selection()==game.target())
        game.input(game.selection()==4?Games::LEFT:Games::RIGHT,now);
      for(int stage=0;stage<3;++stage) { now+=game.interval(); game.tick(now); }
    }
    ASSERT_EQ(Games::FAILED,game.phase()); game.input(Games::ENTER,now); start(game,now);
    EXPECT_EQ(2,game.selection());
    for(uint8_t drop=0;drop<5;++drop) {
      EXPECT_EQ(sequence[drop],game.target());
      if(game.selection()==game.target())
        game.input(game.selection()==4?Games::LEFT:Games::RIGHT,now);
      for(int stage=0;stage<3;++stage) { now+=game.interval(); game.tick(now); }
    }
    EXPECT_EQ(Games::FAILED,game.phase()); EXPECT_EQ(0,game.progress());
  }
  EXPECT_GT(checked,100u); for(bool lane:seen) EXPECT_TRUE(lane);
}
TEST(PetTraining, ChangedShapesRetainThreePositionWrapping) {
  for(auto id:{Games::CHANGED_SHAPE}) {
    Games game; uint32_t now=0; game.begin(0,seedFor(id),false); start(game,now);
    EXPECT_EQ(1,game.selection()); game.input(Games::LEFT,now); EXPECT_EQ(0,game.selection());
    game.input(Games::LEFT,now); EXPECT_EQ(2,game.selection());
    game.input(Games::RIGHT,now); EXPECT_EQ(0,game.selection());
  }
}
TEST(PetTraining, ChangedSymbolIsDifferentAndBoxesOnlySwapDistinctSlots) {
  for(auto id:{Games::BOXES,Games::CHANGED_SHAPE}) {
    Games game; uint32_t now=0; game.begin(now,seedFor(id),false);
    game.input(Games::ENTER,now);
    uint8_t original[3]={game.symbol(0),game.symbol(1),game.symbol(2)};
    if(id==Games::BOXES) for(int i=0;i<6;i+=2) EXPECT_NE(game.swap(i),game.swap(i+1));
    while(game.previewing()) { now+=game.interval(); game.tick(now); }
    if(id==Games::CHANGED_SHAPE) for(int i=0;i<3;++i) {
      if(i==game.target()) EXPECT_NE(original[i],game.symbol(i));
      else EXPECT_EQ(original[i],game.symbol(i));
    }
  }
}
TEST(PetTraining, ChangedShapesStayCoveredForThreeSecondsAndIgnoreSelection) {
  for(bool slow:{false,true}) for(uint32_t boot:{0u,0xfffffff0u}) {
    Games game; uint32_t now=boot;
    game.begin(now,seedFor(Games::CHANGED_SHAPE),slow); game.input(Games::ENTER,now);
    uint8_t original[3]={game.symbol(0),game.symbol(1),game.symbol(2)};
    now+=game.interval(); game.tick(now);
    ASSERT_EQ(Games::COVERED,game.phase()); EXPECT_EQ(3000u,game.interval());
    EXPECT_EQ(3000,game.refreshMs(now));
    game.input(Games::LEFT,now); game.input(Games::RIGHT,now); game.input(Games::ENTER,now+2999);
    EXPECT_EQ(Games::COVERED,game.phase()); EXPECT_EQ(1,game.selection());
    EXPECT_EQ(1,game.refreshMs(now+2999));
    game.tick(now+3000); now+=3000;
    ASSERT_EQ(Games::PLAY,game.phase());
    for(uint8_t i=0;i<3;++i) {
      if(i==game.target()) EXPECT_NE(original[i],game.symbol(i));
      else EXPECT_EQ(original[i],game.symbol(i));
    }
    uint8_t changed=game.symbol(game.target()), target=game.target();
    while(game.selection()==target) game.input(Games::RIGHT,now);
    game.input(Games::ENTER,now); ASSERT_EQ(Games::FAILED,game.phase());
    game.input(Games::ENTER,now); game.input(Games::ENTER,now);
    for(uint8_t i=0;i<3;++i) EXPECT_EQ(original[i],game.symbol(i));
    EXPECT_EQ(target,game.target());
    now+=game.interval(); game.tick(now); EXPECT_EQ(Games::COVERED,game.phase());
    now+=3000; game.tick(now); EXPECT_EQ(changed,game.symbol(target));
  }
}
TEST(PetTraining, CoveredPhaseCanCancelAndLatePreviewDoesNotSkipIt) {
  Games game; game.begin(0,seedFor(Games::CHANGED_SHAPE),false); game.input(Games::ENTER,0);
  game.tick(10000); ASSERT_EQ(Games::COVERED,game.phase());
  game.tick(12999); EXPECT_EQ(Games::COVERED,game.phase());
  game.input(Games::BACK,12999); EXPECT_FALSE(game.active());
  game.tick(13000); EXPECT_FALSE(game.active());
}
TEST(PetTraining, TimeoutsWrapSafelyAndDoNotFastForwardUnseenFrames) {
  Games game; uint32_t now=0xfffffff0UL;
  game.begin(now,seedFor(Games::TIMING),false); game.input(Games::ENTER,now);
  uint8_t first=game.marker();
  game.tick(200); EXPECT_EQ(first == 0 ? 1 : 7,game.marker()); // One visible step, not catch-up.
  game.tick(600); EXPECT_EQ(first == 0 ? 2 : 6,game.marker());
  game.tick(50000); EXPECT_EQ(Games::FAILED,game.phase());
  game.cancel(); EXPECT_FALSE(game.active()); game.tick(60000); EXPECT_FALSE(game.active());
}
TEST(PetTraining, InstructionAndFailureScreensDoNotAnimate) {
  Games game; game.begin(0,seedFor(Games::TIMING),false);
  EXPECT_EQ(5000,game.refreshMs(0));
  game.input(Games::ENTER,0); EXPECT_LE(game.refreshMs(0),160);
  game.input(Games::ENTER,0); EXPECT_EQ(5000,game.refreshMs(0));
}
int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
