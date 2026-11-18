open-simh-extended/tests/vax8300/README.txt  v4 (v4: smp55.ini, smp55_rep.sh (known issues 81); v3: SET REALISM rename; v2: reboot1.ini)

Tests of the second KA820 (patch 7, VAX 8300: SET KA1 ENABLE).

They need the vax8200 simulator of the integration branch (patches 1-7)
and, in the current directory:
  - VMS047_1.R81: a copy of Ulli's VAX/VMS V4.7 RA81 image with the VMS/8300
    multiprocessing key (SYSTEM/MANAGER), for mp2.ini and mp3.ini;
  - k1.rx50, k2.rx50, k3.rx50: copies of a VAX 8200 STABACKIT kit
    (SYSTEM_1, SYSTEM_2, BACKUP), for stab8200.ini.
The scripts enable multiprocessing in VMS with
  DEFINE/SYSTEM/EXEC MP MP8SS
  COPY SYS$SYSTEM:MP_8SS.EXE SYS$SYSTEM:MP8SS.EXE
(the kit has the image as MP_8SS.EXE).

mp3.ini   normal mode: START/CPU, SHOW CPU (IDLE), STOP/CPU, SHOW CPU
          (VMS V4.7: DEVOFFLINE while stopped), START/CPU, SHOW CPU (IDLE).
          About 1 minute.
mp2.ini   realism mode (SET REALISM ENABLED, THROTTLE 400K per processor):
          a MACRO-32 loop of 4,000,000 SOBGTR alone without and with the
          attached processor (10.6 s each), then two at once, one in a
          subprocess (12-13 s; one after the other they need 21 s);
          SHOW CPU during the run: EXECUTE; then STOP/CPU and START/CPU.
          About 10 minutes.
reboot1.ini  realism mode, SET CPU CONHALT: SHUTDOWN with automatic reboot
          while the attached processor runs; VMS stops it, halts, SIMH
          reboots ("Rebooting..."), VMS asks for the date again.  Without
          SET CPU CONHALT (default SIMHALT) the HALT stops the simulator.
stab8200.ini  single processor regression: boot standalone BACKUP from the
          console RX50 (three diskettes) to the $ prompt.

Run, e.g.:  SDL_AUDIODRIVER=dummy BIN/vax8200 mp2.ini > mp2.out < /dev/null

Note: send at most a few DCL commands per expect action; a long type-ahead
overruns the VMS terminal buffer (%RMS-F-RER, DATAOVERUN).

smp55.ini, smp55_rep.sh (2026-11-18, known issues 81, patch 15): VMS V5.5
SMP on the 8300 in Ulli's configuration (128MB, VAXBI-only), boot, login,
SYSGEN HELP PARAMETERS.  Run it slow, several times, e.g. four runs at
THROTTLE 400K (about 10 minutes each):

    IMG=<copy of the V5.5 RA82 image> ./smp55_rep.sh <path>/BIN/vax8200 t400 4 3000 "set throttle 400k"

Each line should end in "*** h1 done"; before patch 15 about half ended in
"FATAL BUG CHECK ... SPLRELERR" or "Exception in interrupt or exception".
