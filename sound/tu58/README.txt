TU58 DECtape II drive sounds (sound/tu58)
=========================================

These sounds of a TU58 drive are synthetic: there are no recordings of a
real drive yet.  They were made by tools/make_tu58_sounds.py v1
(open-simh-extended, branch notes) after the TU58 DECtape II User's Guide
(EK-0TU58-UG-004):

- one servo-controlled DC motor drives the tape through a capstan puck on
  the cartridge's drive roller; an elastomer belt in the cartridge drives
  the two hubs;
- read and write at 30 in/s, search at 60 in/s; the tape stops after each
  command, and a search reverses the tape when the record lies behind;
- the cartridge slides into the drive (no door).

The PDP-11 TDC device and the console TU58 (TD) of the VAX-11/730 and
VAX-11/750 play them in the sound realism mode (SET SOUND ENABLED).

File               Content                                       Use
-----------------  --------------------------------------------  ------------
tu58_read.wav      the tape at 30 in/s, 2 s loop                 read, write
tu58_search.wav    the tape at 60 in/s, 2 s loop                 search
tu58_start.wav     the servo kicking the tape up, 80 ms          start
tu58_stop.wav      braking to a stand, 100 ms                    command end
tu58_reverse.wav   braking, direction change, running up, 150 ms turnaround
tu58_insert.wav    the cartridge slid in and seated              ATTACH
tu58_remove.wav    the cartridge pulled out                      DETACH

All files are mono, 16 bit, 44100 Hz WAV; the loops are seamless.  If a
file is missing, the TU58 uses sound/default_tape (motion.wav for read
and, at twice the rate, for search; start, stop, load and unload).

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
