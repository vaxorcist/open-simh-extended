open-simh-extended/tests/sound/README.txt  v1 (2026-11-07)

Default sound tests (sound patch v14), VAX-11/750 simulator.  Run them
from this directory with an audio output (or SDL_AUDIODRIVER=disk and
SDL_DISKAUDIOFILE=<file> on a host without audio), so the files are
loaded:

    BIN/vax750 tests/sound/d1.ini

d1  SET SOUND ENABLED, SHOW SOUND: system sounds from default_cpu; ATTACH
    of RK07, RL02, RP06, RA60 (empty images): seek sounds from
    default_disk, listed in the "Defaults:" line, no "not found" message.
    Copied with the simulator into a directory without a sound tree, each
    missing default is reported once.
d2  needs own/vax750/running.wav (any WAV file): with SET SOUND PATH=own
    RUNNING comes from the own file, STARTING and STOPPING from
    default_cpu.
