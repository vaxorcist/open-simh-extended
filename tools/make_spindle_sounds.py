# open-simh-extended/tools/make_spindle_sounds.py  v1
#
# Synthesizes the optional spindle sounds of the SIMH sound system for every
# removable-media disk drive with a RUN/STOP (or LOAD) button:
#
#   <prefix>_spinup.wav    T_up seconds, 0 -> nominal speed (plays once)
#   <prefix>_running.wav   about 2 s at nominal speed, a seamless loop
#   <prefix>_spindown.wav  T_down seconds, nominal speed -> 0 (plays once)
#
# One script for all drives (replaces make_rk07_spindle.py v1).  Each drive
# is a row in DRIVES with its rotation speed, its spin-up and spin-down
# times (the defaults of the device's SPINUP/SPINDOWN registers in SIMH)
# and a few sound-character parameters.  Spin times and speed can be
# overridden on the command line, e.g. when SPINUP/SPINDOWN are changed:
# the sounds should last exactly as long as the drive takes in SIMH.
#
# Model (as in make_rk07_spindle.py v1):
# - The spindle turns at f(t) revolutions per second (2400 rpm = 40 Hz).
# - Motor sound = sum of harmonics of f(t), with a "whine" region in the
#   upper harmonics; upper harmonics come up later in the spin-up.  The
#   fundamental is hardly audible on small speakers or hearing aids, so the
#   harmonics carry the sound.
# - Soft distortion (tanh) of the harmonic sum: motor, not organ.
# - Air noise (pack and heads moving through the air; on the large-pack
#   drives also the blower), band-limited around a drive-specific centre
#   frequency, level rising with speed^2.
# - Spin-up f(t) = F (1 - e^(-t/tau)) / (1 - e^(-T/tau)) with tau = 0.3 T:
#   fast at first, reaching the nominal speed exactly at T.  Spin-down:
#   exponential decay (friction, brake) with tau = 0.23 T, reaching zero
#   exactly at T.
# - Frequencies come from phase accumulators, so the pitch glides smoothly.
#   The running loop holds a whole number of revolutions, so every harmonic
#   completes whole cycles; the loop frequency is adjusted by a few ppm so
#   that the loop has a whole number of samples.  The loop's noise is shaped
#   in the frequency domain (periodic), so the loop has no seam.
# - One common gain per drive, so the end of the spin-up, the loop and the
#   start of the spin-down have the same level.  Short fades avoid clicks.
#
# Usage:
#   python3 -I make_spindle_sounds.py --list
#   python3 -I make_spindle_sounds.py [options] <output directory> [drive ...]
#     drive         sound prefix from --list (default: all drives)
#     --tree        write into <output directory>/<prefix>/ (the layout of
#                   SIMH's sound/ directory) instead of the directory itself
#     --spinup S    spin-up time in seconds   (one drive only)
#     --spindown S  spin-down time in seconds (one drive only)
#     --rpm N       rotation speed            (one drive only)
#     --seed N      noise seed (default: per drive, reproducible)
#
# Example: RP06 with SET RP SPINUP=45000: make_spindle_sounds.py --spinup 45 out rp06
#
# Output: mono, 16 bit, 44100 Hz WAV, peak about -7 dBFS.  Needs numpy.

import sys, wave, argparse, zlib
import numpy as np

RATE = 44100
OUT_PEAK = 0.45                 # final normalization: peak about -7 dBFS
LOOP_S = 2.0                    # nominal length of the running loop

# Harmonic profiles (harmonic number: relative amplitude)
H_SMALL = {1: 1.0, 2: 0.55, 3: 0.40, 4: 0.30, 5: 0.18, 6: 0.22, 7: 0.10,       # RK07 (as v1)
           8: 0.20, 9: 0.12, 10: 0.16, 11: 0.08, 12: 0.22, 14: 0.06, 16: 0.05}
H_CART = {1: 1.0, 2: 0.60, 3: 0.35, 4: 0.30, 5: 0.15, 6: 0.25, 8: 0.18,        # RL: small motor, higher whine
          10: 0.10, 12: 0.12, 14: 0.18, 16: 0.12, 18: 0.06, 20: 0.05}
H_LARGE = {1: 1.0, 2: 0.70, 3: 0.50, 4: 0.40, 5: 0.25, 6: 0.30, 7: 0.15,        # 14" packs: big motor
           8: 0.18, 9: 0.10, 10: 0.12, 12: 0.10, 14: 0.05}
H_RA60 = {1: 1.0, 2: 0.60, 3: 0.45, 4: 0.35, 5: 0.22, 6: 0.28, 7: 0.12,         # RA60: quieter motor,
          8: 0.16, 9: 0.10, 10: 0.14, 12: 0.12, 15: 0.06, 18: 0.04}            # more electronics hum

# One row per sound prefix (= sound directory of the drive type in SIMH).
#   rpm, up, down: nominal speed, spin-up and spin-down seconds (the SIMH
#                  SPINUP/SPINDOWN defaults; "prov." = provisional there)
#   harm, drive:   harmonic profile, tanh distortion drive
#   noise, nc, nw: air-noise level at speed, band centre (Hz), band width (octave-ish)
DRIVES = {
    "rk07": dict(desc="RK06/RK07 (HK), sound/rk07 is shared", rpm=2400, up=30, down=30,
                 harm=H_SMALL, drive=2.2, noise=0.10, nc=500.0, nw=0.9),
    "rl02": dict(desc="RL01/RL02 (RL), sound/rl02 is shared", rpm=2400, up=40, down=30,
                 harm=H_CART, drive=2.0, noise=0.08, nc=900.0, nw=0.8),
    "rm03": dict(desc="RM02/RM03 (RP), spin times prov.", rpm=3600, up=30, down=30,
                 harm=H_LARGE, drive=2.4, noise=0.16, nc=420.0, nw=1.0),
    "rp04": dict(desc="RP04 (RP), spin times prov.", rpm=3600, up=30, down=30,
                 harm=H_LARGE, drive=2.6, noise=0.20, nc=380.0, nw=1.1),
    "rp05": dict(desc="RP05 (RP), spin times prov.", rpm=3600, up=30, down=30,
                 harm=H_LARGE, drive=2.6, noise=0.20, nc=380.0, nw=1.1),
    "rp06": dict(desc="RP06 (RP), spin times prov.", rpm=3600, up=30, down=30,
                 harm=H_LARGE, drive=2.6, noise=0.22, nc=360.0, nw=1.1),
    "rm05": dict(desc="RM05 (RP), spin times prov.", rpm=3600, up=30, down=30,
                 harm=H_LARGE, drive=2.4, noise=0.18, nc=400.0, nw=1.0),
    "ra60": dict(desc="RA60 (RQ), DEC: start 35 s, stop 35 s", rpm=3600, up=35, down=35,
                 harm=H_RA60, drive=2.0, noise=0.14, nc=600.0, nw=0.9),
}
# Not here: RM80 and RP07 are fixed disks (no RUN/STOP button, SIMH plays no
# spindle sounds for them); RX01/RX02, RX50/RX33 and TU58 have no spin-up.


class Synth:
    def __init__(self, p, seed):
        self.p = p
        self.F = p["rpm"] / 60.0
        self.rng = np.random.default_rng(seed)
        self.hsum = sum(p["harm"].values())

    def harmonics(self, phase, speed):
        """Harmonic sum with distortion; phase in revolutions, speed 0..1."""
        x = np.zeros_like(phase)
        for h, a in self.p["harm"].items():
            if h * self.F > RATE / 2.2:                 # stay below Nyquist
                continue
            x += a * (speed ** (0.3 + 0.08 * h)) * np.sin(2 * np.pi * h * phase)
        x /= self.hsum
        d = self.p["drive"]
        return np.tanh(d * x) / np.tanh(d)

    def noise(self, n, speed):
        """Air noise, band-limited around nc; level rises with speed^2.
        Shaped in the frequency domain, so it is periodic over n samples."""
        spec = np.fft.rfft(self.rng.standard_normal(n))
        fr = np.fft.rfftfreq(n, 1 / RATE)
        band = np.exp(-((np.log(np.maximum(fr, 1) / self.p["nc"])) ** 2) / (2 * self.p["nw"] ** 2))
        x = np.fft.irfft(spec * band, n)
        x /= np.sqrt(np.mean(x ** 2)) + 1e-12
        return x * self.p["noise"] * speed ** 2

    def render(self, freq):
        """A sound from a per-sample spindle frequency curve."""
        phase = np.cumsum(freq) / RATE
        speed = freq / self.F
        return speed ** 1.3 * self.harmonics(phase, speed) + self.noise(len(freq), speed)

    def make(self, t_up, t_down):
        F = self.F
        t = np.arange(int(round(t_up * RATE))) / RATE
        tau = 0.3 * t_up
        up = F * (1 - np.exp(-t / tau)) / (1 - np.exp(-t_up / tau))
        t = np.arange(int(round(t_down * RATE))) / RATE
        tau_d = 0.233 * t_down
        down = F * (np.exp(-t / tau_d) - np.exp(-t_down / tau_d)) / (1 - np.exp(-t_down / tau_d))

        revs = max(1, int(round(LOOP_S * F)))           # whole revolutions
        n_loop = int(round(revs / F * RATE))            # whole samples
        f_loop = revs * RATE / n_loop                   # (a few ppm off F)
        phase = np.arange(n_loop) * f_loop / RATE
        ones = np.ones(n_loop)
        s_run = self.harmonics(phase, ones) + self.noise(n_loop, ones)

        s_up, s_down = self.render(up), self.render(down)
        g = OUT_PEAK / max(abs(s_run).max(), abs(s_up).max(), abs(s_down).max())
        f = int(0.02 * RATE)                            # 20 ms fades at the ends
        s_up[:f] *= np.linspace(0, 1, f)
        s_down[-f:] *= np.linspace(1, 0, f)
        return g * s_up, g * s_run, g * s_down, revs, n_loop


def write(path, x):
    x = np.clip(np.round(x * 32767), -32768, 32767).astype(np.int16)
    w = wave.open(path, "wb")
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
    w.writeframes(x.tobytes())
    w.close()


def main():
    ap = argparse.ArgumentParser(description="Synthesize SIMH spindle sounds")
    ap.add_argument("outdir", nargs="?")
    ap.add_argument("drives", nargs="*")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--tree", action="store_true")
    ap.add_argument("--spinup", type=float)
    ap.add_argument("--spindown", type=float)
    ap.add_argument("--rpm", type=float)
    ap.add_argument("--seed", type=int)
    a = ap.parse_args()

    if a.list or a.outdir is None:
        print("prefix  rpm   spin-up  spin-down  drive")
        for k, p in DRIVES.items():
            print("%-6s  %4d  %5g s  %7g s    %s" % (k, p["rpm"], p["up"], p["down"], p["desc"]))
        return 0
    names = a.drives or list(DRIVES)
    bad = [n for n in names if n not in DRIVES]
    if bad:
        sys.exit("unknown drive(s): %s (see --list)" % ", ".join(bad))
    if (a.spinup or a.spindown or a.rpm) and len(names) != 1:
        sys.exit("--spinup/--spindown/--rpm need exactly one drive")

    import os
    for n in names:
        p = dict(DRIVES[n])
        if a.rpm:
            p["rpm"] = a.rpm
        t_up = a.spinup or p["up"]
        t_down = a.spindown or p["down"]
        seed = a.seed if a.seed is not None else zlib.crc32(n.encode())
        s_up, s_run, s_down, revs, n_loop = Synth(p, seed).make(t_up, t_down)
        d = os.path.join(a.outdir, n) if a.tree else a.outdir
        os.makedirs(d, exist_ok=True)
        write(os.path.join(d, n + "_spinup.wav"), s_up)
        write(os.path.join(d, n + "_running.wav"), s_run)
        write(os.path.join(d, n + "_spindown.wav"), s_down)
        print("%-5s %4g rpm: spin-up %g s, loop %d revolutions (%.3f s), spin-down %g s -> %s"
              % (n, p["rpm"], t_up, revs, n_loop / RATE, t_down, d))
    return 0


if __name__ == "__main__":
    sys.exit(main())
