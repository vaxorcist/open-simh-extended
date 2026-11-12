# open-simh-extended/tools/make_tu58_sounds.py  v1
#
# v1 (2026-11-12): first version: the TU58 DECtape II drive
#
# Synthesizes the TU58 sounds of the SIMH sound system (SND_TU58 in
# sim_sound.c, used by the PDP-11 TDC device and the console TU58 TD of the
# VAX-11/730 and 750).  There are no recordings of a real TU58 yet, so this
# is a model; real recordings can replace the files one by one.
#
# The drive (TU58 DECtape II User's Guide EK-0TU58-UG-004, 1.3.1, 1.4.1):
# one servo-controlled DC motor drives the tape through a capstan puck
# pressed against the cartridge's drive roller; inside the cartridge an
# elastomer belt drives the two hubs.  Read/write at 30 in/s, search at
# 60 in/s; the tape stops after each command (a start, a stop, a reversal
# at the turnaround of a search).
#
# Files (names as sim_tu58_load loads them: tu58/tu58_<name>.wav, else the
# default_tape file given in brackets):
#
#   tu58_read.wav     the tape moving at 30 in/s: motor whine, puck and
#                     belt rumble, tape hiss; a seamless 2 s loop
#                     (default_tape motion.wav)
#   tu58_search.wav   the same at 60 in/s: everything twice as fast, a
#                     little louder; seamless 2 s loop
#                     (default_tape motion.wav played at twice the rate)
#   tu58_start.wav    the servo kicking the tape up to speed, 80 ms
#                     (default_tape start.wav)
#   tu58_stop.wav     braking to a stand, 100 ms (default_tape stop.wav)
#   tu58_reverse.wav  a reversal: braking, the direction change, running
#                     up the other way, 150 ms (default_tape stop.wav)
#   tu58_insert.wav   the cartridge slid in and seated (the door-less
#                     slot, a firm push), 0.4 s (default_tape load.wav)
#   tu58_remove.wav   the cartridge pulled out, 0.3 s
#                     (default_tape unload.wav)
#
# Loops: every periodic component completes whole cycles in 2 s (all
# frequencies multiples of 0.5 Hz) and the noise is periodic over the loop
# (shaped in the frequency domain), so the loops have no seam.
#
# Usage:  python3 -I make_tu58_sounds.py [--check] <sound directory>
#         writes <sound directory>/tu58/tu58_<name>.wav (44.1 kHz, 16 bit,
#         mono); --check prints length, peak and RMS (and the loop seams).

import os
import sys
import wave

import numpy as np

RATE = 44100
LOOP_S = 2.0
LOOPS = ('read', 'search')


def rng_for(name):
    """Reproducible noise per file."""
    return np.random.default_rng(sum((i + 1) * ord(c) for i, c in enumerate(name)) * 7919)


def shaped_noise(n, lo, hi, rng):
    """Band-limited noise of n samples, unit RMS, periodic over n."""
    spec = rng.normal(size=n // 2 + 1) + 1j * rng.normal(size=n // 2 + 1)
    f = np.fft.rfftfreq(n, 1.0 / RATE)
    band = np.exp(-0.5 * (np.log(np.maximum(f, 1.0) / np.sqrt(lo * hi)) /
                          (0.5 * np.log(hi / lo))) ** 2)
    x = np.fft.irfft(spec * band, n)
    return x / (np.std(x) + 1e-12)


def db(x):
    return 10 ** (x / 20.0)


def rms_to(x, level_db):
    return x * (db(level_db) / (np.sqrt(np.mean(x ** 2)) + 1e-12))


def peak_to(x, level_db):
    return x * (db(level_db) / (np.max(np.abs(x)) + 1e-12))


def env(n, attack, decay, delay=0.0):
    """Exponential attack/decay envelope (seconds), starting after delay."""
    t = np.arange(n) / RATE - delay
    e = (1 - np.exp(-np.maximum(t, 0) / max(attack, 1e-5))) * np.exp(-np.maximum(t, 0) / decay)
    e[t < 0] = 0.0
    return e


def ring(n, f, decay, delay=0.0):
    """A decaying resonance (a struck plastic part)."""
    t = np.arange(n) / RATE - delay
    x = np.sin(2 * np.pi * f * np.maximum(t, 0)) * np.exp(-np.maximum(t, 0) / decay)
    x[t < 0] = 0.0
    return x


def fade(x, fin=0.003, fout=0.003):
    x = x.copy()
    a, b = int(fin * RATE), int(fout * RATE)
    if a:
        x[:a] *= np.linspace(0, 1, a)
    if b:
        x[-b:] *= np.linspace(1, 0, b)
    return x


def write_wav(path, x):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    y = np.clip(np.round(x * 32767.0), -32768, 32767).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(y.tobytes())


# ------------------------------------------------------------ the motion
# At 30 in/s: the puck (about 0.6 in) turns 16 rev/s; the motor's
# commutator whine and the servo PWM give a tone near 1.1 kHz; the drive
# roller and the belt give a rumble of a few hundred Hz; the tape over the
# head and the guides a soft hiss.  At 60 in/s every rate doubles.

def motion_phase(t, f):
    return 2 * np.pi * f * t


def motion(speed, n, rng, t0=0.0):
    """The moving tape at speed (1 = 30 in/s), constant; periodic when the
    frequencies are multiples of 1/LOOP_S."""
    t = np.arange(n) / RATE + t0
    puck = 16.0 * speed                         # rev/s
    x = np.zeros(n)
    for k, a in ((6, 0.30), (12, 0.22), (18, 0.12), (25, 0.08)):   # roller rumble
        x += a * np.sin(motion_phase(t, puck * k) + k)
    whine = 70.0 * puck                         # 1120 Hz at 30 in/s
    x += 0.10 * np.sin(motion_phase(t, whine)) * (1 + 0.15 * np.sin(motion_phase(t, puck)))
    x += 0.04 * np.sin(motion_phase(t, 2 * whine))
    x += 0.35 * shaped_noise(n, 1500.0 * speed, 7000.0, rng)     # tape hiss
    x += 0.25 * shaped_noise(n, 120.0, 600.0 * speed, rng)       # belt and hubs
    return x


def loop(speed, name):
    n = int(LOOP_S * RATE)
    x = motion(speed, n, rng_for(name))
    return rms_to(x, -26.0 if speed < 1.5 else -23.0)


def ramp(n, v0, v1, rng, level_db):
    """The tape changing speed from v0 to v1 (relative to 30 in/s) over n
    samples: the motion with a gliding rate, integrated phase."""
    v = np.linspace(v0, v1, n)
    t = np.cumsum(np.maximum(v, 0.02)) / RATE   # a 'time' that runs with the speed
    x = np.zeros(n)
    puck = 16.0
    for k, a in ((6, 0.30), (12, 0.22), (18, 0.12), (25, 0.08)):
        x += a * np.sin(2 * np.pi * puck * k * t + k)
    x += 0.10 * np.sin(2 * np.pi * 70.0 * puck * t)
    x *= np.minimum(1.0, np.maximum(v, 0.0) + 0.1)
    x += 0.30 * shaped_noise(n, 1500.0, 7000.0, rng) * np.maximum(v, 0.0)
    return rms_to(x, level_db)


def start():
    rng = rng_for('start')
    n = int(0.08 * RATE)
    x = ramp(n, 0.0, 1.0, rng, -27.0)
    x += 0.06 * ring(n, 180.0, 0.012) + 0.03 * shaped_noise(n, 300.0, 3000.0, rng) * env(n, 0.001, 0.01)
    return fade(peak_to(x, -14.0), 0.002, 0.004)


def stop():
    rng = rng_for('stop')
    n = int(0.10 * RATE)
    x = ramp(n, 1.0, 0.0, rng, -27.0) * np.linspace(1, 0.2, n)
    x += 0.05 * ring(n, 210.0, 0.015, delay=0.07) + \
        0.02 * shaped_noise(n, 400.0, 4000.0, rng) * env(n, 0.001, 0.008, delay=0.07)
    return fade(peak_to(x, -15.0), 0.004, 0.01)


def reverse():
    rng = rng_for('reverse')
    n = int(0.15 * RATE)
    h = n // 2
    x = np.concatenate([ramp(h, 2.0, 0.0, rng, -24.0), ramp(n - h, 0.0, 2.0, rng, -24.0)])
    x += 0.08 * ring(n, 190.0, 0.02, delay=h / RATE) + \
        0.04 * shaped_noise(n, 300.0, 3500.0, rng) * env(n, 0.001, 0.01, delay=h / RATE)
    return fade(peak_to(x, -13.0), 0.004, 0.006)


def insert():
    rng = rng_for('insert')
    n = int(0.40 * RATE)
    slide = shaped_noise(n, 600.0, 5000.0, rng) * env(n, 0.03, 0.12) * 0.25
    seat = 0.6 * ring(n, 820.0, 0.010, delay=0.22) + 0.4 * ring(n, 2300.0, 0.004, delay=0.22) + \
        0.5 * shaped_noise(n, 800.0, 6000.0, rng) * env(n, 0.0005, 0.006, delay=0.22)
    thump = 0.5 * ring(n, 140.0, 0.03, delay=0.225)
    return fade(peak_to(slide + seat + thump, -10.0), 0.005, 0.02)


def remove():
    rng = rng_for('remove')
    n = int(0.30 * RATE)
    release = 0.5 * ring(n, 950.0, 0.008, delay=0.02) + \
        0.4 * shaped_noise(n, 900.0, 6000.0, rng) * env(n, 0.0005, 0.005, delay=0.02)
    slide = shaped_noise(n, 700.0, 5000.0, rng) * env(n, 0.02, 0.08, delay=0.05) * 0.22
    return fade(peak_to(release + slide, -12.0), 0.003, 0.03)


def build(outdir):
    files = []
    for name, x in (('read', loop(1.0, 'read')), ('search', loop(2.0, 'search')),
                    ('start', start()), ('stop', stop()), ('reverse', reverse()),
                    ('insert', insert()), ('remove', remove())):
        rel = os.path.join('tu58', 'tu58_%s.wav' % name)
        write_wav(os.path.join(outdir, rel), x)
        files.append((rel, name, x))
    return files


def check(files):
    for rel, name, x in files:
        peak = 20 * np.log10(np.max(np.abs(x)) + 1e-12)
        rms = 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)
        line = '%-24s %6.3f s  peak %6.1f dBFS  rms %6.1f dBFS' % (rel, len(x) / RATE, peak, rms)
        if name in LOOPS:
            dx = np.abs(np.diff(x))
            line += '  seam step %.4f (median step %.4f, max %.4f)' % (abs(x[0] - x[-1]), np.median(dx), np.max(dx))
        print(line)


def main(argv):
    do_check = '--check' in argv[1:]
    args = [a for a in argv[1:] if a != '--check']
    if len(args) != 1:
        print('usage: python3 -I make_tu58_sounds.py [--check] <sound directory>')
        return 2
    files = build(args[0])
    if do_check:
        check(files)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
