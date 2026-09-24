#include "pacman_face.h"

#include <math.h>
#include <string.h>

namespace {
constexpr int16_t kPanelWidth = 220;
constexpr int16_t kPanelHeight = 176;
constexpr float kPacY = kPanelHeight / 2.0f;
constexpr float kRadius = 18.0f;
constexpr float kDotRadius = 3.0f;
constexpr float kPillRadius = 7.0f;
constexpr float kPillX = kPanelWidth / 2.0f;
constexpr float kGhostRadius = 16.0f;
constexpr float kSpeed = 1.5f;
constexpr float kGhostChaseSpeed = 1.1f;
constexpr float kGhostFleeSpeed = 2.0f;
constexpr int16_t kStripHeight = 16;

constexpr uint16_t kGhostChaseColor = ((31) << 11) | ((22) << 5) | 18; // pink-red
constexpr uint16_t kGhostFleeColor = ((14) << 11) | ((44) << 5) | 31; // pale blue

int16_t clampInt(int16_t v, int16_t lo, int16_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// Fills stripBuffer[y-rowOffset][x] = color wherever (px,py) is set true
// for the pixels in [xStart,xEnd] x [rowOffset, rowOffset+stripHeight).
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
} // namespace

void PacManFace::resetRound() {
  _x = kPanelWidth + kRadius;
  for (bool &eaten : _dotEaten) {
    eaten = false;
  }
  _pillEaten = false;
  _ghostFleeing = false;
  _ghostX = -kGhostRadius;
}

void PacManFace::begin(Arduino_GFX *gfx) {
  _mouthPhase = 0.0f;
  constexpr float kMargin = 25.0f;
  float span = kPanelWidth - 2.0f * kMargin;
  for (int i = 0; i < kDotCount; i++) {
    _dotX[i] = kMargin + span * i / (kDotCount - 1);
  }
  resetRound();
  gfx->fillScreen(RGB565_BLACK);
}

void PacManFace::update() {
  _flicker.update();
  _mouthPhase += 0.35f;
  _x -= kSpeed; // right to left

  if (_x < -kRadius) {
    resetRound();
    return;
  }

  for (int i = 0; i < kDotCount; i++) {
    if (!_dotEaten[i] && fabsf(_x - _dotX[i]) < kRadius) {
      _dotEaten[i] = true;
    }
  }
  if (!_pillEaten && fabsf(_x - kPillX) < kRadius + kPillRadius) {
    _pillEaten = true;
    _ghostFleeing = true;
  }

  // Chasing: move toward Pac-Man. Fleeing: move away from Pac-Man.
  float ghostSpeed = _ghostFleeing ? kGhostFleeSpeed : kGhostChaseSpeed;
  bool towardPacman = _ghostX < _x;
  bool moveRight = _ghostFleeing ? !towardPacman : towardPacman;
  _ghostX += moveRight ? ghostSpeed : -ghostSpeed;
}

void PacManFace::draw(Arduino_GFX *gfx) {
  uint16_t color = _flicker.color();
  float halfMouthAngle = 0.12f + 0.55f * fabsf(sinf(_mouthPhase));
  float cosHalfMouth = cosf(halfMouthAngle);
  uint16_t ghostColor = _ghostFleeing ? kGhostFleeColor : kGhostChaseColor;

  static uint16_t stripBuffer[kPanelWidth * kStripHeight];
  for (int16_t rowOffset = 0; rowOffset < kPanelHeight;
       rowOffset += kStripHeight) {
    int16_t stripHeight = kStripHeight;
    if (rowOffset + stripHeight > kPanelHeight) {
      stripHeight = kPanelHeight - rowOffset;
    }
    memset(stripBuffer, 0,
          (size_t)kPanelWidth * stripHeight * sizeof(uint16_t));

    // Pac-Man: moving right-to-left, so the mouth wedge faces -x.
    paintRegion(
        stripBuffer, rowOffset, stripHeight, (int16_t)(_x - kRadius),
        (int16_t)(_x + kRadius), (int16_t)(kPacY - kRadius),
        (int16_t)(kPacY + kRadius), color, [&](int16_t x, int16_t y) {
          float dx = x - _x;
          float dy = y - kPacY;
          float distSq = dx * dx + dy * dy;
          if (distSq > kRadius * kRadius) {
            return false;
          }
          float dist = sqrtf(distSq);
          bool inMouth = dist > 0.5f && (-dx > dist * cosHalfMouth);
          return !inMouth;
        });

    for (int i = 0; i < kDotCount; i++) {
      if (_dotEaten[i]) {
        continue;
      }
      float dotX = _dotX[i];
      paintRegion(stripBuffer, rowOffset, stripHeight,
                  (int16_t)(dotX - kDotRadius), (int16_t)(dotX + kDotRadius),
                  (int16_t)(kPacY - kDotRadius), (int16_t)(kPacY + kDotRadius),
                  color, [&](int16_t x, int16_t y) {
                    float ddx = x - dotX;
                    float ddy = y - kPacY;
                    return ddx * ddx + ddy * ddy <= kDotRadius * kDotRadius;
                  });
    }

    if (!_pillEaten) {
      paintRegion(stripBuffer, rowOffset, stripHeight,
                  (int16_t)(kPillX - kPillRadius),
                  (int16_t)(kPillX + kPillRadius),
                  (int16_t)(kPacY - kPillRadius),
                  (int16_t)(kPacY + kPillRadius), color,
                  [&](int16_t x, int16_t y) {
                    float ddx = x - kPillX;
                    float ddy = y - kPacY;
                    return ddx * ddx + ddy * ddy <= kPillRadius * kPillRadius;
                  });
    }

    // Ghost: rounded dome on top, flat-bottomed body below, with two dark
    // eye cutouts.
    float eyeOffsetX = kGhostRadius * 0.45f;
    float eyeOffsetY = -kGhostRadius * 0.25f;
    float eyeR = kGhostRadius * 0.22f;
    paintRegion(
        stripBuffer, rowOffset, stripHeight, (int16_t)(_ghostX - kGhostRadius),
        (int16_t)(_ghostX + kGhostRadius), (int16_t)(kPacY - kGhostRadius),
        (int16_t)(kPacY + kGhostRadius * 0.8f), ghostColor,
        [&](int16_t x, int16_t y) {
          float dx = x - _ghostX;
          float dy = y - kPacY;
          bool inBody = (dy <= 0) ? (dx * dx + dy * dy <=
                                     kGhostRadius * kGhostRadius)
                                   : (fabsf(dx) <= kGhostRadius &&
                                      dy <= kGhostRadius * 0.8f);
          if (!inBody) {
            return false;
          }
          float lex = dx - (-eyeOffsetX), ley = dy - eyeOffsetY;
          float rex = dx - eyeOffsetX, rey = dy - eyeOffsetY;
          bool inEye = (lex * lex + ley * ley <= eyeR * eyeR) ||
                       (rex * rex + rey * rey <= eyeR * eyeR);
          return !inEye;
        });

    gfx->draw16bitRGBBitmap(0, rowOffset, stripBuffer, kPanelWidth,
                            stripHeight);
  }
}
