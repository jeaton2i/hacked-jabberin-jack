#include "display.h"

namespace {
constexpr int8_t PIN_RD = 8;
constexpr int8_t PIN_WR = 9;
constexpr int8_t PIN_DC = 10;
constexpr int8_t PIN_RST = 11;
constexpr int8_t PIN_CS = 12;
} // namespace

void Display::begin() {
  pinMode(PIN_RD, OUTPUT);
  digitalWrite(PIN_RD, HIGH);

  Arduino_DataBus *bus =
      new Arduino_RPiPicoPAR8(PIN_DC, PIN_CS, PIN_WR, PIN_RD);
  _gfx = new Arduino_ILI9225(bus, PIN_RST, /*rotation=*/3);
  _gfx->begin();
  _gfx->fillScreen(RGB565_BLACK);
}
