<!-- open-simh-extended/docs/SIMH_Sound_handover.md  v36 (v36: VMS V4.7 on the VAX 8200 with the console RX50 CSA1/CSA2, both modes; v35: the VAX 8200 console RX50, patch 5 v32; v34: floppies step 3: the RX50 on the RQ, RX50 controller rules, patch 5 v31; v33: floppies step 2: the RX01 (RX device, VAX-11/780 console floppy CS), patch 5 v30; v32: RX02 VMS MOUNT DRVERR found and fixed (known issues 47), VMS tests vms2/vms3, patch 5 v29; v31: RX02 under VMS V4.7: MOUNT DRVERR (open), RY debug flags CMD/REG, patch 5 v28; v30: floppies, first the RX02 (RY), patch 5 v27; v29: SHUTDOWN spin-down confirmed, silent-sounds workaround; v28: RA81 start 20 s, RA80 40 s from the User Guides, patch 5 v26; v27: bus rule for SDI controllers, patch 5 v25; v26: Ulli's RA decisions, SDI rules, RA80 capacity, patch 5 v24; v25: the fixed RA drives RA70-RA92, patch 5 v23; v24: TK50 closed for now, patch 5 v22, next: the RA Winchester disks; v23: invalid records, calibration marker, throttle, patch 5 v21; v22: smooth TK50 sounds, streamer crossfades, patch 5 v20; v21: TQ position beyond the recorded data, patch 5 v19; v20: TK50 cartridge streamer, MicroVAX II THROTTLE 450K, patch 5 v18; v19: TS capacity display, blank tape OPI, patch 5 v17; v18: tapes, TS11 and TSV05, patch 5 v16; v17: patch 5 v15 on patch 6 v2; v16: default sounds; v15: RC25 rules, KLESI, geometry, unlimited spindles; v14: RC25 tested with VMS V3.5; v13: RC25/RCF25 on the RQ; v12: RB730 fixes, sound rebased on them; v11: RB02/RB80 on the RB730; v10: spindle sound generator; v9: device table; v8: RA60 on the RQ; v7: Massbus disks; v6: RL01/RL02, common spindle support; v5: ONLINE/OFFLINE wait at the prompt; v4: spin-up in wall time at the prompt, power on restores spindles, spin-up bug fixes; v3: VMS boot fixes; v2: phase B; v1: phase A) -->
# SIMH Sound for Open SIMH: status and handover

Date: 2026-11-02 (last update 2026-11-11, VAX 8200 console RX50 under VMS). Prepared with Claude for Ulli (vaxorcist). Everything is pushed to the fork vaxorcist/open-simh-extended (branches `sound`, `integration`, `notes`). Design: `SIMH_Sound_design.md` (v1).

## Phase A (2026-11-02): core, mixer, system sounds

### Where things are

- Repository `/home/claude/open-simh/simh`, branch `sound`, local commit a83e957 on plain Open SIMH 87eb7d5.
- Patch file `opensimh_5_sound_v1.patch` (a single commit, independent of the DMV11/DMF32/8200 patches). It contains binary WAV files, so apply it with `git am --keep-cr` (or `git apply`), not with `patch`.
- New files:
  - `sim_sound.c`, `sim_sound.h`: the sound module;
  - `sound/vax750/blower_starting.wav`, `blower_running.wav`, `blower_stopping.wav`, `README.txt` (description and SIMH license).
- Changed files:
  - `scp.c`: SET/SHOW SOUND, HELP SET SOUND, power-on in `run_cmd()`, `sim_sound_exit()` at the end of `main()`;
  - `VAX/vax750_syslist.c`: `vax_init()` registers the 11/750 system sounds;
  - `makefile`: `sim_sound.c` in `SIM`, separate SDL2 audio detection for every simulator (`SOUND_CCDEFS`/`SOUND_LDFLAGS`, `NOSOUND=1` switches it off), also in the MinGW branch;
  - CMake: `CMakeLists.txt` (option `WITH_SOUND`, feature line), `cmake/dep-locate.cmake` (SDL2 also searched when video is off), `cmake/dep-link.cmake` (interface library `simh_sound`), `cmake/add_simulator.cmake` (source and library). The per-simulator `CMakeLists.txt` files did not need to be regenerated: the generator ignores `${SIM}`;
  - Visual Studio: `sim_sound.c`/`.h` in all 81 `.vcproj` and 78 `.vcxproj`/`.filters` files, `USE_SIM_SOUND` in every project that links SDL2 (all except Intel-MDS, which gets the stubs);
  - `descrip.mms` (VMS): `SIM_SOUND.C` in `SIMH_SOURCE` (stubs, there is no SDL2 on VMS);
  - `.gitattributes`: `*.wav binary`.
- No first-line path/version markers were added to source files.

### Design as implemented

- **Always compiled.** The SET/SHOW SOUND commands, the realism flag (`sim_sound_enabled()`) and the power state work in every build. Only the audio part needs `USE_SIM_SOUND` (SDL2); without it every playback call is a no-op.
- **Own audio device.** `SDL_InitSubSystem(SDL_INIT_AUDIO)` + `SDL_OpenAudioDevice()`, float mono 44.1 kHz, 512-frame blocks (11.6 ms). It coexists with the legacy `SDL_OpenAudio()` bell of `sim_video.c`. `SDL_MAIN_HANDLED` keeps SDL away from `main()`.
- **Non-blocking.** `sim_sound_play()`/`sim_sound_stop()` only append to a 256-entry queue under a mutex held for the copy. The audio callback takes the queue with `SDL_TryLockMutex()`; if it is busy, the commands are taken by the next callback.
- **Mixer.** 64 voices, float sum, group gain × master × 0.5 (−6 dB headroom), soft limiter (tanh above 0.70, never above full scale). Fades for start/stop (minimum 5 ms against clicks). A one-shot can chain into a loop: 50 ms before its end the loop starts with a fade-in and takes over the handle (starting → running). `SND_F_RANDPHASE` starts loops at a random position so identical unit loops add up with √N.
- **Groups:** SYSTEM, DISK, TAPE, EVENT, each 0–200 % and mutable; master 0–200 % and mutable.
- **Power:** RUN and BOOT power on (starting, then running loop); GO/CONTINUE/STEP/NEXT, HALT, ^E, breakpoints and EXPECT stops change nothing. EXIT plays the stopping sound and waits for it (duration + 100 ms); `SET SOUND POWER=OFF` does the same without waiting.
- **Realism mode:** `SET SOUND ENABLED` sets it even if no audio output is available. ENABLED/DISABLED are refused while a disk or tape unit is attached. Sound is **disabled by default**, so existing configurations behave exactly as before.
- **Files:** WAV, loaded once and converted with `SDL_AudioStream`. Search order: SET SOUND PATH (`dir/<sim>/file`, `dir/file`), `SIMH_SOUND_PATH`, `<exe>/sound/<sim>/`, `<exe>/../sound/<sim>/` (BIN directory of a source tree), `./sound/<sim>/`. The cache is keyed by the resolved file, so a changed PATH takes effect; missing files are reported once.

### Commands

```
SET SOUND ENABLED | DISABLED
SET SOUND VOLUME=n | VOLUME=group:n          (0..200 %)
SET SOUND MUTE[=group] | NOMUTE[=group]
SET SOUND POWER=ON | POWER=OFF
SET SOUND PATH=dir[;dir...]
SET SOUND STARTING=file | RUNNING=file | STOPPING=file   (empty = default)
SHOW SOUND
HELP SET SOUND
```

Several items can be combined with commas, e.g. `SET SOUND NOMUTE=SYSTEM,VOLUME=SYSTEM:100`.

### Samples

- `blower_running.wav` was cut by `tools/make_loop.py` v1: the loop length is chosen where the recording correlates best with its start (0.514), the last 0.25 s crossfade into the start. Result: 4.267 s, step at the wrap point 772 (median step 724, 99th percentile 2773) — no click. Level in 0.25 s windows stays between −14 and −17.5 dBFS across the seam, as in the original.
- `blower_starting.wav` and `blower_stopping.wav` are the original recordings, renamed.

### Tests

Test files `test/t1.ini`–`t4.ini` (v1; t3 uses absolute paths of this workspace). Tool `tools/make_loop.py` v1 cuts loops. Audio was rendered with `SDL_AUDIODRIVER=disk` and analyzed.

| Test | Result |
|---|---|
| make vax750 (gcc 13, `-Werror`) | ok |
| make all (80 simulators, `-Werror`) | ok; every simulator links libSDL2 (video simulators such as microvax3900 open the sound device beside their own) |
| CMake: default, `WITH_VIDEO=OFF` (sound still on), `WITH_SOUND=OFF` | ok; vax750 links libSDL2 and plays |
| `NOSOUND=1` build (stubs): SET SOUND ENABLED → "built without SDL2 sound support", realism on, power tracked | ok |
| sim_sound.c with gcc `-Wall -Wextra`, clang, g++ (C++ syntax), with and without SDL2 | no warnings |
| VAX-11/750 diagnostic test (`VAX/tests/vax-diag_test.ini`), sound disabled and enabled | all passed |
| RUN: starting → running loop; halt keeps power; CONTINUE does not replay starting; EXIT plays stopping and waits | ok (rendered audio: relay click, loop at −22 dBFS = −16 dBFS recording − 6 dB headroom, run-down to silence) |
| Loop at the prompt for 10 s; VOLUME=SYSTEM:50 (−6 dB), MUTE=SYSTEM (silence), NOMUTE | ok, no click at any loop wrap (largest step = the relay click in the recording) |
| SET SOUND ENABLED/DISABLED with HK0 attached | refused with a message naming HK0 |
| Bad POWER/VOLUME/group/keyword values | clear messages |
| SET SOUND PATH + RUNNING= user file | the user file from the PATH is used |
| STARTING= missing file | "file not found" once, previous sound kept |
| No audio device (`SDL_AUDIODRIVER=nosuchdriver`) | message once, realism on, everything else works |
| Patch applied with `git am --keep-cr` to a fresh 87eb7d5 | tree identical to the commit |
| Windows (Visual Studio, MinGW), macOS, VMS | **not tested** (no such host here) |

## Phase B (2026-11-02): RK07 pilot, RUN/STOP buttons, realistic timing, CPU speed

### Where things are

- Branch `sound`: phase A commit a83e957 plus the phase B commit 9982a6d.
- Patch file `opensimh_5_sound_v2.patch`: **one cumulative commit** on plain 87eb7d5 that replaces v1 (apply v2 alone, with `git am --keep-cr`). **Superseded by v3** (see the VMS boot fixes below).
- New files: `sound/rk07/rk07-s.wav`, `rk07-m.wav`, `rk07-l.wav` (your synthetic seek sounds, unchanged) and `sound/rk07/README.txt` (description, optional spindle file names, SIMH license).
- Changed files:
  - `PDP11/pdp11_hk.c` (RK611, used by the PDP-11 and VAX simulators): RUN/STOP buttons, spindle state, realism mode, seek sounds, timing model, registers, help;
  - `sim_sound.c`/`.h`: unit loops (`SND_UNITLOOP`, `sim_sound_unit_load/start/stop`), `sim_sound_load_optional()`, chained loops with random phase, per-simulator throttle default (`sim_sound_set_throttle()`), power off now silences every sound (spindles stop with the power);
  - `sim_timer.c`/`.h`: `sim_throt_is_set()` (sim_sound must not override a user throttle);
  - `scp.c`: HELP SET SOUND mentions the CPU speed;
  - `VAX/vax750_syslist.c`: `sim_sound_set_throttle ("300K")`.

### RK611 (HK) behavior

| | Normal mode (sound disabled) | Realism mode (SET SOUND ENABLED) |
|---|---|---|
| ATTACH | drive ready at once (as before) | pack loaded, drive **stopped** until `SET HKn ONLINE` |
| `SET HKn ONLINE` (RUN) | ready at once, attention | spin-up, register `SPINUP` (default 30000 ms simulated), then ready + attention |
| `SET HKn OFFLINE` (STOP) | not ready, VV cleared, attention; pack stays attached; a running operation ends with OPI | the same, plus the spin-down sound |
| UNLOAD function | detaches (as before) | spins down, pack stays attached |
| Command on a stopped drive | drive unsafe (ER_UNS) + attention, as for a drive without pack | same |
| Seek / transfer time | instruction counts (STIME/RTIME, as before) | t(d) = a + b·d^0.69 (RK07 6.5/36.5/71 ms, RK06 8/38/75 ms) + random latency 0–25 ms + 1.136 ms per sector |
| Seek sounds | none | short ≤ SEEK7S (50), medium ≤ SEEK7M (300), else long; recalibrate long; distance 0 silent (RK06: SEEK6S/SEEK6M 25/150) |
| Spindle sounds | none | `rk07_spinup.wav`, `rk07_running.wav` (loop, random phase), `rk07_spindown.wav` if found (none shipped yet) |

- `SHOW HKn SPINDLE` shows "no pack", "offline", "spinning up, n s left" or "online".
- A controller reset (INIT, CS2 controller clear, RUN) does **not** stop a spin-up; the remaining time is kept.
- The spindle state is the hidden register `SPIN`, so SAVE/RESTORE keeps it.
- The seek class is shown with `SET HK DEBUG=OPS` (">>HKn seek sound short/medium/long (n cylinders)").
- Found while testing, not changed: SIMH's RK611 resets every drive's cylinder to 0 on a controller clear, so the next seek is measured from cylinder 0 (a real drive keeps its heads where they are).

### CPU speed

- A simulator registers its realistic speed; the VAX-11/750 uses **THROTTLE 300K** (≈ 0.6 × an 11/780 at ≈ 0.5 MIPS). **Provisional**: calibrate it against your real 11/750, e.g. the time from BOOT to the VMS login prompt.
- SET SOUND ENABLED sets it only if no throttle is set ("Sound: realistic CPU speed, THROTTLE 300K"); SET SOUND DISABLED removes it again. A throttle set by the user is never touched. SHOW SOUND shows the state.
- As with any SET THROTTLE, idling is switched off.

### Tests (phase B)

Test files in `/home/claude/simh-sound/testB`: `hkprog.ini` (PDP-11 programs: wait ready, PACK, SEEK, wait attention, DCLR; READ one sector; PACK without waiting; UNLOAD), `b1.ini`–`b4.ini`, `b6.ini` (b6 needs stand-in spindle files in `spin/rk07/`).

| Test | Result |
|---|---|
| Realism: ATTACH → SPINDLE offline; PACK on the stopped drive | ER_UNS, ATA, DRDY clear |
| ONLINE with SPINUP 2000/1500 ms, program starts with a controller clear during the spin-up | spin-up continues, drive becomes ready with attention |
| Seeks 10 / 300 / 790 cylinders | sound classes short / medium / long; seek times 300 → 790 scale 1.806 (model 1.806) |
| READ at the current cylinder / after a 790-cylinder seek | latency + 1 sector (≈ 12 ms) / seek + latency + sector (≈ 84 ms) |
| Rendered audio | three pulses of 5.5 / 6.2 / 8.3 ms = the s/m/l samples, level 0.15 (gain 0.3 × headroom 0.5) |
| UNLOAD in realism mode | offline, still attached, ATA, VV and DRDY clear |
| Normal mode: ATTACH ready at once, OFFLINE (UNS on PACK), ONLINE, UNLOAD detaches | as before plus the buttons |
| Spin-up duration without a running OS clock | about 30 % short (1.05 s for 1.5 s): SIMH has no calibrated timer when the program does not run the line clock; with an OS running the clock it is calibrated |
| 11/750: user THROTTLE 50 % kept; default 300K set and removed | ok |
| Spindle loops (stand-in files): two drives | +3 dB with the second drive (√N), back down when HK0 goes offline |
| make all, test suites of pdp11, vax730/750/780/8600 | all 80 simulators built (`-Werror`); all diagnostics passed |
| VMS on an RK07 | **not tested** (no VMS kit here) |

## Phase B fixes (2026-11-02 evening): VMS 3.0 boot from the RK07

Tested with Ulli's image `AY-H020E-BE_VMS3.0.RK7` (VAX/VMS V3.0, volume VAXVMSRL3) on the VAX-11/750.

- Branch `sound`: commit 0defe6e on top of 9982a6d. Patch **`opensimh_5_sound_v3.patch`**: one cumulative commit on plain 87eb7d5 that replaces v1 and v2 (apply v3 alone, with `git am --keep-cr`).
- Changed files: `PDP11/pdp11_hk.c`, `sim_sound.c`, `sim_timer.c`, `sim_timer.h`.
- Ready-to-use configuration: `vms/vax750_rk07_sound.ini` v1.
- Rechecked: make all (80 simulators, `-Werror`), VAX diagnostics on the 730/750/780/8600 passed, PDP-11 register check, PDP-11 RK611 tests (b1) unchanged, normal-mode VMS boot still 2–3 s.

### What went wrong

1. **The boot hung** (VMB looping at `BITW #8080,(R7)`): the configuration attached HK0 and booted without `SET HK0 ONLINE`. In the realism mode the drive stays stopped until then. Not a bug, but see "Open question" below.
2. **Mount verification during the boot** ("Device DMA0 is offline"): some transfers took up to 5.4 million instructions (≈ 18 s at 300K) instead of < 0.2 s, and VMS timed out. Cause: `sim_activate_after()` converts microseconds through the timer calibration; during a throttled boot it sometimes tied the event to a much later timer tick. **Fixed:** the RK611 converts realistic durations to instructions itself (`hk_activate_us()`), at the throttle's target rate when a throttle is set as xK/xM (`sim_throt_target_ips()`, new in sim_timer), otherwise at the current rate estimate. Now the longest transfer of the boot is 166 ms (long seek + multi-sector read) and there is no mount verification.
3. **The realistic CPU speed did not apply to the boot:** SIMH runs unthrottled until its calibrated timer has done 3 calibrations (register `INT-THROTTLE THROT_DELAY`, default 3); a fast host boots VMS 3.0 (≈ 78 million instructions) inside that window, in about 2 s. **Fixed:** the realism mode sets THROT_DELAY to 0 (`sim_throt_set_delay()`, new in sim_timer) and restores it when sound is disabled.
4. **SIMH bug with THROT_DELAY 0:** the throttle then computed its rate from a stale start time (set when the throttle was configured at the prompt), measured "0 instructions per second" and printed "WARNING: Host CPU could be too slow". **Fixed in `sim_throt_svc()`:** with no delay it takes a fresh start reading and measures briefly (repeating until 10 ms are covered); the throttle is active after about 30 ms (850,000 instructions).

The pre-existing drive-type errors (er=40) on DCLR occur in the normal mode too: VMS probes the drive type. Not related.

### Results

| Run | Boot time to "job terminated" | Notes |
|---|---|---|
| Sound disabled | 2–3 s | unchanged |
| Realism mode, before the fixes | (seconds, unthrottled) | mount verification |
| Realism mode, fixed | 63 s at THROTTLE 300K | no warning, no mount verification, 365 seek sounds |

Test files in `/home/claude/simh-sound/vms`: `v0b.ini` (normal mode, debug), `v1.ini` (realism mode, SPINUP 1, debug), `v3.ini`/`v4.ini`/`v5.ini` (throttle behavior without sound). The image itself is not part of the delivery.

### Open question (settled 2026-11-03)

Settled: the spin-up runs in wall time at the prompt and in simulated time while the CPU runs (see the next section); BOOT right after ONLINE works (VMB waits).

## Spin-up in wall time at the prompt, POWER=ON restores spindles (2026-11-03)

### Where things are

- Branch `sound`: commit 5c21dbc on top of 0defe6e. Patch **`opensimh_5_sound_v4.patch`**: one cumulative commit on plain 87eb7d5 that replaces v1–v3 (apply v4 alone, with `git am --keep-cr`).
- Changed files: `PDP11/pdp11_hk.c`, `sim_sound.c`, `sim_sound.h`, `scp.c`.
- New synthetic spindle sounds (not in the patch, delivered separately): `rk07_spinup.wav` (30 s, 0 → 2400 rpm), `rk07_running.wav` (2 s seamless loop), `rk07_spindown.wav` (30 s), made by `tools/make_rk07_spindle.py` v1 (superseded by `tools/make_spindle_sounds.py`, see below).

### Behavior

- **Spin-up timing (realism mode):** while the CPU runs, `SPINUP` (default 30000 ms) is simulated time, so the guest always sees the full spin-up (with THROTTLE 300K simulated time = real time). While the simulator is stopped at `sim>` the spin-up goes on in wall time: `SET HK0 ONLINE` at the prompt, wait 30 s, BOOT → the drive is ready at once. A spin-up that finishes at the prompt is completed when it is looked at (`SHOW HKn SPINDLE`, `SHOW HK`) or when execution resumes.
- Implementation: `sim_sound` run hooks (`sim_sound_register_run_hook()`, `sim_sound_run_state()` called by `run_cmd()` just before the first instruction and after execution stops). The RK611 keeps the remaining time (`hk_spin_left_us`) and a wall-clock reference at the prompt and reschedules the rest when execution resumes.
- **Power:** `SET SOUND POWER=OFF` silences everything; units that stay active (spindles online or spinning up) get their running loop back at the next power on (`SET SOUND POWER=ON`, RUN, BOOT), fading in over 0.5 s.
- Normal mode (sound disabled): `SET HKn ONLINE` is instant.

### Bugs found and fixed

1. **RUN and BOOT lost a pending spin-up:** `sim_run_boot_prep()` empties the whole event queue before resetting the devices, so the spin-up event was gone and the drive became ready at once. (This also made my statement of 2026-11-03 morning wrong that "VMB waits for the drive": the spin-up had simply vanished at BOOT.) Fixed by keeping the remaining time in the drive state.
2. **A controller clear (CS1 bit 15) cancelled the spin-up:** it cancels every unit whose function code is a transfer (≥ 010), and the internal spin-up code is 016. VMB issues controller clears in its wait loop, so with a real 30 s spin-up the drive never became ready and VMB hung at PC E92. Fixed: the spin-up is excluded.
3. Normal mode: `SET HKn ONLINE` followed by RUN/BOOT could leave the drive "spinning up" for ever (the queued event was lost); now it is instant.

### Tests

Test files in `/home/claude/simh-sound/testC`: `c1.ini` (spin-up timing), `c2.ini` (power off/on with the spindle sounds); `/home/claude/simh-sound/vms`: `v7.ini` (BOOT right after ONLINE, real 30 s), `v8.ini`, `v9.ini` (traces).

| Test | Result |
|---|---|
| A: ONLINE at the prompt (SPINUP 3000), 2 s at the prompt, RUN | up to speed 299,700 instructions after RUN (1.0 s at 300K) |
| B: ONLINE, RUN at once | up to speed after 899,700 instructions (3.0 s) |
| C: ONLINE, 4 s at the prompt | up to speed at the prompt |
| VMS 3.0: ONLINE, BOOT at once, SPINUP 30000 | VMB waits for the drive, VMS boots, 83 s in total |
| Power off / on with spindle sounds (blower muted) | silence while off, the loop fades back in at the same level (−20 dBFS) |
| Spindle sounds: 30 s spin-up → loop → OFFLINE spin-down | continuous level; 1 dB dip for 0.1 s at the hand-over to the loop |
| Normal mode (b4): ONLINE instant, OFFLINE, UNLOAD detaches | ok |
| RK611 seek tests (b1), normal-mode VMS boot (2 s) | unchanged |
| Build of the affected simulators (pdp11, vax730/750/780/8200/8600) plus pdp8 and altair for the core files, `-Werror`; VAX diagnostics 730/750/780/8600; PDP-11 register check | all ok (a full build of all 80 was not needed: no build files changed) |

## ONLINE/OFFLINE wait at the prompt (2026-11-03)

- Branch `sound`: commit 3c19b3b on top of 5c21dbc. Patch **`opensimh_5_sound_v5.patch`** (cumulative, replaces v1–v4). Changed file: `PDP11/pdp11_hk.c`.
- In the realism mode, `SET HKn ONLINE` at the `sim>` prompt or in a command file waits until the drive is up to speed, `SET HKn OFFLINE` until it is at rest (new register `SPINDOWN`, default 30000 ms), with a message at the start and the end:
  ```
  HK0: spinning up, ready in 30 s ...
  HK0: up to speed, ready
  HK0: spinning down, at rest in 30 s ...
  HK0: at rest
  ```
- `SET -N HKn ONLINE` / `OFFLINE` return at once (the spin-up goes on in the background). OFFLINE of a stopped drive, both commands in the normal mode, and UNLOAD by the guest never wait.
- The wait can not be interrupted (it is at most SPINUP or SPINDOWN).
- Tests: `testC/c3.ini` v1 (ONLINE waited 3 s, OFFLINE 2 s, `-N` at once, OFFLINE of a spinning-up drive 2 s, stopped drive and normal mode at once); `c1.ini` v2 uses `-N` and gives the same timing as before; RK611 simulators built without warnings, VAX diagnostics 730/750/780/8600 passed, PDP-11 register check.

## Phase C, step 1: RL01/RL02 and common spindle support (2026-11-03)

### Where things are

- Branch `sound`: commit 8d49009 on top of 3c19b3b. Patch **`opensimh_5_sound_v6.patch`** (cumulative, replaces v1–v5).
- Changed files: `PDP11/pdp11_rl.c`, `PDP11/pdp11_hk.c`, `sim_sound.c`, `sim_sound.h`. New: `sound/rl02/rl02-s.wav`, `rl02-m.wav`, `rl02-l.wav` (copies of the RK07 seek sounds), `sound/rl02/README.txt`.
- Rollout plan as decided: RK07 (done), RL01/RL02 (done), RP/RM (Massbus), RA60 (RQ). Floppies (RX) and TU58 get no spin-up/spin-down; they may come later together. Until real recordings exist, every drive type reuses the RK07 seek sounds, each in its own sound directory.

### Common spindle support (sim_sound)

- `SND_SPINDLE` per drive, functions `sim_spindle_init/load/start/stop/off/ready/state/wait/show` (see `sim_sound.h`). States: stopped, spinning up, ready, spinning down.
- Each spindle has its own timer unit (hidden internal device `INT-SPINDLE`, not saved), so device resets, controller clears and the emptied event queue of RUN/BOOT never lose a spin-up. While the CPU runs: simulated time (the guest always sees the full time); at the prompt: wall time; waiting with messages at the prompt.
- Also shared: `sim_sound_seek()` / `sim_sound_seek_load()` (short/medium/long seek sounds), `sim_sound_activate_us()` (realistic durations at the throttle rate).
- The RK611 was ported onto it; its behavior is unchanged (tests c1, c3, b1, b4 and the VMS boot v7 give the same results). OFFLINE now has a real spinning-down state (an ONLINE meanwhile spins it up again). The hidden register `HK SPIN` is gone: SAVE/RESTORE no longer keeps the spindle state.

### RL01/RL02 behavior

- Decided: with sound enabled, `SET RLn ONLINE` and `SET RLn LOAD` are the LOAD button pressed in (spin up), `OFFLINE` and `UNLOAD` released (spin down). Without sound, ONLINE/OFFLINE are the drive select as before and LOAD/UNLOAD keep their old short sequence.
- Realism mode:
  - ATTACH only loads the cartridge (drive state "Load Cartridge"); a SEEK or transfer fails (seek time out / drive error) until the drive is ready.
  - Spin-up `SPINUP` default 40000 ms (published "start time" 40 s; pdp-11.nl says about 45 s), then "Lock On" with volume check, like a real load. Spin-down `SPINDOWN` default 30000 ms, then "Load Cartridge" (the cover may be opened). A cover that is open (`SET RLn OPEN`) refuses ONLINE.
  - At the prompt the commands wait (messages as for the RK07); `SET -N` returns at once.
  - Seek time t(d) = a + b·d^0.67 through 15 ms (1 cylinder) and 100 ms (full stroke), head switch only 7 ms; transfers: rotational latency 0–25 ms + 625 µs per sector.
  - Seek sounds from `sound/rl02`: short ≤ `SEEK2S` (30), medium ≤ `SEEK2M` (190), else long (RL01: `SEEK1S`/`SEEK1M`, 15/95). Optional `rl02_spinup.wav`, `rl02_running.wav`, `rl02_spindown.wav`.
  - `SHOW RLn SPINDLE` shows the spindle state (not part of SHOW RLn, which shows the drive select).
- Sources: Computer History Wiki (gunkies.org/wiki/RL02), pdp-11.nl (peripherals/disk/rl-info.html).

### Tests

| Test | Result |
|---|---|
| r1 (PDP-11/70, THROTTLE 1M): after ATTACH "Load Cartridge", SEEK fails (RLCS 102206) | ok |
| ONLINE (SPINUP 3000) waits 3 s → "Lock On", volume check | ok |
| Seeks out 10/100/400 cylinders | short/medium/long; 19,864 / 42,616 / 86,936 µs = the model |
| READ at cylinder 510 | no error (RLCS 000215) |
| OFFLINE (SPINDOWN 2000) waits 2 s → "Load Cartridge"; `SET -N RL0 LOAD` at once; UNLOAD waits | ok |
| Normal mode: ATTACH → "Lock On", ONLINE/OFFLINE = drive select | ok |
| VMS 3.0 on the 750 (realism mode), blank RL02 on DLA0: INITIALIZE, MOUNT, COPY 2 files, DIRECTORY, DISMOUNT (v10) | ok, 19 seek sounds (9 short, 10 long), 93 s; only "%INIT-W-NOBADDATA" (blank container without bad block table; `SET RL0 BADBLOCK` would write one) |
| VMS 3.0 boot from the RK07 with the real 30 s spin-up (v7) on the ported RK611 | ok, 88 s |
| Build of pdp11, vax, microvax1/2, rtvax1000, vax730/750/780/8200/8600 (all with the RL11 or RK611), pdp8, altair; VAX diagnostics 730/750/780/8600 and the MicroVAX 3900 core test; PDP-11 register check | all ok, no warnings |

Test files: `testRL/rlprog.ini` (PDP-11 programs: get status with reset, wait ready, seek; seek without waiting; read one sector), `testRL/r1.ini`; `vms/v10.ini` (RL02 under VMS, realism mode), `vms/v11.ini` (scripted VMS login, normal mode).

## Phase C, step 2: Massbus disks (RP device) (2026-11-03)

### Where things are

- Branch `sound`: commit 4890234 on top of 8d49009. Patch **`opensimh_5_sound_v7.patch`** (cumulative, replaces v1–v6).
- Changed file: `PDP11/pdp11_rp.c` (used by pdp11, vax750, vax780, vax8600). New: `sound/rm03`, `rp04`, `rp05`, `rm80`, `rp06`, `rm05`, `rp07`, each with `<type>-s/m/l.wav` (copies of the RK07 seek sounds) and a `README.txt`.

### Behavior

- **Removable packs** (RM02/RM03, RP04, RP05, RP06, RM05): new `SET RPn ONLINE` / `OFFLINE` = RUN/STOP button (common spindle support). Realism mode: ATTACH only loads the pack, medium online (MOL) and drive ready stay off until the drive is up to speed, then attention; `SPINUP` / `SPINDOWN` default 30000 ms (**provisional**: no published start/stop times found); waiting at the prompt unless `SET -N`; the UNLOAD function (RP family) spins the drive down instead of detaching. Normal mode: ATTACH ready at once as before; ONLINE/OFFLINE take effect at once.
- **Fixed disks** (RM80, RP07): no RUN/STOP button (`SET RPn ONLINE` is refused with a message, `SHOW RPn SPINDLE` says "fixed disk, online"); realistic timing and seek sounds as for the others.
- **Timing per drive type** (realism mode): seek t(d) = a + b·d^e with the exponent fitted to the published track-to-track / average (at a third of the stroke) / maximum seek; search adds the rotational latency; transfers: implied seek + latency + sector time × sectors (byte count from the Massbus adapter).

| Type | Pack | rpm | Seek t2t / avg / max (ms) | Fitted exponent | Seek sound limits (cyl) |
|---|---|---|---|---|---|
| RM03 (RM02) | removable | 3600 | 6 / 30 / 55 | 0.637 | 50 / 300 |
| RP04, RP05 | removable | 3600 | 7 / 28 / 50 | 0.631 | 25 / 150 |
| RP06 | removable | 3600 | 10 / 30 / 55 | 0.730 | 50 / 300 |
| RM05 | removable | 3600 | 6 / 23 / – (curve gives 42.7) | 0.69 | 50 / 300 |
| RM80 | fixed | 3600 | 6 / 25 / 50 | 0.754 | 35 / 205 |
| RP07 | fixed | 3633 | 5 / 23 / – (curve gives 43.9) | 0.69 | 40 / 230 |

- Limits are the registers `SEEKRM03S/M`, `SEEKRP04S/M`, `SEEKRP05S/M`, `SEEKRM80S/M`, `SEEKRP06S/M`, `SEEKRM05S/M`, `SEEKRP07S/M`. Optional spindle sounds `<type>_spinup.wav`, `_running.wav`, `_spindown.wav` (removable types).
- SIMH has one type for RM02 and RM03; the RM02 really turns at 2400 rpm (CDC 9762), the timing uses the RM03 (3600 rpm).
- Sources: Computer History Wiki (gunkies.org/wiki/RP04, RP06, RM03, RM05, RM80, RP07), Columbia University computing history (RP06 maximum seek 55 ms), classiccmp drive list. Published start/stop times exist only for the RM80 (start 20 s avg, stop 9 s avg), which has no RUN/STOP button here.

### Tests

| Test | Result |
|---|---|
| VMS 3.5 on the VAX-11/750, sound disabled (p0): boot from the RP06 DBA0, log in | 2 s (the 750 asks for the date: answered by the script) |
| Realism mode (p1): ONLINE DBA0 and a blank RP06 DBA1 (SPINUP 3000), boot, log in | ok |
| INITIALIZE, MOUNT, COPY 2 files, DIRECTORY on DBA1 | ok (`%INIT-W-NOBADDATA` for the blank container, as with the RL02) |
| `DISMOUNT/UNLOAD DBA1:` | VMS issued UNLOAD: "spinning down", pack still attached |
| `SET RP1 ONLINE` at the prompt, VMS `SHOW DEVICE DB` | DBA1 spun up, on line again |
| Seek sounds during the session | DBA0: 280 short, 10 medium, 116 long; DBA1: 6 short, 10 long; 99 s in total |
| PDP-11 (p2): RM80 ONLINE refused, RM05 `-N ONLINE`, OFFLINE waits, MOL/RDY/ATA bits; normal mode MOL/RDY at once and ONLINE/OFFLINE at once | ok |
| Build pdp11, vax750, vax780, vax8600 (no warnings); VAX diagnostics 750/780/8600; PDP-11 register check | ok |

Test files: `vmsrp/p0.ini` (v2), `p1.ini`, `p2.ini` (v1). The RP06 image itself is not part of the delivery.

## Phase C, step 3: RA60 on the MSCP controllers (RQ device) (2026-11-04)

### Where things are

- Branch `sound`: commit 66c600e on top of 4890234. Patch **`opensimh_5_sound_v8.patch`** (cumulative, replaces v1–v7; checked by `git am --keep-cr` on a fresh 87eb7d5: identical tree).
- Changed files:
  - `PDP11/pdp11_rq.c` (used by pdp11 and all VAX simulators with an MSCP controller);
  - `PDP11/pdp11_rl.c` (SHOW RLn SPINDLE now ends its line: SCP adds no line end for MTAB_NMO entries).
- New: `sound/ra60` with `ra60-s/m/l.wav` (copies of the RK07 seek sounds) and a `README.txt`.

### Behavior

- **Only the RA60** (the only removable-pack drive of the RA series) gets the RUN/STOP button: `SET RQn ONLINE` / `OFFLINE`, using the common spindle support.
  - Other drive types refuse with "the RD54 has no RUN/STOP button (only the RA60)".
  - `SHOW RQn SPINDLE` (MTAB_NMO, shown only when asked) gives "offline", "spinning up, n s left", "online", "spinning down …", "no pack" or "fixed disk (no RUN/STOP button)".
- **While the RA60 is not up to speed**, the unit reports ST_OFL | SB_OFL_NV ("offline, no volume", the MSCP status for a drive stopped by its RUN/STOP switch). This applies to GET UNIT STATUS, ONLINE and transfers.
  - Spinning down clears the unit's online state, so the host must bring it online again (VMS mount verification does that).
  - A transfer serviced while the drive is stopped ends with that status.
  - When the drive is up to speed, the controller sends the "available" attention message, as after ATTACH (UNIT_ATP, rq_tmrsvc).
- **Realism mode:**
  - ATTACH only loads the pack; the drive waits for `SET RQn ONLINE`.
  - SPINUP/SPINDOWN default to 35000 ms (DEC brochure: start 35 s, stop 35 s). They are registers of RQ only, like QTIME/XTIME, and apply to all four controllers.
  - AVAILABLE with the spin-down modifier (VMS `DISMOUNT/UNLOAD`) spins the RA60 down; the pack stays attached.
  - Transfers take seek + rotational latency (0..16.7 ms) + one sector slot per block (43 slots per track: 42 + 1 spare) + 7 ms per cylinder crossed.
  - Seek curve t(d) = a + b·d^e fitted to 7 / 42 / 70 ms (track-to-track / average at a third of the stroke / maximum; DEC RA60/RA80/RA81 brochure, Aug 1982). gunkies.org gives 6.7 / 41.7 / 75 ms and start 60 s / stop 40 s maximum.
  - Seek sounds `ra60-s/m/l.wav`, limits `SEEKRA60S` / `SEEKRA60M` = 100 / 600 cylinders; optional `ra60_spinup/_running/_spindown.wav`.
- **Normal mode:** ATTACH spins up at once (as before), ONLINE/OFFLINE take effect at once, timing unchanged (QTIME/XTIME), AVAILABLE with spin-down unloads as before.
- The per-unit state (spindle, current cylinder) is allocated on first use, because a controller can have up to 254 units.

### Tests (VMS V3.5, freshly installed by Ulli on an RA60 for 4 MB, `testRA/`)

| Test | Result |
|---|---|
| Normal mode (n1): boot DUA0, log in, SHOW DEVICE DU, SHOW MEMORY | ok |
| Normal mode: `SET RQ0 OFFLINE` under the running system, a COPY | "_DUA0: is offline. Mount verification in progress." |
| … then `SET RQ0 ONLINE` | "has completed mount verification", COPY completes |
| Realism mode (r1): ATTACH → both spindles "offline"; `SET RQ0 ONLINE` waits 5 s (SPINUP 5000), boot, log in | ok |
| Realism mode: OFFLINE (waits for the spin-down) / ONLINE under the running system | mount verification as in normal mode |
| Realism mode (r3): data RA60 `SET -N RQ1 ONLINE` (10 s) and MOUNT at once | MOUNT asks the operator "Please mount device _DUA1:" and finishes by itself when the drive is up to speed |
| … COPY, DIRECTORY, DELETE, `DISMOUNT/UNLOAD DUA1:` | ok; RQ1 "spinning down", still attached |
| Realism mode (r4): time to the end of STARTUP | 56 s simulated (= wall), 1,710 seek sounds in r1 (1,641 short, 69 long) |
| Realism mode (b1): BOOT RQ0 with the drive stopped | VMB `%BOOT-F-Failed to initialize device`, as with a stopped drive |
| Blank RA60 MOUNT (n2 normal, r2 realism) | `%MOUNT-F-NOHOMEBLK` in both |
| MicroVAX 3900 console (m1): SHOW DEVICE with the RA60 stopped and running | ok |
| Build vax750, pdp11, microvax3900 (no warnings); 750 diagnostics, MicroVAX 3900 core test | ok |

Notes from testing:
- VMS 3.5's port driver initializes the UDA50 only some seconds after STARTUP, so DUA units appear late on a system booted from another disk (the earlier "no such device DU" was only that).
- The fresh 4 MB installation has no `INIT.EXE`, so INITIALIZE fails.
- A long typed-ahead command string can overflow the terminal type-ahead buffer (78 characters by default). Scripts send one command at a time.

Test files: `testRA/n1.ini`, `n2.ini`, `n4.ini` (normal mode), `r1.ini`, `r2.ini`, `r3.ini`, `r4.ini` (realism mode), `b1.ini`, `m1.ini` (MicroVAX 3900), `t0*.ini` and `t1.ini` (UDA50 checks on the RP06 system). The RA60 image itself is not part of the delivery.

## Phase C, step 4: RB02 and RB80 on the RB730 (VAX-11/730) (2026-11-04)

### Where things are

- Branch `sound`: commit 2b4b1e4 on top of 66c600e. Patch **`opensimh_5_sound_v9.patch`** (cumulative, replaces v1–v8; checked by `git am --keep-cr` on a fresh 87eb7d5: identical tree).
- Changed files:
  - `VAX/vax730_rb.c` (vax730 only);
  - `sim_sound.c` / `sim_sound.h`: new `sim_sound_register_storage (DEVICE *)`. The RB730 does not use sim_disk, so its device is not `DEV_DISK`, and SET SOUND ENABLED/DISABLED would otherwise be allowed while an RB drive is attached.
- No new sound directory: the RB02 uses `sound/rl02` (it is an RL02), the RB80 `sound/rm80` (the R80 is the RM80's drive).

### Behavior

- **RB02:** `SET RBn ONLINE` / `LOAD` and `OFFLINE` / `UNLOAD` are the LOAD button (common spindle support); `SHOW RBn SPINDLE`; `HELP RB` (new).
  - While the drive is not up to speed:
    - GET STATUS shows the drive state (0 load, 1 spin-up, 7 spin-down);
    - RBCS drive ready is off;
    - seeks and transfers end with ERR + DRE + INCMP and spin error.
  - So VMS sees "medium offline": MOUNT asks the operator ("Please mount volume …") and completes by itself when the drive comes up; a mounted volume goes into mount verification.
  - When the drive is up to speed: locked on, heads at cylinder 0, volume check.
- **RB80:** fixed disk (ONLINE refused, "fixed disk (no LOAD button)"), ready when attached.
- **Realism mode:**
  - ATTACH of an RB02 waits for ONLINE.
  - SPINUP 40000 / SPINDOWN 30000 ms, as the RL02.
  - Seek t(d) = a + b·d^e (absolute seeks; distance from the current cylinder):
    - RB02 as the RL02: 15 ms track-to-track, 100 ms maximum, e = 0.67, head switch 7 ms;
    - RB80 as the RM80: 6/25/50 ms, fitted e = 0.754.
  - Transfers: rotational latency (2400 / 3600 rpm) plus one sector time per sector, up to the end of the track; read header waits one sector.
  - Seek sounds: `rl02-s/m/l.wav` and `rm80-s/m/l.wav`; registers `SEEK02S/SEEK02M` (30/190), `SEEK80S/SEEK80M` (35/205); optional `rl02_spinup/_running/_spindown.wav`.
  - The seek with disk address 0xFFFFFFFF keeps the old timing.
- **Normal mode:** ATTACH spins up at once; ONLINE/OFFLINE at once (OFFLINE now also gives the "medium offline" behavior); timing unchanged.

### Tests (VMS V3.5 on an RB80, freshly installed by Ulli for 4 MB, flat image; `testRB/`)

| Test | Result |
|---|---|
| Normal mode (n1): boot DQA0, INITIALIZE/MOUNT/COPY on a blank RB02 (DQA1) | ok (`%INIT-W-NOBADDATA`, blank container) |
| Normal mode: `SET RB1 OFFLINE` with DQA1 mounted, COPY from it | "_DQA1: is offline. Mount verification in progress." |
| … then `SET RB1 ONLINE` | mount verification completed, COPY ok |
| Realism mode (r1): RB80 "fixed disk", RB02 "offline" after ATTACH; boot, log in | ok |
| `SET RB1 ONLINE` (SPINUP 3000), INITIALIZE, `SET RB1 OFFLINE` | ok |
| MOUNT with the drive stopped | VMS: "Please mount volume RB02T in device _DQA1:"; `SET -N RB1 ONLINE` → mounted by itself |
| COPY, `SET RB1 OFFLINE` while mounted, COPY from it, `SET RB1 ONLINE` | mount verification started and completed, COPY ok |
| Seek sounds in r1 | RB0: 293 short, 14 medium, 111 long; RB1: 10 short, 8 long |
| Build vax730, vax750, pdp11 (no warnings); diagnostics 730/750 | ok |

- First attempt (finding, now handled): with only ERR + INCMP and drive ready on (the code path for a missing cartridge), VMS reported "fatal controller error" (MOUNT) and "parity error" (COPY) and started no mount verification. Known issues case 19.
- Test files: `testRB/n1.ini` (v2, normal mode), `testRB/r1.ini` (v2, realism mode). The RB80 image itself is not part of the delivery.
- The device table (`SIMH_Sound_devices.ods`) is at v2 with the RB02 and RB80.

## RB730 fixes (separate patch) and the sound patch rebased on them (2026-11-04)

### Where things are

- **Branch `rb730`** (worktree `/home/claude/open-simh/rb730`) on plain 87eb7d5, five commits:

  | Commit | Step | Cases |
  |---|---|---|
  | cfddc9c | DEC logical block layout, `SET RBn LAYOUT=OLD` | 4 |
  | 13006f4 | sim_disk | 7 |
  | 6d9c3aa | status and error reporting | 1, 2, 3, 19 |
  | 2bd1b66 | range checks | 5 |
  | c2304ad | seek completion per drive, RB80 with 31 sectors per track | 21, 20 |

  Patch **`opensimh_6_rb730_v1.patch`**: one commit on 87eb7d5, independent of the sound work. Checked by `git am --keep-cr`: identical tree.
- **Branch `sound`** rebased onto `rb730`. The old state is kept as branch `sound-v9`.
  - The RB02 realism commit was rebuilt on the fixed RB730 (now 1e0e392).
  - `sim_sound_register_storage` was removed again: the RB device is now `DEV_DISK`.
  - Patch **`opensimh_5_sound_v10.patch`** (cumulative) now applies **on top of `opensimh_6_rb730_v1.patch`**. Apply 6 first, then 5; this gives exactly the `sound` tree.
- **Converter `tools/rb730_layout_convert.py`** (v2): `old2std` / `std2old` for `rb02` / `rb80`; the RB80 goes from 32 to 31 sectors. It replaces `rl02_rb02_convert.py`.
- **Ulli's VMS V3.5 RB80 image** converted to the standard layout: `out/VMS035-4MB_std.RB80` (124,214,272 bytes; gzip 5.6 MB).

### Regression set

`rbtest/rbtest.py` (v3):
- T1, VAX-11/730: boot VMS V3.5 from the RB80, log in, SHOW DEVICE DQ, `MOUNT/NOASSIST` on an empty RB02, then on a blank RB02 INITIALIZE, MOUNT, COPY, DIRECTORY, DUMP, DISMOUNT.
- T2, VAX-11/750 RL11: the same cartridge mounted, listed and dumped; the dump must match T1's.

| Run | Result |
|---|---|
| Upstream baseline | 8/8; empty drive "fatal controller error" |
| Each step: standard flat image, old image with LAYOUT=OLD, VHD | 8/8 |
| After step 3 | empty drive `%MOUNT-F-MEDOFL` |
| Final rb730 | 8/8 in all three variants; "Contains ODS2 File system, Sectors In Volume: 242606" at ATTACH |
| Sound branch rebased, normal mode | 8/8 |
| Sound branch rebased, realism mode (`rbtest/r2/r1.ini`): MOUNT with the drive stopped (operator request), mount verification, COPY/DIRECTORY | ok; "Invalid cylinder" 0 (was 3 before case 21) |

The device table is at v3: RB80 with 31 sectors per track.

## Device table (2026-11-04)

- `SIMH_Sound_devices.ods` (table v1) lists every device prepared for sound, with three sheets:
  - **Disks:** rotation speed, revolution and latency, sector time, published and modelled seek times, average access, spin-up/spin-down times and where they come from, seek-sound limits, registers, sound directory, sources;
  - **Systems:** the system sounds and the default THROTTLE;
  - **Notes:** how the values are derived.
- Generated by `tools/make_sound_devices_ods.py`, from the same data as the device sources.
- **Standing instruction (Ulli): keep this table up to date whenever a device gets sound support.** Add a row to the generator, bump `TABLE_VERSION` and the version history on the Notes sheet, regenerate, and deliver the table with the patch.

## Spindle sound generator for all removable drives (2026-11-04)

- `tools/make_spindle_sounds.py` (v1) replaces `make_rk07_spindle.py` v1 (DRY: one script, the drives as data).
- **Generated files:** `<prefix>_spinup.wav`, `<prefix>_running.wav` (seamless loop, a whole number of revolutions, about 2 s) and `<prefix>_spindown.wav`.
- **Drives,** each with rotation speed, spin-up and spin-down times (the SIMH SPINUP/SPINDOWN defaults) and sound-character parameters (harmonic profile, distortion, air-noise level and band):

  | Prefix | Drive | rpm | Spin-up / spin-down |
  |---|---|---|---|
  | `rk07` | RK06/RK07 | 2400 | 30 / 30 s |
  | `rl02` | RL01/RL02 | 2400 | 40 / 30 s |
  | `rm03`, `rp04`, `rp05`, `rp06`, `rm05` | removable Massbus packs | 3600 | 30 / 30 s (provisional) |
  | `ra60` | RA60 | 3600 | 35 / 35 s |

- RM80 and RP07 are left out: they are fixed disks, and SIMH plays no spindle sounds for them.
- **Usage:** `python3 -I make_spindle_sounds.py --list`, or `python3 -I make_spindle_sounds.py [--tree] <outdir> [prefix ...]`.
  - `--tree` writes the files into `<outdir>/<prefix>/`.
  - `--spinup S`, `--spindown S` and `--rpm N` override the defaults for one drive, e.g. when SPINUP/SPINDOWN are changed, so the sound lasts as long as the drive takes in SIMH.
- **Checks:**
  - all eight sets in 7 s (50 MB);
  - lengths exact;
  - the same level at the end of the spin-up, in the loop and at the start of the spin-down;
  - peak −7 dBFS;
  - the loop wraps without error: harmonics 4·10⁻¹³, noise periodic.
- The generated WAV files are not part of the patch, like the RK07 ones.

## Phase C, step 5: RC25 on the MSCP controllers (RQ device) (2026-11-06)

### Where things are

- Branch `sound` of vaxorcist/open-simh-extended: commit 76d3fd07 on top of 1e0e392; branch `integration`: the same change as 0c1bc2a8.
- Patch **`opensimh_5_sound_v12.patch`** (cumulative, replaces v1–v11, applies on top of `opensimh_6_rb730_v1.patch`; like v11 without the Visual Studio project files). Checked by `git am --keep-cr` on a fresh 87eb7d5: patches 6 + 5 give exactly the tree of v11 plus the RC25 commit; the full order 1, 2, 3, 4, 6, 5 applies.
- Changed files: `PDP11/pdp11_rq.c` (pdp11 and every VAX with an MSCP controller), `sim_sound.c` (known issues case 23). New: `sound/rc25` with `rc25-s/m/l.wav` (copies of the RK07 seek sounds) and a `README.txt`.
- Source of the drive facts: **RC25 Disk Subsystem User Guide, EK-ORC25-UG-002** (Sep 1983), supplied by Ulli.

### Facts from the RC25 User Guide

| Item | Value |
|---|---|
| Platters | two 8" platters on **one spindle**: removable cartridge (even unit number) and fixed platter (next odd unit number) |
| Capacity | 26,061,824 bytes per platter = 50,902 blocks of 512 bytes (31 sectors × 2 surfaces × 821 cylinders) |
| Speed | 2850 ± 9 rpm; average latency 10.5 ms, maximum 21.0 ms |
| Seek | one track 10 ms, average 35 ms max., maximum 55 ms max.; average access 45.5 ms |
| Spiral read | 31 ms per track, 50 s per platter, 1 min 40 s per drive |
| Start / stop | 60 s (includes purge and self-test) / 30 s |
| Run button | alternate action; spins both platters up or down. The host can spin the drive down while Run is pressed; release and press Run again to start it |
| Cartridge | "The fixed disk does not spin-up and run without a removable cartridge in place" |
| Write protect | one button per platter (SIMH: SET RQn LOCKED per unit, as before) |
| Master/slave | a master drive has the controller and serves up to two spindles (overlapped seeks) |

### Behavior

- **Pairs:** an RCF25 belongs to the RC25 in the unit just before it, at an even unit (RQ0/RQ1, RQ2/RQ3, ...). An RCF25 without such an RC25 is a fixed disk of its own, as before. A lone RC25 has its own spindle.
- **Run button:** `SET RQn ONLINE` / `OFFLINE` on **either** unit of a pair acts on the one spindle; `SHOW RQn SPINDLE` shows it ("spindle of RQ0: online", "no cartridge in RQ0, stopped", ...). ONLINE needs the cartridge (RC25 unit attached).
- **Realism mode:**
  - ATTACH only loads the cartridge (or the fixed platter's image); the drive waits for ONLINE. Spin-up `SPINUPRC25` 60000 ms, spin-down `SPINDOWNRC25` 30000 ms; at the prompt the commands wait unless `SET -N`.
  - While the spindle is not up to speed, both units report "offline, no volume"; spinning down takes both out of the online state; at speed both get the "available" attention.
  - No cartridge: the RCF25 stays offline; DETACH of the cartridge stops the fixed platter too.
  - AVAILABLE with spin-down (VMS `DISMOUNT/UNLOAD`) on either unit spins the drive down **only if the other unit is not online** to the host (otherwise the unit only becomes available). See open question 1.
  - Timing (both units, **one shared actuator**): seek curve fitted to 10/35/55 ms (exponent 0.512), rotational latency 0..21.05 ms, 31 sector slots per revolution (0.679 ms per block), plus 9.95 ms per track crossed (31 ms spiral read − one revolution). The model's average access is 45.5 ms, as published.
  - Seek sounds `sound/rc25/rc25-s/m/l.wav`, limits `SEEKRC25S` / `SEEKRC25M` = 50 / 300 cylinders (821 cylinders). Optional `rc25_spinup.wav`, `rc25_running.wav`, `rc25_spindown.wav`.
- **Normal mode:** timing unchanged (QTIME/XTIME); ATTACH of the cartridge spins up at once; the Run button acts on both units at once; an RCF25 works without a cartridge in its RC25 (as before), SHOW says so.
- The RA60 code is now table driven (`rq_rlz_types[]`: one entry per drive type); RA60 behavior and timing are unchanged (test c5). `SET RQ DEBUG=TRACE` now logs each realistic transfer: LBN, blocks, cylinders, time.

### Tests (MicroVAX 3900 console, no operating system yet; `tests/rc25/` on the notes branch)

| Test | Result |
|---|---|
| c1 (realism): RCF25 without cartridge: SHOW "no cartridge in RQ0, stopped", ONLINE refused; cartridge attached: "offline"; `SET RQ1 ONLINE` spins up the drive of RQ0 (waits 2 s) | ok |
| c1: lone RCF25 at RQ2: "fixed disk", ONLINE refused; RA60 without pack refused | ok |
| c1: console SHOW DEVICE lists DUA0 (RC 25), DUA1 (RCF25); BOOT DUA1 reads the blank disk (`?43 FILESTRUCT`) | ok |
| c1: OFFLINE via RQ0 waits 1.5 s; `SET -N RQ0 ONLINE` then DETACH RQ0 during the spin-up: RQ1 "no cartridge in RQ0, stopped" | ok |
| c2 (realism): BOOT DUA1 with the drive stopped | `?4D DEVOFFLINE` |
| c4 (realism): VMB boot block loads 100 blocks at LBN 40000 of DUA1 | long seek 645 cylinders, 156.8 ms (model 147–168 ms) |
| c4: then BOOT DUA0 (cartridge) | first read seeks **646 cylinders back** (shared actuator, 51.4 ms); 10 blocks at LBN 3000: short seek 48 cylinders, 50.5 ms |
| c5: RA60 regression (realism), 50 blocks at LBN 300000 | long seek 1190 cylinders, 86.2 ms (old model 80.2–96.9 ms) |
| c6 (normal mode): RCF25 alone boots; with cartridge both "online"; `SET RQ1 OFFLINE` → BOOT DUA1 `?4D DEVOFFLINE`; ONLINE, DETACH of the cartridge → RCF25 boots again | ok |
| pdp11 with `SET RQ KLESI`, RC25/RCF25: `SET -N RQ0 ONLINE` → "spinning up, 60.0 s left" | ok |
| Build (`-Werror`, no warnings): microvax3900, microvax2, pdp11, vax730, vax750, vax780, vax8600; `pdp11_rq.c` and `sim_sound.c` also with `-Wall -Wextra` | ok |
| VAX diagnostics 730/750/780, MicroVAX 3900 core test | passed |
| VMS on an RC25 | see the next section (2026-11-06) |

Found while testing (not changed): known issues case 22 (`?53` self-test error after a failed VMB boot; also with plain ATTACH in normal mode) and case 24 (RC25 on any controller type).

### Tests with VMS V3.5 on the VAX-11/730 (2026-11-06)

Ulli's image `VMS035.RC25` (26,061,824 bytes = 50,902 blocks; volume VAXVMSRL3, ODS-2) is the **fixed platter**: RQ1 RCF25 = DUA1, the system disk. The cartridge RQ0 RC25 = DUA0 starts as an empty file. Test script `tests/rc25/rc25test.py` v2 (notes branch); the image is copied, the original stays unchanged. VMS V3.5 does not ask for the date on the 730; SYSTEM/MANAGER.

| Test | Result |
|---|---|
| N1 (normal mode): boot DUA1, log in, SHOW DEVICE DU (DUA0–DUA3 on line, DUA1 mounted) | ok, 13 s for the whole run |
| N1: INITIALIZE DUA0 RC25T, MOUNT, COPY 2 files, DIRECTORY/SIZE, DISMOUNT/NOUNLOAD | ok, no error messages |
| N1: `SET RQ0 OFFLINE` (Run button) under the running system, then a DCL command | "_DUA1: is offline. Mount verification in progress." (the system disk turns on the same spindle) |
| N1: `SET RQ1 ONLINE` (button via the fixed unit) | "_DUA1: has completed mount verification", the command completes |
| N1: MOUNT DUA0, DISMOUNT/UNLOAD DUA0 | the cartridge is detached (old normal-mode behavior; known issues case 25) |
| R1 (realism, SPINUPRC25 5000, SPINDOWNRC25 3000): `SET RQ1 ONLINE` with no cartridge | refused: "no cartridge in RQ0: the RC25 does not start without one" |
| R1: cartridge attached, `SET RQ1 ONLINE` (fixed unit) | waits 5 s, "RQ0: up to speed, ready"; boot DUA1, log in |
| R1: the same DCL work on DUA0 | ok, no error messages |
| R1: DISMOUNT/UNLOAD DUA0 with DUA1 online | the drive keeps running (SHOW RQ0 SPINDLE "online"), cartridge stays attached |
| R1: `SET -N RQ0 OFFLINE` under the running system | spin-down 3 s; DUA1 into mount verification |
| R1: `SET RQ0 ONLINE` | waits 5 s for the spin-up; DUA1 completes mount verification |
| R1: transfers / seek sounds (whole run, 42 s) | 839 realistic transfers, longest 124 ms; seek sounds DUA1 226 short / 177 medium / 79 long, DUA0 8 / 4 / 12 (DUA0 seeks start where DUA1 left the shared actuator) |
| No `%...-W/E/F-` messages and no error log entries in either run | ok |

Note: VMS `DISMOUNT` unloads by default (`/NOUNLOAD` keeps the volume loaded); the first script version lost the cartridge that way.

### Spindle sounds and device table

- `tools/make_spindle_sounds.py` **v2**: new drive `rc25` (2850 rpm, spin-up 60 s, spin-down 30 s) and the row field `reach`: the motor is at speed after 25 s (provisional), the rest of the 60 s spin-up (purge, self-test) runs at speed. Check: lengths 60.00 / 2.000 / 30.00 s, level −15.3 dBFS at the end of the spin-up, in the loop and −15.8 at the start of the spin-down, peak −6.9 dBFS, loop of 95 revolutions without a seam. Other drives unchanged.
- `docs/SIMH_Sound_devices.ods` table **v4**: rows RC25 and RCF25 (generator `tools/make_sound_devices_ods.py` v5).
- New tool `tools/make_vmb_seek_disk.py` v2: a raw disk image with a MicroVAX VMB boot block that loads N blocks at a given LBN, to drive realistic transfers from the console.

### Open questions (RC25): answered by Ulli 2026-11-07

1. Host spin-down while the other platter is online: **the whole drive stops** and the other unit goes offline (realistic) — done in v13.
2. Motor run-up 25 s within the 60 s start: **a good guess**, kept.
3. Spiral read 31 ms per track (0.51 MB/s) instead of 0.57 MB/s: **ok**, kept.
4. Lone RC25/RCF25: **refused by SET SOUND ENABLED**, allowed without sound — done in v13.
5. Pairing: by unit index **and** by unit number (even/odd, the drive's unit select plug); SET RQ0 UNIT=m also renumbers RQ1 — done in v13.
6. Image: the system disk is the fixed platter DUA1 (settled 2026-11-06).

## Phase C, step 5b: RC25 realism rules, KLESI, geometry, unlimited spindles (2026-11-07)

### Where things are

- Branch `sound`: commit f5a394af on top of 76d3fd07; branch `integration`: the same change as dcdeceec.
- Patch **`opensimh_5_sound_v13.patch`** (cumulative, replaces v1–v12, applies on top of `opensimh_6_rb730_v1.patch`, without the Visual Studio project files). Checked by `git am --keep-cr` on a fresh 87eb7d5: patches 6 + 5 give exactly the tree of the work branch; the full order 1, 2, 3, 4, 6, 5 applies.
- Changed files: `PDP11/pdp11_rq.c`, `sim_sound.c`, `sim_sound.h`.

### Decisions (Ulli, 2026-11-07)

- SET SOUND ENABLED means **"as realistic as possible"** in general; without sound the old behavior stays for quick setups.
- A KLESI connects only RC25 drives (TU81 and RV20: later; the TU81 is the separate TQ device for now).

### Behavior

- **Geometry (both modes, known issues 18):** RC25/RCF25 31 sectors × 2 surfaces × 821 cylinders in GET UNIT STATUS. VMS V3.5 `F$GETDVI` on DUA1: 31/2/821 (upstream: 50/8/128); MAXBLOCK 50902 unchanged.
- **Configuration rules (realism mode only):**
  - RC25 drives only on a **KLESI**, and a KLESI only connects RC25 drives: a master and at most one slave, i.e. **at most 4 enabled units**, all RC25/RCF25.
  - An RC25 drive is a **pair**: the RC25 at an even unit with an **even unit number** (PLUG, `SET RQn UNIT=`), the RCF25 at the next unit with the next number.
  - `SET SOUND ENABLED` refuses a configuration that breaks a rule (new `sim_sound_register_enable_check()`; the message names the unit and the rule).
  - While sound is enabled:
    - `SET RQ KLESI` sets units 0/1 and 2/3 to RC25/RCF25 (unit numbers as pairs) and disables the units from 4 on; refused if an attached unit would have to change;
    - `SET RQ <other type>` on a KLESI gives the RC25/RCF25 units their initial drive types back;
    - `SET RQn RC25` also sets the next unit to RCF25 (`SET RQn RCF25`: the unit before to RC25);
    - `SET RQn UNIT=m` also renumbers the other half (m must be even for the RC25, odd for the RCF25);
    - `SET RQ DRIVES=n` above 4 on a KLESI, other drive types on a KLESI, and an ATTACH in a configuration that breaks a rule are refused; a refused SET leaves every unit as it was.
  - Without sound: any configuration as before, and `SET RQ0 RC25` does not touch RQ1.
- **AVAILABLE with spin-down** (VMS `DISMOUNT/UNLOAD`, also plain `DISMOUNT`) on either unit of an RC25 drive **always stops the whole drive**; the other unit goes offline (with the system disk on DUA1: mount verification until `SET RQ0 ONLINE`).
- **KLESI details (realism mode):** controller timeout 15 s in SET CONTROLLER CHARACTERISTICS (was 120); ABORT aborts only queued commands, never the active one. Already as the RC25 without a change: step 1 without NV, with DI (and QB on the Q-bus), 15 command credits (= 14 + 1 immediate), DETERMINE ACCESS PATHS / COMPARE CONTROLLER DATA / FLUSH as NOPs, CSR 172150.
- **Spindles (known issues 23):** no fixed limit; chunks of 32 with their own internal timer device (INT-SPINDLE, INT-SPINDLE2, ...).

### Tests

| Test | Result |
|---|---|
| c10 (vax730, commands only): SET SOUND ENABLED with the default configuration | accepted |
| c10: `SET RQ0 RC25` on a UDA50 | refused: "an RC25 needs a KLESI controller (SET RQ KLESI)" |
| c10: `SET RQ KLESI` | units 0–3 RC25/RCF25/RC25/RCF25 with UNIT=0–3, "KLESI, 4 units" |
| c10: `SET RQ0 UNIT=300` / `SET RQ1 UNIT=303` | the other half becomes 301 / 302 |
| c10: odd number on the RC25, even on the RCF25 | refused ("needs an even/odd unit number") |
| c10: `SET RQ1 RD54` on the KLESI, `SET RQ DRIVES=6` | refused, unit unchanged |
| c10: `SET RQ UDA50` | units back to RD54, RD54, RD54, RX50 |
| c10: without sound a lone RC25 on a UDA50; then `SET SOUND ENABLED` | accepted / refused |
| c10: KLESI set without sound with the default RD54 units; `SET SOUND ENABLED` | refused: "a KLESI only connects RC25 drives, RQ0 is an RD54" |
| c10: without sound `SET RQ0 RC25` | RQ1 stays RD54 |
| c1 v2, c2 v4, c4 v5 (MicroVAX 3900, realism with `SET RQ KLESI`) | as before: Run button, no start without cartridge, `?4D DEVOFFLINE` with the drive stopped, transfer times identical (156,823 µs, shared actuator 646 cylinders back) |
| c5 (RA60), c6 (normal mode) | unchanged |
| N1 (VMS V3.5, vax730, normal mode) | all ok, as before |
| R1 v3 (realism, `SET RQ KLESI`): boot, DCL work on DUA0 | ok |
| R1: `DISMOUNT/UNLOAD DUA0:` | the drive stops, "_DUA1: is offline. Mount verification in progress."; `SET RQ0 ONLINE` (5 s) → "has completed mount verification" |
| R1: Run button OFFLINE/ONLINE afterwards | mount verification again, completed; no VMS warnings or errors; 76 s in total |
| g1: geometry via F$GETDVI | 31/2/821/50902 (upstream 50/8/128/50902) |
| pdp11: `SET RQ KLESI`, `SET RQB KLESI` | RQB units numbered 4/5, 6/7 (pairs); spin-up on RQB1 via the fixed unit |
| pdp11: 120 spindles (HK 8, RL 4, RP 8, 100 RA60 on RQ/RQB) | the last one spins up |
| Build `-Werror` (microvax1/2/3900, rtvax1000, pdp11, vax730/750/780/8200/8600); `pdp11_rq.c`, `sim_sound.c` with `-Wall -Wextra` (VAX and PDP-11) | no warnings |
| VAX diagnostics 730/750/780/8600, MicroVAX 3900 core test | passed |

Not tested: the KLESI ABORT and timeout paths (VMS issued neither in the tests).

### Case 22 (`?53` self-test error)

Narrowed down, not fixed: it happens on plain Open SIMH with nothing attached after a failed VMB boot, and the failing test number varies; it looks like a timing problem of the KA655 self-test after BOOT CPU, not an RQ problem (known issues case 22). Proposal: a separate investigation.

## Default sounds ("DRY") (2026-11-07)

### Where things are

- Branch `sound`: commit c4f8d503 on top of f5a394af; branch `integration`: the same change as bf6a01f7.
- Patch **`opensimh_5_sound_v14.patch`** (cumulative, replaces v1–v13, applies on top of `opensimh_6_rb730_v1.patch`, without the Visual Studio project files). Checked by `git am --keep-cr` on a fresh 87eb7d5: identical tree; the full order 1, 2, 3, 4, 6, 5 applies. The patch got smaller (about 950 KB, v13 993 KB): ten copies of the seek sounds are gone.
- Changed: `sim_sound.c`, `sim_sound.h`, `scp.c` (HELP SET SOUND), `VAX/vax750_syslist.c`, the help texts of `PDP11/pdp11_hk.c`, `pdp11_rl.c`, `pdp11_rp.c`, `pdp11_rq.c`, `VAX/vax730_rb.c`.
- Sound tree now:

  | Directory | Files | Was |
  |---|---|---|
  | `sound/default_disk` | `seek-s.wav`, `seek-m.wav`, `seek-l.wav`, `README.txt` (table of all drives with their directory/prefix and seek limits) | `sound/rk07/rk07-s/m/l.wav` |
  | `sound/default_cpu` | `starting.wav`, `running.wav`, `stopping.wav`, `README.txt` | `sound/vax750/blower_starting/running/stopping.wav` |
  | (removed) | | `sound/rl02`, `rm03`, `rm05`, `rm80`, `rp04`, `rp05`, `rp06`, `rp07`, `ra60`, `rc25` (copies of the RK07 seek sounds) |

### Decisions (Ulli)

- Default sounds for disks, tapes and CPUs, used when there are no real sounds.
- Disks: the RK07 seek sounds, directory renamed to `default_disk`, generic file names. **No default spindle sounds.**
- CPUs: the vax750 sounds, moved to `default_cpu`, generic file names; **opt-in per simulator.**
- Tapes: the first tape sound we make (`default_tape`, name reserved as `SND_DEFAULT_TAPE`).

### Behavior

- **Per file:** a device first looks for its own file (`<dir>/<prefix>-s.wav` …, or for a simulator `<sim>/starting.wav` …) and falls back to the default file; both are searched in the usual order (SET SOUND PATH, SIMH_SOUND_PATH, `<exe>/sound`, `<exe>/../sound`, `./sound`). So real recordings can be added later, also by the user, without touching the defaults.
- A missing own file is not reported when the default exists; a missing default is reported once.
- `SET SOUND STARTING=/RUNNING=/STOPPING=` (user files) have no fallback.
- `SHOW SOUND` has a new line `Defaults:`, e.g. `vax750 (system) -> default_cpu, rk07 (seek) -> default_disk, rl02 (seek) -> default_disk, ...`; the system sound lines show the file actually used.
- `sim_sound_use_default_cpu ()`: the opt-in; the vax750 calls it with own names `starting.wav`, `running.wav`, `stopping.wav` in `sound/vax750` (none bundled).
- `HELP SET SOUND` has a new topic "Default sounds"; the device help texts name the fallback.
- Also fixed (known issues case 27): the unit loop and run hook registries grow as needed (were 64 and 8).

### Tests (`tests/sound/` on the notes branch)

| Test | Result |
|---|---|
| d1 (vax750, realism): SHOW SOUND after SET SOUND ENABLED | system sounds from `default_cpu` (0.73 / 4.27 / 1.44 s), `Defaults: vax750 (system) -> default_cpu` |
| d1: ATTACH of RK07, RL02, RP06, RA60 | no "not found" message; `Defaults:` lists rk07, rl02, rp06, ra60 (seek) -> default_disk |
| d2: an own `vax750/running.wav` via SET SOUND PATH | RUNNING from the own file, STARTING/STOPPING from `default_cpu` |
| d1 with the simulator copied to a directory without a sound tree | each missing default reported once (`seek-s.wav not found (sound directory "default_disk")`), later attaches silent |
| c4 (MicroVAX 3900, RC25) with the SDL disk audio driver | the seek sounds play (peak 0.244) |
| R1 (VMS V3.5, vax730, RC25 realism) | all checks ok, as before |
| Build `-Werror` pdp11, vax730, vax750, vax780, microvax3900; `sim_sound.c` with `-Wall -Wextra` with and without SDL2 | no warnings |
| VAX diagnostics 730, 750 | passed |

## Patch 5 v15: rebuilt on patch 6 v2 (2026-11-07)

- Patch 6 v2 (RB730, at most one RB80; branch `rb730` commit 3df3769b) adds a history line at the top of `VAX/vax730_rb.c`, so patch 5 v14 no longer applied after it. **`opensimh_5_sound_v15.patch`** is the same sound work rebuilt on patch 6 v2 (no code change in the sound work). Branch `sound`: the RB80 commit cherry-picked as 2f3bbe8e; `integration`: b80a3eb1.
- Checked: the full order 1 (v11), 2, 3, 4, 6 (v2), 5 (v15) on a fresh 87eb7d5 gives the `integration` tree (apart from the Visual Studio, CI and README files that only the branches have).

## Phase D, step 1: tapes, TS11 and TSV05 (TS device) (2026-11-07)

### Where things are

- Branch `sound`: commit a24907d6 on top of 2f3bbe8e; branch `integration`: the same change as bcf33be5.
- Patch **`opensimh_5_sound_v16.patch`** (cumulative, replaces v1–v15, applies on top of `opensimh_6_rb730_v2.patch`, without the Visual Studio project files). About 4.5 MB (v15: 0.95 MB): the new WAV files are in it as binary patches. Checked by `git am --keep-cr` on a fresh 87eb7d5: patches 6 + 5 give exactly the `sound` tree; the full order 1, 2, 3, 4, 6, 5 gives the `integration` tree apart from the CI and README files.
- Changed files: `sim_sound.c`, `sim_sound.h`, `PDP11/pdp11_ts.c`, `scp.c` (HELP SET SOUND: default tape sounds).
- New files: `sound/default_tape/` (`load.wav`, `unload.wav`, `rewind_start.wav`, `rewind.wav`, `rewind_stop.wav`, `start.wav`, `motion.wav`, `stop.wav`, `README.txt`) and `sound/tsv05/` (`tsv05_load.wav`, `tsv05_unload.wav`, `tsv05_motion.wav`, `tsv05_start.wav`, `tsv05_stop.wav`, `README.txt`).
- Notes branch: `tools/make_tape_sounds.py` v1 (the generator), `tests/ts11/tstest.py` v1 (VMS tests), `docs/SIMH_Sound_devices.ods` table v6 (new sheet **Tapes**, generator v7), `docs/SIMH_known_issues.md` v11 (cases 29–35).
- Sources: **TS11 Subsystem Technical Manual EK-OTS11-TM-003** (Jan 1984; pages 4-4/4-5 "Loading and Threading" are missing in the scan) with Ulli's data sheet, and **TSV05 Tape Transport Subsystem User's Guide EK-TSV05-UG-003**.

### Decisions (Ulli, 2026-11-07)

1. No recordings exist: the sounds are synthesized (reel motors, capstan, start/stop transients, load, unload).
2. The rewind sound glides with the reels' rotation rate (mixer extension).
3. EOT comes from the physical position on the reel, `SET TS REEL=600|1200|2400` (feet, default 2400).
4. A separate TSV05 model for the Qbus.
5. Provisional TS11 `LOADTIME` 8 s and `UNLOADTIME` 8 s (plus the rewind), as registers.
6. DETACH is allowed at any time in the realism mode (may change later).
7. Tests with Ulli's `VMS047-1.R81` (VAX/VMS V4.7, RA81, VHD) on the VAX-11/750 and the MicroVAX II.
8. The first tape sounds are the default tape sounds (`sound/default_tape`).

### Common code (sim_sound)

- **Mixer:** every voice has a playback rate (linear interpolation) that can **glide** (`sim_sound_glide`): linearly, or along the reel curve `SND_GLIDE_REEL` (1/rate² linear in time: a reel at constant tape speed; its radius² changes linearly, its rotation rate is ∝ 1/radius). Voices can start after a **delay** and fade out after a **lifetime** (`sim_sound_play_timed`, `sim_sound_extend`): the audio thread times them, so sounds stay right while the simulator is at the `sim>` prompt. A stopped voice that has not started yet is dropped at once.
- **Realtime timers** (`SND_TIMER`, `sim_sound_timer_*`): simulated time while the CPU runs, wall time at the prompt; own timer units in internal devices per kind (`INT-SPINDLE…`, `INT-REEL…`). Factored out of the spindle code; the spindles now use them (behaviour unchanged).
- **Tape transports** (`SND_REEL`, `sim_reel_*`): states no reel / mounted / loading / ready / moving / rewinding / unloading; the physical position in 1/10000 in from the BOT marker, counted from the records the device moves over; BOT marker 16 ft after the tape start, EOT marker 25 ft before its end (a reel of L ft has L − 41 ft between them); per transport type (`SND_REEL_TYPE`): speeds, gap, tape mark, start/stop, streaming access time, rewind speeds per reel size, erase/clean lengths, load/unload times. Callbacks to the device when a load, a motion, a rewind or an unload is over. Normal mode: everything at once.
- **Sounds:** `<dir>/<prefix>_<name>.wav`, else `default_tape/<name>.wav`; SHOW SOUND lists `ts11 (tape) -> default_tape`.
  - Rewind: `rewind_start`, two `rewind` loops (take-up and supply reel, rate = rev/s ÷ 8, reel glide to the rates at BOT), `rewind_stop` (run-down, BOT overshoot, creep, stop).
  - Start/stop transport: per record `start` + `motion` (gated loop) + `stop`, queued in batches of 8 records with delays.
  - Streaming transport: one `motion` loop extended record by record; `start` when the tape was at rest; `stop` after it has coasted through a gap without a new command.

### TS device

| | TS11 (Unibus) | TSV05 (Qbus) |
|---|---|---|
| Transport | TS04: capstan, tension arms | TS05: reel-to-reel, compliance arm, air threading |
| Read/write | 45 in/s, start 8 ms + stop 8 ms per record (0.180 + 0.162 in) | 25 in/s streaming; 40 ms access from rest |
| Gap / tape mark | 0.6 in (0.5 minimum) / 0.7 in | 0.65 in typical / 0.75 in |
| Rewind | 150 in/s (+1.5 s ramps, overshoot) | 130/150/180 in/s for 600/1200/2400 ft (+2 s) |
| Load / unload | 8 s / 8 s (provisional) | 30 s / 15 s (manual) |
| Erase / clean | 3 in / 10 in and back | 3 in / no-op |
| Model check | 512-byte records: 28.8 ms; full-reel skip 14.8 min (manual: 15–20 min) | 8 KB records 34.7 KB/s; full rewind 159 s (manual: 2.8 min max.) |

- **Operator panel** (both modes; in the normal mode at once):
  - `ATTACH` mounts the reel. Realism mode: not loaded, off-line. Normal mode: loaded and on-line at once, as before.
  - `SET TS LOAD` (LOAD button), `SET TS ONLINE` / `OFFLINE` (ON-LINE switch; ONLINE loads the tape first, also after waiting for an unload), `SET TS REWIND` and `SET TS UNLOAD` (off-line only; the file stays attached), `SET TS REEL=n`, `SHOW TS TRANSPORT`.
  - At the prompt they wait with messages (`TS: loading, at BOT in 8 s ...`, `TS: at BOT`, `TS: unloading, unloaded in 21 s ...`); `SET -N` returns at once.
  - Registers `LOADTIME`, `UNLOADTIME` (defaults set by the bus: TS11 or TSV05), `REEL` (read-only, set with SET), `TPOS` (position, read-only), `ONLSW`.
- **Guest side, realism mode:**
  - Motion commands take their time; the end packet (and the interrupt) come at the end, SSR is clear meanwhile. `ts_svc` holds the end packet (`ts_endcmd` while `ts_defer`); the transport callback sends it.
  - Rewind (Position mode 4): end packet at BOT. Initialize command: the same (SIMH rewinds here, see case 29). Control mode 4 (TSV05 rewind with immediate interrupt): ends at once; a following motion command waits for BOT.
  - Rewind and Unload: ends at once; the tape rewinds and unloads off-line, the file stays attached; `SET TS ONLINE` loads it again.
  - Subsystem initialize (TSSR write, bus init, RUN, BOOT): an on-line tape that is not at BOT rewinds in real time, SSR comes back at BOT.
  - On-line ↔ off-line: volume check and an attention message (if enabled).
  - EOT from the physical position; a forward command at the physical end of the tape fails (MTSE_EOM → TC6, OPI).
- **Normal mode:** unchanged (timing, ATTACH, the guest's unload detaches); the panel commands work at once; the physical position is still counted (`SHOW TS TRANSPORT`).

### Tests (`tests/ts11/tstest.py` v1 on the notes branch; VMS V4.7, SYSTEM/MANAGER)

| Test | Result |
|---|---|
| Panel, vax750 (s1.ini): realism ATTACH → "reel mounted, not loaded"; REWIND refused (not loaded); ONLINE loads (LOADTIME 2000: waits 2 s) and goes on-line; REWIND refused while on-line; OFFLINE, REWIND at BOT 1.5 s, UNLOAD, `-N ONLINE` → "loading, 2.0 s left"; REEL refused while loaded, accepted after DETACH | ok |
| Panel, pdp11 11/70 (p2.ini): normal mode at once (ATTACH on-line, OFFLINE, UNLOAD keeps the file, ONLINE reloads); realism `-N ONLINE` then LOAD waits; `-N UNLOAD` then UNLOAD waits | ok |
| N1 normal mode, vax750 (TS11): INITIALIZE, MOUNT/FOREIGN, BACKUP of SYS$HELP:*.HLB (4324 blocks), BACKUP/LIST, DISMOUNT | 0.2 s per BACKUP as before; 153 ft counted; DISMOUNT detaches (as before) |
| R1 realism mode, vax750 (TS11), THROTTLE 300K | SET TS ONLINE: 8 s load; BACKUP 54 s guest time (model: 298 records × 135.5 ms = 40 s of tape plus CPU); BACKUP/LIST 66 s (rewind 152 ft 13.7 s + reading); DISMOUNT: "unloading, 21.5 s left" (model 21.7 s), file stays attached; SET TS ONLINE waits for the unload, loads (8 s); MOUNT/FOREIGN, BACKUP/LIST, DISMOUNT/NOUNLOAD ok |
| E1 realism mode, vax750, REEL=600, BACKUP/BLOCK_SIZE=2048 of SYS$SYSTEM:*.EXE (14210 blocks) | `%BACKUP-I-RESUME, resuming operation on volume 2`, `mount volume 2 on _MSA0: for writing`: VMS hit the physical EOT and unloaded the reel ("unloading, 54.1 s left": rewind of about 559 ft + 8 s, model 54.2 s) |
| P1 realism mode, vax750: DISMOUNT/NOUNLOAD after the BACKUP, simulator stopped during the guest's rewind | SHOW TS TRANSPORT "rewinding, 11.9 s left"; after `SLEEP 5` "6.9 s left" (wall time at the prompt); CONTINUE: the rewind completes, DISMOUNT returns; `SET -N TS UNLOAD` returns at once |
| Q1 realism mode, MicroVAX II (TSV05), B DUA0 from the console ROM | 30 s load; BACKUP 77 s (model 69 s of tape plus accesses); BACKUP/LIST 88 s (rewind 154 ft at 180 in/s ≈ 12 s); DISMOUNT: unloading 24.8 s left; SET TS ONLINE reloads (30 s), MOUNT/FOREIGN, BACKUP/LIST ok |
| Audio of R1 (SDL disk driver) | during the 152 ft rewinds the take-up reel's whine rises 540 → 590 Hz (model 533 → 611 Hz): the reel glide works |
| Spindle regression after the timer refactoring (sp1.ini): HK0 RK07 ONLINE 2 s, `-N OFFLINE` 1.5 → 0.5 s after `SLEEP 1`, ONLINE again; RA60 ONLINE 1 s; internal devices INT-SPINDLE and INT-REEL | ok, as before |
| Build `-Werror` (pdp11, vax730, vax750, vax780, vax8200, vax8600, microvax1, microvax2, microvax3900, rtvax1000, pdp8, altair) on `sound`; vax750, microvax2, pdp11 on `integration` | no warnings; all VAX diagnostics and the PDP-11 tests passed |
| `sim_sound.c` with gcc `-Wall -Wextra` (with and without SDL2), clang, g++; `pdp11_ts.c` with `-Wall -Wextra` for the VAX 750, MicroVAX II and PDP-11 | no new warnings (only SIMH's usual unused-parameter / missing-initializer style warnings) |
| Sounds (`make_tape_sounds.py --check`) | lengths exact; peaks −9 … −18 dBFS; loop seams within the normal sample-to-sample steps |

Notes from testing:
- First realism run: VMS reported `%INIT-F-DEVOFFLINE`. `ts_svc` returns from inside its switch for several functions (Write Characteristics, Get Status, …), so the held end packet was never sent. Fixed by making `ts_svc` a wrapper around the command code (`ts_svc_cmd`) that always releases it.
- `SET TS ONLINE` while the tape was unloading waited and then did not load; fixed (it waits for the unload, then loads). `SET TS UNLOAD` during an unload now waits instead of failing.
- VMS V4.7 calls the transport MSA0 on both buses (`SHOW DEVICE/FULL`: device type TS11 on the VAX-11/750).

### Suspicious code found (known issues 29–35, not changed)

- 29: the Initialize command rewinds (manuals: a no-op); 30: Write Data Retry is mode 2 in the manuals, SIMH uses mode 1; 31: Control mode 4 (TSV05 only) accepted on the TS11; 32: Rewind and Unload bypasses `ts_detach` (no attention message, normal mode); 33: "TSV11/TSV05"; 34: HELP TS mentions options the device does not have; 35 (SCP): `SET TS LOCKED` refused although HELP lists it (single-unit devices; `SET TS0 LOCKED` works).
- Also seen: the debug labels in `ts_rd`/`ts_wr` are swapped (an access at the base address is shown as TSSR; base is TSBA/TSDB). Case 36.

### Open

- Real TS11 load/unload times (provisional 8 s / 8 s); tape mark length and gap are ANSI-nominal guesses.
- The TSV05 100 in/s mode (special software) is not simulated; subsystem initialize on the TSV05 is modelled like the TS11 (the TSV05 manual does not say).
- Sound levels by ear; recordings of a real transport would replace the synthetic ones file by file.
- Other tape devices (TM11/TU10, TU16/45/77 on the RH, TQK50/TU81 on the TQ) can use `SND_REEL` and `default_tape`.

## Phase D, step 1a: TS fixes after Ulli's first tests (2026-11-08)

- Branch `sound`: commit 24b94ab6; `integration`: b7e2dc30. Patch **`opensimh_5_sound_v17.patch`** replaces v16 (same base; checked: 6 + 5 v17 give the `sound` tree, the full order 1–4, 6, 5 the `integration` tree apart from CI/README).
- **SHOW TS said "unlimited capacity" in the realism mode** (Ulli, vax750): the reel decides EOT there, so the CAPACITY entry now shows "2400 ft reel, EOT at 2359 ft", `SET TS0 CAPACITY` is refused with a hint to `SET TS REEL`, `SHOW TS REEL` works at device level and ends its line. Normal mode: "unlimited capacity" (CAPACITY) as before; `SHOW TS REEL` adds "(realism mode only; EOT from CAPACITY)". (`SET TS CAPACITY` at device level still fails with "Invalid argument": known issues case 35.)
- **`%BACKUP-F-POSITERR, -SYSTEM-F-DRVERR` on the MicroVAX II** (Ulli: BACKUP interrupted with Ctrl/Y, then DISMOUNT/NOUNLOAD, MOUNT/FOREIGN, BACKUP without /REWIND): reproduced in the **normal mode as well** (`tests/ts11/broken_saveset.py`), so not a realism or timing problem. The interrupted BACKUP leaves a save set without tape marks; the next BACKUP skips forward to find the end of the volume, runs beyond the recorded data, the TS answers OPI (class 6), VMS reports a fatal drive error. That is what the hardware does (TS11 manual, XSTAT3 OPI: "a read, space, or skip operation has moved 25 feet of tape without detecting any data"). Remedy in VMS: BACKUP …/REWIND, or INITIALIZE the tape. New in the realism mode: the drive moves the 25 ft of blank tape (TSV05: about 12 s) before OPI. On the vax750 Ulli's BACKUP from a freshly initialized tape runs fine.
- Own bug fixed: `sim_reel_erase` ignored negative lengths, so the TS11 Clean command moved the tape 10 in forward and not back.
- Question from Ulli, could the unthrottled MicroVAX II cause timing problems: not here (the normal mode fails the same way). Tape times are converted with the measured instruction rate, so the guest sees the real durations without a throttle (Q1: BACKUP 77 s VMS time, model about 69 s plus accesses). A realistic MicroVAX II THROTTLE default is still open (only the vax750 has one).
- Tests: vax750 and microvax2 realism console checks of SHOW TS / SHOW TS REEL / SET TS0 CAPACITY / SET TS REEL; broken_saveset.py normal and realism (realism: OPI after 25.7 ft); build `-Werror` of the ten TS simulators: no warnings, all diagnostics passed.

## Phase D, step 2: TK50 (TQ) and the MicroVAX II THROTTLE (2026-11-08)

### Where things are

- Branch `sound`: commit 20b55167 on top of 24b94ab6; `integration`: 0d08d711. (The work branch `tk50-wip` is still on GitHub: the remote refused deleting it. It is squashed into these commits and can be deleted.)
- Patch **`opensimh_5_sound_v18.patch`** replaces v17 (same base, about 6.1 MB with the new WAV files). Checked by `git am --keep-cr` on 87eb7d5: 6 + 5 v18 give exactly the `sound` tree (without the Visual Studio project files); the full order 1–4, 6, 5 gives the `integration` tree apart from CI/README.
- Changed files: `sim_sound.c`, `sim_sound.h` (SND_STREAMER), `PDP11/pdp11_tq.c`, `VAX/vax630_syslist.c` (THROTTLE 450K).
- New files: `sound/tk50/` (`tk50_load.wav`, `tk50_calibrate.wav`, `tk50_unload.wav`, `tk50_start.wav`, `tk50_forward.wav`, `tk50_stop.wav`, `tk50_rstart.wav`, `tk50_reverse.wav`, `tk50_rstop.wav`, `README.txt`).
- Notes branch: `tools/make_tape_sounds.py` v2 (TK50 synthesis), `tools/make_sound_devices_ods.py` v8 and `docs/SIMH_Sound_devices.ods` table v7 (new sheet **Streamers**, MicroVAX II row), `tests/tk50/tktest.py` v2 and `README.txt`, `docs/SIMH_known_issues.md` v12 (case 37).
- Sources: EK-OTK50-TM-002 (TK50 technical manual), EK-TUK50-OM-001, and Ulli's TZ30 recording (pitches, levels, durations, the 3 s stream/reposition cycle).

### Decisions (Ulli, 2026-11-08)

1. TK50 sounds are synthesized after the measured values of the TZ30 recording. The click at 13.57 s of the recording may be a signal beep; the TK50 manuals describe none, so it is left out.
2. Buffer 32 KB; repositioning as in the recording (back up about 60 in).
3. Load 10–15 s, a random value per load; calibration about 2 min for a new cartridge (an empty file); rewind about 80 in/s, up to 90 s; unload 9 s.
4. TK50 only for now; the model is prepared for the TK70 (an `SND_STRM_TYPE` entry); the TU81 later.
5. MicroVAX II THROTTLE 450K, provisional. Calibrating by boot time is meaningless (Ulli).

### Model (`SND_STREAMER` in sim_sound)

- Modes idle / start / stream / repos / move; per type (`SND_STRM_TYPE`): speed 75 in/s, rewind 80 in/s, 45 KB/s, track length and count (22 serpentine tracks of 600 ft), start/stop times, block size, buffer, back-up length, reposition time (1.42 s), load/calibration/unload times.
- Reading: the drive reads ahead until the buffer is full, stops, backs up and waits; it starts again (running up over the backed-up tape) when the host has emptied half the buffer. Writing: a write ends at once while there is room; the drive writes the buffer out and repositions when it runs empty. A host that keeps up hears continuous streaming; a slow one the stream – stop – reverse – start cycle.
- Rewind along the serpentine tape at 80 in/s with the reel glide; EOT at the end of the 22nd track (`TEST_EOT` uses the model in the realism mode).
- Load, calibration and unload run on realtime timers (simulated time while the CPU runs, wall time at the prompt).

### TQ device (realism mode, TK50 only)

- `ATTACH` inserts the cartridge (`-N`/empty file: a new cartridge, calibrated on the first load); the unit has no volume until **`SET TQn LOAD`** (Load/Unload button). `SET TQn UNLOAD` rewinds and unloads; the file stays attached. At the prompt both wait with messages; `SET -N` returns at once. `SHOW TQn TRANSPORT` shows state, track, position and the lights.
- Registers BUFFER, BACKUP, REPOS, LOADMIN, LOADMAX, CALTIME, UNLOADTIME.
- Guest side: every motion command (read, write, tape mark, erase, position, rewind) takes the model's time; the host's AVAILABLE with UNLOAD rewinds and unloads (file stays attached); a rewind with the immediate modifier ends at once while the tape rewinds.
- On the **Unibus VAXen the TQ is a TU81 by default**: `SET TQ TK50` (no unit attached) makes it a TUK50. LOAD/UNLOAD and SHOW TRANSPORT say so when sound is enabled on another type.
- TK70, TU81, TKUSER and the normal mode: unchanged.
- `SET TQ DEBUG=REQ` shows a line per command: opcode, modifiers, bytes moved, time.

### Bug found and fixed: realistic delays ignored with asynchronous tape I/O

SCP activates a unit before calling its asynchronous I/O completion callback (AIO migration), so the `sim_activate` in `sim_sound_activate_us`, called from `tq_io_complete`, was a no-op: every TK50 command ended at once (no rewind time, the model's clock ran ahead). Found with `SET TQ DEBUG=TRACE` (the bottom half ran at the same instruction count). Fix: `sim_activate_abs`. The disk callers (RL, RP, RQ, HK, RB) call it in service or command-start paths, so nothing changes for them. Known issues case 37.

### Tests (`tests/tk50/tktest.py` v2; VMS V4.7, SYSTEM/MANAGER)

| Test | Result |
|---|---|
| K1 normal mode, MicroVAX II (TQK50), on `sound` and on `integration` | BACKUP 2.4 s, BACKUP/LIST 2.5 s (as before); DISMOUNT detaches as before |
| K2 realism mode, MicroVAX II, THROTTLE 450K, new cartridge | load about 10 s + calibration 2 min; BACKUP 69.5 s guest time: the tape **streams for 64 s without a stop** (the host keeps up with 45 KB/s); BACKUP/LIST 128 s (rewind 52 s from 345 ft, the reel whine glides 456 → 680 Hz; reading 63 s); DISMOUNT "rewinding and unloading, 59.0 s left"; SET TQ0 LOAD reloads (about 15 s, no calibration), BACKUP/LIST: 24 files, 4324 blocks |
| K2 with THROTTLE 150K (a slow host) | BACKUP 189 s, BACKUP/LIST 237 s; the audio shows **2 s of streaming, then a reposition, every 3.5 s** during both (the TZ30 recording: about 3 s) |
| K3 realism mode, VAX-11/750 (TUK50, `SET TQ TK50`), THROTTLE 300K, the K2 tape | load without calibration; BACKUP/LIST from BOT 73 s, reads 0.178 s per 8 KB on average (45 KB/s), no repositions; DISMOUNT 58.4 s left; reload, LIST ok |
| Trace after the fix | first LIST 73 s, second (after 345 ft) 125 s; 8200-byte reads every 0.181 s |
| Build `-Werror` of the ten TQ/TS simulators on the work branch; vax750, microvax2, pdp11 on `integration` | no warnings |

Notes from testing:
- VMS V4.7 starts the TMSCP port PTA0 only at the end of the startup; MUA0 appears about 30 s after the login (the unmodified build does the same). The script waits 40 s.
- VMS V4.7 BACKUP/LIST has no `/OUTPUT`; use `/LIST=file`.
- Short positioning near BOT after a read takes about 4.4 s (the read-ahead moved the tape; the drive stops, backs up and repositions): plausible for a TK50, to be compared with a real drive.
- In the first K3 run VMS printed once `ERRFMT - ERROR ACCESSING ERROR LOG FILE, %RMS-F-RSZ`. Not reproduced in a rerun with TQ debug (no error log packet was sent) nor in the normal mode; probably the guest's error log file, but watch for it.

### Open

- BUFFER and BACKUP are provisional (from the TZ30 recording); a real TK50 recording would settle them and the sounds.
- TK70 (prepared), TU81 and the other tapes (TM11/TU10, TU16/45/77) later.
- MicroVAX II THROTTLE 450K and VAX-11/750 300K: calibrate by benchmark rather than boot time.

## Phase D, step 2a: TQ stop on DIRECTORY of an interrupted save set (2026-11-08)

- Ulli (MicroVAX II, VMS V4.7): `MOUNT/OVERRIDE=IDENTIFICATION MUA0:`, `DIRECTORY MUA0:` → "%SIM-ERROR: Unexpected internal error while processing event for TQ0 which returned 66 - I/O error", the simulator stopped.
- Reproduced in the **normal mode as well** (plain Open SIMH code): a BACKUP to the TK50 interrupted with Ctrl/Y leaves no closing tape marks; DIRECTORY skips 2 files, the skip runs beyond the last record (`MTSE_EOM`), and the OP_POS bottom half sends that to `tq_mot_err` (SCPE_IOERR). A COPY to a labelled tape, an empty initialized tape and a complete BACKUP tape list fine in both modes.
- Fix: MTSE_EOM ends the position command with the mapped status (data error, position lost), as a read there does. VMS: `%DIRECT-E-OPENIN … -SYSTEM-F-TAPEPOSLOST`, then `TEST.BCK;1, Total of 1 file`; the simulator keeps running (normal and realism mode). Known issues case 38.
- Branch `sound`: 7fa1aab; `integration`: 248b0cc. Patch **`opensimh_5_sound_v19.patch`** replaces v18 (checked: 6 + 5 v19 give the `sound` tree, the full order 1–4, 6, 5 the `integration` tree apart from CI/README).

## Phase D, step 2b: smooth TK50 sounds (2026-11-08)

- Ulli: "The artificial sounds on the TS11 are very good; those on the TK50 are not—they are very choppy (lacking smooth transitions). Furthermore, there is too much background noise in streaming mode."
- Causes:
  - The forward loop was mostly noise (the 457 Hz tone about 10 % of the power).
  - The start was a whoosh 8 dB over streaming that died away before the loop began.
  - The stop started 10 dB over streaming, with a hard 50 ms dip and a knock.
  - The engine faded the loops in and out over 15 ms only, and played them at a random phase.
  - The TS11 reel engine, by contrast, uses 300 ms overlaps.
- Engine (`strm_timeline` / `strm_sounds` in sim_sound.c):
  - The start, loop and stop sounds crossfade over 0.4 of the start or stop sound.
  - A loop begins at 0.6 of its start, from its first frame (no random phase), and fades in; the start sound ends with the loop's own first 0.4 D fading out, so both add up to the loop.
  - A loop ends where its stop begins and fades out while the stop fades in.
  - The reverse start follows the stop at 0.6 (was 0.7).
  - A loop that starts while the drive is already streaming keeps the random phase and a 30 ms fade.
- Sounds (`tools/make_tape_sounds.py` v3):
  - Forward: the 457 Hz tone with harmonics 914/1371/1828 Hz (the loud 800–1600 Hz octave of the recording is tonal), a slight wow (3 and 7 Hz), the tape noise 17 dB under the tones with the highs cut; −22 dBFS RMS.
  - Reverse: 457/501 Hz beating, 6 dB quieter.
  - Start 0.35 s: a run-up from half speed with a soft air swell.
  - Stop 0.45 s: braking from the loop with the tone gliding down, smooth decay, a soft settle at −20 dB.
  - Reverse start 0.35 s, reverse stop 0.40 s.
- Check: K2 with THROTTLE 150K, with audio, 10 ms RMS envelope of a reposition cycle:
  - Streaming holds ±1 dB for 2.1 s.
  - The stop goes into the reverse without a gap; the reverse ends in a 150 ms fade.
  - About 0.25 s of silence while the drive waits for the host (as on the real drive), then a 0.1 s rise into streaming.
  - BACKUP and LIST results unchanged.
- Branch `sound`: 582b4ad; `integration`: 0e8335b. Patch **`opensimh_5_sound_v20.patch`** replaces v19 (checked as before).

## Phase D, step 2c: Ulli's run of 2026-11-09 (throttle, 2 GB message, calibration)

- **"Host CPU is too slow to simulate 450,000 instructions per second … Throttling disabled."** at BOOT: the throttle without warm-up (THROT_DELAY 0, realism mode) took one 10 ms measurement; idling at `>>>` or a busy start made it too low. Fix in `sim_timer.c` (known issues 40): at least 100 ms, an idled, too slow or doubtful (< 1.25 × wanted) measurement is repeated, a warning only after 30 repetitions without idling. Also fixed there: the too-fast recalibration looped forever without a measured peak rate (a test wrote 20 GB of debug output). Checked: four MicroVAX II boots in parallel (244K and 286K measurements repeated, 450K reached, no warning); one boot: 211 ms measurement, 758K, recalibrated to 450K.
- **"2149429608 bytes of unexamined data remain"** for Ulli's 2 MB `test-interrupt.tap`: `sim_tape.c` set the position 2^31 bytes back after an invalid record length when the bad marker had bit 31 set (known issues 39). The image: 184400 bytes of valid records (an interrupted BACKUP), then the remains of an older recording (SIMH does not truncate). Fixed; the TQ position command also ends with the tape error for MTSE_INVRL/MTSE_FMT instead of stopping the simulator (case 38 widened). Checked with a synthetic image: old "2147995652 bytes", fixed "512004"; MOUNT/OVERRIDE=IDENTIFICATION + DIRECTORY: TAPEPOSLOST, the save set listed, both modes.
- **Calibration on every ATTACH of a still-empty file** (Ulli: "Could that be avoided, e.g. by writing a single tape mark?"): after the calibration the TQ writes an end-of-medium marker at BOT (4 bytes, `ff ff ff ff`). It reads like an empty file in both modes (a tape mark would change what VMS reads at BOT); the guest's first write replaces it. Not for a write-protected file. Checked at the prompt (ATTACH -N, LOAD with calibration, DETACH, ATTACH: "contains 0 bytes", LOAD 15 s without calibration) and with K2 (INITIALIZE over the marker, BACKUP 68 s, LIST 124 s, reload).
- Recording SIMH's sound, see `tests/tk50/README.txt`.
- Branch `sound`: f8c89ec; `integration`: 790328d. Patch **`opensimh_5_sound_v21.patch`** replaces v20 (checked as before).

## Phase D, step 2d: TK50 last changes and status (2026-11-09/10)

- **SHOW TRANSPORT during a rewind** showed "loaded, at BOT, rewinding" (the logical position is the move's target from its start). Now interpolated from the move's start (`mv_t0`, `mv_lfrom` in SND_STREAMER): "loaded, rewinding, 344 ft from BOT", "track 1 of 22, 140 ft from BOT, moving"; "at BOT" only at rest.
- **Reposition not audible** (Ulli: "I hear the tape only moving for several seconds, then slowing down to standstill, short pause (~0,2s) then accelerating again ... I cannot hear the short reposition"): the reverse was a near copy of forward, 6 dB quieter, and started while the stop faded. Now the reverse start follows the stop when it has died away (`r0 = a + d_sp`), and the reverse sounds (`make_tape_sounds.py` v4) lead with 501 Hz, a 600 Hz whine and the hiss of the recording's reverse bands, 2.5 dB under forward. Measured cycle at THROTTLE 150K: forward 2.2 s, braking 0.2 s, standstill 0.04 s, reverse 0.8 s, stop, start.
- **Fewer repositions than on a real TK50** (Ulli remembers 3 to 4 times more during BACKUP/IMAGE):
  - Measured: BACKUP/IMAGE of the RA81 test disk (no disk timing) at 450K: 39.7 KB/s, 20 repositions in 617 s. CPU register **STRSHIFT** (new, VAX: string instructions count one instruction per 2^n bytes, default 5 = unchanged) at 3 gave the same rate: string instructions do not limit BACKUP.
  - Ulli's run with the RA60 (realistic timing): 42489 blocks really written in 784 s = 27.1 KB/s (BACKUP/LIST counts NOBACKUP files such as PAGEFILE.SYS without data).
  - Many small files give a much lower host rate than large ones (two seeks and latencies per file: about 7 KB/s for 1-block files on the RA60); the real 'constant repositioning' probably came from those stretches. Test procedure `tests/tk50/MKSMALL.COM` (1000 one-block files).
  - Not settled: BUFFER (32 KB), BACKUP (60 in), REPOS (1420 ms) are provisional; Ulli can try `DEP TQ BUFFER 16384` etc. by ear. Per reposition about 2.3 s carry no data (REPOS 1.42 s + start 0.13 s + run-up (BACKUP - 3.5 in) / 75 in/s).
- **Ulli, 2026-11-10: "We will stop fine tuning the TK50 tape drive for now."**
- Branch `sound`: 13559b1; `integration`: 0f8cb48. Patch **`opensimh_5_sound_v22.patch`** replaces v21 (checked: 6 + 5 v22 give the `sound` tree; the full order 1-4, 6, 5 the `integration` tree apart from CI/README).

## Phase C, step 6: the fixed RA drives RA70 to RA92 (RQ) (2026-11-10)

### Where things are

- Branch `sound`: commit 8c636388 on top of 13559b16; branch `integration`: the same change as f6be3ad3.
- Patch **`opensimh_5_sound_v23.patch`** replaces v22 (same base, applies on top of `opensimh_6_rb730_v2.patch`). Checked by `git am --keep-cr` on 87eb7d5: 6 + 5 v23 give exactly the `sound` tree (without the Visual Studio project files).
- Changed: `PDP11/pdp11_rq.c` (pdp11 and every VAX with an MSCP controller), `sound/default_disk/README.txt` (table of drives).
- Notes branch: `tools/make_spindle_sounds.py` v4 (RA rows), `tools/make_sound_devices_ods.py` v9 and `docs/SIMH_Sound_devices.ods` table v8, `docs/SIMH_known_issues.md` v15 (case 41), `docs/SIMH_overview.md` v16, `tests/ra/` (ra1–ra4.ini, ratest.py, vhd2raw.py, README.txt).
- Sources (supplied by Ulli): RA80 User Guide EK-0RA80-UG-001 (Jan 1982), RA81 User Guide EK-0RA81-UG-001 (1982), RA90/RA92 Disk Drive User Guide EK-ORA90-UG-002, RA7x/SA7x Pocket Reference Guide EK-RSA7X-PG-002 (Aug 1992), DEC brochure EA-24042-18 (RA60/RA80/RA81, Aug 1982); Computer History Wiki for the RA82, RA70 and RF72.

### Question answered first: UDA50 vs UDA50A in SIMH

Only the model numbers differ: `SET RQ UDA50` reports port model and MSCP controller model 2 (the original UDA50), `UDA50A` 6 (the default of every Unibus simulator). Nothing in the code branches on the two; timing, commands and drive types are the same.

### Drive facts

| Drive | rpm | Seek t2t / avg / max (ms) | Start / stop (s) | Source, remarks |
|---|---|---|---|---|
| RA70 | 4000 | 4 / 19.5 / 35 | 59 / 15 | pocket guide (rpm); CHW (average, start, stop); t2t and max **provisional** |
| RA71 | 3600 | 3 / 13.3 / 26 | 60 / 15 | pocket guide (rpm, at speed within 15 s); seeks and times **provisional** from the RF72 (CHW: 13.3 ms average, < 60 s / < 15 s) |
| RA72 | 3600 | 3 / 13.3 / 26 | 60 / 15 | as the RA71 (the guide does not state its rpm) |
| RA73 | 3600 | 3 / 13.3 / 26 | 60 / 15 | as the RA71 |
| RA80 | 3600 | 6 / 25 / 50 | 40 / 20 | User Guide (seeks; start 40 s max., 15 s typical), brochure (stop 20 s; its start 50 s not used, v28) |
| RA81 | 3600 | 7 / 28 / 50 | 20 / 20 | User Guide maxima (brochure 6/28/52); User Guide "Disk rotation start/stop times": start 20 s, stop 20 s maximum (the brochure's start 50 s not used, v28) |
| RA82 | 3600 | 6 / 24 / 38 | 50 / 20 | CHW |
| RA90 | 3600 | 5.5 / 18.5 / 31.5 | 40 / 30 | User Guide; stop not published (only "inhibit between stop and restart 40 s"): 30 s **provisional** |
| RA92 | 3405 | 3.0 / 16.0 / 29.0 | 40 / 30 | User Guide; stop **provisional** |

Every one of them has a RUN/STOP switch on its front panel (User Guides; pocket guide: "Run/Stop switch in/out" in the drive status, OCP with Run, Fault, Ready, Write Protect, Port A/B).

### Behavior

- **All RA drives now have the RUN/STOP button** of the RA60 (`rq_spindle_owner`: every drive type with a realistic timing entry). The RD/RX/RF and other drives keep "has no RUN/STOP button".
- **Realism mode:** ATTACH loads the disk (the file is the head disk assembly), the drive stays stopped until `SET RQn ONLINE` (**boot scripts need `SET RQ0 ONLINE` before BOOT**, as for the RA60 and the other removable drives; `SET -N` does not wait); "offline, no volume" while stopped, "available" attention when up to speed; AVAILABLE with spin-down (VMS `DISMOUNT`, which unloads by default) spins the drive down, and `SET RQn ONLINE` spins it up again (on the real drive: release and press RUN/STOP).
- Registers (RQ only, all four controllers): `SPINUPRAnn`, `SPINDOWNRAnn` (ms), `SEEKRAnnS`, `SEEKRAnnM` (cylinders) for nn = 70, 71, 72, 73, 80, 81, 82, 90, 92. The RA60 keeps `SPINUP`/`SPINDOWN`/`SEEKRA60S`/`SEEKRA60M`.
- Timing as the RA60: fitted seek curve, latency 0..one revolution, one sector slot per block with **one spare slot per track** (RA81 52, RA92 74, ...), a track-to-track seek per cylinder crossed. Geometry comes from the drive defines (`RQ_RLZ_RA` macro).
- Seek sounds per type `sound/raNN/raNN-s/m/l.wav`, falling back to `default_disk`; limits about 6 % / 37 % of the cylinders (RA81 75/460, RA92 190/1150, ...). Optional spindle sounds `raNN_spinup/_running/_spindown.wav` (`make_spindle_sounds.py` v4 makes them; not bundled).
- **Normal mode:** unchanged, except that `SET RQn ONLINE/OFFLINE` now also works (at once) on the fixed RA drives, as on the RA60.
- No configuration rules for RA drives yet (see open questions).

### Tests

Console (`tests/ra/`, MicroVAX II with `SET RQ KDA50`, and VAX-11/750):

| Test | Result |
|---|---|
| ra1 (realism, MicroVAX II): RA81, RA92, RA70 after ATTACH | "offline" |
| ra1: `SET RQ3 ONLINE` on the RD54 | refused: "the RD54 has no RUN/STOP button (only the RA drives and the RC25)" |
| ra1: RA81 ONLINE (SPINUPRA81 2000) / OFFLINE (1500) / `SET -N RQ1 ONLINE` (RA92, 3000) | waits 2 s / 1.5 s / returns, "spinning up, 3.0 s left" |
| ra1: VMB reads 50 blocks at LBN 800000 of the RA81 | long seek 1120 cylinders, 68.9 ms (model 63–80 ms) |
| ra2: BOOT DUA0 with the RA81 stopped | `?4D DEVOFFLINE, DUA0` |
| ra3 (normal mode) | ready at ATTACH, OFFLINE/ONLINE at once, boots |
| ra4 (realism, VAX-11/750, UDA50A): RA80, RA82 | ONLINE/OFFLINE wait; SHOW SOUND `ra80 (seek) -> default_disk, ra82 (seek) -> default_disk` |
| RC25/RA60 regression c1, c2, c4, c5, c6 (MicroVAX 3900, notes `tests/rc25`) | identical: RC25 156,823 µs and 646 cylinders back 51,380 µs; RA60 86,156 µs; c6 as before (the baseline build showed the known `?53` self-test error of case 22 once) |

VMS V4.7 (`tests/ra/ratest.py` v1, Ulli's `VMS047-1.R81`):

| Test | Result |
|---|---|
| N1 normal mode, MicroVAX II, KDA50, RA81 | workload (DIRECTORY/SIZE of SYS$SYSTEM, COPY/DIRECTORY/DELETE of 24 help libraries) 0.5 s, as before |
| R1 realism, MicroVAX II: ATTACH → both "offline"; `SET RQ0 ONLINE` waits 50 s (the value then); boot | ok; workload 43 s guest time |
| R1: blank RA92 data disk `SET -N RQ1 ONLINE` under VMS (40 s), INITIALIZE, MOUNT, COPY, DIRECTORY | ok |
| R1: `DISMOUNT DUA1:` | the RA92 spins down (30 s), file stays attached |
| R1: MOUNT of the stopped RA92 | `%MOUNT-I-OPRQST, Please mount volume RA92T in device _DUA1:`; `SET -N RQ1 ONLINE` → mounted by itself |
| R1: `SET -N RQ0 OFFLINE` under VMS, a DIRECTORY | "_DUA0: is offline. Mount verification in progress."; `SET -N RQ0 ONLINE` (during the spin-down) → "has completed mount verification", DIRECTORY completes |
| R1 trace | 3448 transfers, longest 87.9 ms, mean 17.6 ms; seek sounds 575 short, 9 medium, 491 long |
| U1 realism, VAX-11/750 (UDA50A), the system as an RA82 | boot ok, workload 51 s; 1739 transfers, mean 18.7 ms, longest 70.0 ms |
| T1 realism, MicroVAX II, the system as an RA72, then an RA71 (60 s spin-up each) | workload 39.0 s / 38.5 s; mean transfer 14.7 / 16.0 ms |
| Build `-Werror` microvax2, microvax3900, microvax1, rtvax1000, vax730, vax750, vax780, vax8200, vax8600, pdp11; microvax2, vax750, pdp11 on `integration` | no warnings (`pdp11_rq.c` with `-Wall -Wextra`: only more of the existing missing-initializer style warnings) |
| VAX-11/750 diagnostics | passed |

Notes from testing:
- **VMS V4.7 cannot boot from an RA90 or RA92** (`%SYSINIT-E- error mounting system device, status = 000008C4`), also in the normal mode of the unmodified `sound` build: VMS V4.7 (1987) predates these drives. The RA92 was tested as a data disk (R1); RA70 and RA80 are too small for the 456 MB system and were tested at the console.
- SIMH's `FORMAT=RAW` is a host device, not a flat file: `ATTACH -N` with it fails ("Cannot create … No such file or directory"). Flat files work in the default SIMH format.
- The first run converted `pdp11_rq.c` from CRLF to LF line endings (Python text mode); fixed before the commit (the file is CRLF).

### Ulli's decisions (2026-11-10, afternoon) and what followed

1. **All RA disks need `SET RQn ONLINE` before booting** in the realism mode (the wait of 40–60 s at the prompt, or `SET -N`): as implemented, unchanged.
2. **Configuration rules:** RA drives only on a KDA50, UDA50 or UDA50A; at most 4 drives per such controller. Implemented (branch `sound` d826b92d, `integration` 84cc0aca):
   - the rule uses SIMH's SDI flag of the drive table (`RQDF_SDI`), so it covers all RA drives and also the ESE20/52/56/58 and the user-sized RA8U;
   - `rq_eff_ctype` resolves the DEFAULT controller type before the first reset as reset will (Unibus UDA50A, MicroVAX I RQDX1, other Qbus RQDX3), so `SET SOUND ENABLED` before a reset checks the right type;
   - `SET RQ UDA50/UDA50A/KDA50` disables the units from 4 on; `SET RQ <type without SDI>` gives the RA units their initial drive types back (RD54 if the initial type is an SDI drive); refused while a unit that would change is attached; `SET RQ DRIVES=` above 4 is refused on an SDI controller; `SET SOUND ENABLED` refuses a configuration that breaks a rule.
   - The default configurations (RD54, RD54, RD54, RX50 on every simulator) keep to the rules, so `SET SOUND ENABLED` works as before; Qbus scripts need `SET RQ KDA50` before `SET RQ0 RA81`.
   - Test `tests/ra/ra5.ini` (MicroVAX II): all 6 refusals and the accepted cases as expected; ra1–ra4, the RC25/RA60 regression c1, c4, c5 (now with `SET RQ KDA50`, c5.ini v2) and c10 unchanged.
   - The VMS test drivers press the RA81 RUN/STOP button and use a KDA50 on the MicroVAX II: `tests/ts11/tstest.py` v2, `tests/tk50/tktest.py` v3 (TS Q1 run on the MicroVAX II: boots and runs).
3. **Provisional values:** accepted as they are.
4. **RA80 capacity (known issues 41): DONE, SIMH is right.** A real RA80 reports **237,212** LBNs in its MSCP unit status (Usenet comp.periphs: "237212-236964 == 248 == 31*8"); a 4.3BSD site used 237,212 blocks to the end of the disk (comp.bugs.4bsd, Dec 1986). The User Guide's 236,964 is just 31 × 14 × 546. The R80 HDA (561 tracks per head) is the RM80 on the Massbus and the RB80 on the RB730, both 31 × 14 × 559 = 242,606 blocks in SIMH (no DSA replacement/table areas); the 4.2/4.3BSD disktab used that geometry for the RA80 and was wrong. Only the comments in `pdp11_rq.c` changed.

Patch **`opensimh_5_sound_v24.patch`** replaces v23 (checked: 6 + 5 v24 on 87eb7d5 give the `sound` tree).

**Bus rule (Ulli, 2026-11-10, evening):** in the realism mode the UDA50 and UDA50A only on the Unibus, the KDA50 only on the Qbus (`sound` 2f5ddece, `integration` 099fc04a). Like all realism rules it applies only with `SET SOUND ENABLED` (confirmed by Ulli), and the ESE/RA8U coverage of the SDI rule is fine with him.
- `rq_rlz_rules` checks the controller type in effect against `UNIBUS` (VAX: fixed per model; PDP-11: `cpu_opt & BUS_U`, i.e. the current CPU model). `SET RQ <type>`, `SET SOUND ENABLED` and ATTACH refuse the wrong bus, e.g. "RQ: the KDA50 is a Qbus controller: on the Unibus use a UDA50 or UDA50A (SET RQ UDA50A)".
- A PDP-11 `SET CPU` to the other bus while sound is enabled is not refused; the next ATTACH (or SET SOUND ENABLED) reports the broken rule.
- Tests: `tests/ra/ra6.ini` (microvax2: UDA50A/UDA50 refused, KDA50 accepted), `ra7.ini` (vax750: KDA50 refused, UDA50A accepted); pdp11 11/73 and 11/70 checked by hand; ra1–ra5, RC25/RA60 c1, c4, c5 (MicroVAX 3900) and c10 (vax730, identical to the build before the RA work) unchanged.

Patch **`opensimh_5_sound_v25.patch`** replaces v24 (checked: 6 + 5 v25 on 87eb7d5 give the `sound` tree).

**RA81 and RA80 start times (Ulli, 2026-11-10, evening):** Ulli found 20 s spin-up and 20 s spin-down for the RA81. The RA81 User Guide (EK-0RA81-UG-001, Table 1-1 "Disk rotation start/stop times") gives start 20 s maximum, stop 20 s maximum; the 50 s came from the DEC brochure (EA-24042-18). The drive manuals now win over the brochure: `SPINUPRA81` 20000 (was 50000), and for the same reason `SPINUPRA80` 40000 (RA80 User Guide "Single Start/Stop Time": start 40 s maximum, 15 s typical; was the brochure's 50 s). Stop times unchanged (20 s; the RA80 User Guide has none, the brochure says 20 s). The RA82's 50 s / 20 s (Computer History Wiki) stays until a manual turns up. `make_spindle_sounds.py` v5 and the device table v9 follow; checked at the prompt: RA81 ONLINE "ready in 20 s", OFFLINE "at rest in 20 s", 41 s together.

Spindle sounds: `make_spindle_sounds.py` (since phase C) synthesizes optional spin-up / running / spin-down sounds for every drive with a RUN/STOP or LOAD button (RK07, RL02, RM03, RP04, RP05, RP06, RM05, RA60, RC25); v4 added the fixed RA drives. They are not bundled (decision: the framework supports spindle loops but ships none), so they play only when the files are put into `sound/<drive>/`.

Patch **`opensimh_5_sound_v26.patch`** replaces v25.

**VMS SHUTDOWN with "spin down the disk volumes = YES" (2026-11-10, evening):** SHUTDOWN dismounts the data disks with spin-down (AVAILABLE with the spin-down modifier: the drive stops, the file stays attached) but never the system disk: its last command is a plain AVAILABLE, so it keeps running, as DEC documents ("Does not spin down the system disk", OpenVMS System Manager's Manual, SHUTDOWN.COM). Checked in a trace here (MicroVAX II, KDA50, RA81 system disk, RA81 data disk) and by Ulli with an RA82 data disk: works.

**Silencing one drive type (workaround, accepted by Ulli; no per-unit NOSOUND):** silent seek sounds for that type in a user sound directory override the defaults, e.g. 10 ms of silence as `~/simh_silent/ra81/ra81-s.wav`, `-m.wav`, `-l.wav` and `SET SOUND PATH=~/simh_silent`; the other drive types keep their sounds. Works per drive type, not per unit. Ulli used it to hear an RA82 data disk spin down without the RA81 system disk.

## Phase E, step 1: floppies, the RX02 (RY device) (2026-11-11)

### Where things are

- Branch `sound`: commit 224e1d1f on top of 9fd51c82; branch `integration`: the same change as dbda2f25.
- Patch **`opensimh_5_sound_v27.patch`** replaces v26 (same base, applies on top of `opensimh_6_rb730_v2.patch`). Checked by `git am --keep-cr` on 87eb7d5: 6 + 5 v27 give exactly the `sound` tree (without the Visual Studio project files).
- Changed: `sim_sound.c`, `sim_sound.h` (SND_FLOPPY, repeated one-shots), `PDP11/pdp11_ry.c` (pdp11 and the VAX-11/730, 750, 780, 8200, 8600), `scp.c` (HELP SET SOUND: floppy defaults).
- New: `sound/rx02/` (ten files and `README.txt`), `sound/default_floppy/` (ten files and `README.txt`).
- Notes branch: `tools/make_floppy_sounds.py` v2, `tools/rt11_dir_rx02.py` v1, `tools/make_sound_devices_ods.py` v11 and `docs/SIMH_Sound_devices.ods` table v10 (new sheet **Floppies**), `docs/SIMH_known_issues.md` v19 (cases 43–46), `docs/SIMH_overview.md` v20, `tests/rx02/` (scripts and README).
- Sources: RX02 Floppy Disk System Technical Manual **EK-0RX02-TM-001** and the RX02 print set **MP00629** (Sep 1978), supplied by Ulli; RX01 maintenance manual EK-RX01-MM-002; for the other floppies see the facts table below.

### Decisions (Ulli, 2026-11-11)

1. The earlier decision "floppies without spin-up/spin-down" is revised: head load and unload sounds and a rotating sound.
2. No recordings yet: the sounds are synthesized (`tools/make_floppy_sounds.py`).
3. **RX02 motors:** checked in the print set MP00629, drawing D-CS-H771-A-1 (H771 power connections): line → line filter → fuse F1 → P1 → P2/P3 "to spin motor #1/#2", no relay and no control signal. The motors run whenever the RX02 has power, with or without a diskette.
4. Sounds ship in `sound/rx02/` etc., plus a fallback `sound/default_floppy/`.
5. The sector position follows the time (no random latency).
6. RX50/RX33 motor-off idle time 2 s, provisional, until Ulli has measured it on his MicroVAX.
7. DETACH during a command is allowed; the command ends with an error, as when a real door is opened.
8. Tests: VMS V4.7 on the 750 with DYA0 (script for Ulli), RT-11 from Ulli's `RT11.RX2` on the PDP-11.
9. Next: the 11/780 console RX01 and the VAXstation 3100 RX33 (as well as the RX01, RX50, RX33 and RX18 on RX/RQ); the PDP-8 RX8E/RX28 is postponed until further notice.

### Floppy drive facts

| | RX01 | RX02 | RX50 | RX33 | RX18 |
|---|---|---|---|---|---|
| Medium, tracks × sectors | 8", 77 × 26 × 128 B, 1 side | 8", 77 × 26 × 128/256 B (FM/MFM), 1 side | 5.25", 80 × 10 × 512 B, 1 side, 2 diskettes per drive | 5.25" HD, 80 × 15 × 512 B, 2 sides | 5.25", 40 × 9 × 512 B, 1 side |
| Speed, revolution | 360 rpm, 166.7 ms | 360 rpm ± 2.5 %, 166.7 ms | 300 rpm, 200 ms | 360 rpm (300 rpm for normal-density media) | 300 rpm, 200 ms |
| Track to track | 10 ms | 6 ms max. (2 step pulses per track) | 6 ms | 3 ms | 20 ms |
| Settle | 25 ms (1976 manual), 20 ms (1975 bulletin) | 25 ms max. | 30 ms (?) | 15 ms | 20 ms |
| Head load | solenoid | solenoid; unloaded if no new command within 200 ms | one solenoid per diskette, about 30 ms (?) | no solenoid: the door loads the heads (FD-55 series) | solenoid with drive select (SA400: 75 ms) |
| Motor | AC, belt | AC on the line (H771), belt, fan on the motor shaft; ready 2 s after power | one 12 V DC motor for both spindles; start 0.5 s | direct drive, start < 500 ms | DC, belt, start 500 ms; off after 2 s idle (Shugart) |
| Positioner | per drive | per drive | one carriage for both diskettes (heads back to back) | per drive | per drive |
| Average access (published) | 488 ms (half stroke) | 262 ms = 77/3 × 6 + 25 + 83 | 164 ms (unclear what it includes) | 91–94 ms (seek + settle) | 275 ms |
| Sources | EK-RX01-MM-002, RX11 option bulletin 1975, pdp8online | EK-0RX02-TM-001, MP00629 | Rainbow 100 Technical Manual EK-PC100-TM-001 ch. 6, gunkies.org | gunkies.org (TEAC FD-55GFR), TEAC FD-55GFR/FD-55 specs | retrocmp.de (RX180 = Shugart SA400L), SA400L OEM manual |

Not published (found): the RX01 head unload delay (200 ms as the RX02, provisional), and how long the RQDX controllers keep the RX50/RX33 motor running (2 s provisional).

### Model (`SND_FLOPPY` in sim_sound)

- Drive types `sim_flp_rx01`, `sim_flp_rx02` (rpm, tracks, sectors, motor kind, power-up steps, register defaults); device registers in an `SND_FLP_REGS` (STEP, SETTLE, HDLOAD, HDUNLOAD, MOTORUP; MOTOROFF for the controller-switched motors, not yet used).
- **Motor** `SND_FLP_MOTOR_AC`: starts with the power (RUN, BOOT, SET SOUND POWER=ON; SET SOUND ENABLED with the power on: just the loop), runs down at power off; at power on the heads step in and back (the drive's power-up homing: RX02 one track, RX01 ten). `SND_FLP_MOTOR_CTRL` (RX50/RX33/RX18) is prepared in the header but not implemented yet.
- **Head:** `sim_floppy_seek` steps (one click per track: a repeated one-shot, one voice), then loads the head (clack, the `head` loop starts) and returns step time + the longer of settle and head load; `sim_floppy_end` (from the device's command end) starts the unload timer (a realtime timer: wall time at the prompt); `sim_floppy_unload` (another drive selected: the RX02 has one head load line).
- **Rotation:** `sim_floppy_rotate` gives the time until a sector has passed under the head, from the realtime clock (`sim_sound_rt_us`) and a random phase per inserted diskette; sectors evenly spaced after the index.
- **Door:** `sim_floppy_insert`/`_remove` (ATTACH/DETACH): door sounds, the `disk` loop while a diskette is in and the motor runs.
- Floppy units count as attached storage for SET SOUND ENABLED/DISABLED (the RY device is not DEV_DISK).
- **Mixer:** `snd_play_repeat` (internal): a one-shot played n times, one every period, after a delay (the voice has `rep_left`, `rep_period`, `rep_pos`).
- **Sounds:** `<dir>/<prefix>_<name>.wav`, else `default_floppy/<name>.wav`: motor_start, motor (loop), motor_stop, disk (loop), head (loop), head_load, head_unload, step, door_close, door_open. SHOW SOUND lists `rx02 (floppy) -> default_floppy` when a default is used.

### RY (RX211/RXV21), realism mode

| Function | Time |
|---|---|
| Read/Write Sector | the other drive's head unloads; steps 6 ms per track, then 25 ms (settle or head load); the sector comes round (360 rpm, 26 sectors) and is read (6.41 ms); no diskette or a sector 0/>26: two revolutions, then the error; a track > 76: refused at once (0040) |
| Initialize (INIT bit, bus init, RUN/BOOT) | both heads home, then drive 0: track 1 with head load, sector 1 (manual: at most 1.8 s) |
| Set Media Density | track 0 with head load, to the index, then per track one revolution plus step and settle: 15.3 s (manual: about 15 s); the step clicks come once per track |
| Read Status | head load, to the index, one revolution (manual: about 250 ms) |
| Fill/Empty Buffer | 1.2 µs per bit between the RX02 and the interface (256 bytes: 2.5 ms) |
| Read Error Code | CTIME (as before) |

- Every command end (`ry_done`) restarts the head unload timer (HDUNLOAD 200 ms).
- ATTACH = diskette in, door closed; DETACH = door opened (new `ry_detach`). A command reaching a drive without a diskette ends with error 0110 instead of stopping the simulator (normal mode: STOP_IOE as before).
- `SHOW RYn DRIVE`: e.g. "diskette in, head loaded (unloads in 0.20 s), track 6, motor running"; `SET RY DEBUG=OPS`: one line per command with its time; `HELP RY` describes the realism mode.
- Normal mode unchanged, except the fix of case 45 (Set Media Density without a diskette crashed SIMH; now error 0110 in both modes).

### Tests (`tests/rx02/` on the notes branch; RT-11SB V05.07 from Ulli's `RT11.RX2`)

| Test | Result |
|---|---|
| n1 normal mode, PDP-11/70: boot DY0, DIR, SHOW | output identical to the unchanged build |
| r1 realism mode: boot, DIR, COPY HELP.SAV (161 blocks), DIR, UNPROTECT, DELETE, DIR | ok; 46 s wall time; 1258 reads (median 10.4 ms, mean 27.0 ms, max 536 ms), 350 writes |
| r1 trace: RT-11's 2:1 interleave | consecutive sectors 1, 3, 5, … take 10.36 ms each = two sector times (12.82 ms) minus the 2.5 ms Empty Buffer: the guest catches every second sector, as on the hardware; seeks with a head load 50–260 ms |
| r1 on the `integration` build | same results |
| a1 audio (SDL disk driver, real time) | motor start, loop at about −27 dBFS (24 and 48 Hz motor, 100/200 Hz hum, 168/336 Hz fan blades), door at DETACH, run-down after power off; steps about 11 dB and the head load about 16 dB above the steady sound above 700 Hz (v1 of the sounds masked the steps: fan noise lowered and darkened, steps and solenoid louder in v2) |
| t1 one Read Sector | 141 ms; SHOW RY0 DRIVE "head loaded (unloads in 0.20 s)", unloaded after 1 s at the prompt |
| t2 DETACH during a Read Sector (realism) | RYCS 104446, error 0110, the simulator keeps running |
| sd0/sd1 Set Media Density without a diskette | before: segmentation fault (exit 139); now error 0110 in both modes |
| sd1 Set Media Density on a new double-density diskette (drive 1) | 15.3 s; SET SOUND DISABLED with RY1 attached refused |
| sd2 | the formatted new image stays an empty file, also in the unchanged build (known issues 46) |
| v1 VAX-11/750 console | RY drive states, power on/off |
| Build `-Werror` pdp11, vax730, vax750, vax780, vax8200, vax8600, microvax2, pdp8, altair; `sim_sound.c` and `pdp11_ry.c` with `-Wall -Wextra` (PDP-11 and VAX) | no warnings |
| VAX diagnostics 730, 750, 780, 8600 | passed |
| VMS V4.7 on the 750 with DYA0 | **not run here** (no VMS image in this workspace): `tests/rx02/vms1.ini` and the DCL commands in `tests/rx02/README.txt` are ready for Ulli |

Notes from testing:
- In this workspace the SDL2 development package was missing at first: the first build had no sound (no `USE_SIM_SOUND`); after installing `libsdl2-dev` the binary must be removed to be relinked (the makefile does not notice the change).
- `SDL_DISKAUDIODELAY=0` makes the SDL disk driver write audio as fast as it can (1.3 GB for a 46 s run); leave it unset for real-time recordings.
- When a script stops the CPU in the middle of an RX02 command, the head stays loaded (the command has not ended): correct, not a bug.

### Open

- RX01 (RX device) and the 11/780 console RX01: done in step 2 (below). RX50, RX33 (RQ and VAXstation 3100), RX18: next, with `SND_FLP_MOTOR_CTRL` (motor on at access, off after MOTOROFF), the shared motor and carriage of the RX50, and the controller's interleave mapping on the RQDX.
- Real RX02 recordings would replace the synthetic sounds file by file; levels by ear.
- Known issues 43, 44 (RX/RY Initialize details), 46 (formatted image not written) and 48 (extended status head load bit) are open.

### RX02 under VMS V4.7: MOUNT fails with DRVERR (Ulli, 2026-11-11, fixed the same day)

- VAX-11/750, VMS V4.7, a new double-density diskette in DYA0 (`TEST.RX02`): `SHOW DEVICE DY` shows DYA0/DYA1 online, device type RX02; `INITIALIZE/DENSITY=DOUBLE DYA0: RX02T` returns without a message; `MOUNT DYA0: RX02T` gives `%MOUNT-F-DRVERR, fatal drive error`.
- Not yet known: whether the normal mode (sound disabled) fails the same way (then it is plain SIMH behaviour), and which RX211 status VMS's DYDRIVER saw. No VMS image here to reproduce it.
- New for the diagnosis (both modes): `SET RY DEBUG=CMD` (every function start with drive and density, INIT, busy GOs that are ignored, every completion with RYCS, RYES, error code, track and sector) and `SET RY DEBUG=REG` (every register access). Branch `sound` aebaeed4, `integration` 0807f9e3. Patch **`opensimh_5_sound_v28.patch`** replaces v27 (checked: 6 + 5 v28 on 87eb7d5 give the `sound` tree).
- Ulli sent his VMS V4.7 image (`VMS047.R81`, SYSTEM/MANAGER); reproduced here in the **normal mode** too, so a plain SIMH bug.
- **Cause (known issues 47):** VMS DYDRIVER (disassembled from the running system: SDA `SHOW DEVICE DYA` gives the driver addresses, then the SIMH command `EXAMINE -V -M`) determines the density at PACKACK with a **single density Read Status** and switches to double density only when RX2ES bit 4 (density error) comes back set. The RY never set the density error on Read Status, so MOUNT read the home block in single density, got error 0240 on each of its retries and gave up with DRVERR. In addition, `ry_done` set RX2CS Error whenever old error bits were still in RX2ES, so the Read Error Code and Read Status after the failed read ended with Error as well.
- **Fix:** Read Status starts with fresh error bits and sets the density error when the diskette density differs from the function density (no RX2CS Error for it); RX2CS Error reports the current function's errors only; Fill/Empty Buffer and Set Media Density clear RX2ES at initiation (RX02 manual). Branch `sound` ff6183f0, `integration` 5e10d695. Patch **`opensimh_5_sound_v29.patch`** replaces v28 (checked: 6 + 5 v29 on 87eb7d5 give the `sound` tree without the Visual Studio project files).
- **Tests** (`tests/rx02/vms2.ini` normal, `vms3.ini` realism, VMS V4.7 on the VAX-11/750; they need a copy of `VMS047.R81` in the current directory): INITIALIZE/DENSITY=DOUBLE DYA0 and /DENSITY=SINGLE DYA1, MOUNT both, COPY LOGINOUT.EXE (107 blocks) to both, DIRECTORY, DISMOUNT: both mounts succeed, no errors, in both modes. The realism run takes about 2 min 50 s of RX02 activity. RT-11 tests n1 (output identical), r1, t1, t2, sd0, sd1 unchanged.
- Script pitfall: in the realism mode several DCL commands typed ahead while a floppy command runs overflow the VMS type-ahead buffer (`%RMS-F-RER, -SYSTEM-W-DATAOVERUN` on the truncated command line); the scripts send one command at a time.
- New suspicious item (known issues 48): the extended status (Read Error Code) word 4 bit 5 is "diskette in drive" in SIMH, the head load bit in the manual. No effect seen.

## Phase E, step 2: the RX01 (RX device) and the VAX-11/780 console floppy (2026-11-11)

### Where things are

- Branch `sound`: commits 74f013b8 and 77609498 on top of ff6183f0; branch `integration`: d6cf4fc0, ef25b181.
- Patch **`opensimh_5_sound_v30.patch`** replaces v29 (checked: 6 + 5 v30 on 87eb7d5 give the `sound` tree without the Visual Studio project files).
- Changed: `PDP11/pdp11_rx.c` (pdp11 only), `VAX/vax780_stddev.c` (vax780), `sim_sound.c`, `sim_sound.h` (drive type alias, RX01 values), `scp.c` (HELP SET SOUND), `sound/rx02/README.txt`.
- Notes branch: `tests/rx01/` (mkrx01.py, prog.ini, n1, r1, t2, vms4, vms5, README.txt), `tools/make_sound_devices_ods.py` v12 and `docs/SIMH_Sound_devices.ods` table v11 (Floppies sheet: RX and CS rows).
- Source: **RX8/RX11 Floppy Disk System Maintenance Manual EK-RX01-MM-002** (Dec 1976), read on the web (svn.so-much-stuff.com mirror of dundas-mac.caltech.edu).

### RX01 facts (EK-RX01-MM-002)

| Item | Value |
|---|---|
| Speed, geometry | 360 rpm ± 2.5 % (166 ms per revolution), 77 tracks × 26 sectors × 128 bytes, FM |
| Track to track | 10 ms maximum |
| Head | loaded after the steps; the microcode (STEPHD) then waits 20 ms (spec: settle 25 ms maximum) |
| Head unload | "to reduce diskette wear": when no new command (RUN) comes within 45.87 ms |
| Read Status | "between one and two diskette revolutions or about 250 ms" (drive ready from the index pulses) |
| Initialize | drive 1 to track 0, then drive 0 reads track 1 sector 1; up to 1.8 s |
| Transfer rates | diskette 4 µs per bit (250 kbit/s); CPU interface to I/O bus 18 µs per 8-bit byte |
| Power up | the heads move ten tracks toward the center and back to track 0 |
| Motor | AC, belt to the spindle (50/60 Hz belts and pulleys), power from the system's AC switch: the same drive as the RX02, motors on the line |
| Average access | 488 ms published (the manual's own sum gives 493 ms; half the stroke); the model's third-stroke average 360 ms |

### Behavior (realism mode)

- **RX (RX11/RXV11, PDP-11):** as the RY: Read/Write Sector (the other drive's head unloads, steps, head load + 20 ms, the sector at its real time, 6.41 ms), Initialize, Read Status (to the index and once round, no head load), Fill/Empty Buffer and the sector/track bytes 18 µs per byte; ATTACH/DETACH are the door; a command reaching a drive without a diskette ends with error 0110 (normal mode: STOP_IOE as before). New: `SHOW RXn DRIVE`, registers STEP, SETTLE, HDLOAD, HDUNLOAD, MOTORUP, debug flags OPS, CMD, REG (CMD and REG in both modes), `HELP RX` (the device had no help; it also explains the shared CSR 177170 with the RY, known issues 49).
- **CS (VAX-11/780 console floppy):** one RX01 drive on the console LSI-11's RXV11. The drive part of a read or write takes its real time, including the 128 bytes between the drive and the RXV11 (18 µs each: write before, read after the drive operation); the bytes between the LSI-11 and the VAX keep XTIME (20 instructions; the real console path speed is not published). Read Status of the console protocol returns the stored error code at once, as before. `SHOW CS DRIVE`, the same registers, debug flag OPS, `HELP CS` (new).
- **Sounds:** the RX01 is the RX02's drive mechanism, so it uses `sound/rx02/rx02_*.wav` unless `sound/rx01/rx01_*.wav` exist (new `SND_FLP_TYPE.alias`; SHOW SOUND: `rx01 (floppy) -> rx02`), then `default_floppy`. No new WAV files. The steps come every 10 ms (the RX02: 6 ms).
- In the realism mode Initialize without a diskette in drive 0 ends after its real time with Initialize Done and error 0010; the normal mode keeps SIMH's immediate error without Initialize Done (known issues 44).

### Tests (`tests/rx01/`)

| Test | Result |
|---|---|
| n1 normal mode, PDP-11/70: BOOT RX0 of the test image, 26 sectors of track 1 in the 2:1 interleave | R0 = 123, data in order; output identical to plain Open SIMH 87eb7d5 |
| r1 realism mode | Initialize 140 ms; boot read 166.6 ms (sector 1 had just passed); interleaved reads 9.68 ms each, one every 12.82 ms (two sector times), track 1 in 0.33 s |
| t2 | DETACH while stepping: error 0110, no stop; Read Status 251 ms, RXES 200; empty drive 1: 0110 after 833 ms (48 steps + head load + two revolutions); normal mode: stops as before |
| vms4 (normal) / vms5 (realism), VMS V4.7 on the VAX-11/780 | SYSGEN CONNECT CONSOLE, MOUNT/FOREIGN CSA1, EXCHANGE INITIALIZE, COPY SYSGEN.EXE (129 blocks) to CSA1 and back, DIFFERENCES: 0 differences in both modes; vms5: 1132 drive operations, VMS's 2:1 interleave with 6-sector skew, reads normally 12.7 ms, the exchange about 22 s |
| Audio (SDL disk driver) | step clicks every 10.0 ms; power-up homing (20 clicks) when the 2 s run-up ends |
| RX02 regression (tests/rx02 n1, r1, t1, t2, sd1) and VMS RX02 test | unchanged (n1 identical; realism runs differ only in timing lines: random rotational phase) |
| Build `-Werror` pdp11, vax780, vax730, vax750, vax8200, vax8600, microvax2, pdp8, altair; `pdp11_rx.c`, `vax780_stddev.c` with `-Wall -Wextra` | no warnings (only SIMH's usual missing-initializer style warnings with -Wextra) |
| VAX diagnostics 730, 750, 780, 8600 | passed |

### Open

- The speed of the console path LSI-11 ↔ VAX (XTIME): a real VAX-11/780 console floppy is slow; a measurement on real hardware (e.g. the time of a COPY of a known file from CSA1) would give a realistic value.
- RX01 Read Error Code: CTIME as before (no published time).
- A real RX01 recording would replace the RX02-based sounds (`sound/rx01/rx01_*.wav`).

## Phase E, step 3: the RX50 on the MSCP controllers (RQ) (2026-11-11)

### Where things are

- Branch `sound`: commit d54c447a on top of 77609498; branch `integration`: 825dad4e.
- Patch **`opensimh_5_sound_v31.patch`** replaces v30 (checked: 6 + 5 v31 on 87eb7d5 give the `sound` tree without the Visual Studio project files).
- Changed: `PDP11/pdp11_rq.c` (pdp11 and every VAX with an MSCP controller), `sim_sound.c`, `sim_sound.h`. New: `sound/rx50/` (ten files and `README.txt`).
- Notes branch: `tools/make_floppy_sounds.py` v3 (RX50), `tools/make_sound_devices_ods.py` v13 and `docs/SIMH_Sound_devices.ods` table v12 (RX50 row), `docs/SIMH_known_issues.md` v22 (case 50), `tests/rx50/` (c1, c2, vms1, vms2, README.txt).

### Decisions (Ulli, 2026-11-11)

1. RX50 drives only on the RQDXn and RUX50 controllers, only in the realism mode.
2. Two RX50 units with **consecutive drive (unit) numbers** form one drive (not the RC25 even/odd scheme); pairing from the lowest number up.
3. RX50 units on other controllers: SET SOUND ENABLED (and SET RQ <other type>) **disables them with a message** instead of refusing (the default Unibus configuration has an RX50 at RQ3 on the UDA50A).
4. A RUX50 only connects RX50 drives.

Not done, on purpose: a bus rule for the RQDXn (Qbus) and the RUX50 (Unibus). A PDP-11 keeps the RQDX3 chosen at start-up for the default 11/73 after `SET CPU 11/70` (known issues 50, also plain Open SIMH), so such a rule refused every Unibus PDP-11 configuration (found by the RX01/RX02 PDP-11/70 tests; removed again).

### RX50 facts

| Item | Value | Source |
|---|---|---|
| Medium | 5.25", 1 side, 80 tracks × 10 sectors × 512 bytes (800 blocks), two diskettes per drive | SIMH, Computer History Wiki |
| Speed | 300 rpm (200 ms per revolution) | Rainbow 100 Technical Manual EK-PC100-TM-001, 6.5.3 |
| Step | at least 6 ms between step pulses; one stepper, one carriage for both diskettes | EK-PC100-TM-001 6.4.1.6 |
| Settle, head load | 30 ms each; one head load solenoid per side | EK-PC100-TM-001 6.5 (table, extraction unclear), 6.4.5 |
| Motor | one spindle motor, MOTOR ON from the controller, at speed within 0.5 s | EK-PC100-TM-001 6.4.1.4 |
| Average access | 164 ms (published) | Computer History Wiki |
| Motor off, head unload | 2 s idle, **provisional** (Ulli will measure on his MicroVAX) | |
| Block mapping (RQDX) | logical track + 1 is the physical track (the last ten blocks on track 0), 2:1 interleave (blocks 0-4 on sectors 1, 3, 5, 7, 9), two-sector skew per track | VMS and RT-11 driver code quoted on TUHS (Jun 2000) and classiccmp; a former DEC floppy firmware engineer confirms that the RQDX3 does it |

### Behavior (realism mode)

- **Model (`sim_sound`):** `SND_FLP_MOTOR_CTRL` is now implemented: `sim_floppy_motor ()` switches the motor on and returns the time until it is at speed, `sim_floppy_end ()` restarts the motor-off timer, `sim_floppy_motor_off ()` stops it. `sim_floppy_share ()` lets a slot use the motor and carriage of another: motor state, motor sounds and head position belong to the first slot; each slot keeps its door, head solenoid and rotational phase. Drive type `sim_flp_rx50`.
- **Transfer:** the other slot's head unloads (the drive selects one side), the motor starts if it was off (0.5 s; the steps go on meanwhile), the head steps 6 ms per track, loads and settles (30 ms), then each block is read when its sector comes round, with one step to each next track. A whole track takes two revolutions (2:1 interleave): about 11 KB/s.
- **Door:** ATTACH/DETACH. **AVAILABLE with spin-down** (VMS DISMOUNT): the motor stops, the diskette stays in (normal mode: detached, as before).
- **Registers** (RQ, all controllers): RX50STEP, RX50SETTLE, RX50HDLOAD, RX50HDUNLOAD, RX50MOTORUP, RX50MOTOROFF. **SHOW RQn DRIVE** (also SPINDLE): e.g. "RX50 drive with RQ2 (unit numbers 1/2), this slot: diskette in, head loaded (unloads in 1.98 s), track 1, motor running (off in 1.98 s)".
- **Rules:** see the decisions; HELP RQ describes them.
- **Sounds:** `sound/rx50/rx50_*.wav` (synthetic: DC motor and belt, no fan, smaller solenoid, 6 ms stepper, lever door), else `default_floppy`.
- Normal mode unchanged.

### Tests (`tests/rx50/`)

| Test | Result |
|---|---|
| c1 (MicroVAX II): default RQDX3 configuration | accepted |
| c1: SET RQ KDA50 | "RQ3 disabled: an RX50 needs an RQDX1, RQDX3, RQDX4 or RUX50 controller, RQ is a KDA50" |
| c1: RQ1, RQ2, RQ3 RX50 on the RQDX3 | drive 1/2 and a lone slot 3; after SET RQ1 RD54: drive 2/3 |
| c2 (VAX-11/750): SET SOUND ENABLED | "SET SOUND ENABLED: RQ3 disabled: ..." (UDA50A default) |
| c2: SET RQ RUX50 / SET RQ1 RD54 / SET RQ UDA50A | RQ0-RQ3 RX50 (drives 0/1, 2/3) / refused / RD54 ×3, RQ3 disabled |
| vms1 (normal) / vms2 (realism), VMS V4.7 on the MicroVAX II, RA81 on a KDA50, one RX50 drive (DUB1/DUB2) on the RQDX3 of RQB | INITIALIZE, MOUNT both, COPY SYSGEN.EXE (129 blocks) to DUB1 and from DUB1 to DUB2, DIFFERENCES: 0, DIRECTORY, DISMOUNT; both modes |
| vms2 trace | 126 transfers, 53 s drive time; 127 blocks in 5.7 s; first transfer after motor off about 0.6 s; 15 slot changes |
| Audio (SDL disk driver, console BOOT DUA1 of a blank RX50) | door, motor start with head load, motor/diskette/head loops, head unload, door, 1.5 s run-down |
| RA/RC25 realism tests ra1-ra7, c1, c5, c6, c10 against the previous build | only the new "RQ3 disabled" messages differ (c6: the known `?53`, case 22, in both) |
| RX01 (tests/rx01 r1) and RX02 (tests/rx02 n1, r1) on the PDP-11/70 | unchanged |
| Build `-Werror` pdp11, microvax1, microvax2, microvax3900, rtvax1000, vax730, vax750, vax780, vax8200, vax8600, pdp8, altair | no warnings |
| VAX diagnostics 730, 750, 780, 8600; MicroVAX 3900 core test | passed |

### Open

- Motor-off and head unload after 2 s idle are provisional (Ulli's measurement on the MicroVAX II).
- At the `sim>` prompt the floppy timers (head unload, motor off) only fire when the simulator looks at the drive (SHOW, continuing, a command); until then the motor keeps humming (also the RX02's head unload). The audio thread could time them instead (as the tape voices).
- RX33 (RQDX3 and VAXstation 3100) and RX18 next.

## Phase E, step 4: the VAX 8200 console RX50 (CS) (2026-11-11)

- Branch `sound`: commit f38de89c on top of d54c447a; `integration`: 454ee6ca. Patch **`opensimh_5_sound_v32.patch`** replaces v31 (checked: 6 + 5 v32 on 87eb7d5 give the `sound` tree without the Visual Studio project files). Changed: `VAX/vax820_stddev.c` (vax8200 only).
- The 8200's console floppy is an RX50 reached through the floppy controller registers at 200B0000 (CS0-CS5, buffer EB/FB, GO); the CPU moves the 512 bytes itself, the guest gives physical tracks and sectors (SIMH's sector table maps them to the image, with the RX50 interleave). CS0 and CS1 are the two slots of one drive: CS1 shares the mechanics of CS0 (`sim_floppy_share`).
- Realism mode (GO): Read/Write Sector: the other slot's head unloads, the motor starts if it was off (0.5 s), steps (6 ms per track), head load and settle (30 ms), the sector at its real time; no diskette: not ready at once (error 0x50); a bad track or sector: two revolutions. Restore and Initialize: motor on, steps to track 0. Other functions: CTIME as before. Every command restarts the head unload and motor-off timers (2 s, provisional).
- Registers STEP, SETTLE, HDLOAD, HDUNLOAD, MOTORUP, MOTOROFF; `SHOW CSn DRIVE` ("RX50 slot A: ..."); ATTACH/DETACH are the door; `HELP CS` (new); sounds from `sound/rx50`. Normal mode unchanged.
- Tests (`tests/rx50_8200/`, a VAX program driving the controller registers): first read 620 ms (motor start, 40 steps), every second sector 40 ms, the very next sector 220 ms (just missed: why the RX50 is interleaved), slot B on the same track without steps 101 ms, 30 steps 280 ms; one motor for both slots, off 2 s after the last command. VAX 8200 diagnostics passed; build without new warnings.
- VMS V4.7 on the 8200 (integration build, VMS047.R81 on an RA81) with CSA1/CSA2, step 2 of the plan agreed with Ulli: no code changes were needed.

| Test (`tests/rx50_8200/vms1.ini` normal, `vms2.ini` realism) | Result |
|---|---|
| SYSGEN CONNECT CONSOLE; SHOW DEVICE CS | CSA1:, CSA2: online |
| MOUNT/FOREIGN CSA1:, CSA2:; EXCHANGE INITIALIZE both /VOLUME_FORMAT=RT11 | ok in both modes |
| EXCHANGE COPY SYSGEN.EXE (129 blocks) to CSA1, CSA1 to CSA2, CSA2 back to disk; DIFFERENCES | no differences, both modes |
| EXCHANGE DIRECTORY CSA1:,CSA2:; DISMOUNT | 1 file, 129 blocks, 657 free on each |
| Realism timing (SET CS DEBUG=FNC) | 576 sector operations, average 63 ms; one motor start (first read 606 ms), slot B right after slot A 62 ms (shared motor) |
| Wall time, realism run (boot to exit) | 78 s |

- The RT-11 file date shows 11-Nov-1994 for 2026: VMS V4.7 EXCHANGE writes the year modulo 32 (1972 + 22), a VMS limit, not a SIMH problem.

## Not done yet (after phase B)

- Floppies: the RX02 (phase E step 1), the RX01 and the 11/780 console RX01 (step 2), the RX50 (step 3), the VAX 8200 console RX50 (step 4) are done; RX33, RX18 next; PDP-8 RX8E/RX28 postponed. TU58 later. (The RA60, the fixed RA drives and the RC25 are done, see phase C steps 3, 5 and 6; the TS11/TSV05 tapes in phase D step 1, the TK50 in phase D step 2; other tapes later.)
- RC25: KLESI ABORT/timeout paths untested; case 22 (KA655 self-test timing) separately.
- Real start/stop times of the RP04/05/06, RM02/03/05 (SPINUP/SPINDOWN are provisional 30 s).
- Calibrate the 11/750 THROTTLE default; tune the seek and spindle gains by ear; spindle recordings of a real RK07.
- Installing the `sound/` directory with `make install`/CMake packaging.
- Other simulators may opt in to `default_cpu`. (Default tape sounds: done with the TS11, phase D step 1.)
- `vid_beep` migration (phase D).
- Overlap policy for repeated events, by listening tests.
