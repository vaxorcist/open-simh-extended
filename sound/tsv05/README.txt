TSV05 tape sounds (sound/tsv05)
===============================

Synthetic sounds of the TS05 transport of the TSV05 (Qbus): reel-to-reel
without a capstan, a compliance arm, streaming at 25 in/s, automatic
threading by air flow.  Made by tools/make_tape_sounds.py (open-simh-extended,
branch notes); there are no recordings of a real drive.

File               Content                                       Use
-----------------  --------------------------------------------  -------------
tsv05_load.wav     door lock, reel seated, blower threading the  load (30 s,
                   leader, run to BOT, 30 s                      the manual's
                                                                 typical time)
tsv05_unload.wav   leader wound onto the reel, door unlock, 15 s unload from BOT
tsv05_motion.wav   reel motors at 25 in/s, 1 s loop              streaming
tsv05_start.wav    start from rest, 80 ms                        access
tsv05_stop.wav     stop after the tape coasted through a gap     stop

load and unload are mono, 16 bit, 22050 Hz WAV; the others 44100 Hz.  The
rewind sounds (rewind_start, rewind, rewind_stop) come from default_tape.

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
