Sounds of the RA60 disk drive (sound/ra60)
==========================================

Used by the MSCP disk controllers (RQ, RQB, RQC, RQD devices) of the
PDP-11 and VAX simulators for RA60 drives when sound is enabled (SET
SOUND ENABLED; see HELP SET SOUND and HELP RQ).

File          Content                                     Use
------------  ------------------------------------------  -------------
ra60-s.wav    short head movement (synthetic)             short seek
ra60-m.wav    medium head movement (synthetic)            medium seek
ra60-l.wav    long head movement (synthetic)              long seek

These are copies of the RK07 seek sounds (sound/rk07) until recordings
of a real RA60 exist.  The files are mono, 16 bit, 16000 Hz WAV.  The
seek class depends on the seek distance: up to RQ register SEEKRA60S
(default 100) cylinders short, up to SEEKRA60M (default 600) medium,
beyond that long (1600 cylinders).  A transfer on the current cylinder
makes no sound.

Optional spindle sounds
-----------------------
If files with these names are found (here or in a directory of SET SOUND
PATH or SIMH_SOUND_PATH), they are used for every RA60 drive:

ra60_spinup.wav      spindle starting (SET RQn ONLINE), plays once
ra60_running.wav     spindle running, loops seamlessly
ra60_spindown.wav    spindle stopping (SET RQn OFFLINE, or an AVAILABLE
                     command with spin-down such as DISMOUNT/UNLOAD),
                     plays once

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
