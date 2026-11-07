Default tape sounds (sound/default_tape)
========================================

These tape transport sounds are synthetic: there are no recordings of a
real drive.  They model a TS11 (TS04 transport: capstan, tension arms, AC
reel motors) and were made by tools/make_tape_sounds.py (open-simh-extended,
branch notes).  Every tape transport with sound support uses them when
sound is enabled (SET SOUND ENABLED; see HELP SET SOUND) and the transport
has no sound of its own.

File               Content                                  Use
-----------------  ---------------------------------------  -------------------
load.wav           relay, tension arms, leader to BOT, 8 s  load sequence
unload.wav         leader wound onto the supply reel, 8 s   unload (from BOT)
rewind_start.wav   relay, reels running up, 1.2 s           start of a rewind
rewind.wav         one reel motor at 8 rev/s, 2 s loop      rewind, played once
                                                            per reel; the pitch
                                                            follows the reel
rewind_stop.wav    run-down, BOT overshoot, creep, stop     end of a rewind
start.wav          capstan kick, tension arm, 60 ms         start of a record
motion.wav         tape at 45 in/s, 1 s loop                record, spacing
stop.wav           capstan stop, 60 ms                      end of a record

load.wav and unload.wav are mono, 16 bit, 22050 Hz WAV; the others 44100 Hz.

The rewind loop is played at the rate (reel rotation in rev/s) / 8 and
glides with the reel radius (sim_sound SND_GLIDE_REEL), so one reel rises
in pitch while the other falls.  A one-shot that is longer than the
transport takes (e.g. LOADTIME shorter than 8 s) is faded out at the end
of the action.

Own sounds of a transport type win over these, file by file: a transport
looks for <dir>/<prefix>_<name>.wav first (searched like every sound file:
SET SOUND PATH, SIMH_SOUND_PATH, <exe>/sound, ./sound) and falls back to
<name>.wav here.  SHOW SOUND lists the defaults in use.

Device  Transport          Directory/prefix  Own files bundled
------  -----------------  ----------------  ---------------------------------
TS      TS11 (Unibus)      ts11              none (uses these)
TS      TSV05 (Qbus)       tsv05             load, unload, motion, start, stop

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
