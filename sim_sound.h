/* sim_sound.h: common sound support for all simulators

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

   02-Nov-26    Ulli    First version (phase A: core, mixer, system sounds)

   Overview
   --------
   sim_sound provides sound output that any simulator and any device can use.
   The audio output is implemented with the SDL2 audio subsystem.  When a
   simulator is built without SDL2 (USE_SIM_SOUND not defined), or when the
   host has no usable audio device, every playback request is silently
   ignored; all other functions (the SET/SHOW SOUND commands, the "realism
   mode" flag, the power state) still work, so a configuration script
   behaves the same on every host.

   Sound classes
   -------------
   - System sounds (group SYSTEM): a "starting" one-shot, a "running" loop
     and a "stopping" one-shot, e.g. the power relay and blower of a
     VAX-11/750.  The system is powered on by the first RUN or BOOT command
     and powered off by EXIT/QUIT or by SET SOUND POWER=OFF.  Returning to
     the sim> prompt (HALT, ^E, breakpoints, STEP, EXPECT matches) does not
     change the power state, just as halting a real CPU does not switch off
     its fans.
   - Unit loops (groups DISK/TAPE): one loop per unit (e.g. a spinning
     spindle); started and stopped by device code.  The audio thread keeps
     playing them while the simulator sits at the sim> prompt.
   - Events (group EVENT or the device's group): fire-and-forget one-shots
     such as a seek.  Playing an event only queues a request and never
     blocks the simulation.

   Realism mode
   ------------
   SET SOUND ENABLED switches on the realism mode (see sim_sound_enabled()),
   independent of whether audio output is actually available.  Devices use
   it to select realistic timing and operator behavior (OFFLINE/ONLINE).
   Because this changes device behavior, SET SOUND ENABLED/DISABLED is
   rejected while any disk or tape unit is attached.

   Mixing
   ------
   All voices are summed in floating point with fixed headroom (-6 dB); a
   soft limiter on the master bus only acts near full scale, so overlapping
   sounds never clip hard.  Identical loops can be started at a random
   position (SND_F_RANDPHASE) so that N of them add up with sqrt(N) like
   independent real machines instead of coherently with N.
*/

#ifndef SIM_SOUND_H_
#define SIM_SOUND_H_     0

#include "sim_defs.h"

#ifdef  __cplusplus
extern "C" {
#endif

/* Mixer groups.  Every voice belongs to one group; each group has its own
   volume and mute switch in addition to the master volume. */

#define SND_GRP_SYSTEM  0                       /* system sounds (fans, power) */
#define SND_GRP_DISK    1                       /* disk spindles and seeks */
#define SND_GRP_TAPE    2                       /* tape drives */
#define SND_GRP_EVENT   3                       /* other events */
#define SND_GRP_COUNT   4                       /* number of groups */

/* Playback flags for sim_sound_play () */

#define SND_F_LOOP      0x0001                  /* loop the sample endlessly */
#define SND_F_RANDPHASE 0x0002                  /* start a loop at a random position */

/* A loaded, converted sample (opaque to callers) */

typedef struct SND_SAMPLE SND_SAMPLE;

/* A playing voice is identified by a handle; 0 means "no voice".
   Handles are never reused during the life of the simulator. */

typedef int32 SND_HANDLE;

/* SCP commands: SET SOUND ... and SHOW SOUND (registered in scp.c) */

t_stat sim_sound_set (int32 flag, CONST char *cptr);
t_stat sim_sound_show (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, CONST char *cptr);

/* Simulator defaults: the sound directory name (e.g. "vax750") and the
   file names of the system sounds.  Called once by a simulator during its
   initialization; any name may be NULL.  The files are searched as
   described in sim_sound_find_file () when sound is enabled. */

void sim_sound_set_system (const char *dir, const char *starting,
                           const char *running, const char *stopping);

/* State queries */

t_bool sim_sound_enabled (void);                /* SET SOUND ENABLED: realism mode is on */
t_bool sim_sound_available (void);              /* audio output is actually open */
t_bool sim_sound_powered (void);                /* system power is on */

/* Hooks called by SCP */

void sim_sound_power_on (void);                 /* RUN or BOOT command */
void sim_sound_power_off (t_bool wait);         /* power off, optionally wait for the stopping sound */
void sim_sound_exit (void);                     /* simulator exit: power off and close audio */

/* Sample and voice API for devices.

   sim_sound_load ()     loads and converts a WAV file (cached by file name).
                         Call it while configuring (e.g. on SET or ATTACH),
                         never in the I/O path: it reads a file.  Returns
                         NULL (and warns once) if the file is not found or
                         sound output is not available.
   sim_sound_play ()     starts playing a sample in a group with a gain
                         (1.0 = as recorded).  Only queues a request; safe
                         to call from any thread.  Returns 0 for a NULL
                         sample or when sound output is not available.
   sim_sound_stop ()     stops a voice, fading it out over fade_ms
                         milliseconds (0 = a short click-free fade).
   sim_sound_duration () length of a sample in seconds (0.0 for NULL).
*/

SND_SAMPLE *sim_sound_load (const char *dir, const char *file);
SND_HANDLE sim_sound_play (SND_SAMPLE *smp, int32 group, double gain, uint32 flags);
void sim_sound_stop (SND_HANDLE handle, uint32 fade_ms);
double sim_sound_duration (const SND_SAMPLE *smp);

#ifdef  __cplusplus
}
#endif

#endif /* SIM_SOUND_H_ */
