# open-simh-extended/tools/make_floppy_sounds.py  v3
#
# v3 (2026-11-11): RX50 (5.25", one DC motor and belt for both diskettes,
#                  6 ms stepper, a solenoid per side, a lever door per slot)
# v2 (2026-11-11): steps and solenoid louder, fan noise darker and 3 dB
#                  quieter (the steps were masked by the fan)
# v1 (2026-11-11): first version: RX02 (8", AC motor) and the default
#                  floppy set (a generic 5.25" drive with a DC motor)
#
# Synthesizes the floppy drive sounds of the SIMH sound system (SND_FLOPPY
# in sim_sound.c).  There are no recordings of a real drive yet (Ulli,
# 2026-11-11: "please synthesize them as for the TS11"), so everything here
# is a model; real recordings can replace the files one by one.
#
# Files (names as sim_floppy loads them: <dir>/<prefix>_<name>.wav, else
# default_floppy/<name>.wav):
#
#   motor_start.wav  the spindle motor running up (RX02: the AC motor and
#                    its fan at power on, 2 s, the drive is ready within 2 s
#                    of power: RX02 Technical Manual 5.3.4.1).  SIMH chains
#                    it into motor.wav (crossfade over its last 50 ms)
#   motor.wav        the motor at speed, a seamless 2 s loop
#   motor_stop.wav   the motor running down after power off
#   disk.wav         a diskette turning in its jacket (the liner), a quiet
#                    seamless loop; plays while a diskette is in and the
#                    spindle turns
#   head.wav         the head and its load pad on the turning diskette, a
#                    quiet seamless loop; plays while the head is loaded
#   head_load.wav    the head load solenoid pulling in (the loud clack of
#                    an 8" drive)
#   head_unload.wav  the solenoid releasing (softer)
#   step.wav         one step of the head stepper (one track); SIMH repeats
#                    it at the step rate (RX02 6 ms per track: a buzz of
#                    about 167 Hz during a long seek)
#   door_close.wav   the front cover closed and latched (ATTACH)
#   door_open.wav    the latch released, the cover springs open (DETACH)
#
# Drives:
#   rx02     RX02 (and the RX01: same mechanics).  AC induction motor wired
#            to the line (H771 power supply: no motor control), belt to the
#            spindle at 360 rpm (6 rev/s), a cooling fan on the motor shaft
#            (Technical Manual 5.3.4.1).  Line 50 Hz (230 V units, RX02-BD):
#            hum at 100 Hz, motor about 1440 rpm (24 rev/s), fan with 7
#            blades (168 Hz).  Written to rx02/rx02_<name>.wav.
#   rx50     RX50 (two 5.25" diskettes in one drive): one 12 V DC spindle
#            motor drives both spindles at 300 rpm (5 rev/s) and is
#            switched by the controller (at speed within 0.5 s: Rainbow 100
#            Technical Manual EK-PC100-TM-001, 6.4.1.4); no fan, no AC hum;
#            motor about 1500 rpm with a commutation whine; one stepper for
#            the common carriage, at least 6 ms per step; a head load
#            solenoid per side; a lever per slot that closes the door and
#            clamps the diskette.  Written to rx50/rx50_<name>.wav.
#   default  a generic 5.25" drive: brushless DC direct drive at 300 rpm
#            (5 rev/s), quiet; lighter solenoid and stepper.  Written to
#            default_floppy/<name>.wav with generic names; used by every
#            floppy drive without files of its own.
#
# Loops: every periodic component completes whole cycles in the loop length
# (2 s: spindle 6 or 5 rev/s, motor 24 rev/s, hum 100 Hz, ...), and the
# noise is periodic over the loop (shaped in the frequency domain), so the
# loops have no seam.
#
# Usage:
#   python3 -I make_floppy_sounds.py <sound directory>
#     writes <sound directory>/rx02/rx02_*.wav,
#            <sound directory>/rx50/rx50_*.wav and
#            <sound directory>/default_floppy/*.wav
#   python3 -I make_floppy_sounds.py --check <sound directory>
#     also prints the length, peak and RMS of every file and the seam step
#     of the loops
#
# Output: mono, 16 bit WAV, 44100 Hz.  Needs numpy.

import os
import sys
import wave

import numpy as np

RATE = 44100
LOOP_S = 2.0


def rng_for(name):
    """Reproducible noise per file."""
    return np.random.default_rng(sum((i + 1) * ord(c) for i, c in enumerate(name)) * 7919)


def shaped_noise(n, lo, hi, rng, periodic=True):
    """Band-limited noise of n samples, unit RMS (periodic over n)."""
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


def ring(n, f, decay, delay=0.0, phase=0.0):
    """A decaying resonance (a struck part)."""
    t = np.arange(n) / RATE - delay
    x = np.sin(2 * np.pi * f * np.maximum(t, 0) + phase) * np.exp(-np.maximum(t, 0) / decay)
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


# ------------------------------------------------------------------ models

class Drive:
    """Parameters of one drive's sound character."""
    def __init__(self, **kw):
        self.__dict__.update(kw)


RX02 = Drive(
    spindle=6.0,            # rev/s (360 rpm)
    motor=24.0,             # motor rev/s (AC induction, 50 Hz, slip)
    hum=100.0,              # magnetostriction hum: twice the line frequency
    hum_amps=(1.0, 0.35, 0.25, 0.1),     # 100, 200, 300, 400 Hz
    hum_db=-30.0,           # level of the hum in the motor loop
    blades=7,               # fan blades on the motor shaft
    fan_db=-31.0,           # fan air noise (mostly below 2 kHz)
    belt_db=-40.0,          # belt and pulleys: once per spindle revolution
    motor_db=-26.0,         # total motor loop RMS
    up_s=2.0,               # run-up (ready within 2 s of power)
    down_s=5.0,             # coast-down
    disk_db=-40.0,          # diskette in its jacket
    head_db=-35.0,          # head and load pad on the diskette
    load_f=(210.0, 820.0, 2600.0),       # solenoid clack resonances
    load_db=-3.0,
    unload_db=-9.0,
    step_f=(1150.0, 420.0),              # stepper click resonances
    step_db=-8.0,
    step_s=0.0055,          # a click fits into the 6 ms step
    door_f=(95.0, 330.0),
    door_db=-8.0,
)

RX50 = Drive(
    spindle=5.0,            # 300 rpm, both spindles on one belt
    motor=25.0,             # DC motor about 1500 rpm
    hum=0.0,                # no AC hum: 12 V DC motor
    hum_amps=(),
    hum_db=-60.0,
    blades=0,               # no fan in the drive
    fan_db=-48.0,           # belt and bearing noise only
    belt_db=-42.0,
    motor_db=-32.0,
    whine=1500.0,           # commutation whine (60 per motor revolution)
    up_s=0.5,               # at speed within 0.5 s
    down_s=1.5,             # two spindles and the belt coast down
    disk_db=-41.0,
    head_db=-37.0,
    load_f=(300.0, 1250.0, 3600.0),      # smaller solenoid than the 8" drives
    load_db=-6.0,
    unload_db=-12.0,
    step_f=(1700.0, 600.0),
    step_db=-10.0,
    step_s=0.0055,          # fits into the 6 ms step
    door_f=(130.0, 520.0),  # the lever turns, the clamp closes on the hub
    door_db=-10.0,
)

DEFAULT = Drive(
    spindle=5.0,            # 300 rpm, direct drive
    motor=5.0,
    hum=0.0,                # no AC hum: brushless DC
    hum_amps=(),
    hum_db=-60.0,
    blades=0,
    fan_db=-60.0,
    belt_db=-60.0,
    motor_db=-34.0,
    whine=1200.0,           # commutation whine of the DC motor
    up_s=0.5,               # motor start 500 ms
    down_s=1.0,
    disk_db=-40.0,
    head_db=-38.0,
    load_f=(380.0, 1500.0, 4200.0),
    load_db=-9.0,
    unload_db=-14.0,
    step_f=(2100.0, 750.0),
    step_db=-12.0,
    step_s=0.0028,          # fits into a 3 ms step (RX33)
    door_f=(160.0, 600.0),
    door_db=-12.0,
)


def motor_tones(d, speed, n):
    """The motor's periodic sound at relative speed(t) (array 0..1) over n
    samples: rotation harmonics, fan blade pass, belt flutter, DC whine."""
    t = np.arange(n) / RATE
    ph_m = np.cumsum(d.motor * speed) / RATE        # motor revolutions
    ph_s = np.cumsum(d.spindle * speed) / RATE      # spindle revolutions
    x = np.zeros(n)
    x += 0.30 * np.sin(2 * np.pi * ph_m) + 0.20 * np.sin(4 * np.pi * ph_m + 0.7)
    if d.blades:
        x += 0.45 * speed * np.sin(2 * np.pi * d.blades * ph_m)
        x += 0.15 * speed * np.sin(4 * np.pi * d.blades * ph_m + 1.1)
    if getattr(d, 'whine', 0.0):
        k = d.whine / d.motor                       # whole cycles per revolution
        x += 0.5 * speed * np.sin(2 * np.pi * k * ph_m)
    flutter = 1.0 + 0.25 * np.sin(2 * np.pi * ph_s)  # belt / spindle once per rev
    return x * flutter


def hum(d, n):
    t = np.arange(n) / RATE
    x = np.zeros(n)
    for k, a in enumerate(d.hum_amps, 1):
        x += a * np.sin(2 * np.pi * k * d.hum * t + 0.3 * k)
    return x


def motor_loop(d, rng):
    n = int(LOOP_S * RATE)
    tones = motor_tones(d, np.ones(n), n)
    air = shaped_noise(n, 200, 2000, rng)
    am = 1 + 0.15 * np.sin(2 * np.pi * d.motor * np.arange(n) / RATE)   # 48 cycles in 2 s
    x = rms_to(tones, d.motor_db - 3) + rms_to(air * am, d.fan_db) + \
        (rms_to(hum(d, n), d.hum_db) if d.hum else 0.0)
    return rms_to(x, d.motor_db)


def tone_gain(d):
    """Scale of motor_tones () that gives the loop's tone level."""
    n = int(LOOP_S * RATE)
    return db(d.motor_db - 3) / (np.sqrt(np.mean(motor_tones(d, np.ones(n), n) ** 2)) + 1e-12)


def motor_start(d, rng, loop):
    """Run-up: the speed rises (an induction motor: fast and smooth), the
    hum is loud at first (inrush) and settles; the end matches the loop."""
    n = int(d.up_s * RATE)
    t = np.arange(n) / RATE
    speed = 1 - np.exp(-t / (d.up_s / 4.5))
    speed = speed / speed[-1]
    x = motor_tones(d, speed, n) * tone_gain(d)
    x += rms_to(shaped_noise(n, 200, 2000, rng, periodic=False), d.fan_db) * speed ** 2
    if d.hum:
        x += rms_to(hum(d, n), d.hum_db) * (1.0 + 2.5 * np.exp(-t / 0.25))
        x += peak_to(ring(n, 70.0, 0.05) + 0.5 * ring(n, 240.0, 0.02), -14.0)   # switch-on knock
    x = fade(x, 0.002, 0.0)
    m = int(0.2 * RATE)                         # match the loop over the last 0.2 s
    g = np.sqrt(np.mean(loop[:m] ** 2)) / (np.sqrt(np.mean(x[-m:] ** 2)) + 1e-12)
    k = min(int(0.6 * RATE), n // 2)
    ramp = np.ones(n)
    ramp[-k:] = np.linspace(1.0, g, k)
    return x * ramp


def motor_stop(d, rng):
    """Power off: the hum stops at once, the motor and fan coast down."""
    n = int(d.down_s * RATE)
    t = np.arange(n) / RATE
    speed = np.exp(-t / (d.down_s / 3.5))
    x = motor_tones(d, speed, n) * tone_gain(d) * speed
    x += rms_to(shaped_noise(n, 200, 2000, rng, periodic=False), d.fan_db) * speed ** 2
    return fade(x, 0.02, 0.05)


def disk_loop(d, rng):
    """The diskette turning in its jacket: the liner rubbing, a soft hiss
    modulated once per revolution (the jacket is not flat)."""
    n = int(LOOP_S * RATE)
    ph = d.spindle * np.arange(n) / RATE
    x = shaped_noise(n, 1200, 7000, rng) * (1 + 0.35 * np.sin(2 * np.pi * ph))
    x += 0.4 * shaped_noise(n, 150, 600, rng) * (1 + 0.5 * np.sin(2 * np.pi * ph + 1.0))
    return rms_to(x, d.disk_db)


def head_loop(d, rng):
    """The head and its pressure pad on the turning diskette: a scraping
    hiss, stronger, with a once-per-revolution swish."""
    n = int(LOOP_S * RATE)
    ph = d.spindle * np.arange(n) / RATE
    swish = 1 + 0.6 * np.sin(2 * np.pi * ph) ** 2
    x = shaped_noise(n, 2000, 9000, rng) * swish
    x += 0.5 * shaped_noise(n, 400, 1500, rng) * swish
    return rms_to(x, d.head_db)


def clack(d, rng, loud, length=0.09):
    """Head load solenoid: armature hits the stop (metallic resonances),
    the load pad lands on the diskette (thud)."""
    n = int(length * RATE)
    f1, f2, f3 = d.load_f
    x = 1.0 * ring(n, f1, 0.018) + 0.6 * ring(n, f2, 0.008, 0.0, 0.5) + 0.35 * ring(n, f3, 0.004, 0.0, 1.2)
    x += 0.5 * shaped_noise(n, 2500, 9000, rng, periodic=False) * env(n, 0.0002, 0.003)
    if loud:
        x += 0.45 * ring(n, f1 * 0.8, 0.012, 0.006)  # pad lands a few ms later
    return fade(peak_to(x, d.load_db if loud else d.unload_db), 0.0005, 0.01)


def step_click(d, rng):
    n = int(d.step_s * RATE)
    fa, fb = d.step_f
    x = ring(n, fa, 0.0012) + 0.7 * ring(n, fb, 0.0018, 0.0, 0.8)
    x += 0.4 * shaped_noise(n, 3000, 10000, rng, periodic=False) * env(n, 0.0001, 0.0006)
    return fade(peak_to(x, d.step_db), 0.0002, 0.0008)


def door(d, rng, closing):
    """Front cover: closing = a push, the latch snapping in, a thud;
    opening = the latch released, the spring-loaded cover popping up."""
    n = int(0.35 * RATE)
    fa, fb = d.door_f
    nz = shaped_noise(n, 1500, 8000, rng, periodic=False)
    if closing:
        x = 0.25 * nz * env(n, 0.03, 0.05)                       # sliding
        x += 1.0 * ring(n, fa, 0.06, 0.12) + 0.6 * ring(n, fb, 0.02, 0.12)
        x += 0.7 * nz * env(n, 0.0002, 0.004, 0.12)              # latch snap
    else:
        x = 0.8 * nz * env(n, 0.0002, 0.003, 0.01)               # latch click
        x += 0.6 * ring(n, fb, 0.03, 0.01) + 0.9 * ring(n, fa * 1.3, 0.05, 0.08)
        x += 0.2 * nz * env(n, 0.02, 0.06, 0.06)
    return fade(peak_to(x, d.door_db), 0.001, 0.02)


def drive_set(d, tag):
    """All ten sounds of a drive, as (name, samples)."""
    loop = motor_loop(d, rng_for(tag + 'motor'))
    return [
        ('motor_start', motor_start(d, rng_for(tag + 'mstart'), loop)),
        ('motor', loop),
        ('motor_stop', motor_stop(d, rng_for(tag + 'mstop'))),
        ('disk', disk_loop(d, rng_for(tag + 'disk'))),
        ('head', head_loop(d, rng_for(tag + 'head'))),
        ('head_load', clack(d, rng_for(tag + 'hload'), True)),
        ('head_unload', clack(d, rng_for(tag + 'hunload'), False, 0.06)),
        ('step', step_click(d, rng_for(tag + 'step'))),
        ('door_close', door(d, rng_for(tag + 'dclose'), True)),
        ('door_open', door(d, rng_for(tag + 'dopen'), False)),
    ]


LOOPS = ('motor', 'disk', 'head')


def build(outdir):
    files = []
    for name, x in drive_set(RX02, 'rx02'):
        rel = 'rx02/rx02_%s.wav' % name
        write_wav(os.path.join(outdir, rel), x)
        files.append((rel, name, x))
    for name, x in drive_set(RX50, 'rx50'):
        rel = 'rx50/rx50_%s.wav' % name
        write_wav(os.path.join(outdir, rel), x)
        files.append((rel, name, x))
    for name, x in drive_set(DEFAULT, 'default'):
        rel = 'default_floppy/%s.wav' % name
        write_wav(os.path.join(outdir, rel), x)
        files.append((rel, name, x))
    return files


def check(files):
    for rel, name, x in files:
        peak = 20 * np.log10(np.max(np.abs(x)) + 1e-12)
        rms = 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)
        line = '%-34s %6.3f s  peak %6.1f dBFS  rms %6.1f dBFS' % (rel, len(x) / RATE, peak, rms)
        if name in LOOPS:
            dx = np.abs(np.diff(x))
            line += '  seam step %.4f (median step %.4f, max %.4f)' % (abs(x[0] - x[-1]), np.median(dx), np.max(dx))
        print(line)


def main(argv):
    do_check = '--check' in argv[1:]
    args = [a for a in argv[1:] if a != '--check']
    if len(args) != 1:
        print('usage: python3 -I make_floppy_sounds.py [--check] <sound directory>')
        return 2
    files = build(args[0])
    if do_check:
        check(files)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
