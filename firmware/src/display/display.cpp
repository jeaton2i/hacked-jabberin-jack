#include "display.h"

// PLACEHOLDER WIRING: this is an ST7789 over hardware SPI purely so the
// project compiles and runs a test pattern out of the box. Replace the pin
// numbers and Arduino_GFX driver class once the panel behind the projector
// ribbon adaptor has been identified (chip, resolution, and which clone-board
// GPIOs it's wired to).
namespace {
constexpr int8_t PIN_DC = 20;
constexpr int8_t PIN_CS = 17;
constexpr int8_t PIN_RST = 21;
} // namespace

void Display::begin() {
  Arduino_DataBus *bus = new Arduino_HWSPI(PIN_DC, PIN_CS);
  _gfx = new Arduino_ST7789(bus, PIN_RST, /*rotation=*/0, /*ips=*/true,
                             WIDTH, HEIGHT);
  _gfx->begin();
  _gfx->fillScreen(RGB565_BLACK);
}
