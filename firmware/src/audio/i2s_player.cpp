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

I2S i2s(OUTPUT, PIN_I2S_BCLK, PIN_I2S_DATA);
bool began = false;

void playSineTone(float freqHz, unsigned long durationMs) {
  size_t sampleCount = (size_t)(kSampleRate * durationMs / 1000);
  for (size_t i = 0; i < sampleCount; i++) {
    float t = (float)i / (float)kSampleRate;
    int16_t sample = (int16_t)(kToneAmplitude * sinf(kTwoPi * freqHz * t));
    i2s.write16(sample, sample);
  }
}
} // namespace

void I2sPlayer::begin() {
  began = i2s.begin(kSampleRate);
  if (!began) {
    Serial.println("I2S audio init failed - check PIN_I2S_BCLK/DATA wiring");
  }
}

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
  for (size_t i = 0; i < length; i++) {
    i2s.write16(samples[i], samples[i]);
  }
}
