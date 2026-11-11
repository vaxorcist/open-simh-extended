RX02 floppy drive sounds (sound/rx02)
=====================================

These sounds of an RX02 drive are synthetic: there are no recordings of a
real drive yet.  They were made by tools/make_floppy_sounds.py
(open-simh-extended, branch notes) after the RX02 Floppy Disk System
Technical Manual (EK-0RX02-TM-001) and the RX02 print set (MP00629):

- an AC induction motor on the power line (the H771 power supply wires
  both spin motors to the line through fuse F1: no motor control), so the
  motors run whenever the RX02 has power, with or without a diskette;
  belt to the spindle at 360 rpm; a cooling fan on the motor shaft;
  50 Hz line (hum at 100 Hz, motor about 1440 rpm, 7 fan blades);
- the loud head load solenoid of an 8" drive (the head unloads 200 ms
  after the last command);
- the stepper: 6 ms per track (one click per track, a buzz of about
  167 Hz during a long seek).

The RX01 (RX device of the PDP-11, the VAX-11/780 console floppy CS) is
the same drive: it uses these files too, unless sound/rx01 has files of
its own (rx01_<name>.wav).  Its steps come every 10 ms and its head
unloads 46 ms after the last command, so the same click sounds slower.

File                  Content                                    Use
--------------------  -----------------------------------------  -----------
rx02_motor_start.wav  motor and fan running up, 2 s              power on
rx02_motor.wav        motor and fan at speed, 2 s loop           power on
rx02_motor_stop.wav   coasting down, 5 s                         power off
rx02_disk.wav         a diskette turning in its jacket, loop     diskette in
rx02_head.wav         head and load pad on the diskette, loop    head loaded
rx02_head_load.wav    head load solenoid                         head load
rx02_head_unload.wav  solenoid releasing                         head unload
rx02_step.wav         one stepper step, 5.5 ms                   per track
rx02_door_close.wav   front cover closed and latched             ATTACH
rx02_door_open.wav    latch released, cover springs open         DETACH

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
