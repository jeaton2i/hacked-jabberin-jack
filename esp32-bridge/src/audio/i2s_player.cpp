#include "i2s_player.h"

#include <Arduino.h>
#include <I2S.h>
#include <math.h>

namespace {
// Every clip and the synthesized test tone are authored at this one fixed
// rate (see i2s_player.h for why it's never changed after begin()). 8kHz
// keeps clips small and is entirely adequate for short speech/tone test
// purposes - this isn't music.
constexpr uint32_t kSampleRate = 8000;

constexpr float kTwoPi = 6.283185307179586f;
constexpr int16_t kToneAmplitude = 12000; // headroom below full-scale int16

bool began = false;

// I2SClass::write(int32_t) writes exactly one channel slot per call (the
// low bitsPerSample/8 bytes of the argument) into the configured
// left-right stereo stream - so a mono sample needs writing twice, once
// per channel, rather than packed into one call the way the RP2040 side's
// write16(l, r) works.
void writeMonoSample(int16_t sample) {
  I2S.write((int32_t)sample);
  I2S.write((int32_t)sample);
}

void playSineTone(float freqHz, unsigned long durationMs) {
  size_t sampleCount = (size_t)(kSampleRate * durationMs / 1000);
  for (size_t i = 0; i < sampleCount; i++) {
    float t = (float)i / (float)kSampleRate;
    int16_t sample = (int16_t)(kToneAmplitude * sinf(kTwoPi * freqHz * t));
    writeMonoSample(sample);
  }
}
} // namespace

void I2sPlayer::begin() {
  // Uses this library's own compiled-in default pins (PIN_I2S_SCK=14,
  // PIN_I2S_FS=25, PIN_I2S_SD=26 on both a classic ESP32 and an ESP32-S3 -
  // see docs/audio-i2s-wiring.md) rather than reassigning them: unlike the
  // RP2040's PIO-based I2S, this hardware peripheral routes through its
  // own GPIO matrix and isn't picky about which pins, and all three
  // defaults are free given this board's own UART bridge link only ever
  // uses GPIO32/33.
  began = I2S.begin(I2S_PHILIPS_MODE, kSampleRate, 16) != 0;
  if (!began) {
    Serial.println("I2S audio init failed - check I2S wiring");
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
    Serial.printf("Audio clip is %uHz but I2S output is fixed at %uHz - "
                 "skipping playback rather than mis-pitching it\n",
                 (unsigned)sampleRate, (unsigned)kSampleRate);
    return;
  }
  for (size_t i = 0; i < length; i++) {
    writeMonoSample(samples[i]);
  }
}
