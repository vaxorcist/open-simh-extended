open-simh-extended/tests/rl02_8600/README.txt  v1

VAX 8600 console RL02 (CS device) in the sound realism mode (2026-11-12).
The console RL02 is an RL02 on the console processor's RLV12; the VAX
reaches it through STXCS/STXDB, one 512-byte block per transfer, moved a
word at a time.  VMS calls it CSA1 (SYSGEN CONNECT CONSOLE).

Files
-----
c1.ini      vax8600 at the sim> prompt: SET CS ONLINE without a cartridge
            (refused), ATTACH (realism: "offline"), SET SOUND DISABLED
            refused while CS is attached, ONLINE waits (SPINUP 2000),
            -N OFFLINE / OFFLINE waits (SPINDOWN 1500), -N LOAD, DETACH;
            normal mode: OFFLINE/ONLINE at once
vms1.ini    VMS V4.7 on the vax8600, normal mode: SYSGEN CONNECT CONSOLE,
            MOUNT/FOREIGN CSA1, EXCHANGE INITIALIZE (RT-11), COPY
            SYSGEN.EXE and LOGINOUT.EXE to CSA1, SYSGEN.EXE back,
            DIFFERENCES, EXCHANGE DIRECTORY, DISMOUNT
vms2.ini    the same in the realism mode (SET RQ0 ONLINE, SET CS ONLINE:
            the RA81 and the RL02 spin up at the prompt, 20 s and 40 s)
vms3.ini    realism mode, the LOAD button under VMS: MOUNT/FOREIGN of the
            stopped drive asks the operator, SET -N CS ONLINE from the
            expect action, the MOUNT completes; releasing the button under
            the mounted volume, EXCHANGE DIRECTORY fails; pressing it again,
            DIRECTORY works

The VMS tests need a copy of VMS047.R81 (SYSTEM/MANAGER) in the current
directory; the console cartridge is created new (blank).

  BIN/vax8600 c1.ini < /dev/null
  BIN/vax8600 vms1.ini > vms1.out < /dev/null
  SDL_AUDIODRIVER=dummy BIN/vax8600 vms2.ini > vms2.out < /dev/null
  SDL_AUDIODRIVER=dummy BIN/vax8600 vms3.ini > vms3.out < /dev/null

Results (2026-11-12)
--------------------
c1      as described; "SET SOUND DISABLED is only allowed while no disk or
        tape is attached (CS is attached)"
vms1    CSA1: Online; initialized, 2 files / 236 blocks, 20176 blocks
        free; DIFFERENCES: 0 records; 21 s for the whole run; the same on
        the integration build
vms2    the same results; 454 block transfers, average 25.3 ms, longest
        38.2 ms (about one revolution per block: the VAX moves each block
        word by word, so the next block's sectors have passed); 20 short
        seek sounds; 121 s, of which 60 s are the spin-ups at the prompt
vms3    "%MOUNT-I-OPRQST, Please mount device _CSA1:", after the LOAD
        button "%MOUNT-I-RQSTDON, operator request canceled - mount
        completed successfully"; with the drive stopped
        "%EXCHANGE-W-READERR, error reading _CSA1:[].;"; after pressing it
        again the directory lists LOGINO.EXE

Note: the 40 s spin-up appeared as about 25 s of VMS time.  The vax8600
has no realistic THROTTLE default, so realistic durations are converted
with SIMH's estimate of the instruction rate, which lags on an unthrottled
CPU (see the handover).
