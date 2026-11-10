open-simh-extended/tests/tk50/README.txt  v3

TK50 tests of the sound realism mode (TQ device, TQK50 on the Qbus,
TUK50 on the Unibus; 2026-11-08), see docs/SIMH_Sound_handover.md,
"Phase D, step 2".

tktest.py v2    VMS V4.7 tests with Ulli's image VMS047-1.R81 (RA81, VHD,
                SYSTEM/MANAGER; not part of the repository):
                  K1  normal mode, MicroVAX II (TQK50): INITIALIZE,
                      BACKUP, BACKUP/LIST, DISMOUNT (regression: nothing
                      may change without SET SOUND ENABLED)
                  K2  realism mode, MicroVAX II: ATTACH -N (a new
                      cartridge), SET TQ0 LOAD (load + 2 min calibration),
                      BACKUP, BACKUP/LIST, DISMOUNT (rewind + unload, the
                      file stays attached), SET TQ0 LOAD again,
                      BACKUP/LIST, DISMOUNT/NOUNLOAD
                  K3  realism mode, VAX-11/750 (TUK50; SET TQ TK50 first,
                      the Unibus VAXen default to the TU81): the K2 tape,
                      load without calibration, BACKUP/LIST, unload, reload
                Usage: python3 -I tktest.py <BIN dir> <VMS047-1.R81> <work dir> [K1|K2|K3 ...]
                AUDIO=1 renders the audio with the SDL disk driver to
                <work dir>/<test>.raw (float mono 44100 Hz).
                THROTTLE=<value> (e.g. 150K) overrides the realistic CPU
                speed in K2/K3: a host too slow for 45 KB/s, so the
                drive stops, backs up and starts again (heard every
                3.5 s at 150K on the MicroVAX II).
                After the login the script waits 40 s: VMS V4.7 starts
                the TMSCP port PTA0 only at the end of the startup, MUA0
                appears about 30 s later.

MKSMALL.COM v1  (v3) VMS command procedure: 1000 test files of one block each
                (@MKSMALL [count] [directory]), for BACKUP of many small
                files (low host rate, many repositions)

Recording SIMH's sound (v2)

Only SIMH, silent, exact levels (SDL disk driver, float32 mono 44100 Hz):

    SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=simh.raw ./microvax2 microvax2.ini
    sox -t raw -r 44100 -e floating-point -b 32 -c 1 simh.raw simh.wav

or

    ffmpeg -f f32le -ar 44100 -ac 1 -i simh.raw simh.wav

While listening (PipeWire or PulseAudio), terminal 1:

    parecord --device=@DEFAULT_MONITOR@ --file-format=wav simh.wav

terminal 2:

    ./microvax2 microvax2.ini

then Ctrl/C in terminal 1.
