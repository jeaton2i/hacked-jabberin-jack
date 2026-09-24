#include "tropical_face.h"

#include <math.h>
#include <string.h>

namespace {
constexpr int16_t kPanelWidth = 220;
constexpr int16_t kPanelHeight = 176;
constexpr int16_t kStripHeight = 16;
constexpr float kPi = 3.14159265359f;

// Scene layout, all in panel pixels. Kept close to the panel's visible
// circular area (centered ~(110, 88), radius ~78 - see the bullseye test
// pattern) rather than filling the whole rectangle.
constexpr float kCrownX = 108.0f, kCrownY = 76.0f; // top of the trunk
constexpr float kBaseX = 96.0f, kBaseY = 142.0f;   // trunk's foot

// Fronds: wide, blunt-tipped wedges - no sun (it just blurred into the
// crown) and no mid-frond bend (a thin bent blade read as a bony finger,
// not a leaf; a wide straight one doesn't need the bend to look natural).
constexpr int kNumFronds = 5;
constexpr float kFrondSpreadDeg = 155.0f; // full fan angle
constexpr float kSwayAmpDeg = 8.0f;       // how far the wind rocks each frond
constexpr float kFrondLen = 43.0f;
constexpr float kFrondBaseWidth = 24.0f; // already includes a small gap so
                                         // neighboring fronds don't fully
                                         // merge into one solid mass
constexpr float kFrondTipWidthFrac = 0.48f; // tip width as a fraction of
                                            // the base - blunt, not a spike

constexpr int kNumWaveLines = 3;
constexpr float kWaveBaseY[kNumWaveLines] = {124.0f, 135.0f, 146.0f};
constexpr float kWaveThickness = 6.0f;
constexpr float kWaveAmplitude = 3.5f;
constexpr float kWaveFreq = 2.0f * kPi / 32.0f;
constexpr int16_t kWaveXStart = 18;
constexpr int16_t kWaveXEnd = kPanelWidth - 18;

int16_t clampInt(int16_t v, int16_t lo, int16_t hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

// Fills stripBuffer wherever (x, y) in the given box (clipped to this
// strip and the panel) satisfies test(x, y). Every shape below is small
// relative to the panel, so bounding the test to its own box (rather than
// scanning the whole strip) is what keeps this cheap per frame.
template <typename PixelTest>
void paintRegion(uint16_t *stripBuffer, int16_t rowOffset,
                 int16_t stripHeight, int16_t xStart, int16_t xEnd,
                 int16_t yStart, int16_t yEnd, uint16_t color,
                 PixelTest test) {
  yStart = clampInt(yStart, rowOffset, rowOffset + stripHeight - 1);
  yEnd = clampInt(yEnd, rowOffset, rowOffset + stripHeight - 1);
  xStart = clampInt(xStart, 0, kPanelWidth - 1);
  xEnd = clampInt(xEnd, 0, kPanelWidth - 1);
  for (int16_t y = yStart; y <= yEnd; y++) {
    for (int16_t x = xStart; x <= xEnd; x++) {
      if (test(x, y)) {
        stripBuffer[(y - rowOffset) * kPanelWidth + x] = color;
      }
    }
  }
}

// Integer coordinates and integer cross-products, not float: this is the
// hot per-pixel path (called for every candidate pixel of every frond/
// trunk shape, every frame), and the RP2040 has no hardware FPU - the
// eye-look face's original ~4fps bug came from exactly this kind of
// per-pixel float math. Vertices are rounded to int16_t once per shape
// per frame (see draw()), not per pixel.
bool inTriangle(int16_t x, int16_t y, int16_t x0, int16_t y0, int16_t x1,
                int16_t y1, int16_t x2, int16_t y2) {
  int32_t d1 = (int32_t)(x - x1) * (y0 - y1) - (int32_t)(x0 - x1) * (y - y1);
  int32_t d2 = (int32_t)(x - x2) * (y1 - y2) - (int32_t)(x1 - x2) * (y - y2);
  int32_t d3 = (int32_t)(x - x0) * (y2 - y0) - (int32_t)(x2 - x0) * (y - y0);
  bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
  bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
  return !(hasNeg && hasPos);
}

int16_t minOf3(int16_t a, int16_t b, int16_t c) {
  int16_t m = a < b ? a : b;
  return m < c ? m : c;
}
int16_t maxOf3(int16_t a, int16_t b, int16_t c) {
  int16_t m = a > b ? a : b;
  return m > c ? m : c;
}

void paintTriangle(uint16_t *stripBuffer, int16_t rowOffset,
                   int16_t stripHeight, int16_t x0, int16_t y0, int16_t x1,
                   int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  int16_t xMin = minOf3(x0, x1, x2), xMax = maxOf3(x0, x1, x2);
  int16_t yMin = minOf3(y0, y1, y2), yMax = maxOf3(y0, y1, y2);
  paintRegion(stripBuffer, rowOffset, stripHeight, xMin, xMax, yMin, yMax,
             color, [&](int16_t x, int16_t y) {
               return inTriangle(x, y, x0, y0, x1, y1, x2, y2);
             });
}

// Convex quad given in perimeter order, split along one diagonal.
void paintQuad(uint16_t *stripBuffer, int16_t rowOffset, int16_t stripHeight,
               int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
               int16_t y2, int16_t x3, int16_t y3, uint16_t color) {
  paintTriangle(stripBuffer, rowOffset, stripHeight, x0, y0, x1, y1, x2, y2,
               color);
  paintTriangle(stripBuffer, rowOffset, stripHeight, x0, y0, x2, y2, x3, y3,
               color);
}

// One traveling wave line: computed column by column since each column's
// band center is a different y (so it doesn't fit the row-major
// paintRegion helper above).
void paintWaveLine(uint16_t *stripBuffer, int16_t rowOffset,
                   int16_t stripHeight, float baseY, float phase,
                   uint16_t color) {
  int16_t stripTop = rowOffset;
  int16_t stripBottom = rowOffset + stripHeight - 1;
  for (int16_t x = kWaveXStart; x <= kWaveXEnd; x++) {
    float yc = baseY + kWaveAmplitude * sinf(x * kWaveFreq + phase);
    float bandTop = yc - kWaveThickness / 2.0f;
    float bandBottom = yc + kWaveThickness / 2.0f;
    if (bandBottom < stripTop || bandTop > stripBottom) {
      continue; // this column's band doesn't reach this strip
    }
    int16_t y0 = clampInt((int16_t)floorf(bandTop), stripTop, stripBottom);
    int16_t y1 = clampInt((int16_t)ceilf(bandBottom), stripTop, stripBottom);
    for (int16_t y = y0; y <= y1; y++) {
      stripBuffer[(y - rowOffset) * kPanelWidth + x] = color;
    }
  }
}
} // namespace

void TropicalFace::begin(Arduino_GFX *gfx) { gfx->fillScreen(RGB565_BLACK); }

void TropicalFace::update() {
  _flicker.update();
  constexpr float kTwoPi = 2.0f * kPi;
  _windPhase += 0.025f;
  if (_windPhase >= kTwoPi) {
    _windPhase -= kTwoPi;
  }
  _wavePhase += 0.05f;
  if (_wavePhase >= kTwoPi) {
    _wavePhase -= kTwoPi;
  }
}

void TropicalFace::draw(Arduino_GFX *gfx) {
  uint16_t color = _flicker.color();

  // Each frond's current (swaying) shape, computed once per frame in
  // float (cheap - a couple of trig calls per frond, not per pixel) and
  // rounded to int16_t here so every pixel test below is pure integer
  // math: a single wide wedge from the crown to a blunt tip.
  struct Frond {
    int16_t crownLeftX, crownLeftY, crownRightX, crownRightY;
    int16_t tipLeftX, tipLeftY, tipRightX, tipRightY;
  };
  Frond fronds[kNumFronds];
  for (int i = 0; i < kNumFronds; i++) {
    float restDeg =
        -kFrondSpreadDeg / 2.0f + i * (kFrondSpreadDeg / (kNumFronds - 1));
    float sway = kSwayAmpDeg * sinf(_windPhase + i * 0.45f);
    float ang = (restDeg + sway) * kPi / 180.0f;
    float dirX = sinf(ang), dirY = -cosf(ang);
    float tipX = kCrownX + dirX * kFrondLen;
    float tipY = kCrownY + dirY * kFrondLen;

    float perpX = -dirY, perpY = dirX;
    float tipWidth = kFrondBaseWidth * kFrondTipWidthFrac;
    fronds[i] = {(int16_t)lroundf(kCrownX + perpX * kFrondBaseWidth / 2),
                (int16_t)lroundf(kCrownY + perpY * kFrondBaseWidth / 2),
                (int16_t)lroundf(kCrownX - perpX * kFrondBaseWidth / 2),
                (int16_t)lroundf(kCrownY - perpY * kFrondBaseWidth / 2),
                (int16_t)lroundf(tipX - perpX * tipWidth / 2),
                (int16_t)lroundf(tipY - perpY * tipWidth / 2),
                (int16_t)lroundf(tipX + perpX * tipWidth / 2),
                (int16_t)lroundf(tipY + perpY * tipWidth / 2)};
  }

  // Trunk: a slightly bowed, tapered quad from foot to crown - also
  // rounded to int16_t once, same reason as the fronds above.
  float trunkDx = kCrownX - kBaseX, trunkDy = kCrownY - kBaseY;
  float trunkLen = sqrtf(trunkDx * trunkDx + trunkDy * trunkDy);
  float trunkNx = -trunkDy / trunkLen, trunkNy = trunkDx / trunkLen;
  constexpr float kTrunkBaseWidth = 8.0f, kTrunkTopWidth = 4.0f;
  float bowX = kBaseX + trunkDx * 0.5f - 3.0f;
  float bowY = kBaseY + trunkDy * 0.5f;

  int16_t baseLeftX = (int16_t)lroundf(kBaseX - trunkNx * kTrunkBaseWidth / 2);
  int16_t baseLeftY = (int16_t)lroundf(kBaseY - trunkNy * kTrunkBaseWidth / 2);
  int16_t baseRightX = (int16_t)lroundf(kBaseX + trunkNx * kTrunkBaseWidth / 2);
  int16_t baseRightY = (int16_t)lroundf(kBaseY + trunkNy * kTrunkBaseWidth / 2);
  int16_t bowLeftX = (int16_t)lroundf(bowX - trunkNx * kTrunkBaseWidth * 0.35f);
  int16_t bowLeftY = (int16_t)lroundf(bowY - trunkNy * kTrunkBaseWidth * 0.35f);
  int16_t bowRightX = (int16_t)lroundf(bowX + trunkNx * kTrunkBaseWidth * 0.35f);
  int16_t bowRightY = (int16_t)lroundf(bowY + trunkNy * kTrunkBaseWidth * 0.35f);
  int16_t topLeftX = (int16_t)lroundf(kCrownX - trunkNx * kTrunkTopWidth / 2);
  int16_t topLeftY = (int16_t)lroundf(kCrownY - trunkNy * kTrunkTopWidth / 2);
  int16_t topRightX = (int16_t)lroundf(kCrownX + trunkNx * kTrunkTopWidth / 2);
  int16_t topRightY = (int16_t)lroundf(kCrownY + trunkNy * kTrunkTopWidth / 2);

  static uint16_t stripBuffer[kPanelWidth * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < kPanelHeight;
       rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > kPanelHeight) {
      stripHeight = kPanelHeight - rowOffset;
    }
    memset(stripBuffer, 0, (size_t)kPanelWidth * stripHeight * sizeof(uint16_t));

    paintQuad(stripBuffer, rowOffset, stripHeight, baseLeftX, baseLeftY,
             bowLeftX, bowLeftY, topLeftX, topLeftY, bowRightX, bowRightY,
             color);
    paintQuad(stripBuffer, rowOffset, stripHeight, bowLeftX, bowLeftY,
             topLeftX, topLeftY, topRightX, topRightY, bowRightX, bowRightY,
             color);

    for (int i = 0; i < kNumFronds; i++) {
      const Frond &f = fronds[i];
      paintQuad(stripBuffer, rowOffset, stripHeight, f.crownLeftX,
               f.crownLeftY, f.crownRightX, f.crownRightY, f.tipRightX,
               f.tipRightY, f.tipLeftX, f.tipLeftY, color);
    }

    for (int li = 0; li < kNumWaveLines; li++) {
      paintWaveLine(stripBuffer, rowOffset, stripHeight, kWaveBaseY[li],
                   _wavePhase + li * 0.7f, color);
    }

    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, kPanelWidth,
                            stripHeight);
  }
}
