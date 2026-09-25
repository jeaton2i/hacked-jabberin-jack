// Throwaway logic sniffer for reverse-engineering the Jabberin' Jack XL's
// built-in controller video-out header. This is NOT the pumpkin firmware —
// it puts the RP2040 in a read-only capture mode so its GPIOs can be wired
// to the XL controller's video-out pins (while that controller drives its
// own display normally) and the resulting activity logged for offline
// analysis. See ../../tools/pin_sniffer_analyze.py for the other half.
//
// Wiring: tie the XL board's GND (header pin 5) to the RP2040's GND. Do NOT
// connect the XL board's 3.3V (pin 6) to the RP2040 — each board should run
// from its own power, only sharing ground, so a wiring mistake here can't
// back-feed either board. Only connect the signal pins below once the XL
// board's logic level has been confirmed to be 3.3V (the RP2040's GPIOs are
// not 5V tolerant).
#include <Arduino.h>
#include <hardware/gpio.h>
#include <pico/time.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/clocks.h>

struct PinMap {
  uint8_t gpio;
  uint8_t headerPin;
  const char *label;
};

// Matches the corrected physical wiring recorded in ../../pin map (verified
// by continuity through the ribbon cable to the panel itself). Pins 1-4,
// 21, 23, 24 are unused on the connector; pins 3-5/22 (GND) and 6 (VCC,
// wired directly to the RP2040's 3V3 rather than a GPIO) aren't signals to
// sniff, so none of those are listed here. Pin 19 is still unidentified
// (untested - possibly ALE by analogy, not wired to a GPIO yet).
static const PinMap kPins[] = {
    {19, 7, "fd7"},
    {18, 8, "fd6"},
    {17, 9, "fd5"},
    {16, 10, "fd4"},
    {0, 11, "fd3"},
    {1, 12, "fd2"},
    {2, 13, "fd1"},
    {3, 14, "fd0"},
    {4, 15, "reb"},
    {5, 16, "wrb"},
    {6, 17, "cle"},
    {7, 18, "cs"},
    {20, 20, "rst"},
};
constexpr size_t kPinCount = sizeof(kPins) / sizeof(kPins[0]);

// ~64KB of the RP2040's ~264KB RAM (the rest is reserved for the PIO
// capture buffer below). Raise if captures keep overflowing before the
// interesting part of a redraw finishes; lower if RAM gets tight.
constexpr uint32_t kCaptureCapacity = 8000;

struct Sample {
  uint32_t bits;     // GPIO0..31 snapshot at the moment of the change
  uint16_t deltaUs;  // time since the previous recorded change, saturating at 0xFFFF
};

static Sample g_buf[kCaptureCapacity];
static uint32_t g_count = 0;
static bool g_overflowed = false;
static uint32_t g_pinMask = 0;

static void printBanner();
static void dumpCapture();

static void doCapture() {
  g_count = 0;
  g_overflowed = false;
  Serial.println("CAPTURING... (send 'x' to stop)");

  uint32_t last = gpio_get_all() & g_pinMask;
  uint32_t lastUs = time_us_32();
  bool stopRequested = false;

  while (g_count < kCaptureCapacity && !stopRequested) {
    uint32_t now = gpio_get_all() & g_pinMask;
    if (now != last) {
      uint32_t nowUs = time_us_32();
      uint32_t delta = nowUs - lastUs;
      g_buf[g_count].deltaUs = (delta > 0xFFFF) ? (uint16_t)0xFFFF : (uint16_t)delta;
      g_buf[g_count].bits = now;
      g_count++;
      last = now;
      lastUs = nowUs;
    }
    // Only poll Serial every so often so the check doesn't slow the hot loop.
    if ((g_count & 0xFFF) == 0 && Serial.available()) {
      int c = Serial.read();
      if (c == 'x') stopRequested = true;
    }
  }
  if (g_count >= kCaptureCapacity) g_overflowed = true;

  Serial.print("CAPTURE_DONE count=");
  Serial.print(g_count);
  Serial.print(" overflow=");
  Serial.println(g_overflowed ? 1 : 0);
  dumpCapture();
}

static void dumpCapture() {
  Serial.println("BEGIN_CAPTURE");
  Serial.print("idx,delta_us");
  for (size_t i = 0; i < kPinCount; i++) {
    Serial.print(',');
    Serial.print(kPins[i].label);
  }
  Serial.println();

  for (uint32_t i = 0; i < g_count; i++) {
    Serial.print(i);
    Serial.print(',');
    Serial.print(g_buf[i].deltaUs);
    for (size_t p = 0; p < kPinCount; p++) {
      Serial.print(',');
      Serial.print((g_buf[i].bits >> kPins[p].gpio) & 1);
    }
    Serial.println();
  }
  Serial.println("END_CAPTURE");
}

// --- Synchronous (PIO) capture ---
//
// The 'a' capture above is a plain software polling loop: it has no fixed
// sample clock, so any real signal running near or above its (jittery,
// variable) polling rate aliases — the same physical pin can look "busy" or
// "quiet" from one run to the next depending on random timing phase. That's
// exactly what happened with fd4/fd5 in testing.
//
// This mode fixes that by using the RP2040's PIO block as a true
// fixed-rate, jitter-free sampler: one PIO instruction ("in pins, 32")
// repeated every N system-clock cycles, DMA'd straight into RAM. Every
// sample is captured at a precisely known instant, so there's no aliasing
// ambiguity about which pin is actually fastest — at the cost of a much
// shorter capture window, since (unlike 'a') it can't skip storing samples
// that didn't change.
constexpr uint32_t kPioCaptureCapacity = 32768;  // 128KB
constexpr float kPioClkDiv = 4.0f;               // ~33.25 MHz at 133MHz sysclk

static uint32_t g_pioBuf[kPioCaptureCapacity];
static uint32_t g_pioCount = 0;
static float g_pioSampleHz = 0;

static const uint16_t kLogicAnalyserProgram[] = {0x4000};  // "in pins, 32"
static const pio_program_t kLogicAnalyserPgm = {
    .instructions = kLogicAnalyserProgram,
    .length = 1,
    .origin = -1,
};

static PIO g_pio = pio0;
static uint g_pioSm = 0;
static uint g_pioOffset = 0;
static int g_dmaChan = -1;
static bool g_pioReady = false;

static void setupPio() {
  g_pioOffset = pio_add_program(g_pio, &kLogicAnalyserPgm);
  g_pioSm = pio_claim_unused_sm(g_pio, true);

  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap(&c, g_pioOffset, g_pioOffset);
  sm_config_set_in_pins(&c, 0);  // base pin 0; "in pins, 32" reads GPIO0..31
  sm_config_set_in_shift(&c, false, true, 32);  // autopush every 32 bits
  sm_config_set_clkdiv(&c, kPioClkDiv);
  pio_sm_init(g_pio, g_pioSm, g_pioOffset, &c);

  g_dmaChan = dma_claim_unused_channel(true);
  g_pioSampleHz = (float)clock_get_hz(clk_sys) / kPioClkDiv;
  g_pioReady = true;
}

static void dumpPioCapture();

static void doPioCapture() {
  if (!g_pioReady) {
    Serial.println("PIO not initialized");
    return;
  }
  g_pioCount = 0;

  dma_channel_config dc = dma_channel_get_default_config(g_dmaChan);
  channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
  channel_config_set_read_increment(&dc, false);
  channel_config_set_write_increment(&dc, true);
  channel_config_set_dreq(&dc, pio_get_dreq(g_pio, g_pioSm, false));
  dma_channel_configure(g_dmaChan, &dc, g_pioBuf, &g_pio->rxf[g_pioSm],
                        kPioCaptureCapacity, false);

  pio_sm_clear_fifos(g_pio, g_pioSm);
  pio_sm_restart(g_pio, g_pioSm);
  pio_sm_set_enabled(g_pio, g_pioSm, false);

  Serial.println("PIO_CAPTURING...");
  dma_channel_start(g_dmaChan);
  pio_sm_set_enabled(g_pio, g_pioSm, true);

  while (dma_channel_is_busy(g_dmaChan)) {
    // fixed-length one-shot capture; runs for kPioCaptureCapacity samples
    // at kPioSampleHz, no early-stop needed at this timescale (<a few ms)
  }

  pio_sm_set_enabled(g_pio, g_pioSm, false);
  g_pioCount = kPioCaptureCapacity;

  Serial.print("PIO_CAPTURE_DONE count=");
  Serial.print(g_pioCount);
  Serial.print(" sample_hz=");
  Serial.println(g_pioSampleHz, 1);
  dumpPioCapture();
}

static void dumpPioCapture() {
  Serial.println("BEGIN_PIO_CAPTURE");
  Serial.print("sample_hz=");
  Serial.println(g_pioSampleHz, 1);
  // Every sample is exactly this far apart in time (fixed-rate, unlike the
  // change-triggered 'a' capture) - reported as delta_ns so this dump uses
  // the same idx,delta,<pins...> shape pin_sniffer_analyze.py already parses.
  uint32_t deltaNs = (uint32_t)(1e9 / g_pioSampleHz + 0.5f);
  Serial.print("idx,delta_ns");
  for (size_t i = 0; i < kPinCount; i++) {
    Serial.print(',');
    Serial.print(kPins[i].label);
  }
  Serial.println();

  for (uint32_t i = 0; i < g_pioCount; i++) {
    Serial.print(i);
    Serial.print(',');
    Serial.print(deltaNs);
    for (size_t p = 0; p < kPinCount; p++) {
      Serial.print(',');
      Serial.print((g_pioBuf[i] >> kPins[p].gpio) & 1);
    }
    Serial.println();
  }
  Serial.println("END_PIO_CAPTURE");
}

static void printBanner() {
  Serial.println();
  Serial.println("=== Jabberin' Jack XL video-out pin sniffer ===");
  for (size_t i = 0; i < kPinCount; i++) {
    Serial.print("GP");
    Serial.print(kPins[i].gpio);
    Serial.print(" -> header pin ");
    Serial.print(kPins[i].headerPin);
    Serial.print(" (");
    Serial.print(kPins[i].label);
    Serial.println(")");
  }
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  a  change-triggered capture (runs until buffer full or 'x')");
  Serial.println("     - long window, but a software polling loop: can alias");
  Serial.println("       on signals faster than it can reliably sample");
  Serial.println("  s  synchronous PIO capture: fixed-rate, jitter-free, no");
  Serial.println("     aliasing, but a much shorter (<1ms) fixed-length window");
  Serial.println("  x  stop an in-progress 'a' capture early");
  Serial.println("  p  print the last completed 'a' capture again as CSV");
  Serial.println("  q  print the last completed 's' capture again as CSV");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 3000) {
    // give the host a few seconds to open the port; don't hang forever if
    // nothing's listening
  }

  g_pinMask = 0;
  for (size_t i = 0; i < kPinCount; i++) {
    gpio_init(kPins[i].gpio);
    gpio_set_dir(kPins[i].gpio, GPIO_IN);
    gpio_disable_pulls(kPins[i].gpio);
    g_pinMask |= (1u << kPins[i].gpio);
  }

  setupPio();
  printBanner();
}

void loop() {
  if (Serial.available()) {
    int c = Serial.read();
    if (c == 'a') {
      doCapture();
    } else if (c == 'p') {
      dumpCapture();
    } else if (c == 's') {
      doPioCapture();
    } else if (c == 'q') {
      dumpPioCapture();
    } else if (c == '\r' || c == '\n') {
      // ignore
    } else {
      printBanner();
    }
  }
}
