# open-simh-extended/tools/make_tape_sounds.py  v2
#
# v2 (2026-11-08): TK50 cartridge streamer set (sound/tk50, SND_STREAMER),
#                  shaped after the measurements of a TZ30 recording
#
# Synthesizes the tape transport sounds of the SIMH sound system (SND_REEL in
# sim_sound.c) for 9-track reel-to-reel drives.  There are no recordings of
# a real drive (Ulli, 2026-11-07: "Please synthesize the sounds: reel motors,
# capstan, start/stop transients, load and unload"), so everything here is a
# model.  The TS11 set is SIMH's default tape set (sound/default_tape, generic
# file names); the TSV05 gets its own files where its transport differs.
#
# Files (names as sim_reel loads them: <dir>/<prefix>_<name>.wav, else
# default_tape/<name>.wav):
#
#   load.wav          the load sequence (TS11: 8 s, TSV05: 30 s with the air
#                     threading), plays once; SIMH cuts it at LOADTIME
#   unload.wav        the unload from BOT (TS11: 8 s, TSV05: 15 s)
#   rewind_start.wav  relay and the reels running up (about 1.2 s)
#   rewind.wav        one reel motor at REF_RPS revolutions per second, a
#                     seamless 2 s loop.  SIMH plays it twice (take-up and
#                     supply reel) at rate = reel rps / REF_RPS and glides
#                     the rate as the radii change, so the pitch rises on
#                     one reel and falls on the other
#   rewind_stop.wav   the reels slowing down, BOT overshoot, the tape
#                     creeping forward to BOT, stop (about 2 s)
#   start.wav         start transient of one record (capstan kick, tension
#                     arm thud), about 60 ms
#   motion.wav        tape moving at read/write speed, a seamless 1 s loop
#                     (gated by SIMH for every record)
#   stop.wav          stop transient, about 60 ms
#
# Drives:
#   ts11   TS11 (TS04 transport): capstan drive, tension arms, AC reel
#          motors; 45 in/s start/stop, rewind 150 in/s by the capstan.
#          Written to default_tape/ with generic names (all eight files).
#   tsv05  TSV05 (TS05 transport): reel-to-reel without capstan, compliance
#          arm, streaming at 25 in/s, automatic threading by air flow (a
#          blower runs during the load).  Own files tsv05_load.wav,
#          tsv05_unload.wav, tsv05_motion.wav, tsv05_start.wav,
#          tsv05_stop.wav; the rewind sounds come from default_tape.
#
#   tk50   TK50 / TZ30 cartridge streamer (SND_STREAMER in sim_sound.c,
#          loaded as tk50/tk50_<name>.wav).  Ulli, 2026-11-08: synthesize
#          sounds matching the measured values of a TZ30 recording (the
#          recording itself is not ours to ship).  Measured there:
#          - forward streaming: a capstan/reel tone at 457 Hz with harmonics
#            914/1371/1828 Hz, octave bands (50-100 ... 6.4k-12.8k Hz,
#            relative dB) -35 -25 -18 -9 0 -8 -18 -13, about -25.5 dBFS RMS
#          - reverse: 457 + 501 Hz beating, bands -27 -17 -10 0 -1 -6 -10 -14,
#            about 6 dB quieter
#          - start: a whoosh of about 0.3 s, +8 dB over streaming, 300-1000 Hz
#          - stop: about 0.4 s, braking tones near 585 Hz, a 50 ms dip
#          - reverse start: 0.3 s, 580-620 Hz; reverse stop ends in a dip
#          - long runs (load/calibration): a tone falling from about 497 to
#            485 Hz at -25 dB; hissy runs at 5-12 kHz falling 492 -> 461 Hz
#          Files: tk50_load.wav (16 s; SIMH cuts it at the random load time
#          of 10-15 s), tk50_calibrate.wav (8 s loop, new cartridges),
#          tk50_unload.wav (9 s), tk50_start.wav, tk50_forward.wav (1 s
#          loop), tk50_stop.wav, tk50_rstart.wav, tk50_reverse.wav (1 s loop,
#          also the rewind, rate-glided by SIMH), tk50_rstop.wav.
#
# Model:
# - Motors are sums of harmonics of their rotation rate from phase
#   accumulators (smooth glides); a "whine" region of upper harmonics (motor
#   slots, commutator), soft distortion (tanh).
# - Air and tape noise is noise shaped in the frequency domain; for loops it
#   is periodic over the loop length, and every periodic component completes
#   whole cycles, so the loops have no seam.
# - Transients: a short low-frequency thud (tension arm or compliance arm),
#   a click, and a burst of motor whine gliding up (start) or down (stop).
# - Levels: peaks about -8 dBFS; the motion loop and the rewind loops are
#   quieter (they overlap).
#
# Usage:
#   python3 -I make_tape_sounds.py <sound directory>
#     writes <sound directory>/default_tape/*.wav,
#            <sound directory>/tsv05/tsv05_*.wav and
#            <sound directory>/tk50/tk50_*.wav
#   python3 -I make_tape_sounds.py --check <sound directory>
#     also prints the length, peak and RMS of every file and the seam step
#     of the loops
#
# Output: mono, 16 bit WAV; 44100 Hz (load/unload: 22050 Hz, they are long).
# Needs numpy.

import os
import sys
import wave

import numpy as np

RATE = 44100
LONG_RATE = 22050
REF_RPS = 8.0                   # rotation rate of rewind.wav (sim_sound.c REEL_SND_REF_RPS)


def rng_for(name):
    """Reproducible noise per file."""
    return np.random.default_rng(sum((i + 1) * ord(c) for i, c in enumerate(name)) * 7919)


def shaped_noise(n, rate, lo, hi, rng, periodic=True, tilt=0.0):
    """Band-limited noise of n samples (periodic over n when periodic)."""
    spec = rng.normal(size=n // 2 + 1) + 1j * rng.normal(size=n // 2 + 1)
    f = np.fft.rfftfreq(n, 1.0 / rate)
    band = np.exp(-0.5 * (np.log(np.maximum(f, 1.0) / np.sqrt(lo * hi)) /
                          (0.5 * np.log(hi / lo))) ** 2)
    if tilt:
        band = band * np.maximum(f, 1.0) ** tilt
    x = np.fft.irfft(spec * band, n)
    return x / (np.std(x) + 1e-12)


def harmonics(phase, amps):
    """Sum of harmonics k*phase (phase in cycles), amps[k-1] for k = 1.."""
    out = np.zeros_like(phase)
    for k, a in enumerate(amps, 1):
        if a:
            out += a * np.sin(2 * np.pi * k * phase)
    return out


def motor(freq, rate, profile, whine_order, whine_amp, dist=1.5):
    """A motor turning at freq(t) (array, rev/s): low harmonics + slot whine."""
    phase = np.cumsum(freq) / rate
    x = harmonics(phase, profile)
    for j, a in enumerate((1.0, 0.45, 0.2)):
        x += whine_amp * a * np.sin(2 * np.pi * whine_order * (j + 1) * phase)
    return np.tanh(dist * x) / dist


def fade(x, rate, fin=0.005, fout=0.005):
    x = x.copy()
    a = int(fin * rate)
    b = int(fout * rate)
    if a:
        x[:a] *= np.linspace(0, 1, a)
    if b:
        x[-b:] *= np.linspace(1, 0, b)
    return x


def env_exp(n, rate, attack, decay):
    t = np.arange(n) / rate
    return (1 - np.exp(-t / max(attack, 1e-4))) * np.exp(-t / decay)


def thud(n, rate, f0, decay, rng):
    t = np.arange(n) / rate
    f = f0 * (1 + 0.6 * np.exp(-t / 0.01))
    return np.sin(2 * np.pi * np.cumsum(f) / rate) * env_exp(n, rate, 0.002, decay)


def click(n, rate, rng, decay=0.004):
    return shaped_noise(n, rate, 1500, 7000, rng, periodic=False) * env_exp(n, rate, 0.0003, decay)


def normalize(x, peak_db):
    p = np.max(np.abs(x)) + 1e-12
    return x * (10 ** (peak_db / 20.0) / p)


def write_wav(path, x, rate):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    y = np.clip(np.round(x * 32767.0), -32768, 32767).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(y.tobytes())


# ---------------------------------------------------------------- TS11 set

REEL_PROFILE = (0.25, 0.5, 0.35, 0.2, 0.12, 0.08)       # reel motor low harmonics
CAPSTAN_PROFILE = (0.2, 0.3, 0.25, 0.15)


def reel_loop(rate, secs, rps, whine_order, rng, air_lo, air_hi, air_lvl):
    """Seamless loop of one reel motor at rps (rps * secs whole revolutions)."""
    n = int(round(secs * rate))
    f = np.full(n, rps)
    x = motor(f, rate, REEL_PROFILE, whine_order, 0.35)
    # windage: noise amplitude-modulated once per revolution (flanges)
    am = 1 + 0.35 * np.sin(2 * np.pi * rps * np.arange(n) / rate)
    x = x + air_lvl * shaped_noise(n, rate, air_lo, air_hi, rng) * am
    return x


def ts11_rewind(rng):
    # rps * 2 s = 16 whole revolutions; the slot whine at 48 * rps = 384 Hz
    x = reel_loop(RATE, 2.0, REF_RPS, 48, rng, 600, 3500, 0.28)
    return normalize(x, -14.0)


def ts11_motion(rng):
    n = RATE                                            # 1 s
    t = np.arange(n) / RATE
    cap = motor(np.full(n, 12.0), RATE, CAPSTAN_PROFILE, 24, 0.5)   # 288 Hz commutator
    reels = 0.4 * motor(np.full(n, 3.0), RATE, REEL_PROFILE, 48, 0.3)
    hiss = 0.22 * shaped_noise(n, RATE, 2500, 9000, rng)             # tape over heads
    hum = 0.15 * np.sin(2 * np.pi * 120 * t)
    return normalize(cap + reels + hiss + hum, -16.0)


def transient(rng, start=True, dur=0.06, thud_f=55.0, whine=288.0, lvl=-9.0):
    n = int(dur * RATE)
    t = np.arange(n) / RATE
    if start:
        f = whine * (0.4 + 0.6 * (1 - np.exp(-t / 0.008)))
        env = env_exp(n, RATE, 0.001, 0.025)
    else:
        f = whine * np.exp(-t / 0.02)
        env = env_exp(n, RATE, 0.0005, 0.02)
    w = 0.5 * np.sin(2 * np.pi * np.cumsum(f) / RATE) * env
    x = w + 0.9 * thud(n, RATE, thud_f, 0.018, rng) + 0.35 * click(n, RATE, rng)
    return normalize(fade(x, RATE, 0.0005, 0.004), lvl)


def run_up(rate, secs, f0, f1, tau, order, rng, air=0.25, profile=REEL_PROFILE):
    n = int(secs * rate)
    t = np.arange(n) / rate
    g = (1 - np.exp(-t / tau)) / (1 - np.exp(-secs / tau))
    f = f0 + (f1 - f0) * g
    x = motor(f, rate, profile, order, 0.35)
    x += air * shaped_noise(n, rate, 600, 3500, rng, periodic=False) * (f / max(f1, f0))
    return x


def ts11_rewind_start(rng):
    n = int(1.2 * RATE)
    x = run_up(RATE, 1.2, 0.3, REF_RPS * 1.4, 0.35, 48, rng)
    x *= np.minimum(1, np.arange(n) / (0.15 * RATE))
    x[:int(0.06 * RATE)] += transient(rng, True, 0.06, 45, 200, 0)[:int(0.06 * RATE)] * 0.6
    return normalize(fade(x, RATE, 0.001, 0.25), -13.0)


def ts11_rewind_stop(rng):
    # 1.0 s run-down, overshoot past BOT, 0.6 s creep forward at 45 in/s, stop
    a = int(1.0 * RATE)
    t = np.arange(a) / RATE
    f = REF_RPS * 1.6 * np.exp(-t / 0.35)
    x1 = motor(f, RATE, REEL_PROFILE, 48, 0.35) + 0.25 * shaped_noise(a, RATE, 600, 3500, rng, False) * f / f[0]
    x1 = fade(x1, RATE, 0.2, 0.05)
    creep = ts11_motion(rng)[:int(0.6 * RATE)]
    creep = fade(creep, RATE, 0.01, 0.01)
    gap = np.zeros(int(0.15 * RATE))
    st = transient(rng, True)
    sp = transient(rng, False)
    x = np.concatenate([x1, gap, st, creep, sp, np.zeros(int(0.1 * RATE))])
    return normalize(x, -13.0)


def ts11_load(rng, secs=8.0, rate=LONG_RATE):
    # relay click, tension arms come up (thud), reels and capstan move the
    # leader forward until BOT (~16 ft at 45 in/s = 4.3 s), stop
    n = int(secs * rate)
    x = np.zeros(n)
    k = int(0.05 * rate)
    x[:k] += 0.6 * click(k, rate, rng, 0.006)
    a = int(0.4 * rate)
    x[a:a + int(0.3 * rate)] += 0.8 * thud(int(0.3 * rate), rate, 40, 0.12, rng)
    m0, m1 = int(1.2 * rate), int(6.6 * rate)
    m = m1 - m0
    mot = np.tile(ts11_motion(rng)[::RATE // rate], int(np.ceil(m / rate)) + 1)[:m]
    reel = run_up(rate, m / rate, 1.0, 3.5, 0.8, 48, rng, 0.15)
    seg = fade(0.8 * mot / (np.max(np.abs(mot)) + 1e-9) * 0.5 + 0.5 * reel, rate, 0.1, 0.05)
    x[m0:m1] += seg
    s = int(6.6 * rate)
    x[s:s + int(0.3 * rate)] += 0.7 * thud(int(0.3 * rate), rate, 50, 0.05, rng)
    return normalize(fade(x, rate, 0.001, 0.3), -10.0)


def ts11_unload(rng, secs=8.0, rate=LONG_RATE):
    # tension off the capstan, the supply reel winds the leader on slowly,
    # speeding up as the take-up reel empties; the tape end flaps, stop
    n = int(secs * rate)
    x = np.zeros(n)
    k = int(0.05 * rate)
    x[:k] += 0.6 * click(k, rate, rng, 0.006)
    a, b = int(0.3 * rate), int(6.8 * rate)
    run = run_up(rate, (b - a) / rate, 0.5, 6.0, 2.5, 48, rng, 0.3)
    x[a:b] += fade(run, rate, 0.2, 0.05)
    # tape end flapping: a few fast clicks, decaying
    fl0 = int(5.6 * rate)
    for i in range(18):
        p = fl0 + int(i * 0.065 * rate)
        if p + k < n:
            x[p:p + k] += (0.5 * (1 - i / 20.0)) * click(k, rate, rng, 0.003)
    s = int(6.9 * rate)
    x[s:s + int(0.3 * rate)] += 0.5 * thud(int(0.3 * rate), rate, 45, 0.06, rng)
    return normalize(fade(x, rate, 0.001, 0.3), -10.0)


# ---------------------------------------------------------------- TSV05 set

def tsv05_motion(rng):
    # no capstan: two reel motors at 25 in/s (1.0 .. 2.2 rev/s) and tape hiss
    n = RATE
    a = motor(np.full(n, 2.0), RATE, REEL_PROFILE, 72, 0.4)          # 144 Hz
    b = 0.7 * motor(np.full(n, 3.0), RATE, REEL_PROFILE, 72, 0.4)    # 216 Hz
    hiss = 0.18 * shaped_noise(n, RATE, 2000, 8000, rng)
    return normalize(a + b + hiss, -18.0)


def tsv05_load(rng, secs=30.0, rate=LONG_RATE):
    # door lock solenoid, the reel is seated (thud), the blower runs up and
    # blows the leader through the path (broad air noise, ~20 s), the
    # take-up reel catches it, the tape runs forward to BOT, blower off
    n = int(secs * rate)
    x = np.zeros(n)
    k = int(0.05 * rate)
    x[:k] += 0.7 * click(k, rate, rng, 0.008)
    x[int(0.6 * rate):int(0.9 * rate)] += 0.8 * thud(int(0.3 * rate), rate, 35, 0.1, rng)
    b0, b1 = int(1.5 * rate), int(24.0 * rate)
    bn = b1 - b0
    t = np.arange(bn) / rate
    up = np.minimum(1, t / 2.0)
    blower = shaped_noise(bn, rate, 300, 2500, rng, False) * up
    blower += 0.3 * np.sin(2 * np.pi * 180 * t) * up                # blower motor hum
    # leader attempts: the reel turns in short bursts
    for i in range(6):
        p = int((3 + i * 3.2) * rate)
        q = p + int(1.0 * rate)
        if q < bn:
            blower[p:q] += 0.6 * fade(run_up(rate, 1.0, 0.5, 2.5, 0.3, 72, rng, 0.0), rate, 0.05, 0.1)
    x[b0:b1] += fade(0.45 * blower, rate, 0.3, 1.5)
    m0, m1 = int(23.0 * rate), int(28.8 * rate)
    mot = np.tile(tsv05_motion(rng)[::RATE // rate], 8)[:m1 - m0]
    x[m0:m1] += fade(1.4 * mot / (np.max(np.abs(mot)) + 1e-9) * 0.5, rate, 0.4, 0.05)
    s = int(28.8 * rate)
    x[s:s + int(0.3 * rate)] += 0.5 * thud(int(0.3 * rate), rate, 45, 0.05, rng)
    return normalize(fade(x, rate, 0.001, 0.5), -10.0)


def tsv05_unload(rng, secs=15.0, rate=LONG_RATE):
    n = int(secs * rate)
    x = np.zeros(n)
    k = int(0.05 * rate)
    a, b = int(0.2 * rate), int(12.5 * rate)
    run = run_up(rate, (b - a) / rate, 0.5, 5.0, 4.0, 72, rng, 0.25)
    x[a:b] += fade(run, rate, 0.3, 0.05)
    fl0 = int(11.0 * rate)
    for i in range(20):
        p = fl0 + int(i * 0.07 * rate)
        x[p:p + k] += (0.45 * (1 - i / 22.0)) * click(k, rate, rng, 0.003)
    s = int(12.6 * rate)
    x[s:s + int(0.3 * rate)] += 0.5 * thud(int(0.3 * rate), rate, 40, 0.06, rng)
    u = int(13.5 * rate)                                            # door unlocks
    x[u:u + k] += 0.7 * click(k, rate, rng, 0.008)
    return normalize(fade(x, rate, 0.001, 0.3), -10.0)


# ---------------------------------------------------------------- TK50 set

# Octave bands of the TZ30 recording (lower edges 50, 100, ... 6400 Hz),
# relative dB
TK50_FWD_BANDS = (-35, -25, -18, -9, 0, -8, -18, -13)
TK50_REV_BANDS = (-27, -17, -10, 0, -1, -6, -10, -14)
TK50_TONE = 457.0               # forward tone (Hz)
TK50_REV_TONE = 501.0           # second reverse tone (beats with 457 Hz)
TK50_FWD_DB = -20.5             # forward loop RMS (recording -25.5 dB, +5 dB)


def octave_noise(n, rate, bands, rng, periodic=True):
    """Noise whose octave band levels follow bands (dB, from 50 Hz up);
    interpolated in log frequency, rolled off below 50 Hz and above 12.8 kHz."""
    spec = rng.normal(size=n // 2 + 1) + 1j * rng.normal(size=n // 2 + 1)
    f = np.maximum(np.fft.rfftfreq(n, 1.0 / rate), 1.0)
    centers = 50.0 * 2 ** (np.arange(len(bands)) + 0.5)     # band centres
    db = np.interp(np.log2(f), np.log2(centers), np.array(bands, float),
                   left=bands[0] - 20, right=bands[-1] - 20)
    db = db - 10 * np.log10(np.maximum(f, 1.0) / 50.0)      # per Hz: octaves widen
    x = np.fft.irfft(spec * 10 ** (db / 20.0), n)
    return x / (np.std(x) + 1e-12)


def rms_to(x, db):
    return x * (10 ** (db / 20.0) / (np.sqrt(np.mean(x ** 2)) + 1e-12))


def tone_set(t, f0, amps, phase0=0.0):
    """Harmonics of f0 (constant or array of Hz) at times t."""
    if np.isscalar(f0):
        ph = f0 * t
    else:
        ph = np.cumsum(f0) / RATE
    return harmonics(ph + phase0, amps)


def tk50_forward(rng, secs=1.0):
    # whole cycles of 457 Hz per second: a seamless 1 s loop
    n = int(secs * RATE)
    t = np.arange(n) / RATE
    tone = tone_set(t, TK50_TONE, (1.0, 0.55, 0.3, 0.2))
    noise = octave_noise(n, RATE, TK50_FWD_BANDS, rng)
    x = 0.35 * tone / np.std(tone) + noise
    return rms_to(x, TK50_FWD_DB)


def tk50_reverse(rng, secs=1.0):
    # 457 and 501 Hz beat at 44 Hz (both whole cycles per second)
    n = int(secs * RATE)
    t = np.arange(n) / RATE
    tone = tone_set(t, TK50_TONE, (1.0, 0.4, 0.2)) + 0.8 * tone_set(t, TK50_REV_TONE, (1.0, 0.3))
    noise = octave_noise(n, RATE, TK50_REV_BANDS, rng)
    x = 0.45 * tone / np.std(tone) + noise
    return rms_to(x, TK50_FWD_DB - 5.5)


def tk50_start(rng, rev=False):
    # whoosh of the tape running up: about 0.3 s, peak +8 dB over streaming
    # at about 0.15 s, band 300-1000 Hz, the tone gliding up to speed
    secs = 0.32
    n = int(secs * RATE)
    t = np.arange(n) / RATE
    env = np.exp(-0.5 * ((t - 0.15) / 0.07) ** 2)             # ~290 ms wide at its foot
    lo, hi = (580, 1200) if rev else (300, 1000)
    whoosh = shaped_noise(n, RATE, lo, hi, rng, periodic=False)
    f_end = 600.0 if rev else TK50_TONE                        # reverse start 580-620 Hz
    f = f_end * (0.55 + 0.45 * (1 - np.exp(-t / 0.08)))
    glide = np.sin(2 * np.pi * np.cumsum(f) / RATE)
    x = env * (whoosh + 0.6 * glide)
    x = fade(x, RATE, 0.01, 0.06)
    lvl = TK50_FWD_DB - (5.5 if rev else 0.0) + 8.0
    return normalize(x, lvl + 3.0)                             # peak ~3 dB over the RMS level


def tk50_stop(rng, rev=False):
    # braking: tones near 585 Hz falling, a short dip (about 50 ms) at the
    # stop, a small settling knock; forward 0.4 s at -10 dB, reverse ends
    # in a dip near -38 dB
    secs = 0.40 if not rev else 0.30
    n = int(secs * RATE)
    t = np.arange(n) / RATE
    f = 585.0 * np.exp(-t / 0.5)
    brake = np.sin(2 * np.pi * np.cumsum(f) / RATE) + 0.5 * np.sin(2 * np.pi * np.cumsum(1.5 * f) / RATE)
    noise = octave_noise(n, RATE, TK50_REV_BANDS if rev else TK50_FWD_BANDS, rng, periodic=False)
    env = np.exp(-t / (0.08 if rev else 0.12))
    x = env * (0.6 * brake + noise)
    d0 = int((secs - 0.12) * RATE)                             # the dip, then the knock
    x[d0:d0 + int(0.05 * RATE)] *= 0.1
    k = int(0.06 * RATE)
    x[d0 + int(0.05 * RATE):d0 + int(0.05 * RATE) + k] += 0.4 * thud(k, RATE, 90, 0.015, rng)
    x = fade(x, RATE, 0.004, 0.02)
    return normalize(x, -10.0 if not rev else -16.0)


def tk50_run(rng, secs, f0, f1, bands, hiss=0.0, rate=LONG_RATE):
    """A long run (calibration track read, BOT search): a tone falling from
    f0 to f1 Hz over the run, octave noise, optional 5-12 kHz hiss."""
    n = int(secs * rate)
    t = np.arange(n) / rate
    f = f0 + (f1 - f0) * t / secs
    tone = harmonics(np.cumsum(f) / rate, (1.0, 0.5, 0.25))
    noise = octave_noise(n, rate, bands, rng, periodic=False)
    x = 0.35 * tone / np.std(tone) + noise
    if hiss and rate > 24000:
        x += hiss * shaped_noise(n, rate, 5000, 12000, rng, periodic=False)
    return fade(x, rate, 0.15, 0.15)


def tk50_load(rng, secs=16.0, rate=LONG_RATE):
    # handle down: the solenoid clicks, the leader buckles (motor bursts),
    # the tape winds slowly to BOT (6 in/s, a low hum), then the head finds
    # the tracks: the calibration track is read forward and in reverse
    # (tones falling about 497 -> 485 Hz, -25 dB in the recording)
    n = int(secs * rate)
    x = np.zeros(n)
    k = int(0.06 * rate)
    x[int(0.05 * rate):int(0.05 * rate) + k] += 0.8 * click(k, rate, rng, 0.006)   # solenoid
    for i, p in enumerate((0.5, 0.9, 1.3)):                                   # buckling
        q = int(p * rate)
        m = int(0.25 * rate)
        x[q:q + m] += 0.5 * fade(run_up(rate, 0.25, 3.0, 6.0, 0.08, 16, rng, 0.2), rate, 0.02, 0.05)
    k2 = int(0.08 * rate)
    x[int(1.7 * rate):int(1.7 * rate) + k2] += 0.5 * thud(k2, rate, 70, 0.02, rng)
    a, b = int(2.0 * rate), int(4.0 * rate)                                   # to BOT, slowly
    tt = np.arange(b - a) / rate
    hum = np.sin(2 * np.pi * 37 * tt) + 0.5 * np.sin(2 * np.pi * 74 * tt)
    x[a:b] += fade(0.15 * hum + 0.1 * octave_noise(b - a, rate, TK50_FWD_BANDS, rng, False), rate, 0.2, 0.2)
    lvl = 10 ** ((TK50_FWD_DB - 5.0) / 20.0)                                 # ~ -25 dB
    pos = 4.2
    fwd = True
    while pos < secs - 0.5:                                                   # calibration track
        d = min(3.4 if fwd else 3.0, secs - 0.3 - pos)
        seg = tk50_run(rng, d, 497 if fwd else 493, 485 if fwd else 482,
                       TK50_FWD_BANDS if fwd else TK50_REV_BANDS, 0.0, rate)
        seg = seg * (lvl / (np.sqrt(np.mean(seg ** 2)) + 1e-12)) * (1.0 if fwd else 0.6)
        p = int(pos * rate)
        x[p:p + len(seg)] += seg
        pos += d + 0.35
        fwd = not fwd
    return fade(x, rate, 0.001, 0.3)


def tk50_calibrate(rng, secs=8.0, rate=LONG_RATE):
    # a new cartridge: the drive writes and reads the calibration tracks for
    # about 2 minutes; a seamless loop of forward and reverse runs (the
    # hissy runs of the recording: 492 -> 461 Hz)
    n = int(secs * rate)
    x = np.zeros(n)
    lvl = 10 ** ((TK50_FWD_DB - 5.0) / 20.0)
    for p0, d, fwd in ((0.2, 3.6, True), (4.2, 3.4, False)):
        seg = tk50_run(rng, d, 492 if fwd else 488, 461 if fwd else 465,
                       TK50_FWD_BANDS if fwd else TK50_REV_BANDS, 0.0, rate)
        seg = seg * (lvl / (np.sqrt(np.mean(seg ** 2)) + 1e-12)) * (1.0 if fwd else 0.6)
        p = int(p0 * rate)
        x[p:p + len(seg)] += seg
        k = int(0.06 * rate)                                                  # direction change
        q = p + len(seg)
        if q + k <= n:
            x[q:q + k] += 0.15 * thud(k, rate, 90, 0.015, rng)
    return x                                                                  # silent ends: no seam


def tk50_unload(rng, secs=9.0, rate=LONG_RATE):
    # after the rewind: the leader is wound back into the cartridge (the
    # supply motor runs, slowing), the leaders unbuckle (motor bursts), the
    # solenoid releases the handle
    n = int(secs * rate)
    x = np.zeros(n)
    a, b = int(0.1 * rate), int(5.5 * rate)
    seg = tk50_run(rng, (b - a) / rate, 470, 380, TK50_REV_BANDS, 0.0, rate)
    seg = seg * (10 ** ((TK50_FWD_DB - 8.0) / 20.0) / (np.sqrt(np.mean(seg ** 2)) + 1e-12))
    x[a:a + len(seg)] += fade(seg, rate, 0.3, 1.0)
    for p in (6.0, 6.5, 7.1):                                                 # unbuckle
        q = int(p * rate)
        m = int(0.3 * rate)
        x[q:q + m] += 0.4 * fade(run_up(rate, 0.3, 6.0, 2.0, 0.1, 16, rng, 0.2), rate, 0.02, 0.08)
    k = int(0.08 * rate)
    x[int(8.2 * rate):int(8.2 * rate) + k] += 0.8 * click(k, rate, rng, 0.008)  # solenoid
    return normalize(fade(x, rate, 0.001, 0.3), -10.0)


# ---------------------------------------------------------------- main

def build(outdir):
    files = []

    def put(rel, x, rate):
        path = os.path.join(outdir, rel)
        write_wav(path, x, rate)
        files.append((rel, x, rate))

    put('default_tape/load.wav', ts11_load(rng_for('load')), LONG_RATE)
    put('default_tape/unload.wav', ts11_unload(rng_for('unload')), LONG_RATE)
    put('default_tape/rewind_start.wav', ts11_rewind_start(rng_for('rstart')), RATE)
    put('default_tape/rewind.wav', ts11_rewind(rng_for('rewind')), RATE)
    put('default_tape/rewind_stop.wav', ts11_rewind_stop(rng_for('rstop')), RATE)
    put('default_tape/start.wav', transient(rng_for('start'), True), RATE)
    put('default_tape/motion.wav', ts11_motion(rng_for('motion')), RATE)
    put('default_tape/stop.wav', transient(rng_for('stop'), False), RATE)
    put('tsv05/tsv05_load.wav', tsv05_load(rng_for('vload')), LONG_RATE)
    put('tsv05/tsv05_unload.wav', tsv05_unload(rng_for('vunload')), LONG_RATE)
    put('tsv05/tsv05_motion.wav', tsv05_motion(rng_for('vmotion')), RATE)
    put('tsv05/tsv05_start.wav', transient(rng_for('vstart'), True, 0.08, 40, 144, -12.0), RATE)
    put('tsv05/tsv05_stop.wav', transient(rng_for('vstop'), False, 0.08, 40, 144, -12.0), RATE)
    put('tk50/tk50_load.wav', normalize(tk50_load(rng_for('kload')), -10.0), LONG_RATE)
    put('tk50/tk50_calibrate.wav', tk50_calibrate(rng_for('kcal')), LONG_RATE)
    put('tk50/tk50_unload.wav', tk50_unload(rng_for('kunload')), LONG_RATE)
    put('tk50/tk50_start.wav', tk50_start(rng_for('kstart')), RATE)
    put('tk50/tk50_forward.wav', tk50_forward(rng_for('kfwd')), RATE)
    put('tk50/tk50_stop.wav', tk50_stop(rng_for('kstop')), RATE)
    put('tk50/tk50_rstart.wav', tk50_start(rng_for('krstart'), True), RATE)
    put('tk50/tk50_reverse.wav', tk50_reverse(rng_for('krev')), RATE)
    put('tk50/tk50_rstop.wav', tk50_stop(rng_for('krstop'), True), RATE)
    return files


def check(files):
    for rel, x, rate in files:
        peak = 20 * np.log10(np.max(np.abs(x)) + 1e-12)
        rms = 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)
        line = '%-30s %6.2f s  peak %6.1f dBFS  rms %6.1f dBFS' % (rel, len(x) / rate, peak, rms)
        if rel.endswith(('rewind.wav', 'motion.wav', 'forward.wav', 'reverse.wav', 'calibrate.wav')):
            d = np.abs(np.diff(x))
            seam = abs(x[0] - x[-1])
            line += '  seam step %.4f (median step %.4f, max %.4f)' % (seam, np.median(d), np.max(d))
        print(line)


def main(argv):
    do_check = False
    args = []
    for a in argv[1:]:
        if a == '--check':
            do_check = True
        else:
            args.append(a)
    if len(args) != 1:
        print(__doc__ if __doc__ else 'usage: make_tape_sounds.py [--check] <sound directory>')
        print('usage: python3 -I make_tape_sounds.py [--check] <sound directory>')
        return 2
    files = build(args[0])
    if do_check:
        check(files)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
