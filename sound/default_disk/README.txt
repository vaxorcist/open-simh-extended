Default disk sounds (sound/default_disk)
========================================

These seek sounds were made by Ulli (vaxorcist); they are synthetic (they
were the RK07 seek sounds).  Every disk drive with sound support uses them
when sound is enabled (SET SOUND ENABLED; see HELP SET SOUND) and the
drive has no seek sounds of its own.

File          Content                                     Use
------------  ------------------------------------------  -------------
seek-s.wav    short head movement (synthetic)             short seek
seek-m.wav    medium head movement (synthetic)            medium seek
seek-l.wav    long head movement (synthetic)              long seek,
                                                          recalibrate

The files are mono, 16 bit, 16000 Hz WAV.

Own sounds of a drive type win over these, file by file: a drive looks
for <dir>/<prefix>-s.wav, -m.wav and -l.wav first (searched like every
sound file: SET SOUND PATH, SIMH_SOUND_PATH, <exe>/sound, ./sound) and
falls back to the file here.  SHOW SOUND lists the defaults in use.

Drive types, their sound directory and prefix, and the seek limits (in
cylinders: up to the first short, up to the second medium, beyond long;
a seek to the current cylinder makes no sound):

Device  Drive        Directory/prefix  Limits (registers, defaults)
------  -----------  ----------------  -------------------------------------
HK      RK06         rk07              SEEK6S/SEEK6M 25/150
HK      RK07         rk07              SEEK7S/SEEK7M 50/300
RL      RL01         rl02              SEEK1S/SEEK1M 15/95
RL      RL02         rl02              SEEK2S/SEEK2M 30/190
RB      RB02         rl02              SEEK02S/SEEK02M 30/190
RB      RB80         rm80              SEEK80S/SEEK80M 35/205
RP      RM02/RM03    rm03              SEEKRM03S/SEEKRM03M 50/300
RP      RP04         rp04              SEEKRP04S/SEEKRP04M 25/150
RP      RP05         rp05              SEEKRP05S/SEEKRP05M 25/150
RP      RM80         rm80              SEEKRM80S/SEEKRM80M 35/205
RP      RP06         rp06              SEEKRP06S/SEEKRP06M 50/300
RP      RM05         rm05              SEEKRM05S/SEEKRM05M 50/300
RP      RP07         rp07              SEEKRP07S/SEEKRP07M 40/230
RQ      RA60         ra60              SEEKRA60S/SEEKRA60M 100/600
RQ      RC25/RCF25   rc25              SEEKRC25S/SEEKRC25M 50/300

Spindle sounds have no default: they must match the drive's rotation
speed and its spin-up and spin-down times.  A drive with a RUN/STOP (or
LOAD) button plays <prefix>_spinup.wav (once), <prefix>_running.wav (a
seamless loop) and <prefix>_spindown.wav (once) from its directory if
they exist (all drives above except the fixed RM80, RP07 and RB80).
tools/make_spindle_sounds.py (open-simh-extended, branch notes)
synthesizes matching sets.

License
-------
Copyright (c) 2026, Ulli (vaxorcist)

Permission is hereby granted, free of charge, to any person obtaining a
copy of these sound files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the author shall not be
used in advertising or otherwise to promote the sale, use or other
dealings in this Software without prior written authorization from the
author.
