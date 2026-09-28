#include "triangle_face.h"

#include <math.h>

namespace {
constexpr float PI_VALUE = 3.14159265359f;

// Generously covers the mouth's bounding box (~141x43 px on the 220x176
// panel) with headroom.
constexpr int16_t kMouthBufferWidth = 200;
constexpr int16_t kMouthBufferHeight = 60;

// Generously covers one eye or the nose's bounding box (~53x53 px for an
// eye on the 220x176 panel) with headroom; reused for each shape in turn.
constexpr int16_t kFeatureBufferSize = 80;

// Darkest point of the vertical shading gradient below, as a fraction of
// full brightness - keeps the topmost points of each carved opening dimly
// visible rather than fading all the way to black, the same idea as
// CandleFlicker::tint()'s kDimFloor for carved image faces.
constexpr float kShadeMinScale = 0.45f;

uint16_t scaleColor(uint16_t color, float scale) {
  if (scale >= 1.0f) {
    return color;
  }
  if (scale < 0.0f) {
    scale = 0.0f;
  }
  uint8_t r5 = (color >> 11) & 0x1F;
  uint8_t g6 = (color >> 5) & 0x3F;
  uint8_t b5 = color & 0x1F;
  uint8_t outR5 = (uint8_t)(r5 * scale + 0.5f);
  uint8_t outG6 = (uint8_t)(g6 * scale + 0.5f);
  uint8_t outB5 = (uint8_t)(b5 * scale + 0.5f);
  return (outR5 << 11) | (outG6 << 5) | outB5;
}

// Simulates a single light source low in the pumpkin body, like a candle
// sitting at its base: every carved opening (eyes, nose, mouth) reads as a
// flat 2D silhouette without this, so each one's color is scaled by how
// far down the face a given point falls within [topY, bottomY] (spanning
// from the eyes' own top down to the mouth's own bottom) - brighter near
// the bottom of each cavity, dimmer near the top, same direction for
// every feature so the whole face reads as lit from one place.
float verticalShadeScale(int16_t y, int16_t topY, int16_t bottomY) {
  int16_t span = bottomY - topY;
  if (span < 1) {
    span = 1;
  }
  float t = (float)(y - topY) / (float)span;
  if (t < 0.0f) {
    t = 0.0f;
  } else if (t > 1.0f) {
    t = 1.0f;
  }
  return kShadeMinScale + (1.0f - kShadeMinScale) * t;
}

int16_t smileArcY(int16_t cornerY, int16_t depth, int16_t x, int16_t leftX,
                  int16_t rightX) {
  float progress = static_cast<float>(x - leftX) / (rightX - leftX);
  return cornerY + depth * sinf(PI_VALUE * progress);
}

// Fills a triangle into an in-memory pixel buffer instead of the display, so
// the whole mouth (arc + tooth cutouts) can be composed in RAM and pushed to
// the panel in a single bitmap blit. Doing this shape-by-shape directly on
// the display is slow enough on the 8-bit parallel bus that each tooth
// visibly "pops in" one at a time as it's drawn.
void fillTriangleInBuffer(uint16_t *buffer, int16_t bufW, int16_t bufH,
                          int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint16_t color) {
  int16_t minX = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
  int16_t maxX = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
  int16_t minY = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
  int16_t maxY = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
  if (minX < 0) minX = 0;
  if (minY < 0) minY = 0;
  if (maxX > bufW - 1) maxX = bufW - 1;
  if (maxY > bufH - 1) maxY = bufH - 1;

  for (int16_t y = minY; y <= maxY; y++) {
    for (int16_t x = minX; x <= maxX; x++) {
      int32_t w0 = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
      int32_t w1 = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
      int32_t w2 = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);
      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) ||
          (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
        buffer[y * bufW + x] = color;
      }
    }
  }
}

// Same shape-fill as fillTriangleInBuffer, but each row is shaded by
// verticalShadeScale() instead of filled with one flat color - the 3D
// look for the eyes and nose (see that function's comment). shadeTopY/
// shadeBottomY are in the same coordinate space as the triangle's own
// points (i.e. buffer-local, not absolute screen coordinates - the caller
// shifts both by the same origin).
void fillTriangleGradedInBuffer(uint16_t *buffer, int16_t bufW, int16_t bufH,
                                int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                int16_t x2, int16_t y2, uint16_t color,
                                int16_t shadeTopY, int16_t shadeBottomY) {
  int16_t minX = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
  int16_t maxX = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
  int16_t minY = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
  int16_t maxY = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
  if (minX < 0) minX = 0;
  if (minY < 0) minY = 0;
  if (maxX > bufW - 1) maxX = bufW - 1;
  if (maxY > bufH - 1) maxY = bufH - 1;

  for (int16_t y = minY; y <= maxY; y++) {
    // Same color for the whole row, so compute it once rather than once
    // per pixel.
    uint16_t shaded =
        scaleColor(color, verticalShadeScale(y, shadeTopY, shadeBottomY));
    for (int16_t x = minX; x <= maxX; x++) {
      int32_t w0 = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
      int32_t w1 = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
      int32_t w2 = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);
      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) ||
          (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
        buffer[y * bufW + x] = shaded;
      }
    }
  }
}

void fillTangentToothInBuffer(uint16_t *buffer, int16_t bufW, int16_t bufH,
                              int16_t originX, int16_t originY,
                              int16_t centerX, int16_t baseY, int16_t width,
                              int16_t height, int16_t baseOffset, float depth,
                              int16_t leftX, int16_t rightX, bool hangsDown) {
  float progress = static_cast<float>(centerX - leftX) / (rightX - leftX);
  float slope = depth * PI_VALUE * cosf(PI_VALUE * progress) /
                (rightX - leftX);
  float tangentLength = sqrtf(1.0f + slope * slope);
  float tangentX = 1.0f / tangentLength;
  float tangentY = slope / tangentLength;
  float normalX = hangsDown ? -tangentY : tangentY;
  float normalY = hangsDown ? tangentX : -tangentX;
  float baseCenterX = centerX - normalX * baseOffset;
  float baseCenterY = baseY - normalY * baseOffset;

  int16_t x1 = baseCenterX - tangentX * width / 2 - originX;
  int16_t y1 = baseCenterY - tangentY * width / 2 - originY;
  int16_t x2 = baseCenterX + tangentX * width / 2 - originX;
  int16_t y2 = baseCenterY + tangentY * width / 2 - originY;
  int16_t x3 = x2 + normalX * height;
  int16_t y3 = y2 + normalY * height;
  int16_t x4 = x1 + normalX * height;
  int16_t y4 = y1 + normalY * height;

  fillTriangleInBuffer(buffer, bufW, bufH, x1, y1, x2, y2, x3, y3,
                       RGB565_BLACK);
  fillTriangleInBuffer(buffer, bufW, bufH, x1, y1, x3, y3, x4, y4,
                       RGB565_BLACK);
}

// Draws one graded (see verticalShadeScale) triangle to the panel, via the
// same "compose in RAM, blit once" approach the mouth already uses -
// reuses one static buffer sized for a single small feature (an eye or
// the nose), since only one of these is ever being composed at a time.
void drawGradedTriangle(Arduino_GFX *gfx, int16_t x0, int16_t y0, int16_t x1,
                        int16_t y1, int16_t x2, int16_t y2, uint16_t color,
                        int16_t shadeTopY, int16_t shadeBottomY) {
  int16_t minX = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
  int16_t maxX = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
  int16_t minY = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
  int16_t maxY = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
  int16_t bufW = maxX - minX + 1;
  int16_t bufH = maxY - minY + 1;

  static uint16_t featureBuffer[kFeatureBufferSize * kFeatureBufferSize];
  for (int16_t i = 0; i < bufW * bufH; i++) {
    featureBuffer[i] = RGB565_BLACK;
  }
  fillTriangleGradedInBuffer(featureBuffer, bufW, bufH, x0 - minX, y0 - minY,
                            x1 - minX, y1 - minY, x2 - minX, y2 - minY, color,
                            shadeTopY - minY, shadeBottomY - minY);
  gfx->draw16bitRGBBitmap(minX, minY, featureBuffer, bufW, bufH);
}

} // namespace

void TriangleFace::begin(Arduino_GFX *gfx) { gfx->fillScreen(RGB565_BLACK); }

void TriangleFace::update() { _flicker.update(); }

void TriangleFace::draw(Arduino_GFX *gfx) {
  int16_t w = gfx->width();
  int16_t h = gfx->height();
  uint16_t faceColor = _flicker.color();

  // Placeholder triangle eyes and mouth, proportioned off screen size so it
  // scales once the real panel resolution is known.
  int16_t eyeY = h * 0.3;
  int16_t eyeSize = w * 0.12;
  int16_t eyeOffsetX = w * 0.28;

  int16_t noseY = h * 0.46;
  int16_t noseSize = eyeSize * 0.55;

  int16_t mouthY = h * 0.60;
  int16_t mouthLeft = w * 0.18;
  int16_t mouthRight = w * 0.82;
  int16_t mouthUpperDepth = h * 0.10;
  int16_t mouthLowerDepth = h * 0.24;

  // One shared light source for every carved opening (see
  // verticalShadeScale's comment) - spans from the eyes' own top down to
  // the mouth's own bottom, so the whole face reads as lit from one place
  // rather than each feature getting its own independent gradient.
  int16_t shadeTopY = eyeY - eyeSize;
  int16_t shadeBottomY = mouthY + mouthLowerDepth;

  drawGradedTriangle(gfx, w / 2 - eyeOffsetX, eyeY - eyeSize,
                     w / 2 - eyeOffsetX - eyeSize, eyeY + eyeSize,
                     w / 2 - eyeOffsetX + eyeSize, eyeY + eyeSize, faceColor,
                     shadeTopY, shadeBottomY);
  drawGradedTriangle(gfx, w / 2 + eyeOffsetX, eyeY - eyeSize,
                     w / 2 + eyeOffsetX - eyeSize, eyeY + eyeSize,
                     w / 2 + eyeOffsetX + eyeSize, eyeY + eyeSize, faceColor,
                     shadeTopY, shadeBottomY);
  drawGradedTriangle(gfx, w / 2, noseY - noseSize, w / 2 - noseSize,
                     noseY + noseSize, w / 2 + noseSize, noseY + noseSize,
                     faceColor, shadeTopY, shadeBottomY);

  // The mouth arc and teeth are composed into this buffer and blitted to
  // the panel in one shot (see fillTriangleInBuffer above for why).
  int16_t mouthWidth = mouthRight - mouthLeft + 1;
  int16_t mouthHeight = mouthLowerDepth + 1;
  static uint16_t mouthBuffer[kMouthBufferWidth * kMouthBufferHeight];

  for (int16_t i = 0; i < mouthWidth * mouthHeight; i++) {
    mouthBuffer[i] = RGB565_BLACK;
  }

  // Same color for every pixel in a given row regardless of column, so
  // computed once per row up front rather than once per pixel (see
  // verticalShadeScale's comment for what this is doing).
  static uint16_t mouthRowColor[kMouthBufferHeight];
  for (int16_t row = 0; row < mouthHeight; row++) {
    mouthRowColor[row] = scaleColor(
        faceColor, verticalShadeScale(mouthY + row, shadeTopY, shadeBottomY));
  }

  for (int16_t x = mouthLeft; x <= mouthRight; x++) {
    int16_t upperY = smileArcY(mouthY, mouthUpperDepth, x, mouthLeft,
                               mouthRight);
    int16_t lowerY = smileArcY(mouthY, mouthLowerDepth, x, mouthLeft,
                               mouthRight);
    int16_t col = x - mouthLeft;
    for (int16_t y = upperY; y <= lowerY; y++) {
      int16_t row = y - mouthY;
      if (row >= 0 && row < mouthHeight) {
        mouthBuffer[row * mouthWidth + col] = mouthRowColor[row];
      }
    }
  }

  int16_t toothWidth = w * 0.045;
  int16_t toothHeight = h * 0.07;
  int16_t toothBaseOffset = w * 0.01;
  int16_t toothSpacing = w * 0.15;
  for (int16_t topTooth = 0; topTooth < 3; topTooth++) {
    int16_t toothCenter = w / 2 + (topTooth - 1) * toothSpacing;
    int16_t toothY = smileArcY(mouthY, mouthUpperDepth, toothCenter,
                               mouthLeft, mouthRight);
    fillTangentToothInBuffer(mouthBuffer, mouthWidth, mouthHeight, mouthLeft,
                             mouthY, toothCenter, toothY, toothWidth,
                             toothHeight, toothBaseOffset, mouthUpperDepth,
                             mouthLeft, mouthRight, true);
  }

  toothSpacing = w * 0.15;
  for (int16_t bottomTooth = 0; bottomTooth < 2; bottomTooth++) {
    int16_t toothCenter = w / 2 + (bottomTooth * 2 - 1) * toothSpacing / 2;
    int16_t toothY = smileArcY(mouthY, mouthLowerDepth, toothCenter,
                               mouthLeft, mouthRight);
    fillTangentToothInBuffer(mouthBuffer, mouthWidth, mouthHeight, mouthLeft,
                             mouthY, toothCenter, toothY, toothWidth,
                             toothHeight, toothBaseOffset, mouthLowerDepth,
                             mouthLeft, mouthRight, false);
  }

  gfx->draw16bitRGBBitmap(mouthLeft, mouthY, mouthBuffer, mouthWidth,
                          mouthHeight);
}
