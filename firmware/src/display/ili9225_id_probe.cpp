#include "ili9225_id_probe.h"

#include <Arduino.h>

namespace {
constexpr int8_t PIN_RD = 8;
constexpr int8_t PIN_WR = 9;
constexpr int8_t PIN_DC = 10;
constexpr int8_t PIN_RST = 11;
constexpr int8_t PIN_CS = 12;
constexpr int8_t DATA_PINS[8] = {0, 1, 2, 3, 4, 5, 6, 7};

void setDataPinsMode(uint8_t mode) {
  for (int8_t pin : DATA_PINS) {
    pinMode(pin, mode);
  }
}

void writeDataByte(uint8_t value) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(DATA_PINS[i], (value >> i) & 0x01);
  }
  digitalWrite(PIN_WR, LOW);
  delayMicroseconds(1);
  digitalWrite(PIN_WR, HIGH);
  delayMicroseconds(1);
}

uint8_t readDataByte() {
  digitalWrite(PIN_RD, LOW);
  delayMicroseconds(1);
  uint8_t value = 0;
  for (int i = 0; i < 8; i++) {
    if (digitalRead(DATA_PINS[i])) {
      value |= (1 << i);
    }
  }
  digitalWrite(PIN_RD, HIGH);
  delayMicroseconds(1);
  return value;
}
} // namespace

uint16_t probeIli9225DriverCode() {
  pinMode(PIN_RD, OUTPUT);
  pinMode(PIN_WR, OUTPUT);
  pinMode(PIN_DC, OUTPUT);
  pinMode(PIN_RST, OUTPUT);
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_RD, HIGH);
  digitalWrite(PIN_WR, HIGH);
  digitalWrite(PIN_CS, HIGH);

  // Hardware reset so the panel is in a known state before we ask it
  // anything.
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(150);

  setDataPinsMode(OUTPUT);
  digitalWrite(PIN_CS, LOW);

  // Select register 0x00 (Driver Code Read) as the active index.
  digitalWrite(PIN_DC, LOW);
  writeDataByte(0x00);
  digitalWrite(PIN_DC, HIGH);

  // The register value is transferred as two 8-bit read cycles, MSB then
  // LSB, mirroring how 16-bit values are written two bytes at a time on
  // this 8-bit bus.
  setDataPinsMode(INPUT);
  uint8_t msb = readDataByte();
  uint8_t lsb = readDataByte();
  setDataPinsMode(OUTPUT);

  digitalWrite(PIN_CS, HIGH);

  return (static_cast<uint16_t>(msb) << 8) | lsb;
}
