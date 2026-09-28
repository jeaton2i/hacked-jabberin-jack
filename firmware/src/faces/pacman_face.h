#pragma once

#include <stddef.h>
#include <stdint.h>

#include "candle_flicker.h"
#include "face.h"

// A carved-pumpkin-style Pac-Man (candle-lit, mouth opening/closing) that
// travels right to left eating a row of dots and one larger power pill.
// A ghost chases Pac-Man until the power pill is eaten, then flees for
// the rest of that pass.
//
// Each pass is timed to the Pac-Man power-pellet audio clip (see
// audio_pacman.h) rather than to a fixed per-tick speed: _x is driven by
// I2sPlayer::elapsedMsAsync() - which resetRound() (re)starts a clip
// against every lap - instead of an independently-running timer, so the
// visual can't drift out of sync with the actual audio feed even if a
// slow frame ever makes pump() fall behind (both read the same counter).
// The power pill sits at the panel's horizontal midpoint, which is also
// the spatial midpoint of the whole travel path, so a linear-in-time
// crossing lands the pill exactly at the clip's own waka-waka -> siren
// transition (its midpoint too) with no separate offset to maintain.
//
// Falls back to a plain millis()-since-round-start timer instead
// whenever !I2sPlayer::ready() (no I2S audio hardware ever came up), so
// this still animates - just without anything to sync to - on a pumpkin
// with no speaker wired at all.
class PacManFace : public Face {
public:
  // Takes the power-pellet clip's data the same way StaticImageFace/
  // CandleLitImageFace take image data - rather than #include-ing
  // assets/audio_pacman.h here too, which would silently embed a second
  // 96KB copy of it in flash alongside main.cpp's own copy (a `const`
  // array like that has internal linkage per translation unit).
  PacManFace(const int16_t *audioSamples, size_t audioLength,
            uint32_t audioSampleRate)
      : _audioSamples(audioSamples), _audioLength(audioLength),
        _audioSampleRate(audioSampleRate),
        _roundDurationMs(
            (uint32_t)((uint64_t)audioLength * 1000 / audioSampleRate)) {}

  void begin(Arduino_GFX *gfx) override;
  void update() override;
  void draw(Arduino_GFX *gfx) override;

  static constexpr int kDotCount = 8;

private:
  void resetRound();

  const int16_t *_audioSamples;
  size_t _audioLength;
  uint32_t _audioSampleRate;
  uint32_t _roundDurationMs; // derived from the clip - see constructor

  CandleFlicker _flicker;
  float _x = 0.0f;
  float _mouthPhase = 0.0f;
  unsigned long _roundStartMillis = 0; // fallback clock - see class comment
  bool _dotEaten[kDotCount] = {};
  float _dotX[kDotCount] = {};

  bool _pillEaten = false;
  float _ghostX = 0.0f;
  bool _ghostFleeing = false;
};
