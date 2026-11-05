<!-- open-simh-extended/docs/SIMH_Sound_handover.md  v12 (RB730 fixes, sound rebased on them; v11: RB02/RB80 on the RB730; v10: spindle sound generator; v9: device table; v8: RA60 on the RQ; v7: Massbus disks; v6: RL01/RL02, common spindle support; v5: ONLINE/OFFLINE wait at the prompt; v4: spin-up in wall time at the prompt, power on restores spindles, spin-up bug fixes; v3: VMS boot fixes; v2: phase B; v1: phase A) -->
# SIMH Sound for Open SIMH: status and handover

Date: 2026-11-02. Prepared with Claude for Ulli (vaxorcist). Nothing has been pushed to any remote. Design: `SIMH_Sound_design.md` (v1).

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

## Not done yet (after phase B)

- RX and TU58 later without spin-up/spin-down; tapes afterwards. (The RA60 is done, see phase C step 3.)
- Real start/stop times of the RP04/05/06, RM02/03/05 (SPINUP/SPINDOWN are provisional 30 s).
- Calibrate the 11/750 THROTTLE default; tune the seek and spindle gains by ear; spindle recordings of a real RK07.
- Installing the `sound/` directory with `make install`/CMake packaging.
- `vid_beep` migration (phase D).
- Overlap policy for repeated events, by listening tests.
