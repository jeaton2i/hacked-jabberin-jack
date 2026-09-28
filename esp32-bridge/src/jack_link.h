#pragma once

#include <Stream.h>

// The wire to the RP2040 controller, however this build talks to it. Two
// implementations exist, selected per PlatformIO environment (see
// platformio.ini's build_src_filter) rather than at runtime, since they
// need different boards/frameworks entirely:
//
//   - uart_link.cpp: a plain 2-wire UART (any classic ESP32, e.g. env
//     `esp32dev`) - see docs/esp32-network-bridge.md's "UART link" wiring.
//   - usb_host_link.cpp: USB-C to USB-C straight into the RP2040's own USB
//     port, with the ESP32 acting as USB host and the RP2040 appearing as
//     the CDC-ACM serial device it already is (env `esp32s3_usbhost`,
//     needs an ESP32-S2/S3/P4 with native USB-OTG) - see the same doc's
//     "USB host link" section.
//
// Both present the RP2040 as an ordinary Arduino Stream so main.cpp's
// sendCommandAndRead() and the HTTP handlers don't need to know or care
// which one is actually in use.
void jackLinkBegin();
Stream &jackLink();

// True once the link can plausibly reach the controller - always true for
// the UART build (a wire has no "device present" signal to check), but
// only true for the USB host build once the RP2040 has actually enumerated
// as a CDC-ACM device. main.cpp uses this to give a clearer error than a
// generic command timeout when nothing is plugged in yet.
bool jackLinkReady();

// Runtime GPIO pin configuration - meaningful only for the UART transport
// (persisted in NVS via Preferences, applied immediately - no reflash or
// reboot needed to try a different pair of pins). The USB host transport
// has no GPIO pins to configure - it always reports unsupported and its
// setter is a no-op that returns false.
bool jackLinkSupportsPinConfig();
// Writes the pins currently in use into *rxPin/*txPin. Only meaningful
// when jackLinkSupportsPinConfig() is true; otherwise writes -1 to both.
void jackLinkGetPins(int *rxPin, int *txPin);
// Persists the given pins and re-applies them live. Returns false (leaving
// the current pins untouched) if unsupported or if rxPin is one of the
// ESP32's input-only, pull-less pins (GPIO34-39) - see uart_link.cpp for
// why that specifically is worth guarding against.
bool jackLinkSetPins(int rxPin, int txPin);
