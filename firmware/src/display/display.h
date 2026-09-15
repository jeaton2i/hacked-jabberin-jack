#pragma once

#include <Arduino_GFX_Library.h>

// Wraps whatever panel ends up on the other side of the projector ribbon
// adaptor so the rest of the code only depends on this interface, not on
// the specific Arduino_GFX driver/pinout.
class Display {
public:
  void begin();
  Arduino_GFX *gfx() { return _gfx; }

  static constexpr int16_t WIDTH = 240;
  static constexpr int16_t HEIGHT = 240;

private:
  Arduino_GFX *_gfx = nullptr;
};
