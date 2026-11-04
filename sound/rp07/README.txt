Sounds of the RP07 disk drive (sound/rp07)
=======================================

Used by the Massbus disk controller (RP device) of the PDP-11 and VAX
simulators when sound is enabled (SET SOUND ENABLED; see HELP SET SOUND
and HELP RP).

File          Content                                     Use
------------  ------------------------------------------  -------------
rp07-s.wav    short head movement (synthetic)             short seek
rp07-m.wav    medium head movement (synthetic)            medium seek
rp07-l.wav    long head movement (synthetic)              long seek,
                                                          recalibrate

These are copies of the RK07 seek sounds (sound/rk07) until recordings
of a real RP07 exist.  The files are mono, 16 bit, 16000 Hz WAV.  The
seek class depends on the seek distance: up to RP register SEEKRP07S
(default 40) cylinders short, up to SEEKRP07M (default 230) medium,
beyond that long (630 cylinders).  A seek to the current cylinder makes
no sound.

The RP07 is a fixed disk: it has no RUN/STOP button in the simulator,
so no spindle sounds are used.

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
