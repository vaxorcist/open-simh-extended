<!-- open-simh-extended/docs/SIMH_Sound_design.md  v1 (Phase 1 result: agreed design, no code yet) -->
# SIMH Sound: design (Phase 1 result)

Date: 2026-11-02. Prepared with Claude for Ulli (vaxorcist). Planning only; nothing is implemented yet and nothing will be pushed.

## Goal

A sound system common to all SIMH simulators, built on SDL2 audio. Without SDL2 (or without an audio device) sounds are silently ignored. Pilot: VAX-11/750 with RK07 (RK611 controller, `PDP11/pdp11_hk.c`).

- Code base: open-simh (`/home/claude/open-simh/simh`), local clone only.
- Build: `makefile` **and** the CMake build. Note: the per-simulator `CMakeLists.txt` files are generated from the makefile by `cmake/generate.py`, so the makefile is changed first and the CMake files are regenerated. The Visual Studio project files are updated as well.

## 1. Core module

- New files `sim_sound.c` / `sim_sound.h` next to `sim_video.c`, compiled into **every** simulator.
- With SDL2: real implementation. Without SDL2: stubs that do nothing and return `SCPE_OK`, so device code needs no `#ifdef`.
- Today SDL2 is only detected and linked for video simulators (`makefile` around line 741: `-DHAVE_LIBSDL -DUSE_SIM_VIDEO`). The sound module needs SDL2 audio for all simulators; the build gets a separate define for it.
- Own audio device via `SDL_InitSubSystem(SDL_INIT_AUDIO)` (reference-counted) and `SDL_OpenAudioDevice()`.
- The existing bell in `sim_video.c` (`vid_beep_setup`, about line 2740) uses the legacy `SDL_OpenAudio()`/`SDL_PauseAudio()` API. It stays untouched until phase D, when it becomes a client of `sim_sound`.
- No SDL2_mixer dependency: a small mixer of our own runs in the audio callback.

## 2. Sound classes

| Class | Trigger | Playback |
|---|---|---|
| starting | RUN or BOOT (power-on of the system) | one-shot; the running loop starts at its end |
| running | after starting | seamless loop |
| stopping | EXIT/QUIT or an explicit power-off command (e.g. `SET CPU POWER=OFF`) | loop ends, one-shot plays |
| event | called by device code | one-shot, fire-and-forget |

- **System loop (fans):** starts on RUN/BOOT. It keeps running on ^E, breakpoints, STEP, EXPECT stops and HALT back to `sim>`; a real 11/750 keeps its fans running until the power switch. On EXIT the simulator waits for the fadeout (about 1.5 s) before ending.
- **STEP, breakpoints, EXPECT/SEND:** never trigger starting or stopping sounds (stop reasons `SCPE_STEP`, breakpoint stops, `SCPE_EXPECT`; `run_cmd()` in `scp.c`).
- **Unit loops (spindles):** one per unit, depending only on the unit state (spun up or not), not on whether the CPU runs. The audio thread keeps them playing at the `sim>` prompt. The framework supports spin-up / running / spin-down per unit; **no spindle samples ship yet**.
- **Events:** only seeks. No read/write sounds.
- **Non-blocking:** an event only enqueues a request (short lock or lock-free ring). All files are loaded and converted to the device format at configuration time. Event calls must be thread-safe (`SIM_ASYNCH_IO` completions).
- **Overlap of the same event:** decided by testing.

## 3. Mixing and volume

- Mixing in floating point with headroom; conversion to 16 bit only at the end.
- Soft limiter on the master bus, acting only near full scale (no hard clipping).
- Uncorrelated sounds add up roughly with √N (three drives ≈ 1.7× one drive), which the headroom must allow for.
- Master volume plus groups **SYSTEM, DISK, TAPE, EVENT**, each with gain and mute.
- No stereo panning.

## 4. Sound files

- Bundled samples are Ulli's own, under the SIMH license, in one directory per CPU / peripheral, e.g. `sound/vax750/`, `sound/rk07/`.
- Users can configure their own sounds. Search order:
  1. `SET SOUND PATH=…`
  2. environment variable `SIMH_SOUND_PATH`
  3. `sound/<name>/` relative to the executable
  4. `sound/<name>/` relative to the current directory
  A user file with the same name earlier in the order overrides the bundled one.
- Format: mono WAV, any sample rate (converted at load time).

### Provided samples (analysis 2026-11-02)

| File | Format | Length | Peak / RMS | Notes |
|---|---|---|---|---|
| `blower_starting.wav` (real 11/750: power relay + fan start) | 44.1 kHz 16 bit | 0.73 s | 0 / −17.8 dBFS | 8 clipped samples (relay click); ends at −16 dB, matching the loop |
| `blower_running19.wav` (real 11/750 fan) | 44.1 kHz 16 bit | 4.87 s | −3.5 / −16.3 dBFS | **seam click** (wrap jump 5233 vs. 99th percentile 2791); to be cut with a crossfade once, offline; fan fundamental ≈ 37 Hz |
| `blower_stopping_fadeout.wav` (real 11/750: relay off + run-down) | 44.1 kHz 16 bit | 1.44 s | 0 / −20.9 dBFS | 3 clipped samples, clean fade to silence |
| `rk07-s.wav` (synthetic) | 16 kHz 16 bit | 6 ms | 0 / −5 dBFS | short seek |
| `rk07-m.wav` (synthetic) | 16 kHz 16 bit | 6 ms | 0 / −5 dBFS | medium seek |
| `rk07-l.wav` (synthetic) | 16 kHz 16 bit | 9 ms | 0 / −5 dBFS | long seek |

The seek pulses are about 11 dB louder than the fan; default gains compensate, final balance by ear.

## 5. SET SOUND and realism mode

- `SET SOUND ENABLED|DISABLED`, `SET SOUND VOLUME=n`, group volumes, `SET SOUND PATH=…`, `SHOW SOUND`; commands to assign user sounds. Exact syntax is fixed in phase A.
- `SET SOUND ENABLED|DISABLED` is only allowed while **no disk and no tape** is attached.
- `SET SOUND ENABLED` switches on the **realism mode**, also when SDL2 or an audio device is missing (same script, same behavior on every host, just silent):
  - realistic disk (and later tape) timing;
  - realistic CPU speed: a fixed default `THROTTLE` per simulator. If the user set THROTTLE explicitly, it is left untouched. The 11/750 default (≈ 0.6 × an 11/780) is calibrated against the real 11/750, e.g. VMS boot time to the login prompt;
  - UNLOAD goes OFFLINE instead of detaching;
  - ATTACH leaves the drive stopped until `SET <unit> ONLINE`.
- With sound disabled everything behaves as today.

## 6. OFFLINE / ONLINE (drive RUN/STOP button)

- `SET <unit> OFFLINE`: heads retract, drive not ready, attention interrupt. The file **stays attached**.
- `SET <unit> ONLINE`: drive spins up (realism mode: realistic spin-up delay), then ready + attention.
- Spin-up and spin-down timing run in **simulated** time (with THROTTLE active this matches wall-clock time).
- Realism mode: scripts must contain `SET HKn ONLINE` after `ATTACH` before booting.
- Generic part (SCP modifier, unit state, spin timing) plus a small adaptation per controller (status bits, attention). Rollout order: **HK** (pilot), then RL, RP/RM (Massbus), RA60 (RQ), and last RX and TU58.

### Current HK behavior (`PDP11/pdp11_hk.c`)

- `hk_attach()` makes the drive ready at once ("ATTACH implicitly spins up the drive").
- `FNC_UNLOAD` in `hk_svc()` calls `hk_detach()`, so a VMS shutdown with spin-down detaches the container today. In realism mode this becomes OFFLINE.
- The controller knows the cylinder (`uptr->CYL`) and the seek distance (`hk_dif[drv]`); timing uses `hk_swait` × distance + `hk_rwait`, in instructions.

## 7. Seek sounds and timing (RK06/RK07)

### Seek classes

| Class | Default (RK07) | Default (RK06) |
|---|---|---|
| short | 1–50 cylinders | 1–25 |
| medium | 51–300 | 26–150 |
| long | above medium; recalibrate | above medium; recalibrate |
| none | distance 0 | distance 0 |

Thresholds are device registers per drive type, set with `DEPOSIT` and shown with `EXAMINE`: `HK SEEK7S`, `HK SEEK7M` (RK07), `HK SEEK6S`, `HK SEEK6M` (RK06).

### Published timing

| | RK07 | RK06 |
|---|---|---|
| Cylinders | 815 | 411 |
| Track-to-track | 6.5 ms | 8 ms |
| Average seek | 36.5 ms | 38 ms |
| Maximum seek | 71 ms | 75 ms |
| Rotation | 2400 rpm, average latency 12.5 ms | same |
| Spin-up | nominal 30 s, max. 60 s | same |
| Spin-down | about 30 s | same |

Sources: Computer History Wiki (gunkies.org/wiki/RK07) and pdp-11.nl (peripherals/disk/rk07-info.html). To be checked against the DEC manual when available.

### Seek-time model (realism mode)

`t(d) = a + b · d^0.69` (d = cylinders moved), fitted through track-to-track, average (at about a third of the stroke, ≈ 272 cylinders) and maximum. Plus a random rotational latency of 0–25 ms. Scheduled with `sim_activate_after()` in microseconds instead of instruction counts.

## 8. Phases

| Phase | Content |
|---|---|
| A | `sim_sound` core with stubs, mixer, limiter, groups, file search, SET/SHOW SOUND, system loop (fans) with suppression rules; makefile + CMake + VS projects; prepared 11/750 blower samples in `sound/vax750/` |
| B | Event API and unit loops; generic OFFLINE/ONLINE; pilot HK: seek classes, registers, realistic timing, UNLOAD → OFFLINE, ATTACH waits for ONLINE; RK07 samples in `sound/rk07/`; per-simulator THROTTLE default for the 11/750 |
| C | OFFLINE/ONLINE and seek sounds for RL, RP/RM, RA60 (RQ), then RX and TU58; afterwards tapes |
| D | Move the `vid_beep` bell onto `sim_sound` |

## Open points (to be settled by testing)

- Overlap policy for repeated events (restart, overlap, drop).
- Default gains of each sample and group.
- The THROTTLE value for the 11/750.
- Exact SCP syntax for assigning user sounds.
