#include "triangle_face.h"

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

  int16_t mouthY = h * 0.65;
  int16_t mouthWidth = w * 0.5;
  int16_t mouthHeight = h * 0.15;
  gfx->fillTriangle(w / 2 - mouthWidth / 2, mouthY,
                     w / 2 + mouthWidth / 2, mouthY,
                     w / 2, mouthY + mouthHeight, RGB565_ORANGE);
}
