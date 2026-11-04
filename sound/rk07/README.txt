Sounds of the RK06/RK07 disk drives (sound/rk07)
=================================================

These sounds were made by Ulli (vaxorcist).  They are used by the RK611
controller (HK device) of the PDP-11 and VAX simulators when sound is
enabled (SET SOUND ENABLED; see HELP SET SOUND and HELP HK).  The RK06
uses the same sounds.

File          Content                                     Use
------------  ------------------------------------------  -------------
rk07-s.wav    short head movement (synthetic)             short seek
rk07-m.wav    medium head movement (synthetic)            medium seek
rk07-l.wav    long head movement (synthetic)              long seek,
                                                          recalibrate

The files are mono, 16 bit, 16000 Hz WAV.  The seek class depends on the
seek distance: up to HK register SEEK7S (default 50) cylinders short, up
to SEEK7M (default 300) medium, beyond that long (RK06: SEEK6S/SEEK6M,
default 25/150).  A seek to the current cylinder makes no sound.

Optional spindle sounds
-----------------------
No spindle recordings exist yet.  If files with these names are found
(here or in a directory of SET SOUND PATH or SIMH_SOUND_PATH), they are
used for every drive:

rk07_spinup.wav      spindle starting (SET HKn ONLINE), plays once
rk07_running.wav     spindle running, loops seamlessly
rk07_spindown.wav    spindle stopping (SET HKn OFFLINE, UNLOAD), plays once

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
