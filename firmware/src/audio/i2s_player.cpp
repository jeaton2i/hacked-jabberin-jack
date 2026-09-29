#include "i2s_player.h"

#include <Arduino.h>
#include <I2S.h>
#include <math.h>

namespace {
// MAX98357A wiring - see docs/audio-i2s-wiring.md. GP27 (LRC) isn't picked
// independently here: the arduino-pico I2S library's PIO program fixes it
// at BCLK's pin + 1, so it's implicit in PIN_I2S_BCLK below, not a
// separate constant.
constexpr int8_t PIN_I2S_BCLK = 26; // -> MAX98357A BCLK
constexpr int8_t PIN_I2S_DATA = 28; // -> MAX98357A DIN

// Every clip and the synthesized test tone are authored at this one fixed
// rate (see i2s_player.h for why it's never changed after begin()).
// 8kHz keeps clips small and is entirely adequate for short speech/tone
// test purposes - this isn't music.
constexpr uint32_t kSampleRate = 8000;

constexpr float kTwoPi = 6.283185307179586f;
constexpr int16_t kToneAmplitude = 12000; // headroom below full-scale int16

// The clips/tone were audibly clipping at volumeScale's true 1.0 ceiling
// (real recorded audio, e.g. the Pac-Man clip, already sits close to
// full-scale - multiplying that by 1.0 leaves it there, and the
// MAX98357A/speaker chain distorts before hitting the digital ceiling).
// Same pattern as CandleFlicker::brightness(): setVolume()/volume() still
// store and report whatever was actually requested (so "volume 100"
// keeps meaning "100%" to anyone asking), but every sample gets this
// lower real ceiling applied instead of the raw scale, the same way
// brightness's color() clamps per-channel at the point of use rather
// than in setBrightness() itself.
constexpr float kMaxEffectiveVolume = 0.8f;

// Bigger than this library's own default (6 buffers x 64 words, ~48ms at
// 8kHz) - pump() is only fed once per main loop() iteration, and that
// loop's own draw() work plus its flat 16ms delay() can add up to well
// past 48ms some iterations, which would otherwise starve the DMA buffer
// mid-clip (an audible glitch) rather than just fall a bit behind on
// elapsedMsAsync()'s accuracy. 24 x 128 words = 3072 samples, ~384ms at
// 8kHz - comfortable headroom, and still a trivial ~12KB of RAM.
constexpr size_t kAsyncBuffers = 24;
constexpr size_t kAsyncBufferWords = 128;

I2S i2s(OUTPUT, PIN_I2S_BCLK, PIN_I2S_DATA);
bool began = false;
float volumeScale = 1.0f;

float effectiveVolume() {
  return volumeScale > kMaxEffectiveVolume ? kMaxEffectiveVolume
                                           : volumeScale;
}

// startClipAsync()/pump() state - see i2s_player.h.
const int16_t *asyncSamples = nullptr;
size_t asyncLength = 0;
size_t asyncPos = 0;

void playSineTone(float freqHz, unsigned long durationMs) {
  size_t sampleCount = (size_t)(kSampleRate * durationMs / 1000);
  float volume = effectiveVolume();
  for (size_t i = 0; i < sampleCount; i++) {
    float t = (float)i / (float)kSampleRate;
    int16_t sample =
        (int16_t)(kToneAmplitude * volume * sinf(kTwoPi * freqHz * t));
    i2s.write16(sample, sample);
  }
}
} // namespace

void I2sPlayer::begin() {
  i2s.setBuffers(kAsyncBuffers, kAsyncBufferWords);
  began = i2s.begin(kSampleRate);
  if (!began) {
    Serial.println("I2S audio init failed - check PIN_I2S_BCLK/DATA wiring");
  }
}

bool I2sPlayer::ready() { return began; }

void I2sPlayer::setVolume(float volume) {
  if (volume < 0.0f) {
    volume = 0.0f;
  } else if (volume > 1.0f) {
    volume = 1.0f;
  }
  volumeScale = volume;
}

float I2sPlayer::volume() { return volumeScale; }

void I2sPlayer::playTestTone() {
  if (!began) {
    return;
  }
  playSineTone(523.25f, 150); // C5
  playSineTone(783.99f, 150); // G5
}

void I2sPlayer::playClip(const int16_t *samples, size_t length,
                         uint32_t sampleRate) {
  if (!began) {
    return;
  }
  if (sampleRate != kSampleRate) {
    Serial.print("Audio clip is ");
    Serial.print(sampleRate);
    Serial.print("Hz but I2S output is fixed at ");
    Serial.print(kSampleRate);
    Serial.println("Hz - skipping playback rather than mis-pitching it");
    return;
  }
  float volume = effectiveVolume();
  for (size_t i = 0; i < length; i++) {
    int16_t sample = (int16_t)(samples[i] * volume);
    i2s.write16(sample, sample);
  }
}

void I2sPlayer::startClipAsync(const int16_t *samples, size_t length,
                               uint32_t sampleRate) {
  if (!began) {
    return;
  }
  if (sampleRate != kSampleRate) {
    Serial.print("Audio clip is ");
    Serial.print(sampleRate);
    Serial.print("Hz but I2S output is fixed at ");
    Serial.print(kSampleRate);
    Serial.println("Hz - skipping playback rather than mis-pitching it");
    return;
  }
  asyncSamples = samples;
  asyncLength = length;
  asyncPos = 0;
}

void I2sPlayer::pump() {
  if (!began || !asyncSamples || asyncPos >= asyncLength) {
    return;
  }
  // i2s.availableForWrite() is in bytes, 4 per write16() call (one 32-bit
  // L+R word) - see I2S::availableForWrite(). Only writing while there's
  // room is what makes this non-blocking: write16() itself busy-waits if
  // the buffer's actually full, which asking first avoids hitting.
  float volume = effectiveVolume();
  while (asyncPos < asyncLength && i2s.availableForWrite() >= 4) {
    int16_t sample = (int16_t)(asyncSamples[asyncPos] * volume);
    i2s.write16(sample, sample);
    asyncPos++;
  }
  if (asyncPos >= asyncLength) {
    asyncSamples = nullptr;
  }
}

void I2sPlayer::stopAsync() {
  asyncSamples = nullptr;
  asyncLength = 0;
  asyncPos = 0;
}

bool I2sPlayer::isPlayingAsync() { return asyncSamples != nullptr; }

uint32_t I2sPlayer::elapsedMsAsync() {
  return (uint32_t)((uint64_t)asyncPos * 1000 / kSampleRate);
}
