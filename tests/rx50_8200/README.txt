open-simh-extended/tests/rx50_8200/README.txt  v3 (v3: physical track 0 crash, known issues 54: t0, stab1-3; v2: VMS tests vms1/vms2)

VAX 8200 console floppy (CS device, an RX50 drive with two slots) in the
sound realism mode (2026-11-11).

Files
-----
mkprog.py   (v2: optional arguments slot,track,sector ...) writes prog.ini: a VAX program at 1000 that drives the console
            floppy controller registers (200B0000) directly: read sector
            for a list of (slot, track, sector), polling CS0 Done
prog.ini    the do-file made by mkprog.py
vms1.ini    VMS V4.7 on the vax8200, normal mode: CSA1/CSA2 (CS0/CS1, new
            blank diskettes), SYSGEN CONNECT CONSOLE, MOUNT/FOREIGN, EXCHANGE
            INITIALIZE/COPY/DIRECTORY, DIFFERENCES, DISMOUNT
vms2.ini    the same in the sound realism mode (SET SOUND ENABLED, SET RQ0
            ONLINE)
t0prog.ini  made by "mkprog.py 0,0,1": read track 0 sector 1 of slot A
t0.ini      vax8200, normal mode: the track 0 read on an image that has
            its block number in every block (known issues 54)
stab1.ini   VMS V4.7 @SYS$UPDATE:STABACKIT CSA1: (Ulli's case): three new
            diskettes, copy verification, normal mode
stab2.ini   microvax2: VMS reads the SYSTEM_1 diskette of stab1 as an RX50
            on an RQDX3 (DUB1): ANALYZE, DUMP/HEADER, DIFFERENCES
stab3.ini   STABACKIT as stab1, realism mode
t1.ini      vax8200, realism mode: runs the program with SET CS DEBUG=FNC
            (one line per command with its realistic time), then SHOW CSn
            DRIVE before and after 3 s at the prompt

Commands (in a directory with prog.ini and t1.ini):

  python3 -c "open('a.rx50','wb').write(bytes(409600))"
  python3 -c "open('b.rx50','wb').write(bytes(409600))"
  BIN/vax8200 t1.ini

Results (2026-11-11)
--------------------
  slot A track 40 sector 3     620.4 ms  motor start 0.5 s, 40 steps, head load
  slot A track 40 sector 5      40.0 ms  every second sector: two sector times
  slot A track 40 sector 7      40.0 ms
  slot A track 40 sector 9      40.0 ms
  slot A track 40 sector 10    220.0 ms  the very next sector: just missed
  slot A track 40 sector 9     180.0 ms  back one sector
  slot B track 40 sector 7     101.2 ms  no steps (one carriage), A unloads,
                                         B loads (own diskette phase)
  slot B track 10 sector 1     280.0 ms  30 steps + settle
  slot A track 79 sector 10    638.8 ms  69 steps, B unloads, A loads

After the program: both slots "motor running (off in 2.00 s)"; after 3 s
at the prompt: heads unloaded, "motor off" for both (one motor).

VMS tests (2026-11-11)
----------------------
Need the integration build (patch 3) and a copy of VMS047.R81 (SYSTEM /
MANAGER) in the current directory:

  BIN/vax8200 vms1.ini > vms1.out < /dev/null
  SDL_AUDIODRIVER=dummy BIN/vax8200 vms2.ini > vms2.out < /dev/null

Both modes: CSA1: and CSA2: online, both diskettes initialized as RT-11
volumes, SYSGEN.EXE (129 blocks) copied disk -> CSA1 -> CSA2 -> disk with no
differences, directory 1 file / 657 blocks free on each.  Realism: 576 sector
operations, average 63 ms, one motor start (first read 606 ms); the whole run
takes 78 s.  The RT-11 file date reads 1994 for 2026: VMS V4.7 EXCHANGE
stores the year modulo 32 (a VMS limit).

Physical track 0 (2026-11-12, known issues 54)
----------------------------------------------
  python3 -c "b=bytearray(409600)
  for n in range(800): b[n*512]=n&255; b[n*512+1]=n>>8
  open('num.rx50','wb').write(b)"
  BIN/vax8200 t0.ini < /dev/null
      plain Open SIMH and the old build: segmentation fault (exit 139);
      fixed: DBUF[0..1] = 17 03 = block 791 (logical track 79, skew 8)
  BIN/vax8200 stab1.ini > stab1.out < /dev/null         (integration build)
      old build: segmentation fault after "Copying the first 136 blocks of
      SYS.EXE to SYSTEM_1"; fixed: "The kit is complete.", 47 x
      %STABACKIT-S-VERIFIED, volumes SYSTEM_1, SYSTEM_2, BACKUP
  BIN/microvax2 stab2.ini > stab2.out < /dev/null       (needs stab1_1.rx50)
      SYSTEM_1 mounted on DUB1; VMB.EXE: no differences; SYS.EXE's first
      extent is LBN 727-799; its blocks 790-799 (physical track 0) are
      identical to the copy STABACKIT verified (host-side comparison);
      DIFFERENCES of SYS.EXE differs in record 1 only as far as shown (a
      partial copy: STABACKIT splits SYS.EXE over SYSTEM_1 and SYSTEM_2)
  SDL_AUDIODRIVER=dummy BIN/vax8200 stab3.ini > stab3.out < /dev/null
      realism mode: complete, 47 verified copies, 5.5 min of VMS time, 20
      operations on track 0
