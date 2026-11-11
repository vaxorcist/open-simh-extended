open-simh-extended/tests/rx02/README.txt  v2

RX02 (RY device) tests for the sound realism mode (2026-11-11).

The scripts expect, in the current directory:
  rt.rx2      a copy of Ulli's RT11.RX2 (RT-11SB V05.07, double density;
              not part of the repository; the scripts write to it)
  ../../sound the sound tree of the sound branch (SET SOUND PATH); in a
              checkout of the sound branch run them from tests/rx02 there,
              or change the PATH line.
Audio can be recorded with SDL's disk driver (real time):
  SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=a1.raw BIN/pdp11 a1.ini </dev/null
(32-bit float mono 44100 Hz; do not set SDL_DISKAUDIODELAY=0: the audio
thread then runs as fast as it can).

Script   Simulator  Mode     What
-------  ---------  -------  ---------------------------------------------
n1.ini   pdp11      normal   RT-11 boot from DY0, DIR, SHOW (output must be
                             identical to plain Open SIMH)
r1.ini   pdp11      realism  RT-11 boot, DIR, COPY of HELP.SAV (161 blocks),
                             UNPROTECT, DELETE, DIR; SET RY DEBUG=OPS
                             trace in r1.dbg (one line per command and time)
a1.ini   pdp11      realism  audio check: boot, DIR, 2 s at the prompt (head
                             unload), DETACH (door), power off (run-down)
a2.ini   pdp11      realism  as a1 with silent motor/disk/head loops from
                             silent/rx02/ (make them: 0.1 s of silence)
t1.ini   pdp11      realism  one Read Sector by a small program; SHOW RY0
                             DRIVE shows the head unload timer
t2.ini   pdp11      realism  DETACH during a Read Sector: error 0110, the
                             simulator keeps running
sd0.ini  pdp11      normal   Set Media Density on an empty drive (crashed
                             before the fix: known issues 45)
sd1.ini  pdp11      both     Set Media Density: empty drive (normal and
                             realism), new double-density diskette in drive 1
                             (15.3 s), SET SOUND DISABLED refused while
                             attached
sd2.ini  pdp11      normal   the formatted new image stays an empty file
                             (known issues 46)
v1.ini   vax750     realism  RY at the VAX-11/750 console: drive states,
                             power on/off
vms2.ini vax750     normal   VMS V4.7: INITIALIZE double (DYA0) and single
                             (DYA1), MOUNT, COPY, DIRECTORY, DISMOUNT; needs
                             a copy of VMS047.R81 (known issues 47: MOUNT
                             failed with DRVERR on double density)
vms3.ini vax750     realism  as vms2.ini in the sound realism mode

Results (2026-11-11): see docs/SIMH_Sound_handover.md (v30), floppy section.
tools/rt11_dir_rx02.py lists the RT-11 directory of an RX02 image (it
decodes the DY handler's 2:1 interleave with a 6-sector skew per track).

VMS test (interactive, for Ulli; vms2.ini/vms3.ini run the same automatically):
  BIN/vax750 tests/rx02/vms1.ini     (from a directory with VMS047-1.R81)
then, logged in as SYSTEM:

SHOW DEVICE DY
INITIALIZE/DENSITY=DOUBLE DYA0: RX02T
MOUNT DYA0: RX02T
COPY SYS$SYSTEM:LOGINOUT.EXE DYA0:[000000]
DIRECTORY/SIZE/DATE DYA0:[000000]
DISMOUNT DYA0:

Expected: INITIALIZE runs Set Media Density (about 15 s, the head steps
once per revolution: a slow ticking) and writes the volume structure;
MOUNT, COPY and DIRECTORY step and clack audibly; vms1.dbg has one line
per RX02 command with its time.  Then at the sim> prompt (Ctrl/E):
DETACH RY0 (the door) and CONTINUE; a further MOUNT DYA0: should fail
(drive not ready: RYES drive ready is clear without a diskette), without
stopping the simulator.  (The "! rm -f" line in vms1.ini is a host command
for Linux/macOS; on Windows delete dyt.rx2 by hand.)
