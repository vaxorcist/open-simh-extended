Default CPU sounds (sound/default_cpu)
======================================

These recordings were made by Ulli (vaxorcist) on his own VAX-11/750.
They are the default system sounds of every simulator that opts in to
them (sim_sound_use_default_cpu () in its initialization; at present the
vax750) when sound is enabled (SET SOUND ENABLED; see HELP SET SOUND) and
the simulator has no system sounds of its own.

File          Content                                     Use
------------  ------------------------------------------  ---------
starting.wav  power relay closes, blower starts           STARTING
running.wav   blower running (cut to loop seamlessly)     RUNNING
stopping.wav  power relay opens, blower runs down         STOPPING

All files are mono, 16 bit, 44100 Hz WAV.

running.wav was cut from a 4.9 s recording into a 4.27 s loop: the end
crossfades (0.25 s) into the start at the point where the recording
resembles its own start best, so it repeats without a click.
starting.wav and stopping.wav are the original recordings (originally
blower_starting.wav and blower_stopping_fadeout.wav; up to sound patch
v13 these files were sound/vax750/blower_*.wav).

Own sounds of a simulator win over these, file by file: the simulator
looks for starting.wav, running.wav and stopping.wav in its own sound
directory first (e.g. sound/vax750/; searched like every sound file: SET
SOUND PATH, SIMH_SOUND_PATH, <exe>/sound, ./sound) and falls back to the
file here.  SET SOUND STARTING=, RUNNING= and STOPPING= name other files
(no fallback for those).  SHOW SOUND lists the defaults in use.

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
