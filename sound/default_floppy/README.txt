Default floppy sounds (sound/default_floppy)
============================================

These floppy drive sounds are synthetic: there are no recordings of a real
drive yet.  They model a generic 5.25" drive (brushless DC direct-drive
motor at 300 rpm, a light head load solenoid and stepper) and were made by
tools/make_floppy_sounds.py (open-simh-extended, branch notes).  Every
floppy drive with sound support uses them when sound is enabled (SET SOUND
ENABLED; see HELP SET SOUND) and the drive has no sound of its own.

File             Content                                     Use
---------------  ------------------------------------------  ------------------
motor_start.wav  the spindle motor running up, 0.5 s         motor on
motor.wav        the motor at speed, 2 s loop                motor running
motor_stop.wav   the motor running down, 1 s                 motor off
disk.wav         a diskette turning in its jacket, 2 s loop  diskette in and
                                                             the spindle turning
head.wav         the head on the turning diskette, 2 s loop  head loaded
head_load.wav    head load solenoid pulling in               head load
head_unload.wav  the solenoid releasing                      head unload
step.wav         one stepper step (one track), 2.8 ms        repeated at the
                                                             step rate
door_close.wav   cover closed and latched                    ATTACH
door_open.wav    latch released, cover springs open          DETACH

All files are mono, 16 bit, 44100 Hz WAV.  The loops are seamless.

Own sounds of a drive type win over these, file by file: a drive looks for
<dir>/<prefix>_<name>.wav first (searched like every sound file: SET SOUND
PATH, SIMH_SOUND_PATH, <exe>/sound, ./sound) and falls back to <name>.wav
here.  SHOW SOUND lists the defaults in use.  To silence one sound, put a
short silent WAV file of that name into a directory of SET SOUND PATH.

Device  Drive        Directory/prefix  Own files bundled
------  -----------  ----------------  ------------------------------------
RY      RX02         rx02              all ten (8", AC motor and fan)

Copyright (c) 2026, Ulli (vaxorcist)

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the author shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the author.
