#include "clock_face.h"

#include <string.h>

namespace {
constexpr int16_t kStripHeight = 16;

// Layout, in pixels: four digit boxes (H H : M M) plus a colon, centered
// on the panel. Sized so the layout's own corners stay within ~67px of
// center - comfortably inside the ~75px radius other faces target for the
// pumpkin's visible circular cutout (see docs/rp2040-display-pinout.md).
constexpr int16_t kDigitW = 24;
constexpr int16_t kDigitH = 46;
constexpr int16_t kSegThick = 6;
constexpr int16_t kGap = 5;
constexpr int16_t kColonW = 9;
constexpr int16_t kColonDotSize = kSegThick;

bool maskGet(const uint8_t *mask, int32_t index) {
  return (mask[index >> 3] >> (index & 7)) & 1;
}

void maskSet(uint8_t *mask, int32_t index) {
  mask[index >> 3] |= (1 << (index & 7));
}

uint16_t scaleColor(uint16_t color, uint8_t alpha) {
  if (alpha == 255) {
    return color;
  }
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = color & 0x1F;
  r = (uint16_t)r * alpha / 255;
  g = (uint16_t)g * alpha / 255;
  b = (uint16_t)b * alpha / 255;
  return (r << 11) | (g << 5) | b;
}

// Standard seven-segment encoding (bit0=a/top, bit1=b/top-right,
// bit2=c/bottom-right, bit3=d/bottom, bit4=e/bottom-left, bit5=f/top-left,
// bit6=g/middle) - the same segment-lettering/lit-pattern convention used
// on every seven-segment datasheet and calculator display, not anyone's
// creative work.
constexpr uint8_t kDigitSegments[10] = {
    0b0111111, // 0
    0b0000110, // 1
    0b1011011, // 2
    0b1001111, // 3
    0b1100110, // 4
    0b1101101, // 5
    0b1111101, // 6
    0b0000111, // 7
    0b1111111, // 8
    0b1101111, // 9
};
constexpr uint8_t kDashSegment = 0b1000000; // just the middle segment - the
                                           // "--:--" not-yet-synced glyph

// Is the point (lx, ly), in a digit box's own local coordinates, inside
// one of its lit segments?
bool digitPixelOn(int16_t lx, int16_t ly, uint8_t segMask) {
  int16_t half = kDigitH / 2;
  int16_t halfSeg = kSegThick / 2;
  // a - top
  if ((segMask & 0b0000001) && ly >= 0 && ly < kSegThick && lx >= kSegThick &&
      lx < kDigitW - kSegThick) {
    return true;
  }
  // g - middle
  if ((segMask & 0b1000000) && ly >= half - halfSeg && ly < half + halfSeg &&
      lx >= kSegThick && lx < kDigitW - kSegThick) {
    return true;
  }
  // d - bottom
  if ((segMask & 0b0001000) && ly >= kDigitH - kSegThick && ly < kDigitH &&
      lx >= kSegThick && lx < kDigitW - kSegThick) {
    return true;
  }
  // f - top-left
  if ((segMask & 0b0100000) && lx >= 0 && lx < kSegThick &&
      ly >= kSegThick && ly < half - halfSeg) {
    return true;
  }
  // b - top-right
  if ((segMask & 0b0000010) && lx >= kDigitW - kSegThick && lx < kDigitW &&
      ly >= kSegThick && ly < half - halfSeg) {
    return true;
  }
  // e - bottom-left
  if ((segMask & 0b0010000) && lx >= 0 && lx < kSegThick &&
      ly >= half + halfSeg && ly < kDigitH - kSegThick) {
    return true;
  }
  // c - bottom-right
  if ((segMask & 0b0000100) && lx >= kDigitW - kSegThick && lx < kDigitW &&
      ly >= half + halfSeg && ly < kDigitH - kSegThick) {
    return true;
  }
  return false;
}

constexpr int16_t kLayoutWidth = 4 * kDigitW + kColonW + 4 * kGap;
constexpr int16_t kLayoutHeight = kDigitH;
} // namespace

void ClockFace::begin(Arduino_GFX *gfx) {
  _gfx = gfx;
  _width = gfx->width();
  _height = gfx->height();
  refreshDisplay();
}

void ClockFace::setSyncedTime(uint8_t hour, uint8_t minute, uint8_t second) {
  _synced = true;
  _syncedHour = hour;
  _syncedMinute = minute;
  _syncedSecond = second;
  _syncMillis = millis();
  if (_gfx) {
    refreshDisplay();
  }
}

void ClockFace::update() {
  _flicker.update();
  if (_gfx) {
    refreshDisplay();
  }
}

void ClockFace::setUse12Hour(bool use12Hour) {
  if (_use12Hour == use12Hour) {
    return;
  }
  _use12Hour = use12Hour;
  _lastDisplayedHour = 255; // force refreshDisplay() to actually re-render,
                           // even though the underlying time hasn't moved
  if (_gfx) {
    refreshDisplay();
  }
}

void ClockFace::refreshDisplay() {
  if (!_synced) {
    if (!_everRendered) {
      render(0, 0, true);
      _everRendered = true;
    }
    return;
  }

  unsigned long elapsedSec = (millis() - _syncMillis) / 1000UL;
  unsigned long totalSec =
      (unsigned long)_syncedHour * 3600UL + (unsigned long)_syncedMinute * 60UL +
      _syncedSecond + elapsedSec;
  totalSec %= 86400UL; // wrap a day - see class comment, no date tracking here
  uint8_t hour24 = (uint8_t)(totalSec / 3600UL);
  uint8_t minute = (uint8_t)((totalSec % 3600UL) / 60UL);
  uint8_t second = (uint8_t)(totalSec % 60UL);
  bool colonOn = (second % 2) == 0;

  uint8_t hour = hour24;
  if (_use12Hour) {
    hour = hour24 % 12;
    if (hour == 0) {
      hour = 12; // midnight/noon - never displayed as "0"
    }
  }

  if (_everRendered && hour == _lastDisplayedHour &&
      minute == _lastDisplayedMinute && colonOn == _lastColonOn) {
    return; // nothing a viewer would actually see has changed
  }
  render(hour, minute, colonOn);
  _lastDisplayedHour = hour;
  _lastDisplayedMinute = minute;
  _lastColonOn = colonOn;
  _everRendered = true;
}

void ClockFace::render(uint8_t displayHour, uint8_t displayMinute,
                       bool colonOn) {
  int32_t maskBytes = ((int32_t)_width * _height + 7) / 8;
  if (!_renderedMask) {
    _renderedMask = new (std::nothrow) uint8_t[maskBytes];
    if (!_renderedMask) {
      return; // leave whatever was already on screen alone
    }
  }
  memset(_renderedMask, 0, maskBytes);

  uint8_t segs[4];
  if (_synced) {
    // 12-hour mode leaves the leading hour digit dark (no lit segments)
    // rather than showing "0" for a single-digit hour - see setUse12Hour().
    segs[0] = (_use12Hour && displayHour < 10) ? 0
                                              : kDigitSegments[displayHour / 10];
    segs[1] = kDigitSegments[displayHour % 10];
    segs[2] = kDigitSegments[displayMinute / 10];
    segs[3] = kDigitSegments[displayMinute % 10];
  } else {
    segs[0] = segs[1] = segs[2] = segs[3] = kDashSegment;
  }

  int16_t startX = _width / 2 - kLayoutWidth / 2;
  int16_t startY = _height / 2 - kLayoutHeight / 2;
  int16_t digitX[4] = {
      startX,
      (int16_t)(startX + kDigitW + kGap),
      (int16_t)(startX + 2 * kDigitW + kGap + kColonW + 2 * kGap),
      (int16_t)(startX + 3 * kDigitW + kColonW + 4 * kGap),
  };
  int16_t colonX = (int16_t)(startX + 2 * kDigitW + 2 * kGap);
  int16_t colonDotUpperY = startY + kLayoutHeight * 3 / 10;
  int16_t colonDotLowerY = startY + kLayoutHeight * 7 / 10 - kColonDotSize;

  for (uint8_t d = 0; d < 4; d++) {
    for (int16_t ly = 0; ly < kDigitH; ly++) {
      int16_t y = startY + ly;
      if (y < 0 || y >= _height) {
        continue;
      }
      for (int16_t lx = 0; lx < kDigitW; lx++) {
        int16_t x = digitX[d] + lx;
        if (x < 0 || x >= _width) {
          continue;
        }
        if (digitPixelOn(lx, ly, segs[d])) {
          // Mirrored, same convention as every other content-drawing face
          // (see TextFace/StaticImageFace) - this panel needs it.
          maskSet(_renderedMask, (int32_t)y * _width + (_width - 1 - x));
        }
      }
    }
  }

  if (colonOn) {
    for (int16_t ly = 0; ly < kColonDotSize; ly++) {
      for (int16_t lx = 0; lx < kColonDotSize; lx++) {
        int16_t x = colonX + (kColonW - kColonDotSize) / 2 + lx;
        if (x < 0 || x >= _width) {
          continue;
        }
        int16_t mirroredX = _width - 1 - x;
        int16_t yUpper = colonDotUpperY + ly;
        int16_t yLower = colonDotLowerY + ly;
        if (yUpper >= 0 && yUpper < _height) {
          maskSet(_renderedMask, (int32_t)yUpper * _width + mirroredX);
        }
        if (yLower >= 0 && yLower < _height) {
          maskSet(_renderedMask, (int32_t)yLower * _width + mirroredX);
        }
      }
    }
  }
}

void ClockFace::draw(Arduino_GFX *gfx) {
  if (!_renderedMask) {
    return;
  }
  uint16_t color = _flicker.color();

  static uint16_t stripBuffer[220 * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < _height; rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > _height) {
      stripHeight = _height - rowOffset;
    }
    for (int16_t sy = 0; sy < stripHeight; sy++) {
      int16_t y = rowOffset + sy;
      for (int16_t x = 0; x < _width; x++) {
        bool ink = maskGet(_renderedMask, (int32_t)y * _width + x);
        stripBuffer[sy * _width + x] = ink ? color : RGB565_BLACK;
      }
    }
    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, _width, stripHeight);
  }
}
