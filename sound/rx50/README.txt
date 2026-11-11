RX50 floppy drive sounds (sound/rx50)
=====================================

These sounds of an RX50 drive are synthetic: there are no recordings of a
real drive yet.  They were made by tools/make_floppy_sounds.py v3
(open-simh-extended, branch notes) after the Rainbow 100 Technical Manual
(EK-PC100-TM-001, chapter 6: the RX50 diskette drive):

- one drive holds two 5.25" diskettes (slots A and B); one 12 V DC spindle
  motor drives both spindles at 300 rpm and is switched on by the
  controller (at speed within half a second) and off after an idle time;
  no fan, no AC hum;
- one stepper moves the common head carriage, at least 6 ms per step;
- a head load solenoid per side;
- a lever per slot closes the door and clamps the diskette.

In SIMH an RX50 slot is an RQ unit; two RX50 units with consecutive unit
numbers (e.g. DUA1 and DUA2) are one drive and share the motor, the
carriage and these motor sounds.

File                  Content                                    Use
--------------------  -----------------------------------------  -----------
rx50_motor_start.wav  the DC motor running up, 0.5 s             motor on
rx50_motor.wav        motor and belt at speed, 2 s loop          motor on
rx50_motor_stop.wav   coasting down, 1.5 s                       motor off
rx50_disk.wav         a diskette turning in its jacket, loop     diskette in
rx50_head.wav         head and load pad on the diskette, loop    head loaded
rx50_head_load.wav    head load solenoid                         head load
rx50_head_unload.wav  solenoid releasing                         head unload
rx50_step.wav         one stepper step, 5.5 ms                   per track
rx50_door_close.wav   lever turned, clamp closing                ATTACH
rx50_door_open.wav    lever released, door opening               DETACH

All files are mono, 16 bit, 44100 Hz WAV; the loops are seamless.  Drives
without sounds of their own use sound/default_floppy (see its README).

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
