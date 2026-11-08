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

   07-Nov-26    Ulli    Tape transports (SND_REEL), realtime timers shared
                        with the spindles, playback rate with glides,
                        delayed and time-limited voices
   03-Nov-26    Ulli    Spindles: common spin-up/spin-down support for
                        removable-media disk drives; realistic durations
   03-Nov-26    Ulli    Run hooks for devices; power on restores unit loops
   02-Nov-26    Ulli    Phase B: unit loops, optional samples, throttle default
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
   - Timed voices: a voice can start after a delay, fade out after a given
     lifetime and change its playback rate (pitch and speed) in a glide.
     The audio thread times them by itself, so they stay correct while the
     simulator sits at the sim> prompt (e.g. a tape rewinding: the reel
     whine rises and falls as the tape moves from one reel to the other).

   Realism mode
   ------------
   SET SOUND ENABLED switches on the realism mode (see sim_sound_enabled()),
   independent of whether audio output is actually available.  Devices use
   it to select realistic timing and operator behavior (OFFLINE/ONLINE).
   Because this changes device behavior, SET SOUND ENABLED/DISABLED is
   rejected while any disk or tape unit is attached.  A simulator can also
   register a realistic CPU speed (sim_sound_set_throttle ()); it is set
   with SET THROTTLE when sound is enabled, unless the user has already set
   a throttle himself, and removed again when sound is disabled.

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

/* Glide curves for sim_sound_glide () */

#define SND_GLIDE_LINEAR 0                      /* the rate changes linearly */
#define SND_GLIDE_REEL   1                      /* 1/rate^2 changes linearly: a tape reel at
                                                   constant tape speed (its radius^2 changes
                                                   linearly, its rotation rate is ~ 1/radius) */

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

/* Default sounds, used when a device or simulator has no sound of its own
   (per file: an own file always wins).  The default directories are
   searched like every sound directory (SET SOUND PATH, SIMH_SOUND_PATH,
   <exe>/sound, ./sound):
     SND_DEFAULT_DISK   seek sounds seek-s.wav, seek-m.wav, seek-l.wav
                        (used by sim_sound_seek_load ()); no default
                        spindle sounds
     SND_DEFAULT_TAPE   tape transport sounds load.wav, unload.wav,
                        rewind_start.wav, rewind.wav, rewind_stop.wav,
                        start.wav, motion.wav, stop.wav (the synthetic
                        TS11 sounds; used by sim_reel_set_type ())
     SND_DEFAULT_CPU    system sounds starting.wav, running.wav,
                        stopping.wav, only for a simulator that calls
                        sim_sound_use_default_cpu () (opt-in) during its
                        initialization
   SHOW SOUND lists the defaults in use. */

#define SND_DEFAULT_DISK        "default_disk"
#define SND_DEFAULT_TAPE        "default_tape"
#define SND_DEFAULT_CPU         "default_cpu"

void sim_sound_use_default_cpu (void);

/* Simulator default for the realistic CPU speed in the realism mode, as a
   SET THROTTLE argument (e.g. "300K"); NULL or "" for none. */

void sim_sound_set_throttle (const char *spec);

/* State queries */

t_bool sim_sound_enabled (void);                /* SET SOUND ENABLED: realism mode is on */
t_bool sim_sound_available (void);              /* audio output is actually open */
t_bool sim_sound_powered (void);                /* system power is on */

/* Hooks called by SCP */

void sim_sound_power_on (void);                 /* RUN or BOOT command */
void sim_sound_power_off (t_bool wait);         /* power off, optionally wait for the stopping sound */
void sim_sound_exit (void);                     /* simulator exit: power off and close audio */
void sim_sound_run_state (t_bool running);      /* instruction execution starts/stops */

/* Run hooks: a device can register a function that is called whenever
   instruction execution starts (running = TRUE, just before the first
   instruction, after any RUN/BOOT reset) or stops (running = FALSE, back
   at the sim> prompt, including STEP and breakpoints).  Used for things
   that go on in real time while the simulator is stopped, such as a disk
   spindle spinning up.  Registering the same function twice is harmless. */

void sim_sound_register_run_hook (void (*fn)(t_bool running));

/* Configuration checks: SET SOUND ENABLED calls every registered function
   before it switches the realism mode on; a function returns SCPE_OK, or
   prints why the configuration cannot run in the realism mode and returns
   an error, which refuses SET SOUND ENABLED (e.g. the RQ: RC25 drives
   only as pairs on a KLESI).  Registering the same function twice is
   harmless. */

void sim_sound_register_enable_check (t_stat (*fn) (void));

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
   sim_sound_play_rate () like sim_sound_play (), at a playback rate (1.0 =
                         as recorded, 2.0 = an octave higher and twice as
                         fast), fading in over fade_ms.
   sim_sound_play_timed () also with a delay before the start and a
                         lifetime (seconds, 0 = none) after which the voice
                         fades out over fade_ms.  The audio thread does the
                         timing, so it goes on at the sim> prompt.
   sim_sound_glide ()    changes the playback rate of a voice to rate over
                         secs seconds along a curve (SND_GLIDE_xxx).
   sim_sound_extend ()   gives a voice a new lifetime of secs from now (0:
                         none).
*/

SND_SAMPLE *sim_sound_load (const char *dir, const char *file);
SND_SAMPLE *sim_sound_load_optional (const char *dir, const char *file);
SND_HANDLE sim_sound_play (SND_SAMPLE *smp, int32 group, double gain, uint32 flags);
void sim_sound_stop (SND_HANDLE handle, uint32 fade_ms);
double sim_sound_duration (const SND_SAMPLE *smp);
SND_HANDLE sim_sound_play_rate (SND_SAMPLE *smp, int32 group, double gain, uint32 flags,
                                double rate, uint32 fade_ms);
SND_HANDLE sim_sound_play_timed (SND_SAMPLE *smp, int32 group, double gain, uint32 flags,
                                 double rate, double delay_s, double life_s, uint32 fade_ms);
void sim_sound_glide (SND_HANDLE handle, double rate, double secs, int32 curve);
void sim_sound_extend (SND_HANDLE handle, double secs);

/* Unit loops: the sound of a unit that runs continuously, e.g. a disk
   spindle, made of three optional samples: a start one-shot (spin-up), a
   loop (running) and a stop one-shot (spin-down).  Missing samples are
   simply skipped, so a device can call these functions whether or not
   such sounds exist.

   sim_sound_unit_load ()   loads the three samples (quietly: none of them
                            is required).  Call it on ATTACH or similar.
   sim_sound_unit_start ()  plays the start sample followed seamlessly by
                            the loop; the loop starts at a random position
                            so that several identical units sound like
                            independent machines (sqrt(N) growth).
   sim_sound_unit_stop ()   fades the loop out and plays the stop sample.
*/

typedef struct {
    SND_SAMPLE *start;                          /* start one-shot (or NULL) */
    SND_SAMPLE *run;                            /* loop (or NULL) */
    SND_SAMPLE *stop;                           /* stop one-shot (or NULL) */
    SND_HANDLE handle;                          /* playing start/loop voice */
    t_bool active;                              /* unit started (between start and stop) */
    t_bool silenced;                            /* silenced by power off, restored by power on */
    int32 group;                                /* group and gain of the last start */
    double gain;
    } SND_UNITLOOP;

void sim_sound_unit_load (SND_UNITLOOP *ul, const char *dir, const char *start,
                          const char *run, const char *stop);
void sim_sound_unit_start (SND_UNITLOOP *ul, int32 group, double gain);
void sim_sound_unit_stop (SND_UNITLOOP *ul, int32 group, double gain);

/* Realistic durations.  In the realism mode devices schedule realistic
   times given in microseconds.  sim_activate_after () is not used for
   that: it goes through the timer calibration, which during a throttled
   boot can tie an event to a much later timer tick.  These functions
   convert at the throttle's target rate when a throttle is set as xK or
   xM (the realism mode sets one), else at the current rate estimate. */

double sim_sound_inst_per_us (void);
void sim_sound_activate_us (UNIT *uptr, double usecs);

/* Realtime timers: a realistic duration that is simulated time while the
   CPU runs (the guest always sees the full time) and wall time while the
   simulator is stopped at the sim> prompt.  Each timer has its own timer
   unit (internal devices INT-<kind>, INT-<kind>2, ...; no fixed limit),
   which device resets, controller clears and the emptied event queue of
   RUN/BOOT do not affect: at the prompt the remaining time is kept in the
   structure and scheduled again when execution resumes.  When the time is
   over, done () is called (at the prompt: when the timer is next looked
   at with sim_sound_timer_busy (), or when execution resumes); it may
   start the timer again.

   sim_sound_timer_init ()    once per timer; kind names its internal
                              device ("SPINDLE", "REEL"); FALSE if out of
                              memory.  Later calls do nothing.
   sim_sound_timer_start ()   time usecs from now (restarts a busy timer).
   sim_sound_timer_cancel ()  stop timing, without calling done ().
   sim_sound_timer_busy ()    still timing? (at the prompt it first takes
                              the wall time into account)
   sim_sound_timer_left_s ()  seconds left. */

typedef struct SND_TIMER {
    UNIT *unit;                                 /* own timer unit (NULL: not initialized) */
    void (*done) (struct SND_TIMER *t);         /* the time is over */
    void *ctx;                                  /* owner (e.g. the spindle) */
    t_bool busy;                                /* timing */
    t_bool paused;                              /* at the prompt: the time is kept in left_us */
    double left_us;                             /* ... time left */
    uint32 ref_ms;                              /* ... at this wall clock time */
    } SND_TIMER;

t_bool sim_sound_timer_init (SND_TIMER *t, const char *kind, void (*done) (SND_TIMER *t), void *ctx);
void sim_sound_timer_start (SND_TIMER *t, double usecs);
void sim_sound_timer_cancel (SND_TIMER *t);
t_bool sim_sound_timer_busy (SND_TIMER *t);
double sim_sound_timer_left_s (SND_TIMER *t);

/* Spindles: the drive RUN/STOP button of removable-media disk drives.

   A SND_SPINDLE per drive is stopped, spinning up, ready or spinning
   down.  The device tells it about the RUN/STOP button and asks for its
   state; it calls back the device when the drive is up to speed and when
   it is at rest.

   - Normal mode (sound disabled): start and stop take effect at once and
     the callbacks are made at once.
   - Realism mode: the spin-up takes *spinup_ms and the spin-down
     *spindown_ms (the device's registers), timed by a realtime timer
     (SND_TIMER, internal devices INT-SPINDLE, INT-SPINDLE2, ...): while
     the CPU runs this is simulated time, so the guest always sees the
     full time; while the simulator is stopped at the sim> prompt it goes
     on in wall time.  The spindle sounds (prefix_spinup.wav, prefix_running.wav,
     prefix_spindown.wav) play if they exist.

   sim_spindle_init ()   once per drive (e.g. in the device reset); later
                         calls do nothing.
   sim_spindle_load ()   load the optional spindle sounds (on ATTACH).
   sim_spindle_start ()  RUN: spin up (also from spinning down).
   sim_spindle_stop ()   STOP: spin down (also from spinning up).
   sim_spindle_off ()    the pack is gone (DETACH): stopped at once, no
                         callback; the spin-down sound plays if it turned.
   sim_spindle_ready ()  stopped -> ready at once without sound or callback
                         (normal-mode ATTACH, which spins up at once).
   sim_spindle_state ()  SND_SPIN_xxx; at the prompt it first takes the
                         wall time into account (and may call back).
   sim_spindle_wait ()   at the prompt: wait, with messages, until the
                         drive is ready or at rest.
   sim_spindle_show ()   "offline", "spinning up, n s left", "online", ... */

#define SND_SPIN_STOPPED 0                      /* at rest (offline) */
#define SND_SPIN_UP      1                      /* spinning up */
#define SND_SPIN_READY   2                      /* up to speed (online) */
#define SND_SPIN_DOWN    3                      /* spinning down */

typedef struct SND_SPINDLE {
    UNIT *uptr;                                 /* drive unit (NULL: not initialized) */
    int32 *spinup_ms;                           /* device register: spin-up time */
    int32 *spindown_ms;                         /* device register: spin-down time */
    void (*on_ready) (UNIT *uptr);              /* up to speed (may be NULL) */
    void (*on_stopped) (UNIT *uptr);            /* at rest (may be NULL) */
    int32 group;                                /* sound group and gain */
    double gain;
    int32 state;                                /* SND_SPIN_xxx */
    SND_TIMER tmr;                              /* spin-up/spin-down timer */
    SND_UNITLOOP sound;                         /* spindle sounds */
    } SND_SPINDLE;

void sim_spindle_init (SND_SPINDLE *sp, UNIT *uptr, int32 *spinup_ms, int32 *spindown_ms,
                       void (*on_ready) (UNIT *uptr), void (*on_stopped) (UNIT *uptr),
                       int32 group, double gain);
void sim_spindle_load (SND_SPINDLE *sp, const char *dir, const char *prefix);
void sim_spindle_start (SND_SPINDLE *sp);
void sim_spindle_stop (SND_SPINDLE *sp);
void sim_spindle_off (SND_SPINDLE *sp);
void sim_spindle_ready (SND_SPINDLE *sp);
int32 sim_spindle_state (SND_SPINDLE *sp);
void sim_spindle_wait (SND_SPINDLE *sp);
t_stat sim_spindle_show (FILE *st, SND_SPINDLE *sp);

/* Tape transports (reel-to-reel 9-track drives): the operator panel
   (load, rewind, unload, on-line), the physical tape position and the
   timing of tape motion, and the transport sounds.

   The device keeps the controller semantics; the SND_REEL keeps the
   transport.  A transport type (SND_REEL_TYPE, a constant table in the
   device) gives the speeds, gaps, start/stop and load/unload times.

   Physical position: inches from the BOT marker, counted from the
   records the device moves over (bytes / bpi + the interrecord gap; a
   tape mark; an erase).  It gives the rewind time, the reel radii (the
   pitch of the reel motors) and the physical end of tape: the EOT marker
   is REEL_EOT_FT before the end of the tape, the BOT marker
   REEL_LEADER_FT after its start, so a reel of L feet has L - 41 feet
   between the markers.

   States: no reel -> mounted (not loaded) -> loading -> ready (loaded,
   at rest) <-> moving / rewinding; ready or rewinding -> unloading ->
   mounted.  The device is called back (on_done) when a load, a motion,
   a rewind or an unload is over.

   - Normal mode (sound disabled): everything takes effect at once (the
     callbacks are made at once); the position is still counted.
   - Realism mode: the times are realistic; long actions (load, rewind,
     unload) go on in wall time at the prompt (realtime timer, internal
     devices INT-REEL, ...).

   Sounds: <dir>/<prefix>_<name>.wav, else default_tape/<name>.wav, for
   the names load, unload, rewind_start, rewind (a loop of a reel motor,
   played once per reel with a pitch glide that follows the reel's
   rotation rate), rewind_stop, start, motion (a loop while the tape
   moves at read/write speed), stop.

   sim_reel_init ()       once per transport.
   sim_reel_set_type ()   the transport type; loads the sounds.
   sim_reel_mount ()      ATTACH: the reel is mounted; normal mode: also
                          loaded at once (at BOT).
   sim_reel_remove ()     DETACH: everything stops, no reel.
   sim_reel_load ()       LOAD button: thread the tape and go to BOT.
   sim_reel_rewind ()     rewind to BOT (button or command).
   sim_reel_unload ()     rewind and unload onto the supply reel.
   sim_reel_abort ()      stop what is going on (subsystem initialize),
                          without callback; the position is where the
                          tape got to.
   sim_reel_motion_begin (), sim_reel_record (), sim_reel_tapemark (),
   sim_reel_erase (), sim_reel_motion_end ():
                          (sim_reel_erase: a length of tape moved without
                          records, e.g. an erase or blank tape; negative:
                          in reverse)
                          a guest motion command: count the records the
                          device moves over, then start the motion;
                          motion_end returns the time in microseconds
                          (0: done at once, no callback; normal mode).
   sim_reel_past_eot ()   at or beyond the EOT marker.
   sim_reel_past_end ()   the physical end of the tape is reached.
   sim_reel_state (), sim_reel_wait (), sim_reel_show (): state (at the
                          prompt the wall time is taken into account),
                          waiting with messages at the prompt, display. */

#define SND_REEL_NOREEL     0                   /* no reel mounted */
#define SND_REEL_MOUNTED    1                   /* reel mounted, tape not loaded */
#define SND_REEL_LOADING    2                   /* load sequence */
#define SND_REEL_READY      3                   /* loaded, tape at rest */
#define SND_REEL_MOVING     4                   /* guest motion command */
#define SND_REEL_REWINDING  5                   /* rewinding to BOT */
#define SND_REEL_UNLOADING  6                   /* rewinding and unloading */

#define SND_REEL_DONE_LOAD   1                  /* on_done (): what is over */
#define SND_REEL_DONE_MOTION 2
#define SND_REEL_DONE_REWIND 3
#define SND_REEL_DONE_UNLOAD 4

#define SND_REEL_NSMP       8                   /* number of transport sounds */

#define REEL_LEADER_FT      16                  /* tape start to the BOT marker */
#define REEL_EOT_FT         25                  /* EOT marker to the tape end */

typedef struct {
    const char *name;                           /* "TS11" */
    const char *dir;                            /* sound directory, e.g. "ts11" */
    const char *prefix;                         /* sound file prefix, e.g. "ts11" */
    double ips;                                 /* read/write speed (in/s) */
    double rewind_ips[3];                       /* rewind speed with 600, 1200, 2400 ft reels */
    double bpi;                                 /* recording density */
    double gap_in;                              /* interrecord gap */
    double tm_in;                               /* tape mark with its gap */
    double start_ms;                            /* start time (start/stop motion) */
    double stop_ms;                             /* stop time */
    double ramp_in;                             /* tape moved during a start plus a stop */
    double access_ms;                           /* streaming drive: access from rest; 0: the
                                                   tape starts and stops for every record */
    double rewind_extra_ms;                     /* ramps, BOT overshoot and creep back */
    double erase_in;                            /* one erase */
    double clean_in;                            /* clean: forward and back again */
    int32 load_ms;                              /* default LOADTIME */
    int32 unload_ms;                            /* default UNLOADTIME (from BOT) */
    } SND_REEL_TYPE;

typedef struct SND_REEL {
    UNIT *uptr;                                 /* tape unit (NULL: not initialized) */
    const SND_REEL_TYPE *type;                  /* transport type */
    int32 *load_ms;                             /* device registers: LOADTIME */
    int32 *unload_ms;                           /*  UNLOADTIME */
    int32 *reel_ft;                             /*  reel length 600/1200/2400 */
    void (*on_done) (UNIT *uptr, int32 what);   /* a load/motion/rewind/unload is over */
    int32 group;                                /* sound group and gain */
    double gain;
    int32 state;                                /* SND_REEL_xxx */
    uint32 pos;                                 /* position from the BOT marker, 1/10000 in
                                                   (a register for SAVE/RESTORE) */
    double rew_from_in;                         /* rewinding: position at the start */
    double rew_ips;                             /*  ... speed */
    double rew_us;                              /*  ... time of the rewind part */
    double tot_us;                              /*  ... time of the whole action */
    double mot_us;                              /* motion being counted: time */
    double mot_in;                              /*  ... distance (+ forward, - reverse) */
    int32 mot_items;                            /*  ... records and tape marks */
    t_bool mot_rev;                             /*  ... direction of the last item */
    t_bool streaming;                           /* streaming drive: the tape still moves */
    t_bool stream_rev;                          /*  ... in this direction */
    SND_TIMER tmr;                              /* load/motion/rewind/unload timer */
    SND_TIMER coast;                            /* streaming: the tape coasts through a gap */
    SND_SAMPLE *smp[SND_REEL_NSMP];             /* the transport sounds */
    SND_HANDLE h[SND_REEL_NSMP + 1];            /* voices in use (one more: second reel) */
    } SND_REEL;

void sim_reel_init (SND_REEL *r, UNIT *uptr, int32 *load_ms, int32 *unload_ms, int32 *reel_ft,
                    void (*on_done) (UNIT *uptr, int32 what), int32 group, double gain);
void sim_reel_set_type (SND_REEL *r, const SND_REEL_TYPE *type);
void sim_reel_mount (SND_REEL *r);
void sim_reel_remove (SND_REEL *r);
t_stat sim_reel_load (SND_REEL *r);
t_stat sim_reel_rewind (SND_REEL *r);
t_stat sim_reel_unload (SND_REEL *r);
void sim_reel_abort (SND_REEL *r);
void sim_reel_motion_begin (SND_REEL *r);
void sim_reel_record (SND_REEL *r, uint32 bytes, t_bool reverse);
void sim_reel_tapemark (SND_REEL *r, t_bool reverse);
void sim_reel_erase (SND_REEL *r, double inches);
double sim_reel_motion_end (SND_REEL *r);
double sim_reel_pos_in (SND_REEL *r);
void sim_reel_set_bot (SND_REEL *r);
t_bool sim_reel_past_eot (SND_REEL *r);
t_bool sim_reel_past_end (SND_REEL *r);
int32 sim_reel_state (SND_REEL *r);
void sim_reel_wait (SND_REEL *r);
t_stat sim_reel_show (FILE *st, SND_REEL *r);

/* Seek sounds: three samples (prefix-s.wav, prefix-m.wav, prefix-l.wav
   in dir, else seek-s/m/l.wav in SND_DEFAULT_DISK) for short, medium and
   long seeks.  sim_sound_seek () picks one by the
   distance and the device's limits (cylinders): up to s_lim short, up to
   m_lim medium, else long; distance 0 plays nothing unless forced long
   (a recalibrate).  Only in the realism mode. */

void sim_sound_seek_load (SND_SAMPLE *smp[3], const char *dir, const char *prefix);
int32 sim_sound_seek (SND_SAMPLE *smp[3], int32 dist, int32 s_lim, int32 m_lim,
                      t_bool force_long, int32 group, double gain);

#ifdef  __cplusplus
}
#endif

#endif /* SIM_SOUND_H_ */
