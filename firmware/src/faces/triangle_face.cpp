#include "triangle_face.h"

#include <math.h>

namespace {
constexpr float PI_VALUE = 3.14159265359f;

int16_t smileArcY(int16_t cornerY, int16_t depth, int16_t x, int16_t leftX,
                  int16_t rightX) {
  float progress = static_cast<float>(x - leftX) / (rightX - leftX);
  return cornerY + depth * sinf(PI_VALUE * progress);
}

void fillTangentTooth(Arduino_GFX *gfx, int16_t centerX, int16_t baseY,
                      int16_t width, int16_t height, int16_t baseOffset,
                      float depth,
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

  int16_t x1 = baseCenterX - tangentX * width / 2;
  int16_t y1 = baseCenterY - tangentY * width / 2;
  int16_t x2 = baseCenterX + tangentX * width / 2;
  int16_t y2 = baseCenterY + tangentY * width / 2;
  int16_t x3 = x2 + normalX * height;
  int16_t y3 = y2 + normalY * height;
  int16_t x4 = x1 + normalX * height;
  int16_t y4 = y1 + normalY * height;

  gfx->fillTriangle(x1, y1, x2, y2, x3, y3, RGB565_BLACK);
  gfx->fillTriangle(x1, y1, x3, y3, x4, y4, RGB565_BLACK);
}
} // namespace

void TriangleFace::begin(Arduino_GFX *gfx) {
  gfx->fillScreen(RGB565_BLACK);
}

void TriangleFace::draw(Arduino_GFX *gfx) {
  int16_t w = gfx->width();
  int16_t h = gfx->height();

  gfx->fillScreen(RGB565_BLACK);

  // Placeholder triangle eyes and mouth, proportioned off screen size so it
  // scales once the real panel resolution is known.
  int16_t eyeY = h * 0.3;
  int16_t eyeSize = w * 0.12;
  int16_t eyeOffsetX = w * 0.28;

  gfx->fillTriangle(w / 2 - eyeOffsetX, eyeY - eyeSize,
                     w / 2 - eyeOffsetX - eyeSize, eyeY + eyeSize,
                     w / 2 - eyeOffsetX + eyeSize, eyeY + eyeSize,
                     RGB565_ORANGE);
  gfx->fillTriangle(w / 2 + eyeOffsetX, eyeY - eyeSize,
                     w / 2 + eyeOffsetX - eyeSize, eyeY + eyeSize,
                     w / 2 + eyeOffsetX + eyeSize, eyeY + eyeSize,
                     RGB565_ORANGE);

  int16_t noseY = h * 0.46;
  int16_t noseSize = eyeSize * 0.55;
  gfx->fillTriangle(w / 2, noseY - noseSize,
                     w / 2 - noseSize, noseY + noseSize,
                     w / 2 + noseSize, noseY + noseSize, RGB565_ORANGE);

  int16_t mouthY = h * 0.60;
  int16_t mouthLeft = w * 0.18;
  int16_t mouthRight = w * 0.82;
  int16_t mouthUpperDepth = h * 0.10;
  int16_t mouthLowerDepth = h * 0.24;

  for (int16_t x = mouthLeft; x <= mouthRight; x++) {
    int16_t upperY = smileArcY(mouthY, mouthUpperDepth, x, mouthLeft,
                               mouthRight);
    int16_t lowerY = smileArcY(mouthY, mouthLowerDepth, x, mouthLeft,
                               mouthRight);
    gfx->fillRect(x, upperY, 1, lowerY - upperY + 1, RGB565_ORANGE);
  }

  int16_t toothWidth = w * 0.045;
  int16_t toothHeight = h * 0.07;
  int16_t toothBaseOffset = w * 0.01;
  int16_t toothSpacing = w * 0.15;
  for (int16_t topTooth = 0; topTooth < 3; topTooth++) {
    int16_t toothCenter = w / 2 + (topTooth - 1) * toothSpacing;
    int16_t toothY = smileArcY(mouthY, mouthUpperDepth, toothCenter,
                               mouthLeft, mouthRight);
    fillTangentTooth(gfx, toothCenter, toothY, toothWidth, toothHeight,
                     toothBaseOffset, mouthUpperDepth, mouthLeft, mouthRight,
                     true);
  }

  toothSpacing = w * 0.15;
  for (int16_t bottomTooth = 0; bottomTooth < 2; bottomTooth++) {
    int16_t toothCenter = w / 2 + (bottomTooth * 2 - 1) * toothSpacing / 2;
    int16_t toothY = smileArcY(mouthY, mouthLowerDepth, toothCenter,
                               mouthLeft, mouthRight);
    fillTangentTooth(gfx, toothCenter, toothY, toothWidth, toothHeight,
                     toothBaseOffset, mouthLowerDepth, mouthLeft, mouthRight,
                     false);
  }
}
