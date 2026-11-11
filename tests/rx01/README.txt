open-simh-extended/tests/rx01/README.txt  v1

RX01 tests for the sound realism mode (2026-11-11): the RX device (RX11/
RXV11 with two RX01 drives) of the PDP-11 and the console floppy (CS
device, the RX01 of the console LSI-11) of the VAX-11/780.

There is no RX01 diskette with an operating system here (the RT-11 RX02
diskette has no DX handler), so the PDP-11 tests use small programs and a
generated test image, and the console floppy is tested under VMS V4.7.

Files
-----
mkrx01.py   makes the test image: python3 -I mkrx01.py t.rx1
            (every sector holds its track and sector number; track 1
            sector 1 holds MOV #123,R0; HALT for BOOT RX0)
prog.ini    do-file: a program that reads track 1 of drive 0 in the 2:1
            interleave of the RT-11 DX handler (1, 3, ..., 25, 2, ..., 26),
            Read Sector then Empty Buffer into memory from 2000 on

Script   Simulator  Mode     What
-------  ---------  -------  ---------------------------------------------
n1.ini   pdp11      normal   BOOT RX0 (R0 = 123), prog.ini, data checks;
                             output identical to plain Open SIMH 87eb7d5
r1.ini   pdp11      realism  the same with SET RX DEBUG=OPS;CMD to r1.dbg:
                             one line per command with its realistic time
t2.ini   pdp11      both     (a) DETACH while the head steps to track 48:
                             error 0110, the simulator keeps running;
                             (b) Read Status: 1-2 revolutions, RXES 200;
                             (c) Read Sector on the empty drive 1: error
                             0110 after 833 ms (48 steps, head load, two
                             revolutions); (d) normal mode: the same stops
                             the simulator (STOP_IOE), as before
vms4.ini vax780     normal   VMS V4.7: SYSGEN CONNECT CONSOLE, MOUNT/FOREIGN
                             CSA1, EXCHANGE INITIALIZE (RT-11 volume),
                             EXCHANGE COPY SYSGEN.EXE (129 blocks) to the
                             console floppy and back, DIFFERENCES (none),
                             DISMOUNT; needs a copy of VMS047.R81
vms5.ini vax780     realism  the same in the sound realism mode
                             (SDL_AUDIODRIVER=dummy is fine); SET CS
                             DEBUG=OPS to vms5.dbg

Commands (from a directory with t.rx1, prog.ini and the scripts; the VMS
tests with a copy of VMS047.R81):

  python3 -I mkrx01.py t.rx1
  BIN/pdp11 n1.ini
  BIN/pdp11 r1.ini
  BIN/pdp11 t2.ini
  BIN/vax780 vms4.ini > vms4.out
  BIN/vax780 vms5.ini > vms5.out

Results (2026-11-11)
--------------------
- n1: boot R0 = 000123; 2000 = 012700 (the boot sector), 2200 = 001401,
  2400 = 002401, 10200 = 015001; identical to plain Open SIMH.
- r1: Initialize 140 ms; the boot ROM's read of track 1 sector 1 right
  after it waits a whole revolution (166.6 ms: the sector has just passed);
  the interleaved reads take 9.68 ms each, one every 12.82 ms (two sector
  times: 9.68 ms + the Empty Buffer 128 x 18 us + the program), track 1 in
  0.33 s; the head unloads 46 ms after the last command.
- t2: (a) RXCS 100046, RXERR 110; (b) 251 ms, R0 = 000200; (c) RXCS
  100066, RXERR 110 after 833.33 ms; (d) "Unexpected I/O error ... RX1 -
  Unit not attached", as plain Open SIMH.
- vms4/vms5: "%EXCHANGE-S-INITIALIZED", SYSGEN.EXE 129 blocks, Free space
  357 blocks, "Number of difference sections found: 0" in both modes.
  vms5: 1132 console floppy operations, VMS uses a 2:1 interleave with a
  6-sector skew per track; a read normally takes 12.7 ms (two sector
  times), the whole exchange about 22 s.
- Audio (pdp11, SDL disk driver): the step clicks come every 10.0 ms; the
  power-up homing (20 clicks) when the 2 s motor run-up ends; SHOW SOUND
  "Defaults: rx01 (floppy) -> rx02".
