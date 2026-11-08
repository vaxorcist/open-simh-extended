open-simh-extended/tests/ts11/README.txt  v2

TS11/TSV05 tests of the sound realism mode (2026-11-07), see
docs/SIMH_Sound_handover.md, "Phase D, step 1".

tstest.py v1    VMS V4.7 tests with Ulli's image VMS047-1.R81 (RA81, VHD,
                SYSTEM/MANAGER; not part of the repository):
                  N1  normal mode, VAX-11/750 (TS11)
                  R1  realism mode, VAX-11/750: load, BACKUP, LIST, unload,
                      reload
                  E1  realism mode, VAX-11/750, 600 ft reel: physical EOT,
                      BACKUP asks for volume 2
                  P1  realism mode, VAX-11/750: a guest rewind goes on in
                      wall time at the sim> prompt
                  Q1  realism mode, MicroVAX II (TSV05)
                Usage: python3 -I tstest.py <BIN dir> <VMS047-1.R81> <work dir> [tests]
                AUDIO=1 renders the audio with the SDL disk driver to
                <work dir>/<test>.raw (float mono 44100 Hz).
s1.ini v1       panel commands, vax750, realism mode (no OS)
p2.ini v1       panel commands, pdp11 (11/70, TS11), normal and realism mode
sp1.ini v1      spindle regression after the SND_TIMER refactoring (vax750)
broken_saveset.py v1  interrupted BACKUP, then BACKUP without /REWIND
                (MicroVAX II): POSITERR/DRVERR in both modes (OPI)
