#define FIRMWARE_ZEN_BUILD 1

#include <gtest/gtest.h>
#include <vector>

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
