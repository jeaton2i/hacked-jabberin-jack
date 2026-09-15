#include "test_pattern_face.h"

namespace {
constexpr uint16_t BARS[] = {RGB565_WHITE, RGB565_YELLOW, RGB565_CYAN,
                              RGB565_GREEN, RGB565_MAGENTA, RGB565_RED,
                              RGB565_BLUE,  RGB565_BLACK};
constexpr size_t BAR_COUNT = sizeof(BARS) / sizeof(BARS[0]);
} // namespace

void TestPatternFace::begin(Arduino_GFX *gfx) {
  _frame = 0;
}

void TestPatternFace::update() {
  _frame++;
}

void TestPatternFace::draw(Arduino_GFX *gfx) {
  int16_t barHeight = gfx->height() / BAR_COUNT;
  // Rotate which color starts each bar so it's obvious the loop is running,
  // not just that a static image is on screen.
  for (size_t i = 0; i < BAR_COUNT; i++) {
    uint16_t color = BARS[(i + _frame / 30) % BAR_COUNT];
    gfx->fillRect(0, i * barHeight, gfx->width(), barHeight, color);
  }
}
