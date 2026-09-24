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
constexpr float kCrownX = 102.0f, kCrownY = 76.0f; // where the fronds meet
constexpr float kBaseX = 86.0f, kBaseY = 148.0f;   // trunk's foot

constexpr float kTrunkBaseWidth = 11.0f, kTrunkTopWidth = 3.0f;
constexpr float kTrunkBow = 12.0f; // how far the trunk bows sideways
constexpr int kTrunkSegments = 10;

// Each frond is a quadratic-bezier "hook": it leaves the crown heading
// baseAngDeg (0 = straight up, + = toward +x), then curves by curveDeg
// more over its length, tapering from width0 down to width1. Modeled on
// a reference stencil's asymmetric arrangement - a few fronds arcing up
// and over to hook downward on the right, one drooping low on the left -
// rather than a symmetric fan.
struct FrondSpec {
  float baseAngDeg;
  float curveDeg;
  float armLen;
  float width0, width1;
  float swayScale; // fraction of kSwayAmpDeg this frond actually gets
};
constexpr FrondSpec kFronds[] = {
    {30.0f, 100.0f, 32.0f, 13.0f, 5.0f, 0.8f},
    {62.0f, 75.0f, 27.0f, 12.0f, 4.0f, 0.9f},
    {90.0f, 55.0f, 24.0f, 11.0f, 4.0f, 1.0f},
    {-10.0f, -45.0f, 22.0f, 11.0f, 4.0f, 0.7f},
    {-50.0f, -100.0f, 36.0f, 12.0f, 5.0f, 1.1f},
};
constexpr int kNumFronds = sizeof(kFronds) / sizeof(kFronds[0]);
constexpr int kFrondSegments = 10;
constexpr float kSwayAmpDeg = 8.0f; // how far the wind rocks each frond

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
// hot per-pixel path (called for every candidate pixel of every ribbon
// segment, every frame), and the RP2040 has no hardware FPU - the
// eye-look face's original ~4fps bug came from exactly this kind of
// per-pixel float math. Vertices are rounded to int16_t once per shape
// per frame (see the ribbon builders below), not per pixel.
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

// Turns a sampled centerline (n+1 points, in float) plus a width taper
// from width0 (at the first point) to width1 (at the last) into the
// ribbon's left/right edges, rounded to int16_t once - same reasoning as
// inTriangle() above.
void ribbonFromCenterline(const float *cx, const float *cy, int n,
                          float width0, float width1, int16_t *leftX,
                          int16_t *leftY, int16_t *rightX, int16_t *rightY) {
  for (int i = 0; i <= n; i++) {
    float dx, dy;
    if (i < n) {
      dx = cx[i + 1] - cx[i];
      dy = cy[i + 1] - cy[i];
    } else {
      dx = cx[i] - cx[i - 1];
      dy = cy[i] - cy[i - 1];
    }
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) {
      len = 0.001f;
    }
    float px = -dy / len, py = dx / len;
    float t = (float)i / n;
    float w = width0 + (width1 - width0) * t;
    leftX[i] = (int16_t)lroundf(cx[i] + px * w / 2);
    leftY[i] = (int16_t)lroundf(cy[i] + py * w / 2);
    rightX[i] = (int16_t)lroundf(cx[i] - px * w / 2);
    rightY[i] = (int16_t)lroundf(cy[i] - py * w / 2);
  }
}

void paintRibbon(uint16_t *stripBuffer, int16_t rowOffset,
                 int16_t stripHeight, const int16_t *leftX,
                 const int16_t *leftY, const int16_t *rightX,
                 const int16_t *rightY, int n, uint16_t color) {
  for (int i = 0; i < n; i++) {
    paintQuad(stripBuffer, rowOffset, stripHeight, leftX[i], leftY[i],
             leftX[i + 1], leftY[i + 1], rightX[i + 1], rightY[i + 1],
             rightX[i], rightY[i], color);
  }
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

  // Trunk centerline: bowed sideways via a half-sine offset rather than a
  // bezier hook (fronds use that below) - simpler, and it's just one
  // shape.
  float trunkCx[kTrunkSegments + 1], trunkCy[kTrunkSegments + 1];
  for (int i = 0; i <= kTrunkSegments; i++) {
    float t = (float)i / kTrunkSegments;
    trunkCx[i] = kBaseX + (kCrownX - kBaseX) * t - kTrunkBow * sinf(t * kPi);
    trunkCy[i] = kBaseY + (kCrownY - kBaseY) * t;
  }
  int16_t trunkLX[kTrunkSegments + 1], trunkLY[kTrunkSegments + 1];
  int16_t trunkRX[kTrunkSegments + 1], trunkRY[kTrunkSegments + 1];
  ribbonFromCenterline(trunkCx, trunkCy, kTrunkSegments, kTrunkBaseWidth,
                      kTrunkTopWidth, trunkLX, trunkLY, trunkRX, trunkRY);

  // Each frond's current (swaying) shape: a quadratic bezier from the
  // crown, computed once per frame in float (cheap - a handful of trig
  // calls per frond, not per pixel) and rounded to int16_t via
  // ribbonFromCenterline so every pixel test below is pure integer math.
  int16_t frondLX[kNumFronds][kFrondSegments + 1];
  int16_t frondLY[kNumFronds][kFrondSegments + 1];
  int16_t frondRX[kNumFronds][kFrondSegments + 1];
  int16_t frondRY[kNumFronds][kFrondSegments + 1];
  for (int f = 0; f < kNumFronds; f++) {
    const FrondSpec &spec = kFronds[f];
    float sway = kSwayAmpDeg * spec.swayScale * sinf(_windPhase);
    float ang0 = (spec.baseAngDeg + sway) * kPi / 180.0f;
    float dir0X = sinf(ang0), dir0Y = -cosf(ang0);
    float p1X = kCrownX + dir0X * spec.armLen;
    float p1Y = kCrownY + dir0Y * spec.armLen;
    float ang1 = (spec.baseAngDeg + spec.curveDeg + sway) * kPi / 180.0f;
    float dir1X = sinf(ang1), dir1Y = -cosf(ang1);
    float p2X = p1X + dir1X * spec.armLen;
    float p2Y = p1Y + dir1Y * spec.armLen;

    float cx[kFrondSegments + 1], cy[kFrondSegments + 1];
    for (int i = 0; i <= kFrondSegments; i++) {
      float t = (float)i / kFrondSegments;
      float u = 1.0f - t;
      cx[i] = u * u * kCrownX + 2 * u * t * p1X + t * t * p2X;
      cy[i] = u * u * kCrownY + 2 * u * t * p1Y + t * t * p2Y;
    }
    ribbonFromCenterline(cx, cy, kFrondSegments, spec.width0, spec.width1,
                        frondLX[f], frondLY[f], frondRX[f], frondRY[f]);
  }

  static uint16_t stripBuffer[kPanelWidth * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < kPanelHeight;
       rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > kPanelHeight) {
      stripHeight = kPanelHeight - rowOffset;
    }
    memset(stripBuffer, 0, (size_t)kPanelWidth * stripHeight * sizeof(uint16_t));

    paintRibbon(stripBuffer, rowOffset, stripHeight, trunkLX, trunkLY,
               trunkRX, trunkRY, kTrunkSegments, color);
    for (int f = 0; f < kNumFronds; f++) {
      paintRibbon(stripBuffer, rowOffset, stripHeight, frondLX[f],
                 frondLY[f], frondRX[f], frondRY[f], kFrondSegments, color);
    }

    for (int li = 0; li < kNumWaveLines; li++) {
      paintWaveLine(stripBuffer, rowOffset, stripHeight, kWaveBaseY[li],
                   _wavePhase + li * 0.7f, color);
    }

    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, kPanelWidth,
                            stripHeight);
  }
}
