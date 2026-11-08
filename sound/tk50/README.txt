TK50 tape sounds (sound/tk50)
=============================

Synthetic sounds of the TK50 cartridge streamer (also the TZ30): 75 in/s,
22 serpentine tracks, a 32 KB buffer, start/stop with repositioning.  Made
by tools/make_tape_sounds.py v2 (open-simh-extended, branch notes), shaped
after measurements of a TZ30 recording (pitch 457 Hz with harmonics,
octave band levels, durations of the 3 s reposition cycle); the recording
itself is not included.  Used by the TK50 in the sound realism mode
(PDP11/pdp11_tq.c, SND_STREAMER in sim_sound.c).

File                Content                                      Use
------------------  -------------------------------------------  -------------
tk50_load.wav       solenoid, leader buckling, slow wind to BOT, SET TQn LOAD
                    calibration track read forward/reverse,      (cut at the
                    16 s                                         random 10-15 s)
tk50_calibrate.wav  forward and reverse calibration runs,        new cartridge,
                    8 s loop                                     2 min
tk50_unload.wav     leader wound in, unbuckling, solenoid, 9 s   unload after
                                                                 the rewind
tk50_start.wav      start whoosh, 0.32 s                         streaming start
tk50_forward.wav    streaming forward, 457 Hz tone, 1 s loop     read/write/space
tk50_stop.wav       braking, dip, settling knock, 0.4 s          stop
tk50_rstart.wav     reverse start, 0.32 s                        reposition,
                                                                 space reverse
tk50_reverse.wav    reverse, 457/501 Hz beating, 1 s loop        reposition,
                                                                 rewind (glided)
tk50_rstop.wav      reverse stop, 0.3 s                          reposition end

load, calibrate and unload are mono, 16 bit, 22050 Hz WAV; the others
44100 Hz.

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
