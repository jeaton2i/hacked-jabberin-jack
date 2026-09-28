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

// What actually reads as "carved," per a real jack-o-lantern reference
// photo, is an actual visible wall of pumpkin flesh around each opening -
// a band of a distinctly darker, more saturated color with a fairly crisp
// boundary where it meets the glow, not a smooth gradient. A soft
// continuous blend from edge to center (tried first) doesn't read as a
// wall at all, no matter how dark it gets at the very edge, because
// nothing in the image marks where the "wall" actually ends. So each
// opening is now drawn in two explicit zones:
//   1. A fixed-width wall band (kWallWidthPx, in real screen pixels - not
//      scaled to the shape's size, so it stays visible even on the small
//      nose) right at the cut edge, itself fading from edgeColor
//      (darkest, right at the true boundary) to wallColor (its inner
//      face, where it meets the glow) - still a solid, clearly-flesh-toned
//      band rather than a thin highlight.
//   2. The glowing interior beyond that band, fading from that same
//      wallColor into hotColor over kInnerGlowBandFraction of whatever
//      inradius remains inside the wall - saturating quickly so most of
//      the interior reads as a uniform hot glow, like looking through the
//      cut at a light source.
constexpr float kWallWidthPx = 4.0f;
constexpr float kInnerGlowBandFraction = 0.5f;

uint16_t blendColor(uint16_t colorA, uint16_t colorB, float t) {
  if (t <= 0.0f) {
    return colorA;
  }
  if (t >= 1.0f) {
    return colorB;
  }
  int aR = (colorA >> 11) & 0x1F;
  int aG = (colorA >> 5) & 0x3F;
  int aB = colorA & 0x1F;
  int bR = (colorB >> 11) & 0x1F;
  int bG = (colorB >> 5) & 0x3F;
  int bB = colorB & 0x1F;
  int outR = aR + (int)((bR - aR) * t + 0.5f);
  int outG = aG + (int)((bG - aG) * t + 0.5f);
  int outB = aB + (int)((bB - aB) * t + 0.5f);
  return ((uint16_t)outR << 11) | ((uint16_t)outG << 5) | (uint16_t)outB;
}

// The hot core color a carved opening's interior glows toward: pushes
// green up toward red (yellow) and adds a touch of blue, for a
// white-hot look rather than just a brighter version of the same orange.
uint16_t hotColor(uint16_t base) {
  int r5 = (base >> 11) & 0x1F;
  int g6 = (base >> 5) & 0x3F;
  int hotG = g6 + (63 - g6) * 3 / 5;
  if (hotG > 63) {
    hotG = 63;
  }
  return ((uint16_t)r5 << 11) | ((uint16_t)hotG << 5) | (uint16_t)6;
}

// The wall band's inner face color, where it meets the glowing interior -
// a solid, clearly-flesh-toned red-orange, less green than the base flame
// color (deeper red) and a bit darker.
uint16_t wallColor(uint16_t base) {
  int r5 = (base >> 11) & 0x1F;
  int g6 = (base >> 5) & 0x3F;
  int wallR = (int)(r5 * 0.8f);
  int wallG = (int)(g6 * 0.35f);
  return ((uint16_t)wallR << 11) | ((uint16_t)wallG << 5);
}

// The wall band's outer face color, right at the true cut edge - notably
// darker still than wallColor, like the pumpkin's own skin barely
// catching any light at all at that grazing angle.
uint16_t edgeColor(uint16_t base) {
  int r5 = (base >> 11) & 0x1F;
  int g6 = (base >> 5) & 0x3F;
  int edgeR = (int)(r5 * 0.4f);
  int edgeG = (int)(g6 * 0.1f);
  return ((uint16_t)edgeR << 11) | ((uint16_t)edgeG << 5);
}

// Saturates a raw pixel distance into a 0..1 blend amount: 0 at dist=0,
// 1 once dist reaches span. Shared by both the wall band's own
// edge->wall fade and the interior's wall->hot fade, just with a
// different dist/span each time.
float saturateOverSpan(float dist, float span) {
  if (span < 1.0f) {
    span = 1.0f;
  }
  float t = dist / span;
  if (t < 0.0f) {
    t = 0.0f;
  } else if (t > 1.0f) {
    t = 1.0f;
  }
  return t;
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

// Same shape-fill as fillTriangleInBuffer, but with a visible wall band
// plus glowing interior (see kWallWidthPx's comment) instead of one flat
// color.
void fillTriangleGradedInBuffer(uint16_t *buffer, int16_t bufW, int16_t bufH,
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

  // w0/w1/w2 below are each edge's cross product evaluated at (x, y),
  // i.e. (signed distance from that edge line) * (that edge's length) -
  // dividing by each edge's own length converts it into an actual
  // perpendicular pixel distance from that edge.
  float len01 = sqrtf((float)(x1 - x0) * (x1 - x0) + (float)(y1 - y0) * (y1 - y0));
  float len12 = sqrtf((float)(x2 - x1) * (x2 - x1) + (float)(y2 - y1) * (y2 - y1));
  float len20 = sqrtf((float)(x0 - x2) * (x0 - x2) + (float)(y0 - y2) * (y0 - y2));
  if (len01 < 1.0f) len01 = 1.0f;
  if (len12 < 1.0f) len12 = 1.0f;
  if (len20 < 1.0f) len20 = 1.0f;

  // Inradius = area / semiperimeter - the maximum possible "distance from
  // the nearest edge" for this triangle, reached only at its incenter.
  float area = 0.5f * fabsf((float)(x1 - x0) * (y2 - y0) -
                           (float)(y1 - y0) * (x2 - x0));
  float semiperimeter = (len01 + len12 + len20) * 0.5f;
  float inradius = semiperimeter > 0.0f ? area / semiperimeter : 1.0f;
  float remainingInradius = inradius - kWallWidthPx;
  if (remainingInradius < 1.0f) {
    remainingInradius = 1.0f;
  }

  uint16_t hot = hotColor(color);
  uint16_t wall = wallColor(color);
  uint16_t edge = edgeColor(color);

  for (int16_t y = minY; y <= maxY; y++) {
    for (int16_t x = minX; x <= maxX; x++) {
      int32_t w0 = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
      int32_t w1 = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
      int32_t w2 = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);
      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) ||
          (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
        float dist0 = fabsf((float)w0) / len01;
        float dist1 = fabsf((float)w1) / len12;
        float dist2 = fabsf((float)w2) / len20;
        float minDist = dist0 < dist1 ? (dist0 < dist2 ? dist0 : dist2)
                                       : (dist1 < dist2 ? dist1 : dist2);
        uint16_t pixelColor;
        if (minDist < kWallWidthPx) {
          pixelColor = blendColor(edge, wall,
                                  saturateOverSpan(minDist, kWallWidthPx));
        } else {
          pixelColor = blendColor(
              wall, hot,
              saturateOverSpan(minDist - kWallWidthPx,
                               remainingInradius * kInnerGlowBandFraction));
        }
        buffer[y * bufW + x] = pixelColor;
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

// Draws one walled/glowing (see kWallWidthPx) triangle to the panel, via
// the same "compose in RAM, blit once" approach the mouth already uses -
// reuses one static buffer sized for a single small feature (an eye or
// the nose), since only one of these is ever being composed at a time.
void drawGradedTriangle(Arduino_GFX *gfx, int16_t x0, int16_t y0, int16_t x1,
                        int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
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
                            x1 - minX, y1 - minY, x2 - minX, y2 - minY, color);
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

  drawGradedTriangle(gfx, w / 2 - eyeOffsetX, eyeY - eyeSize,
                     w / 2 - eyeOffsetX - eyeSize, eyeY + eyeSize,
                     w / 2 - eyeOffsetX + eyeSize, eyeY + eyeSize, faceColor);
  drawGradedTriangle(gfx, w / 2 + eyeOffsetX, eyeY - eyeSize,
                     w / 2 + eyeOffsetX - eyeSize, eyeY + eyeSize,
                     w / 2 + eyeOffsetX + eyeSize, eyeY + eyeSize, faceColor);
  drawGradedTriangle(gfx, w / 2, noseY - noseSize, w / 2 - noseSize,
                     noseY + noseSize, w / 2 + noseSize, noseY + noseSize,
                     faceColor);

  // The mouth arc and teeth are composed into this buffer and blitted to
  // the panel in one shot (see fillTriangleInBuffer above for why).
  int16_t mouthWidth = mouthRight - mouthLeft + 1;
  int16_t mouthHeight = mouthLowerDepth + 1;
  static uint16_t mouthBuffer[kMouthBufferWidth * kMouthBufferHeight];

  for (int16_t i = 0; i < mouthWidth * mouthHeight; i++) {
    mouthBuffer[i] = RGB565_BLACK;
  }

  uint16_t mouthHot = hotColor(faceColor);
  uint16_t mouthWall = wallColor(faceColor);
  uint16_t mouthEdge = edgeColor(faceColor);

  for (int16_t x = mouthLeft; x <= mouthRight; x++) {
    int16_t upperY = smileArcY(mouthY, mouthUpperDepth, x, mouthLeft,
                               mouthRight);
    int16_t lowerY = smileArcY(mouthY, mouthLowerDepth, x, mouthLeft,
                               mouthRight);
    int16_t col = x - mouthLeft;
    // Local cavity half-thickness at this column, for the interior glow's
    // own remaining span beyond the wall band - the mouth's cross-section
    // varies with x (it's arc-bounded, not a straight-edged shape like the
    // eyes/nose), so this can't be computed once for the whole shape.
    float halfThickness = (lowerY - upperY) * 0.5f;
    float remainingHalfThickness = halfThickness - kWallWidthPx;
    if (remainingHalfThickness < 1.0f) {
      remainingHalfThickness = 1.0f;
    }
    for (int16_t y = upperY; y <= lowerY; y++) {
      int16_t row = y - mouthY;
      if (row >= 0 && row < mouthHeight) {
        float distFromEdge = (float)(y - upperY) < (float)(lowerY - y)
                                  ? (float)(y - upperY)
                                  : (float)(lowerY - y);
        uint16_t pixelColor;
        if (distFromEdge < kWallWidthPx) {
          pixelColor = blendColor(
              mouthEdge, mouthWall, saturateOverSpan(distFromEdge, kWallWidthPx));
        } else {
          pixelColor = blendColor(
              mouthWall, mouthHot,
              saturateOverSpan(distFromEdge - kWallWidthPx,
                               remainingHalfThickness * kInnerGlowBandFraction));
        }
        mouthBuffer[row * mouthWidth + col] = pixelColor;
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
