#include "triangle_face.h"

#include <math.h>

namespace {
constexpr float PI_VALUE = 3.14159265359f;

// Generously covers the mouth's bounding box (~141x43 px on the 220x176
// panel) with headroom.
constexpr int16_t kMouthBufferWidth = 200;
constexpr int16_t kMouthBufferHeight = 60;

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

uint16_t candleColor(float illumination) {
  uint8_t red = 255.0f * illumination;
  uint8_t green = 145.0f * illumination;
  return RGB565(red, green, 0);
}
} // namespace

void TriangleFace::begin(Arduino_GFX *gfx) {
  _flickerPhase = 0.0f;
  _illumination = 0.82f;
  gfx->fillScreen(RGB565_BLACK);
}

void TriangleFace::update() {
  _flickerPhase += 0.09f;
  if (_flickerPhase >= 2.0f * PI_VALUE) {
    _flickerPhase -= 2.0f * PI_VALUE;
  }

  _illumination = 0.82f + 0.10f * sinf(_flickerPhase) +
                  0.05f * sinf(_flickerPhase * 2.37f) +
                  0.03f * sinf(_flickerPhase * 5.11f);
}

void TriangleFace::draw(Arduino_GFX *gfx) {
  int16_t w = gfx->width();
  int16_t h = gfx->height();
  uint16_t faceColor = candleColor(_illumination);

  // Placeholder triangle eyes and mouth, proportioned off screen size so it
  // scales once the real panel resolution is known.
  int16_t eyeY = h * 0.3;
  int16_t eyeSize = w * 0.12;
  int16_t eyeOffsetX = w * 0.28;

  gfx->fillTriangle(w / 2 - eyeOffsetX, eyeY - eyeSize,
                     w / 2 - eyeOffsetX - eyeSize, eyeY + eyeSize,
                     w / 2 - eyeOffsetX + eyeSize, eyeY + eyeSize,
                     faceColor);
  gfx->fillTriangle(w / 2 + eyeOffsetX, eyeY - eyeSize,
                     w / 2 + eyeOffsetX - eyeSize, eyeY + eyeSize,
                     w / 2 + eyeOffsetX + eyeSize, eyeY + eyeSize,
                     faceColor);

  int16_t noseY = h * 0.46;
  int16_t noseSize = eyeSize * 0.55;
  gfx->fillTriangle(w / 2, noseY - noseSize,
                     w / 2 - noseSize, noseY + noseSize,
                     w / 2 + noseSize, noseY + noseSize, faceColor);

  int16_t mouthY = h * 0.60;
  int16_t mouthLeft = w * 0.18;
  int16_t mouthRight = w * 0.82;
  int16_t mouthUpperDepth = h * 0.10;
  int16_t mouthLowerDepth = h * 0.24;

  // The mouth arc and teeth are composed into this buffer and blitted to
  // the panel in one shot (see fillTriangleInBuffer above for why).
  int16_t mouthWidth = mouthRight - mouthLeft + 1;
  int16_t mouthHeight = mouthLowerDepth + 1;
  static uint16_t mouthBuffer[kMouthBufferWidth * kMouthBufferHeight];

  for (int16_t i = 0; i < mouthWidth * mouthHeight; i++) {
    mouthBuffer[i] = RGB565_BLACK;
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
        mouthBuffer[row * mouthWidth + col] = faceColor;
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
