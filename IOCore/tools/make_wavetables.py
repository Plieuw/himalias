#!/usr/bin/env python3
"""
Himalia wavetable generator -> IOCore/HimaliaSketch/wavetables.h

Up to --tables wavetables (default 8) of --waves single-cycle waves each
(default 64), stored as packed 12 bit (the resolution of the DAC).
  - RATCHET knob picks the table, SAMPLE SELECT morphs through its waves.
  Flash budget: tables x waves x 4 levels x 384 bytes must stay below ~950 KB,
  e.g. 8 x 64 (768 KB) or 16 x 32 (768 KB).

Sources:
  every *.wav in the wavetables folder, sorted by file name (prefix them with
  01_, 02_ ... to set the order). Serum / Vital style: single-cycle frames one
  after the other. The frame size is read from the Serum 'clm' chunk when it is
  there, otherwise --frame (default 2048). --waves frames are picked evenly
  (all frames when the file has fewer).
  Free slots are filled with the built-in tables:
      basic   = sine > triangle > saw > square > pulse
      formant = vocal / formant sweep

Every wave is stored band limited in WT_LEVELS versions so high notes do not
alias. Needs sox; numpy makes it fast (pip3 install numpy), without numpy it
works but is slow for big wavetables.

Usage (inside the docker container):
    python3 /PRJ/IOCore/tools/make_wavetables.py
"""
import argparse
import glob
import math
import os
import struct
import subprocess
import sys

try:
    import numpy as np
except ImportError:
    np = None

WAVES = 64                          # waves per table (--waves)
MAX_TABLES = 8                      # (--tables)
LEN = 256                           # samples per stored wave
MAXH = LEN // 2 - 1                 # 127 harmonics
LEVEL_HARM = [127, 36, 10, 3]       # harmonics per band-limit level
LEVELS = len(LEVEL_HARM)

HERE = os.path.dirname(os.path.abspath(__file__))
DEF_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "wavetables"))
DEF_SKETCH = os.path.normpath(os.path.join(HERE, "..", "HimaliaSketch"))


# ------------------------------------------------------------- built-in spectra
# spectrum = list of (cos, sin) amplitude for harmonics 1..MAXH
def spec_sines(amps):
    return [(0.0, a) for a in amps]


def sine():
    return spec_sines([1.0] + [0.0] * (MAXH - 1))


def triangle():
    return spec_sines([((1.0 if (n // 2) % 2 == 0 else -1.0) / (n * n)) if n % 2 else 0.0
                       for n in range(1, MAXH + 1)])


def saw():
    return spec_sines([1.0 / n for n in range(1, MAXH + 1)])


def square():
    return spec_sines([(1.0 / n if n % 2 else 0.0) for n in range(1, MAXH + 1)])


def pulse(width):
    out = []
    for n in range(1, MAXH + 1):
        a = 2.0 / (n * math.pi) * math.sin(n * math.pi * width)
        out.append((a * math.cos(n * math.pi * width), a * math.sin(n * math.pi * width)))
    return out


def formant(center, width=2.5, tilt=0.6):
    return spec_sines([(1.0 / n ** tilt) * (0.15 + 3.0 * math.exp(-((n - center) / width) ** 2))
                       for n in range(1, MAXH + 1)])


def mix(a, b, t):
    return [(x0 * (1 - t) + y0 * t, x1 * (1 - t) + y1 * t) for (x0, x1), (y0, y1) in zip(a, b)]


def from_keyframes(keys):
    out = []
    for i in range(WAVES):
        pos = i * (len(keys) - 1) / (WAVES - 1)
        k = min(int(pos), len(keys) - 2)
        out.append(mix(keys[k], keys[k + 1], pos - k))
    return out


BUILTIN = [
    ("basic (built-in)", lambda: from_keyframes([sine(), triangle(), saw(), square(), pulse(0.25), pulse(0.1)])),
    ("formant (built-in)", lambda: [formant(2 * 12 ** (i / (WAVES - 1))) for i in range(WAVES)]),
]


# ------------------------------------------------------------- wav import
def serum_frame_size(path):
    d = open(path, "rb").read()
    i = 12
    while i + 8 <= len(d):
        cid = d[i:i + 4]
        sz = struct.unpack("<I", d[i + 4:i + 8])[0]
        if cid == b"clm ":
            txt = d[i + 8:i + 8 + sz].decode("latin1", "replace")
            digits = "".join(ch for ch in txt[3:] if ch.isdigit() or ch == " ").split()
            if txt.startswith("<!>") and digits:
                return int(digits[0])
        i += 8 + sz + (sz & 1)
    return None


def load_wav_mono(path):
    raw = subprocess.check_output(["sox", "-V1", path, "-t", "raw", "--bits", "16",
                                   "--encoding", "signed-integer", "--endian", "little",
                                   "-c", "1", "-"])
    n = len(raw) // 2
    return [v / 32768.0 for v in struct.unpack("<%dh" % n, raw[: n * 2])]


def spectrum_of_frame(frame):
    n = len(frame)
    if np is not None:
        X = np.fft.rfft(np.asarray(frame, dtype=float))
        out = []
        for h in range(1, MAXH + 1):
            c = X[h] if h < len(X) else 0j
            out.append((2 * c.real / n, -2 * c.imag / n))
        return out
    out = []
    for h in range(1, MAXH + 1):
        c = s = 0.0
        w = 2 * math.pi * h / n
        for k, x in enumerate(frame):
            c += x * math.cos(w * k)
            s += x * math.sin(w * k)
        out.append((2 * c / n, 2 * s / n))
    return out


def table_from_wav(path, default_frame):
    frame_len = serum_frame_size(path) or default_frame
    data = load_wav_mono(path)
    frames = len(data) // frame_len
    if frames < 1:
        raise SystemExit("%s is shorter than one frame of %d samples" % (path, frame_len))
    # spread WAVES picks over the file; with fewer frames, frames repeat so the
    # knob still covers the whole table evenly
    picks = [round(i * (frames - 1) / (WAVES - 1)) for i in range(WAVES)] if frames > 1 else [0] * WAVES
    return [spectrum_of_frame(data[p * frame_len:(p + 1) * frame_len]) for p in picks], frames, frame_len


# ------------------------------------------------------------- render
def render(spec, harmonics):
    # Lanczos sigma softens the top harmonics (less ringing)
    def sigma(h):
        if harmonics < 2:
            return 1.0
        x = math.pi * h / (harmonics + 1)
        return math.sin(x) / x
    if np is not None:
        X = np.zeros(LEN // 2 + 1, dtype=complex)
        for h in range(1, harmonics + 1):
            c, s = spec[h - 1]
            X[h] = sigma(h) * (c - 1j * s) * LEN / 2
        return list(np.fft.irfft(X, LEN))
    out = []
    for k in range(LEN):
        ph = 2 * math.pi * k / LEN
        v = 0.0
        for h in range(1, harmonics + 1):
            c, s = spec[h - 1]
            if c or s:
                v += sigma(h) * (c * math.cos(h * ph) + s * math.sin(h * ph))
        out.append(v)
    return out


def main():
    global WAVES, MAX_TABLES
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", default=DEF_DIR, help="folder with wavetable .wav files")
    ap.add_argument("--sketch", default=DEF_SKETCH)
    ap.add_argument("--frame", type=int, default=2048, help="frame size when the wav has no Serum clm chunk")
    ap.add_argument("--tables", type=int, default=MAX_TABLES, help="max number of tables (default 8)")
    ap.add_argument("--waves", type=int, default=WAVES, help="waves per table (default 64)")
    a = ap.parse_args()
    WAVES, MAX_TABLES = a.waves, a.tables
    budget = MAX_TABLES * WAVES * LEVELS * (LEN * 3 // 2)
    if budget > 950 * 1024:
        raise SystemExit("too big: %d tables x %d waves = %d KB (max ~950 KB)" % (MAX_TABLES, WAVES, budget // 1024))

    tables = []
    files = sorted(glob.glob(os.path.join(a.dir, "*.wav")))
    if len(files) > MAX_TABLES:
        print("note: %d wav files, only the first %d are used" % (len(files), MAX_TABLES))
        files = files[:MAX_TABLES]
    for p in files:
        spec, frames, flen = table_from_wav(p, a.frame)
        tables.append((os.path.basename(p), spec))
        print("  table %2d  %-30s %d frames of %d" % (len(tables), os.path.basename(p), frames, flen))
    for name, fn in BUILTIN:
        if len(tables) < MAX_TABLES:
            tables.append((name, fn()))
            print("  table %2d  %s" % (len(tables), name))

    out = ["// GENERATED by tools/make_wavetables.py -- do not edit by hand\n",
           "#pragma once\n#include <stdint.h>\n\n",
           "#define WT_TABLES %d\n#define WT_WAVES  %d\n#define WT_LEN    %d\n#define WT_LEVELS %d\n"
           "#define WT_BYTES  %d   // packed 12 bit: 2 samples in 3 bytes\n\n"
           % (len(tables), WAVES, LEN, LEVELS, LEN * 3 // 2),
           "static const uint8_t wt_level_harm[WT_LEVELS] = { %s };\n\n" % ", ".join(map(str, LEVEL_HARM)),
           "// tables (RATCHET knob, left to right):\n"]
    for i, (name, _) in enumerate(tables):
        out.append("//  %2d  %s\n" % (i + 1, name))
    out.append("\nstatic const uint8_t wavetables[WT_TABLES * WT_WAVES][WT_LEVELS][WT_BYTES] = {\n")
    for t, (name, spec_list) in enumerate(tables):
        rendered = [[render(spec, h) for h in LEVEL_HARM] for spec in spec_list]
        # one scale per table, so the morph keeps the loudness relations of the original
        tpeak = max(max(abs(v) for v in lv[0]) for lv in rendered) or 1.0
        for w, levels in enumerate(rendered):
            peak0 = max(abs(v) for v in levels[0]) or 1.0
            out.append("  { // table %d (%s) wave %d\n" % (t + 1, name, w))
            for lv in levels:
                # band limiting can remove most of the sound at high notes: boost, at most 4x
                peak = max(abs(v) for v in lv) or 1e-9
                boost = min(4.0, max(1.0, peak0 / peak))
                scale = min(32000.0 / tpeak * boost, 32000.0 / peak)   # never clip
                v12 = [max(0, min(4095, int(round(v * scale / 16.0)) + 2048)) for v in lv]
                packed = []
                for k in range(0, LEN, 2):
                    p0, p1 = v12[k], v12[k + 1]
                    packed += [p0 & 0xFF, (p0 >> 8) | ((p1 & 0x0F) << 4), p1 >> 4]
                out.append("    {" + ",".join(map(str, packed)) + "},\n")
            out.append("  },\n")
    out.append("};\n")
    dst = os.path.join(a.sketch, "wavetables.h")
    with open(dst, "w") as f:
        f.write("".join(out))
    print("wrote %s (%d tables x %d waves, %.0f KB)" % (dst, len(tables), WAVES, len(tables) * WAVES * LEVELS * LEN * 1.5 / 1024.0))


if __name__ == "__main__":
    sys.exit(main())
