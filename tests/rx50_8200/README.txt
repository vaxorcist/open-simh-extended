open-simh-extended/tests/rx50_8200/README.txt  v2 (v2: VMS tests vms1/vms2)

VAX 8200 console floppy (CS device, an RX50 drive with two slots) in the
sound realism mode (2026-11-11).

Files
-----
mkprog.py   writes prog.ini: a VAX program at 1000 that drives the console
            floppy controller registers (200B0000) directly: read sector
            for a list of (slot, track, sector), polling CS0 Done
prog.ini    the do-file made by mkprog.py
vms1.ini    VMS V4.7 on the vax8200, normal mode: CSA1/CSA2 (CS0/CS1, new
            blank diskettes), SYSGEN CONNECT CONSOLE, MOUNT/FOREIGN, EXCHANGE
            INITIALIZE/COPY/DIRECTORY, DIFFERENCES, DISMOUNT
vms2.ini    the same in the sound realism mode (SET SOUND ENABLED, SET RQ0
            ONLINE)
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
