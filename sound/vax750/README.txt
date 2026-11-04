Sounds of the VAX-11/750 simulator (sound/vax750)
==================================================

These recordings were made by Ulli (vaxorcist) on his own VAX-11/750.
They are used by the vax750 simulator when sound is enabled
(SET SOUND ENABLED; see HELP SET SOUND).

File                  Content                                     Use
--------------------  ------------------------------------------  ---------
blower_starting.wav   power relay closes, blower starts           STARTING
blower_running.wav    blower running (cut to loop seamlessly)     RUNNING
blower_stopping.wav   power relay opens, blower runs down         STOPPING

All files are mono, 16 bit, 44100 Hz WAV.

blower_running.wav was cut from a 4.9 s recording into a 4.27 s loop:
the end crossfades (0.25 s) into the start at the point where the
recording resembles its own start best, so it repeats without a click.
blower_starting.wav and blower_stopping.wav are the original recordings
(originally blower_starting.wav and blower_stopping_fadeout.wav).

To use your own sounds, put files with the same names into a directory
listed in SET SOUND PATH or in the environment variable SIMH_SOUND_PATH
(as <dir>/vax750/<file> or <dir>/<file>), or name other files with
SET SOUND STARTING=, RUNNING= and STOPPING=.

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
