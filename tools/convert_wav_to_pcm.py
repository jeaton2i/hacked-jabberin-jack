"""Converts a source WAV file into a raw 16-bit PCM mono C array for the
firmware's I2S audio playback (see firmware/src/audio/).

Downmixes to mono (averaging channels) and resamples (via simple linear
interpolation - these are short speech/tone clips, not music, so this is
plenty) to --rate, trims leading/trailing silence below a noise-floor
threshold, and applies a short fade-in/out to avoid clicks at the cut
points.

Usage:
    python convert_wav_to_pcm.py <input.wav> <output .h> <array name>
        [--rate HZ] [--no-trim] [--fade-ms MS]
"""

import sys
import wave

import numpy as np

DEFAULT_RATE = 8000
DEFAULT_FADE_MS = 5


def load_mono(path):
    with wave.open(path, "rb") as w:
        channels = w.getnchannels()
        sampwidth = w.getsampwidth()
        rate = w.getframerate()
        raw = w.readframes(w.getnframes())
    if sampwidth != 2:
        raise SystemExit(f"only 16-bit PCM WAV is supported, got {sampwidth*8}-bit")
    samples = np.frombuffer(raw, dtype=np.int16).astype(np.float32)
    if channels > 1:
        samples = samples.reshape(-1, channels).mean(axis=1)
    return samples, rate


def trim_silence(samples, threshold_ratio=0.03, pad_start_s=0.02, pad_end_s=0.05, rate=DEFAULT_RATE):
    peak = np.max(np.abs(samples))
    if peak == 0:
        return samples
    threshold = peak * threshold_ratio
    above = np.where(np.abs(samples) > threshold)[0]
    if len(above) == 0:
        return samples
    start = max(0, above[0] - int(pad_start_s * rate))
    end = min(len(samples), above[-1] + int(pad_end_s * rate))
    return samples[start:end]


def resample(samples, src_rate, dst_rate):
    if src_rate == dst_rate:
        return samples
    dst_len = round(len(samples) * dst_rate / src_rate)
    src_times = np.arange(len(samples))
    dst_times = np.linspace(0, len(samples) - 1, dst_len)
    return np.interp(dst_times, src_times, samples)


def apply_fade(samples, rate, fade_ms):
    fade_len = int(rate * fade_ms / 1000)
    if fade_len <= 0 or fade_len * 2 >= len(samples):
        return samples
    samples = samples.copy()
    ramp = np.linspace(0, 1, fade_len)
    samples[:fade_len] *= ramp
    samples[-fade_len:] *= ramp[::-1]
    return samples


def main():
    args = sys.argv[1:]
    if len(args) < 3:
        print(__doc__)
        sys.exit(1)

    in_path, out_path, array_name = args[0], args[1], args[2]
    options = args[3:]

    rate = DEFAULT_RATE
    trim = True
    fade_ms = DEFAULT_FADE_MS

    i = 0
    while i < len(options):
        opt = options[i]
        if opt == "--rate":
            rate = int(options[i + 1])
            i += 2
        elif opt == "--no-trim":
            trim = False
            i += 1
        elif opt == "--fade-ms":
            fade_ms = float(options[i + 1])
            i += 2
        else:
            raise SystemExit(f"unknown option: {opt}")

    samples, src_rate = load_mono(in_path)
    samples = resample(samples, src_rate, rate)
    if trim:
        samples = trim_silence(samples, rate=rate)
    samples = apply_fade(samples, rate, fade_ms)

    values = np.clip(samples, -32768, 32767).astype(np.int16)

    with open(out_path, "w") as f:
        f.write("#pragma once\n\n")
        f.write("#include <stddef.h>\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"constexpr uint32_t {array_name}_sample_rate = {rate};\n")
        f.write(f"constexpr size_t {array_name}_length = {len(values)};\n\n")
        f.write(f"const int16_t {array_name}[] = {{\n")
        for i in range(0, len(values), 12):
            row = values[i:i + 12]
            f.write("  " + ", ".join(str(v) for v in row) + ",\n")
        f.write("};\n")

    duration_ms = len(values) / rate * 1000
    print(f"wrote {out_path}: {len(values)} samples @ {rate}Hz "
          f"({duration_ms:.0f}ms, {len(values)*2} bytes)")


if __name__ == "__main__":
    main()
