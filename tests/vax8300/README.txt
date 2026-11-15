open-simh-extended/tests/vax8300/README.txt  v1

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
mp2.ini   realism mode (SET SOUND ENABLED, THROTTLE 400K per processor):
          a MACRO-32 loop of 4,000,000 SOBGTR alone without and with the
          attached processor (10.6 s each), then two at once, one in a
          subprocess (12-13 s; one after the other they need 21 s);
          SHOW CPU during the run: EXECUTE; then STOP/CPU and START/CPU.
          About 10 minutes.
stab8200.ini  single processor regression: boot standalone BACKUP from the
          console RX50 (three diskettes) to the $ prompt.

Run, e.g.:  SDL_AUDIODRIVER=dummy BIN/vax8200 mp2.ini > mp2.out < /dev/null

Note: send at most a few DCL commands per expect action; a long type-ahead
overruns the VMS terminal buffer (%RMS-F-RER, DATAOVERUN).
