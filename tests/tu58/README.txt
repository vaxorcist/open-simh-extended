open-simh-extended/tests/tu58/README.txt  v1

TU58 DECtape II in the sound realism mode (2026-11-12): the PDP-11 TDC
device and the console TU58 (TD) of the VAX-11/730 and 750.  Background,
model and results: docs/SIMH_TU58_sound_notes.md, docs/SIMH_Sound_handover.md.

Files
-----
vms1.ini      VAX-11/750, normal mode: VMS V4.7, console TU58 = CSA1 after
              SYSGEN CONNECT CONSOLE, blank cartridge: MOUNT/FOREIGN,
              EXCHANGE INITIALIZE (RT-11), COPY LOGINOUT.EXE (107 blocks) to
              CSA1 and back, DIFFERENCES, EXCHANGE DIRECTORY, DISMOUNT
vms2.ini      the same, realism mode
vms3.ini      VAX-11/730, normal mode, the same workload
vms4.ini      VAX-11/730, realism mode
vms5.ini      VAX-11/750, realism mode: Ulli's VAX-11/750 console cartridge
              (TU58_VAX_11750_CONSOLE.T58, mounted /NOWRITE): EXCHANGE
              DIRECTORY; VMB.EXE and PCS750.BIN copied twice in block mode,
              DIFFERENCES of the two reads
vms6.ini      VAX-11/730, realism mode: Ulli's 11/725-730 diagnostic
              supervisor cartridge (BE-T175I-DE_TU5835_11725-730_DIAG_
              SUPER.T58): DIRECTORY, ENSAA.EXE (229 blocks) copied twice,
              DIFFERENCES
mkdd_prep.py  prepares a copy of the RT-11 V5.3 RL02 kit (rtv53_rl.dsk) so
              it boots to the monitor (empties STARTA.COM, the automatic
              installation, on the copy)
mkdd.ini      PDP-11: builds the bootable RT-11 V5.3 SJ TU58 rt11v53_dd.dsk
              from the prepared kit (INITIALIZE DD0:, COPY/SYSTEM RT11SJ,
              DD, TT, SWAP; COPY PIP, DUP, DIR, STARTS.COM; COPY/BOOT) and
              boots it (BOOT DD0:)
rt1.ini       PDP-11, normal mode: BOOT TDC0 (SIMH's TDC boot ROM), DIR,
              COPY DUP.SAV TEST.SAV, DIR, UNPROTECT and DELETE, DIR
rt2.ini       the same, realism mode

The VMS tests need a copy of VMS047.R81 (SYSTEM/MANAGER), vms5/vms6 also
the cartridge file in the current directory (they work on copies).

  BIN/vax750 vms1.ini > vms1.out < /dev/null
  SDL_AUDIODRIVER=dummy BIN/vax750 vms2.ini > vms2.out < /dev/null
  python3 -I mkdd_prep.py rtv53_rl.dsk work_rl.dsk
  BIN/pdp11 mkdd.ini < /dev/null
  BIN/pdp11 rt1.ini > rt1.out < /dev/null
  SDL_AUDIODRIVER=dummy BIN/pdp11 rt2.ini > rt2.out < /dev/null

Results (2026-11-12, sound branch 3aff591b)
-------------------------------------------
vms1/vms3   0 differences; CSA1 107 blocks used, 397 free
vms2        0 differences; 982 motions, 139.5 s of tape time, 5 min 12 s
            wall; records of a block 69.9 ms apart (2 slots minus the
            34 ms of a packet on the line), written records 95 ms apart
vms4        0 differences; 1323 motions, 202 s wall (after the clock fix,
            known issues 55: before, consecutive records took 13 s)
vms5        directory of the 750 console cartridge (71 files, 263 blocks);
            VMB.EXE and PCS750.BIN: 0 differences between the two reads;
            720 motions, 96 s of tape time
vms6        5 files, 398 blocks; ENSAA.EXE: 0 differences between the two
            reads
mkdd        rt11v53_dd.dsk: 8 files, 210 blocks, 294 free; BOOT DD0: and
            BOOT TDC0 bring up RT-11SJ V05.03; pad the file to 262,144 bytes
rt1         all steps ok
rt2         all steps ok; 1882 motions, 703 s of tape time; COPY shuttles
            between DUP.SAV (track 0, second pass) and TEST.SAV (start of
            track 1): searches of 12-27 s; an RT-11 boot with a DIR takes
            about 3 minutes (20 commands, mostly searches between distant
            blocks: under the 1:2 interleave a block takes 12.5 in of tape)

Pitfalls found while writing the scripts:
- after SYSGEN CONNECT CONSOLE wait for the next SYSGEN> prompt before
  typing (characters typed earlier are lost; plain Open SIMH the same);
- EXCHANGE takes .BIN files for RT-11 formatted binary
  (%EXCHANGE-W-BINRECFMT): copy them with input/TRANSFER_MODE=BLOCK
  (the qualifier after the input file in VMS V4.7);
- SIMH replaces %name% in SEND strings by environment variables: use * as
  the VMS wildcard in scripts, not %;
- a cartridge file copied from a read-only upload stays read-only: SIMH
  attaches it write-locked (EXCHANGE: "volume is write-locked").
