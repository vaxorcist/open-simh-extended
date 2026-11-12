<!-- open-simh-extended/docs/SIMH_TU58_sound_notes.md  v3 (v3: clock fix, sounds, VAX-11/730 and PDP-11 RT-11 tests, Ulli's cartridges; v2: model coded (branch tu58-wip), first VMS results; v1: facts, current SIMH code, planned model, progress; written before the code so the work survives a stop) -->
# TU58 DECtape II in the sound realism mode: working notes

Started 2026-11-12 for Ulli (vaxorcist). Phase F, step 1 of the sound project. This file is the running record of the TU58 work: facts, design, progress. It is updated at every step so the work can be picked up after an interruption (weekly limit). The handover (`SIMH_Sound_handover.md`) gets the summary when the step is done.

Ulli: "although it is a tape drive, it is used like a disk drive, i.e. files are accessed via their directory entries" (RT-11, VMS EXCHANGE, the VAX consoles: block-addressed, random access).

## 1. Facts

Sources: TU58 DECtape II User's Guide **EK-0TU58-UG-004** (Dec 1983, bitsavers; sections given below), Computer History Wiki (TU58), cctalk 2015 ("Sector Interleave", quoting EK-0TU58-UG-001 p. 1-4).

| Item | Value | Source |
|---|---|---|
| Cartridge | 42.7 m (140 ft) of 3.81 mm (0.150 in) tape; 262,144 bytes = 512 blocks of 512 bytes | UG 1.4.1 |
| Tracks | 2 tracks, 256 blocks each | UG 1.2 |
| Records | each track 1024 "individually numbered, firmware-interleaved records" of 128 bytes; 4 records make a block | UG 1.4.1, UG-001 p. 1-4 |
| Record addressing | special address mode: records 0-2047 (block × 4) | UG 3.2.3.1 |
| Tape speed | 30 in/s read/write, 60 in/s search | UG 1.4.1 |
| Density | 800 bits/in (ratio encoding), 41.7 µs per data bit, 24 kbit/s | UG 1.4.1 |
| Access time | average 9.3 s, maximum 28 s | UG 1.4.1 |
| Serial line | 150 to 38.4 kbaud (jumpers), factory setting 38.4 kbaud, 1 start + 1 stop bit, no parity | UG 1.4.1, 4.2.4, 4.7.1 |
| Drives | one or two per controller, only one runs at a time | UG 1.4.1 |
| Motor | one servo motor (speed and direction), tachometer feedback, current limiting, anti-runaway timer ("motor stopped" error -33) | UG 1.3.1, 1.2, C-2 |
| Run indicator | lights while the tape moves; do not touch the cartridge while it is on; removal during motion does no harm to the mechanism, a write in progress may lose data | UG 5.1, 2.5.2 |
| Power-up | initializes, runs its diagnostics, ready within 1 s | UG 2.4.1 |
| Cartridge life | 5000 end-to-end-and-back passes | UG 1.4.1 |
| Write/verify | writes, then backs up and reads the blocks just written | UG 3.3 |
| Newtape | host routine: seek to block 777 (octal), then to block 0 (a full wind and rewind, retensions the tape) | UG appendix A, 1.4.4 |

Not published (found): start/stop and reversal times, the exact record order on the tape, rewind speed other than the search speed.

## 2. Derived model (provisional where marked)

- **Record order (inference, provisional):** Figure 1-4 of the UG groups the blocks by 128 (#0, #128, #256, #384) and the interleave is 1:2. Assumed: on each track the 1024 record slots alternate between two halves, slot = 2 × (r mod 512) + (r div 512) for record r (0-1023) of the track. Blocks 0-127 (records 0-511) use every second slot over the whole tape in one pass, blocks 128-255 the slots between them; track 1 (blocks 256-511) the same. Both tracks are read in the forward direction; the head has both tracks, track selection is electronic.
- **Lengths:** 1024 slots per track over the usable tape; the model uses 1600 in (1024 × 1.5625 in per slot; a 128-byte record is 1.28 in at 800 bits/in, the rest header and gap). 1600 in at 60 in/s = 26.7 s; with acceleration and turnaround about 28 s: the published maximum. A third of it (random to random) about 9 s: the published average 9.3 s.
- **Times per record:** a slot passes in 52 ms at 30 in/s; a record is read in 42.7 ms (1024 bits × 41.7 µs). The 4 records of a block lie 2 slots apart: reading a block takes 7 slots + 1 record ≈ 0.41 s; consecutive blocks stream at 8 slots per block (0.42 s, about 1.2 KB/s).
- **Serial line:** 38.4 kbaud, 10 bits per character: 260 µs per byte, 33 ms per 128-byte data packet (register; the VAX console TU58 lines are also run at 38.4 kbaud, to be checked).
- **Positioning:** search at 60 in/s from the current position (per drive) to a run-up distance before the target slot; if the target is behind, or too close ahead, the tape reverses (turnaround, provisional 0.1 s) and searches back; then reads at 30 in/s. Run-up, turnaround and acceleration are provisional registers.
- **Position after a command:** where it ended (after the last record transferred); the tape stops after each command (the run indicator goes off).
- **Write:** the TU58 has a 128-byte buffer: it sends Continue, receives a data packet (33 ms on the line) and writes it when its slot comes; the 1:2 interleave leaves one slot (52 ms) for the next packet. Write/verify: then back to the block's first record and read the records again.
- **Cartridge:** ATTACH = insert (click), DETACH = remove (allowed; a command in progress ends with "no cartridge"). Position after insert: BOT (provisional).

## 3. SIMH today (`PDP11/pdp11_td.c`, Open SIMH 87eb7d5)

- One file for every TU58: the PDP-11 TDC device (up to 16 DL-based controllers, 2 drives each) and the console TU58 (device TD) of the VAX-11/730 (`VAX/vax730_stddev.c`) and VAX-11/750 (`VAX/vax750_stddev.c`), connected by `td_connect_console_device`.
- Timing in instructions: `STIME` per block of the seek, `CTIME` command, `XTIME` per byte on the line, `ITIME` init. The seek distance is always counted from block 0 (`t = abs (ctlr->block - 0)`), there is no tape position.
- States: READ (seek) → READ1 (build a 128-byte data packet) → READ2 (send it, a byte per XTIME) → ... → END/END1 (end packet); WRITE → WRITE1 (Continue) → host data packet → WRITE2 (copy into the buffer) → ...; POSITION; BOOTSTRAP; INIT.
- The image is a 256 KB file in block order, buffered in memory (`UNIT_BUF`).

## 4. Plan

1. Model in sim_sound or in pdp11_td.c (per drive: position, track, motion), realistic times in the states READ/READ1, WRITE1/WRITE2, POSITION, write/verify; the line at the baud rate; registers; SHOW TDCn/TDn DRIVE; debug flag for the motion.
2. Sounds (synthetic, `tools/make_tape_sounds.py`): motor at search and read speed (loops), start, stop, reversal, cartridge insert and remove; `sound/tu58`, fallback `default_tape`.
3. Tests: PDP-11 (TDC, a program or RT-11 if an image is available), VAX-11/750 console TU58 under VMS V4.7 (SYSGEN CONNECT CONSOLE, CSA1, EXCHANGE), VAX-11/730 console.
4. Device table (new rows), known issues for anything found, handover, patch.

## 5. Progress

- 2026-11-12: facts collected, model designed (this file v1). Nothing coded yet.
- 2026-11-12 (v2): coded, work branch **`tu58-wip`** (commit 2182acbf on the `sound` tree, not yet in a patch):
  - `sim_sound.c/.h`: `SND_TU58` (tape position per drive, record slots, search/read speeds, start/turnaround/run-up, timed motion sounds start/search/read/reverse/stop, insert/remove; fallback to `default_tape`), `sim_tu58_record_us`, `_records_us`, `_position_us`, `_stop`, `_show`.
  - `PDP11/pdp11_td.c/.h`: realistic times in READ/READ1/READ2, WRITE/WRITE1/WRITE2 (+ write/verify read-back), POSITION, BOOTSTRAP, END (the tape stops); bytes TU58 to host at BAUD (38400); registers STARTMS 60, TURNMS 150, RUNUP 150 (1/100 in), BAUD; `SHOW TDCn DRIVE`, debug flag MOTION; `td_attach`/`td_detach` (cartridge in/out) also for the VAX console TD; storage registration (SET SOUND guard).
  - Fixed on the way (both modes): command for a unit number the controller does not have (unit pointer outside the unit array) now ends with "bad unit" 0370; a cartridge removed during a transfer ends it with "no cartridge" instead of using the freed buffer.
  - `VAX/vax730_stddev.c`, `vax750_stddev.c`: the console TD gets real registers STARTMS/TURNMS/RUNUP/BAUD and SHOW TD DRIVE.
  - Builds without warnings: pdp11, vax730, vax750.
- First tests (`tests/tu58/vms1.ini` normal, `vms2.ini` realism; VMS V4.7 on the VAX-11/750, console TU58 = CSA1 after SYSGEN CONNECT CONSOLE, blank cartridge): MOUNT/FOREIGN, EXCHANGE INITIALIZE (RT-11), COPY LOGINOUT.EXE (107 blocks) to CSA1 and back, DIFFERENCES 0, DIRECTORY 107 blocks / 397 free, DISMOUNT: both modes ok. Realism: whole run 5 min 12 s wall; 982 motions, 139.5 s of tape time; records within a block 69.9 ms after the previous packet (2 slots minus the 34 ms of the packet on the line), written records 95 ms apart, a block from a stopped tape 0.34-0.66 s, longest search 24 s.
  - Script pitfall: after SYSGEN CONNECT CONSOLE the next command must wait for the SYSGEN> prompt (typed early, characters are lost; plain Open SIMH the same).
- Found, not changed (to go into the known issues): the console TD register table of the 730/750 shows dummies (`td_regval` for every register: EXAMINE/DEPOSIT TD CTIME etc. do nothing); the command modifier byte (special address mode = record numbers, write/verify) is ignored; host to TU58 bytes are not timed (transmit done at once).
- Asked Ulli (14:15) for images: an RT-11 TU58 for the PDP-11, the VAX-11/750 and VAX-11/730 console cartridges.
- Next: synthetic sounds (`sound/tu58`), PDP-11 TDC test, VAX-11/730 console, device table, known issues, handover, patch.
- 2026-11-12 (v3), branch `tu58-wip` commit df273de2:
  - **Bug in my own sim_sound code, fixed (known issues 55):** `sim_sound_rt_us` divided all instructions since the last start by the *current* rate estimate; on an unthrottled simulator (VAX-11/730, PDP-11) the estimate changes every second by up to ±20 %, so the clock jumped by seconds after a long run. The TU58 model then took a record as passed and searched back (13-20 s instead of 0.1 s). Now piecewise linear. Also affects the floppy rotation and the tape models on unthrottled simulators (fixed with it). The 750 was not affected (THROTTLE 300K).
  - Sounds: `tools/make_tu58_sounds.py` v1 (notes branch), `sound/tu58/` (read and search loops, start, stop, reverse, insert, remove, README).
  - VAX-11/730 console TU58 (`tests/tu58/vms3.ini` normal, `vms4.ini` realism, same workload as on the 750): both ok, 0 differences; realism 1323 motions, 202 s wall.
  - **RT-11 TU58 for the PDP-11:** Ulli's RT-11 V5.7 RX02 kit has no DD.SYS; built from Ulli's RT-11 V5.3 RL02 kit (`rtv53_rl.dsk`): `tests/tu58/mkdd_prep.py` (empties STARTA.COM on a copy: the kit boots the automatic installation, which ignores Ctrl/C) and `tests/tu58/mkdd.ini` (INITIALIZE DD0:, COPY/SYSTEM RT11SJ/DD/TT/SWAP, COPY PIP/DUP/DIR/STARTS.COM, COPY/BOOT, BOOT DD0:). Result `rt11v53_dd.dsk` (8 files, 210 blocks, 294 free; padded to 262,144 bytes); rebuilt byte-identical by the documented procedure. SIMH's own TDC boot ROM boots it (`BOOT TDC0`; in 2016 that failed on cctalk, works now).
  - PDP-11 tests `tests/tu58/rt1.ini` (normal) / `rt2.ini` (realism): BOOT TDC0, DIR, COPY DUP.SAV TEST.SAV, DIR, UNPROTECT + DELETE, DIR. Normal ok. Realism: boot block 0.42 s, a block streams at 69.8 ms per record; COPY shuttles between DUP.SAV (track 0, second pass) and TEST.SAV (start of track 1): searches of 12-27 s each time (39 over 2 s in 536 s of tape time) - the real TU58 behaviour ("file placement was an art").
  - Ulli's cartridges: VAX-11/750 console (`TU58_VAX_11750_CONSOLE.T58`, RT-11 volume, 71 files, 263 blocks: VMB.EXE, BOOT58.EXE, PCS750.BIN, *BOO.CMD, ...) and 11/725-730 diagnostic supervisor (`BE-T175I-DE_TU5835_11725-730_DIAG_SUPER.T58`: ENSAA.EXE 229 blocks, EVSAA.HLP, DIAGBO.EXE, DEFBOO.CMD). Read tests `vms5.ini` (750) and `vms6.ini` (730): directory, copies twice in block mode (EXCHANGE takes .BIN for formatted binary otherwise: %EXCHANGE-W-BINRECFMT), DIFFERENCES.
  - `BOOT TD0/R5:10` of the diagnostic cartridge on the vax730: SIMH's internal VMB looks for a standalone (Files-11) boot file: `%BOOT-F-Unable to locate BOOT file`; plain Open SIMH 87eb7d5 the same (a real 730 boots the console cartridge through its console microcode). Known issues 60.
- Known issues to add (numbers reserved): 55 FIXED (rt clock jumps, my code), 56 FIXED (TU58 command for a unit the controller does not have: unit pointer outside the unit array; now "bad unit" 0370), 57 FIXED (cartridge removed during a TU58 transfer: the freed buffer was used; now "no cartridge"), 58 OPEN (VAX-11/730/750 console TD register table: every entry is the dummy `td_regval`), 59 OPEN (TU58 command modifier ignored: special address mode, write/verify only timed in the realism mode), 60 OPEN (vax730 BOOT TD cannot boot an RT-11 console cartridge), 61 OPEN pitfall (RT-11 V5.7 installs LS at CSR 176500, the TDC's address).
