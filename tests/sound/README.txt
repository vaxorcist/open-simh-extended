open-simh-extended/tests/sound/README.txt  v2 (2026-11-07; v2: SET REALISM rename, realism_cmd.ini, enable_dryrun.ini)

Default sound tests (sound patch v14), VAX-11/750 simulator.  Run them
from this directory with an audio output (or SDL_AUDIODRIVER=disk and
SDL_DISKAUDIOFILE=<file> on a host without audio), so the files are
loaded:

    BIN/vax750 tests/sound/d1.ini

d1  SET REALISM ENABLED, SHOW SOUND: system sounds from default_cpu; ATTACH
    of RK07, RL02, RP06, RA60 (empty images): seek sounds from
    default_disk, listed in the "Defaults:" line, no "not found" message.
    Copied with the simulator into a directory without a sound tree, each
    missing default is reported once.
d2  needs own/vax750/running.wav (any WAV file): with SET SOUND PATH=own
    RUNNING comes from the own file, STARTING and STOPPING from
    default_cpu.

Realism mode console tests (2026-11-17, patch 13), VAX 8200 simulator, no
audio output or disk image needed; the expected output is in each file:

    BIN/vax8200 tests/sound/realism_cmd.ini
    BIN/vax8200 tests/sound/enable_dryrun.ini

realism_cmd    SET REALISM ENABLED/DISABLED, SHOW REALISM, SET SOUND
               ENABLED/DISABLED as the older synonym, abbreviations, the
               error messages, HELP SET REALISM
enable_dryrun  known issues 76: a SET REALISM ENABLED refused by one check
               (TB a TK70) leaves the units of another (RQ, SDI rule) as
               they were
