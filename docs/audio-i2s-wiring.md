# I2S Audio Wiring

Gives the pumpkin a voice - a synthesized test tone, one embedded test
speech clip, and (RP2040 only) a Pac-Man power-pellet clip so far,
playable from either board, with a runtime `volume` control:

- The RP2040 can drive its own I2S speaker directly, and works standalone
  even with no ESP32 bridge present (`audio <tone|voice|pacman>`).
- The optional ESP32 bridge board (see `docs/esp32-network-bridge.md`) can
  drive a second, independent I2S speaker of its own, playable from its
  web UI or triggered remotely by the RP2040 (`audio <esp-tone|esp-voice>`).

Both sides use the exact same board and wiring pattern, just on their own
GPIOs, and are entirely independent hardware - wire up whichever one(s)
you actually want a speaker on.

## Board

A **MAX98357A** I2S Class-D amplifier breakout (e.g. the
[Adafruit one](https://www.adafruit.com/product/3006) or an equivalent
generic clone - several exist under different brand names on Amazon,
same chip). It takes I2S digital audio straight in and drives a small
speaker directly - no separate DAC or amp needed.

## RP2040 wiring

| MAX98357A pin | RP2040 GPIO | Pico header pin |
| --- | ---: | ---: |
| BCLK | GP26 | 31 |
| LRC | GP27 | 32 |
| DIN | GP28 | 34 |
| GND | GND | any GND pin |
| VIN | 3V3(OUT) or 5V | 36, or VBUS (pin 40) for more headroom |

**LRC (GP27) isn't an independent choice.** The RP2040's I2S output runs
over PIO, and this core's I2S library fixes LRC at whatever pin follows
BCLK - i.e. BCLK's GPIO number + 1. Moving `PIN_I2S_BCLK` in
`firmware/src/audio/i2s_player.cpp` moves LRC with it automatically; there
is no separate LRC pin constant to set. Given GP0-15 are the display bus,
GP16-18 are the buttons, and GP20/21 are the (fixed-choice, see
`docs/esp32-network-bridge.md`) ESP32 bridge link, GP26/27/28 are free and
conveniently already consecutive, which is also this I2S library's own
default pin assignment.

## ESP32 bridge wiring

| MAX98357A pin | ESP32 GPIO |
| --- | ---: |
| BCLK | GPIO14 |
| LRC | GPIO25 (GPIO27 on an ESP32-S2 specifically - not relevant to this project's two supported boards, a classic ESP32 and an ESP32-S3) |
| DIN | GPIO26 |
| GND | any GND pin |
| VIN | 3V3 or 5V (its own supply - see the power note below) |

Unlike the RP2040, the ESP32's I2S peripheral routes through its own GPIO
matrix and isn't picky about which GPIOs are used, so `i2s_player.cpp`
just uses the Arduino I2S library's own compiled-in defaults rather than
reassigning them - all three are free given the ESP32 bridge's own UART
link (see `docs/esp32-network-bridge.md`) only ever uses GPIO32/33.

## Common notes for both

Plus a small 4-8Ω speaker on the module's speaker output terminals, on
each board that gets one.

Leave the module's own GAIN and SD (shutdown) pins/solder pads at their
board's default - unconnected is fine for a basic single-speaker setup;
see the specific board's own docs if you want a different fixed gain.

If the speaker draws enough current to matter, power VIN from a 5V rail
(the RP2040's VBUS, pin 40; the ESP32's own 5V/USB supply) or a separate
supply rather than 3V3 - same reasoning as the ESP32 bridge's own
UART-link power note: don't load down either board's onboard regulator
beyond what it's meant to supply. If both boards have their own amp
wired up, this matters even more - don't tie their VINs together, same as
never tying their 3V3 rails together.

## Testing

Directly on either board:

```
audio tone     # RP2040: synthesized two-note chime on its own speaker
audio voice    # RP2040: plays its test speech clip
audio pacman   # RP2040: plays the Pac-Man power-pellet clip (see PacManFace)
volume [percent]  # show, or set (0-100, 100 = original level), this board's own playback volume
```

Send any of these over the serial console (USB, or the ESP32 bridge's
relayed command protocol) - the bridge's own web UI has an "Audio"
section wired to the same three buttons plus volume/mute, see
`docs/esp32-network-bridge.md`.

The ESP32 can also have its own, separate speaker (a second MAX98357A on
its own GPIOs, entirely independent of the RP2040's). That one has no
serial console to test from directly and isn't wired to any button on
the web UI (it controls the RP2040's speaker, above) - use its
`POST /api/local-audio` endpoint instead (body `tone` or `voice`), which
plays immediately with no RP2040 involvement at all.

## Triggering ESP32 playback from the RP2040

```
audio esp-tone     # queue the test tone for the ESP32's speaker
audio esp-voice    # queue the test speech clip for the ESP32's speaker
```

These don't play anything immediately - they just set a flag
(`pendingEsp32Audio` in `firmware/src/main.cpp`) that the ESP32 bridge
polls for on its own (every 500ms, whether or not its web page happens to
be open) via a dedicated `audiotrigger` command, which atomically reads
and clears the flag so a trigger only ever fires once.

This is deliberately a **poll**, not the RP2040 pushing a message
whenever it feels like it. The RP2040<->ESP32 link's existing protocol is
strictly request/response (the ESP32 always asks, the RP2040 only ever
replies) - if the RP2040 could write to that same wire unprompted, a
trigger firing while the ESP32 happens to be mid-request for something
else (e.g. `status`) could splice into and corrupt that reply. Piggy-
backing on the same request/response shape everything else already uses
avoids that entirely, at the cost of that same ~500ms of latency, which
is fine for a sound-effect trigger.

## Where the test clips come from

- The tone is synthesized on the fly in each side's own `i2s_player.cpp` -
  no asset file.
- The voice clip (`sample-audio/test_word.wav` -> `firmware/src/assets/
  test_word.h` and `esp32-bridge/src/assets/test_word.h` - both boards
  get their own copy of the same generated header) was generated locally
  with Windows' built-in `System.Speech.Synthesis` text-to-speech engine
  (not a downloaded recording), then converted with
  `tools/convert_wav_to_pcm.py`, which trims silence, resamples, and
  emits a mono 16-bit PCM C array the same way
  `tools/convert_image_to_rgb565.py` emits image arrays:

  ```
  python tools/convert_wav_to_pcm.py <input.wav> <firmware or esp32-bridge>/src/assets/<name>.h <name>
  ```

  All clips play at a single fixed 8kHz sample rate on both boards (see
  `kSampleRate` in each `i2s_player.cpp`) - `playClip()` refuses to play
  anything recorded at a different rate rather than mis-pitching it, so
  convert new clips with the default rate (or pass `--rate 8000`
  explicitly).
- The Pac-Man power-pellet clip (RP2040 only, `sample-audio/
  pacman_power_pellet_6s.wav` -> `firmware/src/assets/audio_pacman.h`)
  was trimmed from real Pac-Man arcade gameplay audio, timed so the
  waka-waka -> siren transition lands at its exact midpoint - see
  `PacManFace` below for why that matters, and note `--no-trim` was used
  (unlike the voice clip) since its fades/timing were already tuned by
  hand and silence-trimming could have disturbed them.

## Why manual test playback is blocking, but PacManFace isn't

`I2sPlayer::playTestTone()`/`playClip()` block until finished on both
boards - no face animation, button, web request, or serial polling
happens during that time (on whichever board is actually playing). Fine
for a manually-triggered test clip; not fine for a face that needs to
keep animating while a clip plays.

`PacManFace` uses a second, non-blocking path instead:
`I2sPlayer::startClipAsync()` queues a clip without blocking, and
`pump()` - called once every `loop()` iteration on the RP2040 regardless
of which face is active - feeds it only as much of the I2S output's DMA
buffer as currently has room, so it never blocks either. The face then
drives its own animation progress from `I2sPlayer::elapsedMsAsync()`
(how much of the clip has actually been fed so far) rather than an
independently-running timer, so the visual can't drift out of sync with
the audio even if a slow frame ever makes `pump()` fall behind - and
falls back to a plain wall-clock timer via `I2sPlayer::ready()` if no
I2S hardware ever came up at all, so the face still animates (just
unsynced) with no speaker wired. `selectFace()` calls `stopAsync()`
whenever the display switches to a different face, so nothing queued
this way keeps playing over an unrelated one.

This is still not a true DMA-callback-driven background mixer (only one
clip can be queued this way at a time, and `pump()` needs to be called
regularly rather than truly running in the background) - just enough to
not-block, which is what a per-face animation sync actually needs. A
real overlapping-audio use case would still want that fuller rewrite.

## Why I2S is never reconfigured after boot

Neither board has a runtime "change audio pins" command the way the
ESP32 bridge's *link* pins have one - each `I2sPlayer::begin()` runs once,
in `setup()`, at one fixed sample rate, and is never torn down or
re-begun afterward. This mirrors a real lesson learned building the ESP32
bridge: reconfiguring the RP2040's UART1 live (also a PIO-adjacent
peripheral) reliably hard-faulted the chip. The RP2040's I2S is also
PIO-based, and there's no reason to assume it would be any more forgiving
of the same live-reconfiguration mistake - so neither implementation
attempts it, even though the ESP32's own I2S peripheral (not PIO-based)
was never actually implicated in that specific failure.
