Sounds of the RC25 disk drive (sound/rc25)
==========================================

Used by the MSCP disk controllers (RQ, RQB, RQC, RQD devices) of the
PDP-11 and VAX simulators for RC25 drives when sound is enabled (SET
SOUND ENABLED; see HELP SET SOUND and HELP RQ).  An RC25 drive is one
spindle with two platters: the removable cartridge (unit type RC25) and
the fixed platter (unit type RCF25, the next unit).  Both units use
these sounds; they share one head actuator.

File          Content                                     Use
------------  ------------------------------------------  -------------
rc25-s.wav    short head movement (synthetic)             short seek
rc25-m.wav    medium head movement (synthetic)            medium seek
rc25-l.wav    long head movement (synthetic)              long seek

These are copies of the RK07 seek sounds (sound/rk07) until recordings
of a real RC25 exist.  The files are mono, 16 bit, 16000 Hz WAV.  The
seek class depends on the seek distance: up to RQ register SEEKRC25S
(default 50) cylinders short, up to SEEKRC25M (default 300) medium,
beyond that long (821 cylinders).  A transfer on the current cylinder
makes no sound.

Optional spindle sounds
-----------------------
If files with these names are found (here or in a directory of SET SOUND
PATH or SIMH_SOUND_PATH), they are used for every RC25 drive:

rc25_spinup.wav      spindle starting (SET RQn ONLINE, the Run button),
                     plays once; the real drive takes about 60 s
                     including the air purge and the self-test
rc25_running.wav     spindle running (2850 rpm), loops seamlessly
rc25_spindown.wav    spindle stopping (SET RQn OFFLINE, or an AVAILABLE
                     command with spin-down such as DISMOUNT/UNLOAD),
                     plays once (30 s)

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
