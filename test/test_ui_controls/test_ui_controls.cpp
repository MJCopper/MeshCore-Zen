#define FIRMWARE_ZEN_BUILD 1

#include <gtest/gtest.h>
#include <vector>
#include "../../examples/companion_radio/zen-overlay/app/zen/SignalIndicator.h"

#include <helpers/ui/ZenUIScreen.h>
#include "../../examples/companion_radio/zen-overlay/app/ui-new/icons.h"
#include "../../examples/companion_radio/zen-overlay/app/ui-new/ScreenHistory.h"
#include "../../examples/companion_radio/zen-overlay/app/ui-new/UIFramework.h"

class RecordingDisplay : public ZenDisplayDriver {
public:
  struct Bounds { int x, y, w, h; };
  std::vector<Bounds> draws;
  int cursor_x = 0, cursor_y = 0;
  int char_w, line_h;
  bool single_font = false;
  bool isSingleFont() const override { return single_font; }

  RecordingDisplay(int w, int h, int cw = 6, int lh = 8)
      : ZenDisplayDriver(w, h), char_w(cw), line_h(lh) {}
  bool isOn() override { return true; }
  void turnOn() override {}
  void turnOff() override {}
  void clear() override {}
  void startFrame(ColorVal = 0) override { draws.clear(); }
  void setTextSize(int) override {}
  void setColor(ColorVal) override {}
  void setCursor(int x, int y) override { cursor_x = x; cursor_y = y; }
  void print(const char* text) override {
    draws.push_back({cursor_x, cursor_y, (int)strlen(text) * char_w, line_h});
  }
  void fillRect(int x, int y, int w, int h) override { draws.push_back({x, y, w, h}); }
  void drawRect(int x, int y, int w, int h) override { draws.push_back({x, y, w, h}); }
  void drawXbm(int x, int y, const uint8_t*, int w, int h) override { draws.push_back({x, y, w, h}); }
  uint16_t getTextWidth(const char* text) override { return strlen(text) * char_w; }
  int getCharWidth() const override { return char_w; }
  int getLineHeight() const override { return line_h; }
  void endFrame() override {}
  bool inBounds() const {
    for (const Bounds& b : draws)
      if (b.x < 0 || b.y < 0 || b.w < 0 || b.h < 0 ||
          b.x + b.w > width() || b.y + b.h > height()) return false;
    return true;
  }
};

class TestScreen : public ZenUIScreen {
public:
  int shown = 0, hidden = 0;
  int render(ZenDisplayDriver&) override { return UI_REFRESH_STATIC_MS; }
  void onShow() override { shown++; }
  void onHide() override { hidden++; }
};

TEST(UIControls, MenuSelectionWrapsInBothDirections) {
  EXPECT_EQ(wrapSelection(0, 4, -1), 3);
  EXPECT_EQ(wrapSelection(3, 4, 1), 0);
  EXPECT_EQ(wrapSelection(1, 4, 1), 2);
  EXPECT_EQ(wrapSelection(2, 4, -1), 1);
  EXPECT_EQ(wrapSelection(3, 0, 1), 0);
}

TEST(UIControls, FrameworkMenuMovementUsesTheSharedWrapContract) {
  int selected = 0;
  EXPECT_TRUE(zenui::moveWrapped(KEY_UP, 3, selected));
  EXPECT_EQ(2, selected);
  EXPECT_TRUE(zenui::moveWrapped(KEY_DOWN, 3, selected));
  EXPECT_EQ(0, selected);
  EXPECT_FALSE(zenui::moveWrapped(KEY_ENTER, 3, selected));
}

TEST(UIControls, MenuModelKeepsLabelsActionsAndAvailabilityTogether) {
  static const zenui::MenuItem items[] = {
    { "First", 7, true }, { "Second", 9, false },
  };
  zenui::MenuModel<2> model(items);
  EXPECT_EQ(2, model.count());
  EXPECT_STREQ("First", model.label(0));
  EXPECT_EQ(9, model.action(1));
  EXPECT_FALSE(model.enabled(1));
}

TEST(UILayout, EmbeddedListsScrollAndRemainInsideOLED) {
  RecordingDisplay display(128, 64);
  int scroll = 0, reserve_seen = 0;
  int visible = drawListAt(display, 20, 9, 8, scroll,
      [&](int, int y, bool selected, int reserve) {
    reserve_seen = reserve;
    drawRowSelection(display, y, selected, reserve);
  });
  EXPECT_EQ(visible, 4);
  EXPECT_EQ(scroll, 5);
  EXPECT_GT(reserve_seen, 0);
  EXPECT_TRUE(display.inBounds());
}

TEST(UILayout, EmbeddedListsRemainInsideEinkGeometry) {
  RecordingDisplay display(250, 122, 12, 16);
  int scroll = 0;
  int visible = drawListAt(display, 42, 12, 11, scroll,
      [&](int, int y, bool selected, int reserve) {
    drawRowSelection(display, y, selected, reserve);
  });
  EXPECT_EQ(visible, 4);
  EXPECT_EQ(scroll, 8);
  EXPECT_TRUE(display.inBounds());
}

TEST(UILayout, LabelValueRowsConstrainLongValues) {
  RecordingDisplay display(128, 64);
  drawLabelValueRow(display, 20, "Pos", "-33.1234 151.1234");
  EXPECT_TRUE(display.inBounds());
}

TEST(UINavigation, FixedHistoryRetainsMostRecentDestinations) {
  TestScreen a, b, c;
  ScreenHistory<2> history;
  history.push(&a);
  history.push(&b);
  history.push(&c);
  EXPECT_EQ(history.size(), 2);
  EXPECT_EQ(history.pop(), &c);
  EXPECT_EQ(history.pop(), &b);
  EXPECT_EQ(history.pop(), nullptr);
}

TEST(UINavigation, TransitionRunsSymmetricLifecycleOnce) {
  TestScreen a, b;
  ZenUIScreen* current = nullptr;
  EXPECT_TRUE(ScreenTransition::apply(current, &a));
  EXPECT_EQ(a.shown, 1);
  EXPECT_TRUE(ScreenTransition::apply(current, &b));
  EXPECT_EQ(a.hidden, 1);
  EXPECT_EQ(b.shown, 1);
  EXPECT_FALSE(ScreenTransition::apply(current, &b));
  EXPECT_EQ(b.shown, 1);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(UIControls, SignalIndicatorFitsEveryDisplayAndShowsOnlyActiveBars) {
  // OLED, portrait E-ink, landscape E-ink; default-font fallback included.
  const int configs[][4] = {{128,64,9,1}, {122,250,9,1}, {250,122,18,1}, {128,64,8,0}};
  for (const auto& c : configs) {
    RecordingDisplay d(c[0], c[1], 6, c[2]);
    d.single_font = c[3];
    const int icon_height = c[2] - (c[3] ? 2 : 0);
    for (unsigned bars = 0; bars <= 4; ++bars) {
      d.draws.clear();
      EXPECT_EQ(zen::SignalIndicator::draw(d, bars), c[0] - 12);
      ASSERT_EQ(d.draws.size(), bars ? bars : 12);
      EXPECT_TRUE(d.inBounds());
      int previous_height = 0;
      for (const auto& rect : d.draws) {
        EXPECT_GE(rect.x, c[0] - 12);
        EXPECT_LE(rect.x + rect.w, c[0]);
        EXPECT_LE(rect.y + rect.h, icon_height);
        if (bars) {
          EXPECT_EQ(rect.w, 2);
          EXPECT_GT(rect.h, previous_height);
          EXPECT_EQ(rect.y + rect.h, icon_height);
          previous_height = rect.h;
        }
      }
    }
  }
}

#include "../../examples/companion_radio/zen-overlay/app/zen/BatteryIndicator.h"

TEST(UIControls, BatteryStatesFitEveryModeAndKeepSignalAndNameSpace) {
  using Charge = zen::BatteryChargeState;
  const int configs[][4] = {{128,64,9,1}, {122,250,9,1}, {250,122,18,1}};
  for (const auto& c : configs) {
    RecordingDisplay d(c[0], c[1], c[3] && c[2] == 18 ? 12 : 6, c[2]);
    d.single_font = c[3];
    const int right = c[0] - zen::SignalIndicator::WIDTH - 2;
    for (unsigned mode = 0; mode < 3; ++mode) {
      for (int mv : {0, 3300, 3800, 4200}) {
        int normal_left = 0;
        for (auto state : {Charge::BATTERY, Charge::EXTERNAL_UNKNOWN, Charge::CHARGING, Charge::COMPLETE}) {
          d.draws.clear();
          const int left = zen::BatteryIndicator::draw(d, mv, mode, right, state);
          ASSERT_TRUE(d.inBounds());
          // Same reservation used by the status-icon priority loop.
          EXPECT_GE(left, 5*d.getCharWidth());
          for (const auto& rect : d.draws) {
            EXPECT_GE(rect.x, left);
            EXPECT_LE(rect.x + rect.w, right);
            EXPECT_LE(rect.y + rect.h, d.getLineHeight());
          }
          if (state == Charge::BATTERY) normal_left = left;
          else if (mode == 0) EXPECT_EQ(left, normal_left);
          else EXPECT_EQ(normal_left-left, c[2] == 18 ? 12 : 7);
        }
      }
    }
  }
}

class BatteryRasterDisplay : public RecordingDisplay {
public:
  std::vector<int> pixels;
  int color=LIGHT;
  BatteryRasterDisplay(int w,int h,int lh=9) : RecordingDisplay(w,h,6,lh),pixels(w*h) { single_font=true; }
  void setColor(ColorVal c) override { color=c; }
  void fillRect(int x,int y,int w,int h) override {
    RecordingDisplay::fillRect(x,y,w,h);
    for (int row=y;row<y+h;++row) for (int col=x;col<x+w;++col)
      if (row>=0 && row<height() && col>=0 && col<width()) pixels[row*width()+col]=color;
  }
  void drawRect(int x,int y,int w,int h) override {
    RecordingDisplay::drawRect(x,y,w,h);
    for (int row=y;row<y+h;++row) for (int col=x;col<x+w;++col)
      if ((row==y || row==y+h-1 || col==x || col==x+w-1) &&
          row>=0 && row<height() && col>=0 && col<width()) pixels[row*width()+col]=color;
  }
  int at(int x,int y) const { return pixels[y*width()+x]; }
};

TEST(UIControls, ChargingRetainsFillAndBlinkOffMatchesBatteryOnEveryDisplay) {
  using Charge=zen::BatteryChargeState;
  const int configs[][3]={{128,64,9},{122,250,9},{250,122,18}};
  for (const auto& c:configs) for (int mv : {3450,3650,3800,4100}) {
    BatteryRasterDisplay normal(c[0],c[1],c[2]),off(c[0],c[1],c[2]),on(c[0],c[1],c[2]);
    normal.setColor(BatteryRasterDisplay::DARK);
    zen::BatteryIndicator::draw(normal,mv,0,c[0]-14,Charge::BATTERY);
    zen::BatteryIndicator::draw(off,mv,0,c[0]-14,Charge::CHARGING,false);
    zen::BatteryIndicator::draw(on,mv,0,c[0]-14,Charge::CHARGING,true);
    EXPECT_EQ(normal.pixels,off.pixels); EXPECT_NE(off.pixels,on.pixels);
    EXPECT_TRUE(on.inBounds()); EXPECT_EQ(on.color,BatteryRasterDisplay::LIGHT);
    EXPECT_EQ(on.draws[0].y,0); EXPECT_EQ(on.draws[0].h,c[2]-2);
    EXPECT_EQ(off.draws[0].y,0); EXPECT_EQ(off.draws[0].h,c[2]-2);
  }
}
TEST(UIControls, BoltHasWhiteInteriorBlackOutlineAndVisibleLevelBesideIt) {
  BatteryRasterDisplay d(128,64);
  int left=zen::BatteryIndicator::draw(d,3800,0,100,zen::BatteryChargeState::CHARGING,true);
  EXPECT_EQ(d.at(left+6,2),1); // white bolt
  EXPECT_EQ(d.at(left+7,2),1); // centred white bolt
  EXPECT_EQ(d.at(left+5,2),0); // outline erases part of the white level bar
  EXPECT_EQ(d.at(left+2,2),1); // visible level
  EXPECT_EQ(d.at(left,2),1); EXPECT_EQ(d.at(left+13,2),1); // intact shell
  EXPECT_EQ(d.color,BatteryRasterDisplay::LIGHT);
}
TEST(UIControls, CompleteUsesRegularFullBarAndWhiteOutlinedTick) {
  BatteryRasterDisplay low(128,64),high(128,64);
  int left=zen::BatteryIndicator::draw(low,3300,0,100,zen::BatteryChargeState::COMPLETE);
  zen::BatteryIndicator::draw(high,4200,0,100,zen::BatteryChargeState::COMPLETE);
  EXPECT_EQ(low.pixels,high.pixels);
  EXPECT_EQ(low.at(left+1,1),0); // regular black padding, not solid white
  EXPECT_EQ(low.at(left+2,2),1); EXPECT_EQ(low.at(left+11,2),1); // full level bar
  EXPECT_EQ(low.at(left+9,2),1); // centred white tick
  EXPECT_EQ(low.at(left+8,2),0); // black outline
  EXPECT_EQ(low.color,BatteryRasterDisplay::LIGHT);
}
TEST(UIControls, ChargingPhaseChangesEverySecondAndAlignsRefreshDeadline) {
  using Indicator=zen::BatteryIndicator; using Charge=zen::BatteryChargeState;
  EXPECT_TRUE(Indicator::boltVisible(0)); EXPECT_TRUE(Indicator::boltVisible(999));
  EXPECT_FALSE(Indicator::boltVisible(1000)); EXPECT_FALSE(Indicator::boltVisible(1999));
  EXPECT_TRUE(Indicator::boltVisible(2000));
  EXPECT_EQ(Indicator::refreshDelay(Charge::CHARGING,0),1000u);
  EXPECT_EQ(Indicator::refreshDelay(Charge::CHARGING,999),1u);
  EXPECT_EQ(Indicator::refreshDelay(Charge::CHARGING,1000),1000u);
  for (auto state : {Charge::BATTERY,Charge::COMPLETE,Charge::EXTERNAL_UNKNOWN})
    EXPECT_EQ(Indicator::refreshDelay(state,999),0u);
  for (int mode : {1,2}) {
    BatteryRasterDisplay on(128,64),off(128,64);
    int a=Indicator::draw(on,3800,mode,100,Charge::CHARGING,true);
    int b=Indicator::draw(off,3800,mode,100,Charge::CHARGING,false);
    EXPECT_EQ(a,b); EXPECT_NE(on.pixels,off.pixels);
    EXPECT_EQ(on.draws[0].x,off.draws[0].x);
  }
}

TEST(UIControls, UnknownTriangleContainsSmallXAndStaysHollowOnEveryDisplay) {
  const int configs[][3]={{128,64,9},{122,250,9},{250,122,18}};
  for (const auto& c:configs) {
    BatteryRasterDisplay d(c[0],c[1],c[2]);
    int left=zen::SignalIndicator::draw(d,0,true);
    EXPECT_EQ(left,c[0]-12); EXPECT_TRUE(d.inBounds());
    const int bottom=c[2]-1,cy=2*bottom/3;
    for (int x=0;x<12;++x) {
      EXPECT_EQ(d.at(left+x,bottom),1); // flat bottom
      EXPECT_EQ(d.at(left+x,bottom*(11-x)/11),1); // rising diagonal
    }
    for (int y=0;y<c[2];++y) EXPECT_EQ(d.at(left+11,y),1); // vertical right edge
    for (int i=0;i<3;++i) {
      EXPECT_EQ(d.at(left+7+i,cy-1+i),1);
      EXPECT_EQ(d.at(left+9-i,cy-1+i),1);
    }
    EXPECT_EQ(d.at(left+8,cy-1),0); // hollow gap between X's top corners
    EXPECT_EQ(d.at(left+4,bottom-1),0); // triangle is not filled
    for (const auto& r:d.draws) {
      EXPECT_GE(r.x,left); EXPECT_LE(r.x+r.w,c[0]);
      EXPECT_LE(r.y+r.h,c[2]);
    }
    BatteryRasterDisplay cross(c[0],c[1],c[2]),bars(c[0],c[1],c[2]);
    zen::SignalIndicator::draw(cross,0,false);
    zen::SignalIndicator::draw(bars,4,false);
    EXPECT_NE(d.pixels,cross.pixels); EXPECT_NE(d.pixels,bars.pixels);
  }
}

TEST(UIControls, BatteryPixelsMatchApprovedDesignShiftedUpOnePixel) {
  using Charge=zen::BatteryChargeState;
  struct Example { int mv; Charge::State state; const char* rows[9]; };
  const Example examples[] = {
    {3800,Charge::BATTERY,{
      "0000000000000000",
      "1111111111111100",
      "1000000000000100",
      "1011111000000111",
      "1011111000000111",
      "1011111000000111",
      "1000000000000100",
      "1111111111111100",
      "0000000000000000",
    }},
    {3800,Charge::CHARGING,{
      "0000000000000000",
      "1111110001111100",
      "1000000100000100",
      "1011001100000111",
      "1011011111000111",
      "1011000010000111",
      "1000000100000100",
      "1111110001111100",
      "0000000000000000",
    }},
    {4200,Charge::BATTERY,{
      "0000000000000000",
      "1111111111111100",
      "1000000000000100",
      "1011111111110111",
      "1011111111110111",
      "1011111111110111",
      "1000000000000100",
      "1111111111111100",
      "0000000000000000",
    }},
    {4150,Charge::COMPLETE,{
      "0000000000000000",
      "1111111111111100",
      "1000000000000100",
      "1011111001010111",
      "1011000010010111",
      "1011010100110111",
      "1000001000000100",
      "1111100011111100",
      "0000000000000000",
    }},
  };
  for (const auto& e:examples) {
    BatteryRasterDisplay d(128,64);
    const int left=zen::BatteryIndicator::draw(d,e.mv,0,100,e.state,true);
    ASSERT_FALSE(d.draws.empty());
    EXPECT_EQ(d.draws[0].y,0); EXPECT_EQ(d.draws[0].h,7); // original shell height
    EXPECT_EQ(d.draws[0].w,14);
    for (int y=0;y<9;++y) for (int x=0;x<16;++x)
      EXPECT_EQ(d.at(left+x,y),y+1<9 ? e.rows[y+1][x]-'0' : 0) << "state=" << e.state << " x=" << x << " y=" << y;
  }
}
