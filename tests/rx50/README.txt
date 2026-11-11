open-simh-extended/tests/rx50/README.txt  v1

RX50 tests for the sound realism mode (2026-11-11): RX50 diskettes on the
MSCP controllers (RQ device).

Rules of the realism mode (Ulli, 2026-11-11):
- RX50 drives only on an RQDX1 (RQDX2), RQDX3, RQDX4 or RUX50 controller;
  a RUX50 only connects RX50 drives.
- SET SOUND ENABLED and SET RQ <other controller type> disable RX50 units
  on other controllers, with a message (the default Unibus configuration
  has an RX50 at RQ3 on the UDA50A).  SET RQ RUX50 makes units 0-3 RX50
  units; SET RQ <other type> on a RUX50 gives the units their initial
  drive types back.
- Two RX50 units with consecutive unit numbers (DUA1/DUA2, paired from the
  lowest number up) are the two slots of one drive: one spindle motor, one
  head carriage.

Script   Simulator  Mode     What
-------  ---------  -------  ---------------------------------------------
c1.ini   microvax2  realism  rules on the Qbus: default RQDX3 accepted;
                             SET RQ KDA50 disables the RX50 units; pairs by
                             consecutive unit numbers (1/2 + lone 3, then
                             2/3 after RQ1 becomes an RD54)
c2.ini   vax750     realism  rules on the Unibus: SET SOUND ENABLED
                             disables RQ3 (RX50 on the UDA50A); SET RQ
                             RUX50: RQ0-RQ3 RX50 (drives 0/1, 2/3); an RD54
                             on the RUX50 refused; SET RQ UDA50A: RD54 x 3,
                             RX50 at RQ3 disabled
vms1.ini microvax2  normal   VMS V4.7 (RA81 on a KDA50 = DUA0) with one
                             RX50 drive on the second controller (RQDX3):
                             DUB1 and DUB2 (both slots); INITIALIZE both,
                             MOUNT, COPY SYSGEN.EXE to DUB1, from DUB1 to
                             DUB2, DIFFERENCES, DIRECTORY, DISMOUNT; needs
                             a copy of VMS047.R81
vms2.ini microvax2  realism  the same in the realism mode (SET RQB
                             DEBUG=TRACE to vms2.dbg: one line per RX50
                             transfer with its time); SDL_AUDIODRIVER=dummy
                             is fine

Commands (from a directory with the scripts; the VMS tests with a copy of
VMS047.R81):

  BIN/microvax2 c1.ini
  BIN/vax750 c2.ini
  BIN/microvax2 vms1.ini > vms1.out
  BIN/microvax2 vms2.ini > vms2.out

Results (2026-11-11)
--------------------
- c1/c2: as described above.
- vms1/vms2: "%MOUNT-I-MOUNTED" for both diskettes, SYSGEN.EXE 129 blocks
  on each, "Number of difference sections found: 0", both modes.
- vms2: 126 RX50 transfers, 53 s of drive time in total.  A transfer of
  127 blocks takes 5.7 s (13 tracks, each read in two revolutions because
  of the 2:1 interleave: about 11 KB/s); the first transfer after the
  motor was off takes about 0.6 s (motor start 0.5 s); 15 changes between
  the two slots (the other slot's head unloads).
- Audio (console BOOT DUA1 of a blank RX50, SDL disk driver): door at
  ATTACH; at the boot read the motor starts with the head load; motor,
  diskette and head loops while the motor runs; head unload, door opening
  at DETACH, run-down of 1.5 s.  At the sim> prompt the motor-off and head
  unload timers only fire when the simulator looks at the drive (SHOW,
  continuing, a command): until then the motor keeps humming.
