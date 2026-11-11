/* sim_sound.c: common sound support for all simulators

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

   09-Nov-26    Ulli    Streamers: the reverse start of a reposition
                        follows the stop when it has died away
   09-Nov-26    Ulli    Streamers: SHOW TRANSPORT shows where the tape is
                        during a rewind or move (was "at BOT, rewinding")
   08-Nov-26    Ulli    Streamers: crossfaded start/loop/stop sounds
   08-Nov-26    Ulli    Cartridge streamers (SND_STREAMER, TK50), realtime
                        clock
   07-Nov-26    Ulli    Tape transports (SND_REEL); realtime timers shared by
                        spindles and transports; playback rate with glides,
                        delayed and time-limited voices; default tape sounds
   03-Nov-26    Ulli    Spindles (common RUN/STOP support for removable-media
                        disks), seek sound helper, realistic durations
   03-Nov-26    Ulli    Run hooks for devices; power on restores the unit loops
                        that power off silenced
   02-Nov-26    Ulli    Phase B: unit loops, optional samples, throttle default,
                        power off silences every sound
   02-Nov-26    Ulli    First version (phase A: core, mixer, system sounds)

   See sim_sound.h for an overview of the sound classes, the realism mode
   and the mixing rules.

   Structure of this file
   ----------------------
   1. Common state and helpers.  This part is always compiled: the SET/SHOW
      SOUND commands, the realism flag and the power state work in every
      build, with or without SDL2.
   2. The SDL2 audio backend (USE_SIM_SOUND defined): sample loading and
      conversion, the command queue, the voices and the mixer that runs in
      the SDL audio callback thread.
   3. Stubs used when the simulator is built without SDL2: every playback
      function does nothing.
   4. The public functions built on top of 1-3.

   Threading
   ---------
   The SDL audio callback runs in a thread of its own.  The simulator
   threads never touch the voices directly: sim_sound_play () and
   sim_sound_stop () append a command to a small queue protected by a
   mutex that is only held for a few instructions.  The callback takes the
   queue with SDL_TryLockMutex (); if the mutex happens to be busy, the
   commands are simply picked up by the next callback (a few milliseconds
   later).  So the audio thread never blocks the simulation and the
   simulation never waits for the mixer.

   Internal audio format
   ---------------------
   All samples are converted once, when they are loaded, to 32-bit float,
   mono, SND_RATE Hz.  The output device is opened with this format; SDL
   converts to whatever the host hardware needs.
*/

#include "sim_defs.h"
#include "sim_sound.h"
#include <math.h>

/* ------------------------------------------------------------------------
   1. Common state
   ------------------------------------------------------------------------ */

#define SND_RATE        44100                   /* internal sample rate (Hz) */
#define SND_HEADROOM    0.5f                    /* -6 dB master headroom */
#define SND_LIMIT_KNEE  0.70f                   /* soft limiter starts here */
#define SND_VOL_MAX     200                     /* volumes are 0..200 % */

#define SND_SYS_STARTING 0                      /* indices of the system sounds */
#define SND_SYS_RUNNING  1
#define SND_SYS_STOPPING 2
#define SND_SYS_COUNT    3

static const char *snd_group_names[SND_GRP_COUNT] = {
    "SYSTEM", "DISK", "TAPE", "EVENT" };

static const char *snd_sys_names[SND_SYS_COUNT] = {
    "STARTING", "RUNNING", "STOPPING" };

static t_bool snd_enabled = FALSE;              /* SET SOUND ENABLED (realism mode) */
static t_bool snd_power = FALSE;                /* system power state */
static void flp_power (t_bool on, t_bool direct);   /* floppy drives (end of file) */
static UNIT *flp_attached_unit (void);
static void flp_forget_sounds (void);
static int32 snd_master_vol = 100;              /* master volume (%) */
static t_bool snd_master_mute = FALSE;          /* master mute */
static int32 snd_group_vol[SND_GRP_COUNT] = {   /* group volumes (%) */
    100, 100, 100, 100 };
static t_bool snd_group_mute[SND_GRP_COUNT];    /* group mutes */
static char snd_path[CBUFSIZE] = "";            /* SET SOUND PATH list */

static char snd_sys_dir[CBUFSIZE] = "";         /* simulator sound directory, e.g. "vax750" */
static char snd_sys_default[SND_SYS_COUNT][CBUFSIZE];/* simulator default system sound files */
static char snd_sys_user[SND_SYS_COUNT][CBUFSIZE];   /* SET SOUND STARTING/RUNNING/STOPPING */
static SND_SAMPLE *snd_sys_smp[SND_SYS_COUNT];  /* loaded system sounds (may be NULL) */
static SND_HANDLE snd_sys_handle = 0;           /* voice of the starting/running chain */
/* Registries without a fixed limit (up to sound patch v13: 64 unit loops
   and 8 run hooks; a unit loop beyond 64 was not restored at power on,
   known issues case 27): arrays that grow as needed.  They hold pointers,
   so growing them moves nothing that is in use. */

static SND_UNITLOOP **snd_unitloops = NULL;     /* every unit loop ever loaded */
static int32 snd_n_unitloops = 0;
static void (**snd_runhooks) (t_bool running) = NULL;
static int32 snd_n_runhooks = 0;

/* Default sounds, used when a device or simulator has no sound of its
   own (2026-11-07, Ulli: "DRY"):
   - disks: seek sounds sound/default_disk/seek-s.wav, seek-m.wav,
     seek-l.wav (the former synthetic RK07 seek sounds); there are no
     default spindle sounds (they must match the drive's speed and times);
   - tapes: sound/default_tape/load.wav, unload.wav, rewind_start.wav,
     rewind.wav, rewind_stop.wav, start.wav, motion.wav, stop.wav (the
     synthetic TS11 transport sounds, tools/make_tape_sounds.py);
   - floppies (2026-11-11): sound/default_floppy/motor_start.wav, motor.wav,
     motor_stop.wav, disk.wav, head.wav, head_load.wav, head_unload.wav,
     step.wav, door_close.wav, door_open.wav (a synthetic 5.25" drive,
     tools/make_floppy_sounds.py), motor sounds included;
   - CPUs: sound/default_cpu/starting.wav, running.wav, stopping.wav (the
     recordings of a real VAX-11/750), only for simulators that opt in with
     sim_sound_use_default_cpu ().
   The fallback is per file: a device's own file wins.  Defaults are found
   through the same search order as every sound file.  SHOW SOUND lists
   which defaults are in use. */

static t_bool snd_sys_use_default = FALSE;      /* the simulator opted in to default_cpu */
static char *snd_default_uses = NULL;           /* "dir (what), ..." for SHOW SOUND */

static const char *snd_sys_default_file[SND_SYS_COUNT] = {
    "starting.wav", "running.wav", "stopping.wav" };
static char snd_throttle[CBUFSIZE] = "";        /* simulator default throttle (realism mode) */
static t_bool snd_throttle_set = FALSE;         /* the throttle was set by sim_sound */
static uint32 snd_throttle_delay = 0;           /* THROT_DELAY before sim_sound set it */

/* A loaded sample.  Samples are kept in a list for the whole life of the
   simulator, so pointers handed out stay valid even while voices still
   play them.  A sample whose file was not found (or could not be read)
   stays in the list with data == NULL so that the warning is given once. */

struct SND_SAMPLE {
    struct SND_SAMPLE *next;                    /* next sample in the cache */
    char *key;                                  /* "dir/file" as requested */
    char *path;                                 /* resolved file name (NULL: not found) */
    float *data;                                /* converted samples (NULL: unusable) */
    uint32 frames;                              /* number of frames in data */
    };

static SND_SAMPLE *snd_cache = NULL;            /* all samples ever requested */

/* Return the active file name of a system sound: the user's choice if one
   was set, otherwise the simulator default (may be ""). */

static const char *snd_sys_file (int32 idx)
{
return (snd_sys_user[idx][0] != '\0') ? snd_sys_user[idx] : snd_sys_default[idx];
}

/* Find the first attached disk or tape unit.  SET SOUND ENABLED/DISABLED is
   refused while one exists, because the realism mode changes the timing
   and the operator behavior of these devices. */

static UNIT *snd_attached_storage_unit (void)
{
DEVICE *dptr;
uint32 i, j;

for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
    if ((DEV_TYPE (dptr) != DEV_DISK) && (DEV_TYPE (dptr) != DEV_TAPE))
        continue;
    for (j = 0; j < dptr->numunits; j++) {
        if (dptr->units[j].flags & UNIT_ATT)
            return &dptr->units[j];
        }
    }
return flp_attached_unit ();                    /* floppies (not DEV_DISK devices) */
}

/* Directory of the executable, ending with a path separator, or "" if it
   can not be determined.  Used for the "sound/<dir>/" search rule. */

static const char *snd_exe_dir (void);

/* Does a file exist and can it be read? */

static t_bool snd_file_readable (const char *path)
{
FILE *f = sim_fopen (path, "rb");

if (f == NULL)
    return FALSE;
fclose (f);
return TRUE;
}

/* Try "base[/dir]/file"; on success leave the name in out. */

static t_bool snd_try (char *out, size_t outsize, const char *base, const char *dir, const char *file)
{
size_t blen = strlen (base);
const char *sep = ((blen > 0) && (base[blen - 1] != '/') && (base[blen - 1] != '\\')) ? "/" : "";

strlcpy (out, base, outsize);
strlcat (out, sep, outsize);
if ((dir != NULL) && (*dir != '\0')) {
    strlcat (out, dir, outsize);
    strlcat (out, "/", outsize);
    }
strlcat (out, file, outsize);
return snd_file_readable (out);
}

/* Search a sound file.  The search order (first match wins) is:

     0. the file name itself if it contains a path separator (the user
        named a specific file),
     1. each directory of SET SOUND PATH=d1;d2;...  as d/<dir>/<file>, then
        as d/<file>,
     2. each directory of the environment variable SIMH_SOUND_PATH, in the
        same way (';' separates, and on non-Windows hosts also ':'),
     3. <executable directory>/sound/<dir>/<file>, then
        <executable directory>/../sound/<dir>/<file> (for simulators run
        from the BIN directory of a source tree),
     4. sound/<dir>/<file> relative to the current directory.

   So a user file with the same name in a directory earlier in the order
   overrides a bundled one.  Returns TRUE and the name in out if found. */

static t_bool snd_search_list (char *out, size_t outsize, const char *list, const char *dir, const char *file, t_bool colon_too)
{
char buf[CBUFSIZE];
char *p, *e;

strlcpy (buf, list, sizeof (buf));
for (p = buf; *p != '\0'; p = e) {
    for (e = p; (*e != '\0') && (*e != ';') && !(colon_too && (*e == ':')); e++)
        ;
    if (*e != '\0')
        *e++ = '\0';
    while (isspace ((unsigned char)*p))         /* trim the directory */
        p++;
    if (*p == '\0')
        continue;
    if (snd_try (out, outsize, p, dir, file) ||
        snd_try (out, outsize, p, NULL, file))
        return TRUE;
    }
return FALSE;
}

static t_bool snd_find_file (char *out, size_t outsize, const char *dir, const char *file)
{
const char *env = getenv ("SIMH_SOUND_PATH");
const char *exe = snd_exe_dir ();
char base[CBUFSIZE];
#if defined (_WIN32)
t_bool colon_too = FALSE;                       /* "C:\..." must not be split */
#else
t_bool colon_too = TRUE;
#endif

if ((strchr (file, '/') != NULL) || (strchr (file, '\\') != NULL)) {
    strlcpy (out, file, outsize);               /* 0. explicit path */
    if (snd_file_readable (out))
        return TRUE;
    }
if (snd_search_list (out, outsize, snd_path, dir, file, colon_too))     /* 1. */
    return TRUE;
if ((env != NULL) &&
    snd_search_list (out, outsize, env, dir, file, colon_too))          /* 2. */
    return TRUE;
if (*exe != '\0') {                                                     /* 3. */
    strlcpy (base, exe, sizeof (base));
    strlcat (base, "sound", sizeof (base));
    if (snd_try (out, outsize, base, dir, file))
        return TRUE;
    strlcpy (base, exe, sizeof (base));
    strlcat (base, "../sound", sizeof (base));
    if (snd_try (out, outsize, base, dir, file))
        return TRUE;
    }
return snd_try (out, outsize, "sound", dir, file);                      /* 4. */
}

/* Parse a volume "n" (0..SND_VOL_MAX %) */

static t_stat snd_parse_volume (const char *cptr, int32 *vol)
{
t_stat r;
int32 v = (int32)get_uint (cptr, 10, SND_VOL_MAX, &r);

if (r != SCPE_OK)
    return sim_messagef (SCPE_ARG, "Invalid volume: %s (0..%d %%)\n", cptr, SND_VOL_MAX);
*vol = v;
return SCPE_OK;
}

/* Find a group by name (abbreviations allowed); -1 if unknown */

static int32 snd_find_group (const char *name)
{
int32 i;

for (i = 0; i < SND_GRP_COUNT; i++) {
    if ((*name != '\0') && (MATCH_CMD (name, snd_group_names[i]) == 0))
        return i;
    }
return -1;
}

/* ------------------------------------------------------------------------
   2. SDL2 audio backend
   ------------------------------------------------------------------------ */

#if defined (USE_SIM_SOUND)

/* sim_sound.c never defines main (), and the simulator's main () is in
   scp.c (or in sim_video.c for some video builds), so tell SDL not to
   redirect main () in this compilation unit. */
#define SDL_MAIN_HANDLED
#include <SDL.h>

#define SND_VOICES      64                      /* maximum simultaneous voices */
#define SND_QSIZE       256                     /* command queue entries */
#define SND_BLOCK       512                     /* frames per audio callback (11.6 ms) */
#define SND_CHAIN_XFADE ((uint32)(SND_RATE / 20))/* 50 ms crossfade one-shot -> loop */
#define SND_CLICK_FADE  ((uint32)(SND_RATE / 200))/* 5 ms fade to avoid clicks */

/* Commands from the simulator threads to the audio thread */

#define SND_CMD_PLAY    1                       /* start a voice */
#define SND_CMD_STOP    2                       /* fade out and stop a voice */
#define SND_CMD_STOPALL 3                       /* fade out and stop every voice */
#define SND_CMD_RATE    4                       /* change the playback rate of a voice */
#define SND_CMD_EXTEND  5                       /* give a voice a new lifetime */

typedef struct {
    int32 type;                                 /* SND_CMD_xxx */
    SND_HANDLE handle;                          /* voice handle */
    SND_SAMPLE *smp;                            /* sample to play */
    SND_SAMPLE *chain;                          /* loop that follows a one-shot (or NULL) */
    int32 group;                                /* mixer group */
    float gain;                                 /* voice gain */
    uint32 flags;                               /* SND_F_xxx */
    uint32 chain_flags;                         /* SND_F_xxx of the chained loop */
    uint32 fade;                                /* fade-in (PLAY) or fade-out (STOP) in frames;
                                                   RATE: glide length in frames */
    double rate;                                /* PLAY: initial rate; RATE: target rate */
    int32 curve;                                /* RATE: SND_GLIDE_xxx */
    uint32 delay;                               /* PLAY: silent frames before the start */
    uint32 life;                                /* PLAY: fade out after this many frames (0: never) */
    uint32 life_fade;                           /* PLAY: ... over this many frames */
    uint32 rep_count;                           /* PLAY: play the sample this many more times */
    uint32 rep_period;                          /* PLAY: ... every rep_period frames (0: no repeats) */
    } SND_CMD;

/* A voice: one sample being played.  Only the audio thread touches the
   voices while the device is open. */

typedef struct {
    t_bool active;                              /* voice in use */
    SND_HANDLE handle;                          /* handle (0 after a chain handed it on) */
    SND_SAMPLE *smp;                            /* sample */
    SND_SAMPLE *chain;                          /* loop to start near the end (or NULL) */
    uint32 chain_flags;                         /* flags of that loop */
    double pos;                                 /* next frame (fractional when the rate is not 1) */
    double rate;                                /* playback rate (1.0 = as recorded) */
    double glide_step;                          /* per-frame change of rate (linear) or of
                                                   1/rate^2 (reel curve) during a glide */
    double glide_inv2;                          /* reel curve: current 1/rate^2 */
    uint32 glide_left;                          /* frames left in the glide (0: none) */
    int32 glide_curve;                          /* SND_GLIDE_xxx */
    uint32 delay;                               /* frames of silence before the start */
    uint32 life;                                /* frames until the voice fades out (0: never) */
    uint32 life_fade;                           /* ... fade length in frames */
    uint32 rep_left;                            /* repetitions still to come (step clicks) */
    uint32 rep_period;                          /* ... one every rep_period frames (0: none) */
    uint32 rep_pos;                             /* frames since the current repetition began */
    t_bool loop;                                /* loop at the end */
    int32 group;                                /* mixer group */
    float gain;                                 /* voice gain */
    float fade;                                 /* fade gain 0..1 */
    float fade_step;                            /* per-frame change of fade */
    } SND_VOICE;

static SDL_AudioDeviceID snd_dev = 0;           /* open audio device (0 = none) */
static t_bool snd_subsystem = FALSE;            /* SDL audio subsystem initialized by us */
static SDL_mutex *snd_qlock = NULL;             /* protects the command queue */
static SND_CMD snd_queue[SND_QSIZE];            /* command ring buffer */
static int32 snd_qhead = 0;                     /* next command to take */
static int32 snd_qtail = 0;                     /* next free entry */
static SND_VOICE snd_voices[SND_VOICES];        /* the voices (audio thread) */
static SDL_atomic_t snd_handle_seq;             /* handle generator */
static SDL_atomic_t snd_active_count;           /* voices active (for SHOW SOUND) */
static uint32 snd_rand_state = 12345;           /* random phases (audio thread) */
static char snd_error[256] = "";                /* why audio output is not open */
static char snd_driver[64] = "";                /* SDL audio driver in use */
static char snd_device_name[256] = "";          /* host audio device in use */
static int32 snd_device_rate = 0;               /* obtained output rate */

static const char *snd_exe_dir (void)
{
static char dir[CBUFSIZE];
static t_bool done = FALSE;

if (!done) {
    char *base = SDL_GetBasePath ();            /* works without SDL_Init () */

    done = TRUE;
    if (base != NULL) {
        strlcpy (dir, base, sizeof (dir));
        SDL_free (base);
        }
    }
return dir;
}

/* Soft limiter: transparent below SND_LIMIT_KNEE, then a tanh curve that
   approaches (but never exceeds) full scale. */

static float snd_limit (float x)
{
float a = (float)fabs (x);
float y;

if (a <= SND_LIMIT_KNEE)
    return x;
y = SND_LIMIT_KNEE + (1.0f - SND_LIMIT_KNEE) *
    (float)tanh ((a - SND_LIMIT_KNEE) / (1.0f - SND_LIMIT_KNEE));
return (x < 0.0f) ? -y : y;
}

/* Small random generator for loop start phases (audio thread only) */

static uint32 snd_random (void)
{
snd_rand_state = snd_rand_state * 1103515245u + 12345u;
return (snd_rand_state >> 8);
}

/* Start a voice (audio thread) */

static SND_VOICE *snd_voice_start (SND_HANDLE handle, SND_SAMPLE *smp, SND_SAMPLE *chain,
                                   uint32 chain_flags, int32 group, float gain, uint32 flags, uint32 fade,
                                   double rate, uint32 delay, uint32 life, uint32 life_fade)
{
int32 i;
SND_VOICE *v;

if ((smp == NULL) || (smp->data == NULL) || (smp->frames == 0))
    return NULL;
for (i = 0; i < SND_VOICES; i++) {
    if (!snd_voices[i].active)
        break;
    }
if (i == SND_VOICES)                            /* all voices busy: drop the request */
    return NULL;
v = &snd_voices[i];
memset (v, 0, sizeof (*v));
v->active = TRUE;
v->handle = handle;
v->smp = smp;
v->chain = ((chain != NULL) && (chain->data != NULL)) ? chain : NULL;
v->chain_flags = chain_flags | SND_F_LOOP;
v->loop = (flags & SND_F_LOOP) ? TRUE : FALSE;
v->pos = ((flags & SND_F_RANDPHASE) && v->loop) ? (double)(snd_random () % smp->frames) : 0.0;
v->rate = (rate > 0.0) ? rate : 1.0;
v->delay = delay;
v->life = life;
v->life_fade = life_fade;
v->group = ((group >= 0) && (group < SND_GRP_COUNT)) ? group : SND_GRP_EVENT;
v->gain = gain;
if (fade > 0) {                                 /* fade in */
    v->fade = 0.0f;
    v->fade_step = 1.0f / (float)fade;
    }
else {
    v->fade = 1.0f;
    v->fade_step = 0.0f;
    }
SDL_AtomicAdd (&snd_active_count, 1);
return v;
}

/* Fade out and stop a voice (audio thread) */

static void snd_voice_fade_out (SND_VOICE *v, uint32 fade)
{
if (fade < SND_CLICK_FADE)
    fade = SND_CLICK_FADE;
v->chain = NULL;                                /* a stopped one-shot does not chain */
v->fade_step = -1.0f / (float)fade;
}

static void snd_voice_end (SND_VOICE *v)
{
v->active = FALSE;
SDL_AtomicAdd (&snd_active_count, -1);
}

/* Start a glide of the playback rate to target over frames (audio thread).
   SND_GLIDE_LINEAR changes the rate linearly.  SND_GLIDE_REEL changes
   1/rate^2 linearly: the rotation rate of a tape reel whose radius r
   changes at constant tape speed (r^2 grows or shrinks linearly with time,
   the rotation rate is proportional to 1/r). */

static void snd_voice_glide (SND_VOICE *v, double target, uint32 frames, int32 curve)
{
if (target <= 0.0)
    return;
if (frames == 0) {                              /* at once */
    v->rate = target;
    v->glide_left = 0;
    return;
    }
v->glide_curve = curve;
v->glide_left = frames;
if (curve == SND_GLIDE_REEL) {
    v->glide_inv2 = 1.0 / (v->rate * v->rate);
    v->glide_step = ((1.0 / (target * target)) - v->glide_inv2) / (double)frames;
    }
else
    v->glide_step = (target - v->rate) / (double)frames;
}

/* Execute the queued commands (audio thread, queue mutex held) */

static void snd_process_queue (void)
{
while (snd_qhead != snd_qtail) {
    SND_CMD *c = &snd_queue[snd_qhead];
    int32 i;

    snd_qhead = (snd_qhead + 1) % SND_QSIZE;
    switch (c->type) {
        case SND_CMD_PLAY: {
            SND_VOICE *v = snd_voice_start (c->handle, c->smp, c->chain, c->chain_flags, c->group, c->gain,
                                            c->flags, c->fade, c->rate, c->delay, c->life, c->life_fade);

            if ((v != NULL) && (c->rep_period > 0)) {   /* repeated one-shot (step clicks) */
                v->rep_left = c->rep_count;
                v->rep_period = c->rep_period;
                v->rep_pos = 0;
                }
            break;
            }
        case SND_CMD_RATE:
            for (i = 0; i < SND_VOICES; i++) {
                if (snd_voices[i].active && (snd_voices[i].handle == c->handle))
                    snd_voice_glide (&snd_voices[i], c->rate, c->fade, c->curve);
                }
            break;
        case SND_CMD_STOP:
            for (i = 0; i < SND_VOICES; i++) {
                if (snd_voices[i].active && (snd_voices[i].handle == c->handle)) {
                    if (snd_voices[i].delay > 0)        /* not started yet: gone */
                        snd_voice_end (&snd_voices[i]);
                    else
                        snd_voice_fade_out (&snd_voices[i], c->fade);
                    }
                }
            break;
        case SND_CMD_EXTEND:
            for (i = 0; i < SND_VOICES; i++) {
                if (snd_voices[i].active && (snd_voices[i].handle == c->handle) &&
                    (snd_voices[i].fade_step >= 0.0f))  /* not fading out */
                    snd_voices[i].life = c->life;
                }
            break;
        case SND_CMD_STOPALL:
            for (i = 0; i < SND_VOICES; i++) {
                if (snd_voices[i].active) {
                    if (snd_voices[i].delay > 0)        /* not started yet: gone */
                        snd_voice_end (&snd_voices[i]);
                    else
                        snd_voice_fade_out (&snd_voices[i], c->fade);
                    }
                }
            break;
        }
    }
}

/* The mixer: SDL audio callback.  Fills one block of mono float output. */

static void SDLCALL snd_callback (void *userdata, Uint8 *stream, int len)
{
float *out = (float *)stream;
int32 n = len / (int32)sizeof (float);
float group_gain[SND_GRP_COUNT];
float master;
int32 i, k;

if (SDL_TryLockMutex (snd_qlock) == 0) {        /* take new commands if the */
    snd_process_queue ();                       /* queue is free right now */
    SDL_UnlockMutex (snd_qlock);
    }
for (k = 0; k < SND_GRP_COUNT; k++)
    group_gain[k] = snd_group_mute[k] ? 0.0f : (float)snd_group_vol[k] / 100.0f;
master = snd_master_mute ? 0.0f : ((float)snd_master_vol / 100.0f) * SND_HEADROOM;

memset (out, 0, (size_t)len);
for (k = 0; k < SND_VOICES; k++) {
    SND_VOICE *v = &snd_voices[k];
    const float *data;
    uint32 frames;
    float g;

    if (!v->active)
        continue;
    if (v->delay >= (uint32)n) {                /* not started yet */
        v->delay -= (uint32)n;
        continue;
        }
    /* A one-shot followed by a loop (system starting -> running): when the
       crossfade region is reached within this block, start the loop with a
       fade-in, hand the handle on to it and fade the one-shot out. */
    if ((v->chain != NULL) && !v->loop) {
        uint32 xfade = SND_CHAIN_XFADE;

        if (xfade > v->smp->frames / 2)
            xfade = v->smp->frames / 2;
        if (xfade == 0)
            xfade = 1;
        if (v->pos + (double)n * v->rate >= (double)(v->smp->frames - xfade)) {
            snd_voice_start (v->handle, v->chain, NULL, 0, v->group, v->gain, v->chain_flags, xfade,
                             v->rate, 0, 0, 0);
            v->handle = 0;
            v->chain = NULL;
            v->fade_step = -1.0f / (float)xfade;
            }
        }
    data = v->smp->data;
    frames = v->smp->frames;
    g = v->gain * group_gain[v->group];
    for (i = 0; i < n; i++) {
        uint32 ip;
        float x;

        if (v->delay > 0) {                     /* starts within this block */
            v->delay--;
            continue;
            }
        if (v->life > 0) {                      /* limited lifetime: fade out */
            if (--v->life == 0)
                snd_voice_fade_out (v, v->life_fade);
            }
        if (v->rep_period > 0) {                /* repeated one-shot: the next repetition */
            if ((v->rep_pos >= v->rep_period) && (v->rep_left > 0)) {
                v->pos = 0.0;                   /* starts again (a longer sample is cut off) */
                v->rep_pos = 0;
                v->rep_left--;
                }
            v->rep_pos++;
            }

        if (v->pos >= (double)frames) {
            if (!v->loop) {                     /* one-shot finished */
                if (v->rep_left > 0)            /* silence until the next repetition */
                    continue;
                snd_voice_end (v);
                break;
                }
            v->pos = fmod (v->pos, (double)frames);
            }
        ip = (uint32)v->pos;
        if (v->rate == 1.0)                     /* as recorded: no interpolation */
            x = data[ip];
        else {                                  /* resample: linear interpolation */
            float fr = (float)(v->pos - (double)ip);
            uint32 nx = ip + 1;

            if (nx >= frames)                   /* a loop wraps, a one-shot ends */
                nx = v->loop ? 0 : ip;
            x = data[ip] + (data[nx] - data[ip]) * fr;
            }
        out[i] += x * g * v->fade;
        v->pos += v->rate;
        if (v->glide_left > 0) {                /* rate glide */
            if (v->glide_curve == SND_GLIDE_REEL) {
                v->glide_inv2 += v->glide_step;
                if (v->glide_inv2 > 1e-6)
                    v->rate = 1.0 / sqrt (v->glide_inv2);
                }
            else
                v->rate += v->glide_step;
            if (v->rate < 0.01)
                v->rate = 0.01;
            v->glide_left--;
            }
        if (v->fade_step != 0.0f) {
            v->fade += v->fade_step;
            if (v->fade >= 1.0f) {              /* fade-in complete */
                v->fade = 1.0f;
                v->fade_step = 0.0f;
                }
            else if (v->fade <= 0.0f) {         /* fade-out complete */
                snd_voice_end (v);
                break;
                }
            }
        }
    }
for (i = 0; i < n; i++)                         /* master volume, headroom, limiter */
    out[i] = snd_limit (out[i] * master);
}

/* Queue a command; returns FALSE if the queue is full or audio is closed.
   This is the only place where simulator threads synchronize with the
   audio thread, and the mutex is held only for the copy. */

static t_bool snd_queue_cmd (const SND_CMD *cmd)
{
t_bool ok = FALSE;
int32 next;

if ((snd_dev == 0) || (snd_qlock == NULL))
    return FALSE;
SDL_LockMutex (snd_qlock);
next = (snd_qtail + 1) % SND_QSIZE;
if (next != snd_qhead) {
    snd_queue[snd_qtail] = *cmd;
    snd_qtail = next;
    ok = TRUE;
    }
SDL_UnlockMutex (snd_qlock);
return ok;
}

static SND_HANDLE snd_play_rate (SND_SAMPLE *smp, SND_SAMPLE *chain, uint32 chain_flags,
                                 int32 group, double gain, uint32 flags, uint32 fade_ms,
                                 double rate);
static SND_HANDLE snd_play_full (SND_SAMPLE *smp, SND_SAMPLE *chain, uint32 chain_flags,
                                 int32 group, double gain, uint32 flags, uint32 fade_ms,
                                 double rate, double delay_s, double life_s, uint32 life_fade_ms);

static SND_HANDLE snd_play_ex (SND_SAMPLE *smp, SND_SAMPLE *chain, uint32 chain_flags,
                               int32 group, double gain, uint32 flags, uint32 fade_ms)
{
return snd_play_rate (smp, chain, chain_flags, group, gain, flags, fade_ms, 1.0);
}

static SND_HANDLE snd_play_rate (SND_SAMPLE *smp, SND_SAMPLE *chain, uint32 chain_flags,
                                 int32 group, double gain, uint32 flags, uint32 fade_ms,
                                 double rate)
{
return snd_play_full (smp, chain, chain_flags, group, gain, flags, fade_ms, rate, 0.0, 0.0, 0);
}

static SND_HANDLE snd_play_full (SND_SAMPLE *smp, SND_SAMPLE *chain, uint32 chain_flags,
                                 int32 group, double gain, uint32 flags, uint32 fade_ms,
                                 double rate, double delay_s, double life_s, uint32 life_fade_ms)
{
SND_CMD cmd;

if ((snd_dev == 0) || (smp == NULL) || (smp->data == NULL))
    return 0;
memset (&cmd, 0, sizeof (cmd));
cmd.type = SND_CMD_PLAY;
cmd.handle = SDL_AtomicAdd (&snd_handle_seq, 1) + 1;
cmd.smp = smp;
cmd.chain = chain;
cmd.chain_flags = chain_flags;
cmd.group = group;
cmd.gain = (float)gain;
cmd.flags = flags;
cmd.fade = (uint32)(((double)fade_ms * SND_RATE) / 1000.0);
cmd.rate = rate;
cmd.delay = (delay_s > 0.0) ? (uint32)(delay_s * SND_RATE) : 0;
cmd.life = (life_s > 0.0) ? (uint32)(life_s * SND_RATE) + 1 : 0;
cmd.life_fade = (uint32)(((double)life_fade_ms * SND_RATE) / 1000.0);
return snd_queue_cmd (&cmd) ? cmd.handle : 0;
}

/* A one-shot played count times, one every period_s seconds, starting
   after delay_s: e.g. the step clicks of a floppy head stepping over n
   tracks (one voice for the whole seek). */

static SND_HANDLE snd_play_repeat (SND_SAMPLE *smp, int32 group, double gain, uint32 count,
                                   double period_s, double delay_s)
{
SND_CMD cmd;

if ((snd_dev == 0) || (smp == NULL) || (smp->data == NULL) || (count == 0))
    return 0;
memset (&cmd, 0, sizeof (cmd));
cmd.type = SND_CMD_PLAY;
cmd.handle = SDL_AtomicAdd (&snd_handle_seq, 1) + 1;
cmd.smp = smp;
cmd.group = group;
cmd.gain = (float)gain;
cmd.rate = 1.0;
cmd.delay = (delay_s > 0.0) ? (uint32)(delay_s * SND_RATE) : 0;
cmd.rep_count = count - 1;
cmd.rep_period = (uint32)(period_s * SND_RATE);
if (cmd.rep_period == 0)
    cmd.rep_period = 1;
return snd_queue_cmd (&cmd) ? cmd.handle : 0;
}

static void snd_extend_ex (SND_HANDLE handle, double secs)
{
SND_CMD cmd;

if (handle == 0)
    return;
memset (&cmd, 0, sizeof (cmd));
cmd.type = SND_CMD_EXTEND;
cmd.handle = handle;
cmd.life = (secs > 0.0) ? (uint32)(secs * SND_RATE) + 1 : 0;
snd_queue_cmd (&cmd);
}

static void snd_glide_ex (SND_HANDLE handle, double rate, double secs, int32 curve)
{
SND_CMD cmd;

if ((handle == 0) || (rate <= 0.0))
    return;
memset (&cmd, 0, sizeof (cmd));
cmd.type = SND_CMD_RATE;
cmd.handle = handle;
cmd.rate = rate;
cmd.curve = curve;
cmd.fade = (secs > 0.0) ? (uint32)(secs * SND_RATE) : 0;
snd_queue_cmd (&cmd);
}

static void snd_stop_ex (int32 type, SND_HANDLE handle, uint32 fade_ms)
{
SND_CMD cmd;

memset (&cmd, 0, sizeof (cmd));
cmd.type = type;
cmd.handle = handle;
cmd.fade = (uint32)(((double)fade_ms * SND_RATE) / 1000.0);
snd_queue_cmd (&cmd);
}

/* Open the audio output.  On failure the reason is kept for SHOW SOUND
   and everything continues silently. */

static t_bool snd_audio_open (void)
{
SDL_AudioSpec want, have;
const char *drv;

if (snd_dev != 0)
    return TRUE;
snd_error[0] = '\0';
SDL_SetMainReady ();                            /* main () is not SDL's */
if (SDL_InitSubSystem (SDL_INIT_AUDIO) != 0) {  /* reference counted by SDL */
    snprintf (snd_error, sizeof (snd_error), "SDL audio init failed: %s", SDL_GetError ());
    return FALSE;
    }
snd_subsystem = TRUE;
memset (&want, 0, sizeof (want));
want.freq = SND_RATE;
want.format = AUDIO_F32SYS;
want.channels = 1;
want.samples = SND_BLOCK;
want.callback = snd_callback;
snd_qlock = SDL_CreateMutex ();
memset (snd_voices, 0, sizeof (snd_voices));
SDL_AtomicSet (&snd_active_count, 0);
snd_qhead = snd_qtail = 0;
/* No changes allowed: SDL converts our format to the hardware format */
snd_dev = SDL_OpenAudioDevice (NULL, 0, &want, &have, 0);
if ((snd_dev == 0) || (snd_qlock == NULL)) {
    snprintf (snd_error, sizeof (snd_error), "no audio device: %s", SDL_GetError ());
    if (snd_dev != 0)
        SDL_CloseAudioDevice (snd_dev);
    snd_dev = 0;
    if (snd_qlock != NULL)
        SDL_DestroyMutex (snd_qlock);
    snd_qlock = NULL;
    SDL_QuitSubSystem (SDL_INIT_AUDIO);
    snd_subsystem = FALSE;
    return FALSE;
    }
drv = SDL_GetCurrentAudioDriver ();
strlcpy (snd_driver, drv ? drv : "?", sizeof (snd_driver));
snd_device_rate = have.freq;
#if SDL_VERSION_ATLEAST(2, 24, 0)
if (1) {                                        /* name of the default device */
    char *name = NULL;
    SDL_AudioSpec spec;

    if (SDL_GetDefaultAudioInfo (&name, &spec, 0) == 0) {
        strlcpy (snd_device_name, name ? name : "", sizeof (snd_device_name));
        SDL_free (name);
        }
    }
#endif
SDL_PauseAudioDevice (snd_dev, 0);              /* start the callbacks */
return TRUE;
}

static void snd_audio_close (void)
{
if (snd_dev != 0) {
    SDL_CloseAudioDevice (snd_dev);             /* waits for a running callback */
    snd_dev = 0;
    }
if (snd_qlock != NULL) {
    SDL_DestroyMutex (snd_qlock);
    snd_qlock = NULL;
    }
if (snd_subsystem) {
    SDL_QuitSubSystem (SDL_INIT_AUDIO);
    snd_subsystem = FALSE;
    }
memset (snd_voices, 0, sizeof (snd_voices));
SDL_AtomicSet (&snd_active_count, 0);
snd_qhead = snd_qtail = 0;
snd_driver[0] = '\0';
snd_device_name[0] = '\0';
}

/* Load and convert a WAV file to the internal format (float, mono,
   SND_RATE).  SDL_AudioStream does the format and rate conversion. */

static t_bool snd_load_wav (SND_SAMPLE *smp)
{
SDL_AudioSpec spec;
Uint8 *buf = NULL;
Uint32 len = 0;
SDL_AudioStream *stream;
int avail;

if (SDL_LoadWAV (smp->path, &spec, &buf, &len) == NULL) {
    sim_printf ("Sound: can't read %s: %s\n", smp->path, SDL_GetError ());
    return FALSE;
    }
stream = SDL_NewAudioStream (spec.format, spec.channels, spec.freq,
                             AUDIO_F32SYS, 1, SND_RATE);
if ((stream == NULL) ||
    (SDL_AudioStreamPut (stream, buf, (int)len) != 0) ||
    (SDL_AudioStreamFlush (stream) != 0)) {
    sim_printf ("Sound: can't convert %s: %s\n", smp->path, SDL_GetError ());
    if (stream != NULL)
        SDL_FreeAudioStream (stream);
    SDL_FreeWAV (buf);
    return FALSE;
    }
SDL_FreeWAV (buf);
avail = SDL_AudioStreamAvailable (stream);
smp->data = (float *)malloc ((size_t)avail + sizeof (float));
if (smp->data == NULL) {
    SDL_FreeAudioStream (stream);
    return FALSE;
    }
smp->frames = (uint32)(SDL_AudioStreamGet (stream, smp->data, avail) / (int)sizeof (float));
SDL_FreeAudioStream (stream);
if (smp->frames == 0) {
    free (smp->data);
    smp->data = NULL;
    sim_printf ("Sound: %s contains no audio data\n", smp->path);
    return FALSE;
    }
return TRUE;
}

static int32 snd_active_voices (void)
{
return (snd_dev != 0) ? SDL_AtomicGet (&snd_active_count) : 0;
}

static void snd_show_backend (FILE *st)
{
SDL_version v;

SDL_GetVersion (&v);
if (snd_dev != 0) {
    fprintf (st, "Output:  SDL %d.%d.%d, driver %s", v.major, v.minor, v.patch, snd_driver);
    if (snd_device_name[0] != '\0')
        fprintf (st, ", device \"%s\"", snd_device_name);
    fprintf (st, ", %d Hz\n", snd_device_rate);
    }
else {
    if (snd_enabled)
        fprintf (st, "Output:  none (%s) - sounds are ignored\n", snd_error[0] ? snd_error : "not open");
    else
        fprintf (st, "Output:  SDL %d.%d.%d available, opened when sound is enabled\n", v.major, v.minor, v.patch);
    }
}

#else /* !defined (USE_SIM_SOUND) */

/* ------------------------------------------------------------------------
   3. Stubs: built without SDL2.  The realism mode and the power state still
      work; nothing is ever played.
   ------------------------------------------------------------------------ */

static const char *snd_exe_dir (void)
{
return "";
}

static t_bool snd_audio_open (void)
{
return FALSE;
}

static void snd_audio_close (void)
{
}

static t_bool snd_load_wav (SND_SAMPLE *smp)
{
return FALSE;
}

static SND_HANDLE snd_play_ex (SND_SAMPLE *smp, SND_SAMPLE *chain, uint32 chain_flags,
                               int32 group, double gain, uint32 flags, uint32 fade_ms)
{
return 0;
}

static SND_HANDLE snd_play_rate (SND_SAMPLE *smp, SND_SAMPLE *chain, uint32 chain_flags,
                                 int32 group, double gain, uint32 flags, uint32 fade_ms,
                                 double rate)
{
return 0;
}

static void snd_glide_ex (SND_HANDLE handle, double rate, double secs, int32 curve)
{
}

static void snd_extend_ex (SND_HANDLE handle, double secs)
{
}

static SND_HANDLE snd_play_repeat (SND_SAMPLE *smp, int32 group, double gain, uint32 count,
                                   double period_s, double delay_s)
{
return 0;
}

static SND_HANDLE snd_play_full (SND_SAMPLE *smp, SND_SAMPLE *chain, uint32 chain_flags,
                                 int32 group, double gain, uint32 flags, uint32 fade_ms,
                                 double rate, double delay_s, double life_s, uint32 life_fade_ms)
{
return 0;
}

#define SND_CMD_STOP    2
#define SND_CMD_STOPALL 3

static void snd_stop_ex (int32 type, SND_HANDLE handle, uint32 fade_ms)
{
}

static int32 snd_active_voices (void)
{
return 0;
}

static void snd_show_backend (FILE *st)
{
fprintf (st, "Output:  none - this simulator was built without SDL2 sound support\n");
}

#define snd_dev 0

#endif /* USE_SIM_SOUND */

/* ------------------------------------------------------------------------
   4. Public functions
   ------------------------------------------------------------------------ */

t_bool sim_sound_enabled (void)
{
return snd_enabled;
}

t_bool sim_sound_available (void)
{
return (snd_dev != 0);
}

t_bool sim_sound_powered (void)
{
return snd_power;
}

void sim_sound_set_throttle (const char *spec)
{
strlcpy (snd_throttle, spec ? spec : "", sizeof (snd_throttle));
}

/* Realism mode: apply or remove the simulator's default throttle.  A
   throttle the user set himself is never touched. */

static void snd_apply_throttle (t_bool enable)
{
if (enable) {
    if ((snd_throttle[0] != '\0') && !sim_throt_is_set ()) {
        if (sim_set_throt (1, snd_throttle) == SCPE_OK) {
            snd_throttle_set = TRUE;
            /* realistic from power-on: no unthrottled warm-up phase */
            snd_throttle_delay = sim_throt_set_delay (0);
            sim_printf ("Sound: realistic CPU speed, THROTTLE %s\n", snd_throttle);
            }
        }
    }
else {
    if (snd_throttle_set) {
        if (sim_throt_is_set ())
            sim_set_throt (0, NULL);
        sim_throt_set_delay (snd_throttle_delay);       /* restore the warm-up */
        }
    snd_throttle_set = FALSE;
    }
}

void sim_sound_set_system (const char *dir, const char *starting,
                           const char *running, const char *stopping)
{
strlcpy (snd_sys_dir, dir ? dir : "", sizeof (snd_sys_dir));
strlcpy (snd_sys_default[SND_SYS_STARTING], starting ? starting : "", CBUFSIZE);
strlcpy (snd_sys_default[SND_SYS_RUNNING], running ? running : "", CBUFSIZE);
strlcpy (snd_sys_default[SND_SYS_STOPPING], stopping ? stopping : "", CBUFSIZE);
}

static SND_SAMPLE *snd_load (const char *dir, const char *file, t_bool quiet)
{
SND_SAMPLE *smp;
char key[CBUFSIZE];
char path[CBUFSIZE];

if (!sim_sound_available () || (file == NULL) || (*file == '\0'))
    return NULL;
strlcpy (key, dir ? dir : "", sizeof (key));
strlcat (key, "/", sizeof (key));
strlcat (key, file, sizeof (key));
/* The file is searched every time (a few fopen calls), so that a changed
   SET SOUND PATH takes effect; the cache is keyed by the resolved file. */
if (!snd_find_file (path, sizeof (path), dir, file)) {
    for (smp = snd_cache; smp != NULL; smp = smp->next) {
        if ((smp->path == NULL) && (strcmp (smp->key, key) == 0))
            return NULL;                        /* already reported */
        }
    smp = (SND_SAMPLE *)calloc (1, sizeof (*smp));
    if (smp != NULL) {                          /* remember the failure: warn once */
        smp->key = strdup (key);
        smp->next = snd_cache;
        snd_cache = smp;
        }
    if (!quiet)
        sim_printf ("Sound: file %s not found (sound directory \"%s\")\n", file, dir ? dir : "");
    return NULL;
    }
for (smp = snd_cache; smp != NULL; smp = smp->next) {   /* loaded before? */
    if ((smp->path != NULL) && (strcmp (smp->path, path) == 0))
        return (smp->data != NULL) ? smp : NULL;
    }
smp = (SND_SAMPLE *)calloc (1, sizeof (*smp));
if (smp == NULL)
    return NULL;
smp->key = strdup (key);
smp->path = strdup (path);
smp->next = snd_cache;                          /* unreadable files stay cached: warn once */
snd_cache = smp;
if (!snd_load_wav (smp))
    return NULL;
return smp;
}

SND_SAMPLE *sim_sound_load (const char *dir, const char *file)
{
return snd_load (dir, file, FALSE);
}

/* An optional sample: no message if it does not exist (a file that exists
   but can not be read is still reported) */

SND_SAMPLE *sim_sound_load_optional (const char *dir, const char *file)
{
return snd_load (dir, file, TRUE);
}

/* Remember for SHOW SOUND that "dir (what)" uses a default (once each) */

static void snd_note_default (const char *dir, const char *what, const char *ddir)
{
char entry[CBUFSIZE];
size_t len;
char *n;

snprintf (entry, sizeof (entry), "%s (%s) -> %s", (dir && *dir) ? dir : "?", what, ddir);
if ((snd_default_uses != NULL) && (strstr (snd_default_uses, entry) != NULL))
    return;
len = (snd_default_uses ? strlen (snd_default_uses) + 2 : 0) + strlen (entry) + 1;
n = (char *) realloc (snd_default_uses, len);
if (n == NULL)
    return;
if (snd_default_uses == NULL)
    n[0] = '\0';
else
    strlcat (n, ", ", len);
strlcat (n, entry, len);
snd_default_uses = n;
}

/* A sound with a default: the device's own file dir/file if it exists
   (no message if not), otherwise the default ddir/dfile (reported once if
   that is missing too).  ddir == NULL: no default, dir/file is reported
   if missing. */

static SND_SAMPLE *snd_load_with_default (const char *dir, const char *file,
                                          const char *ddir, const char *dfile, const char *what)
{
SND_SAMPLE *smp;

if (ddir == NULL)
    return sim_sound_load (dir, file);
if ((smp = sim_sound_load_optional (dir, file)) != NULL)
    return smp;
if ((smp = sim_sound_load (ddir, dfile)) != NULL)
    snd_note_default (dir, what, ddir);
return smp;
}

void sim_sound_use_default_cpu (void)
{
snd_sys_use_default = TRUE;
}

/* Forget cached entries of a file, so that SET SOUND PATH changes and newly
   added files are picked up on the next load. */

static void snd_cache_forget_failures (void)
{
SND_SAMPLE *smp;

for (smp = snd_cache; smp != NULL; smp = smp->next) {
    if (smp->data == NULL) {                    /* never matches again */
        if (smp->key != NULL)
            smp->key[0] = '\0';
        if (smp->path != NULL)
            smp->path[0] = '\0';
        }
    }
}

SND_HANDLE sim_sound_play (SND_SAMPLE *smp, int32 group, double gain, uint32 flags)
{
return snd_play_ex (smp, NULL, 0, group, gain, flags, 0);
}

SND_HANDLE sim_sound_play_rate (SND_SAMPLE *smp, int32 group, double gain, uint32 flags,
                                double rate, uint32 fade_ms)
{
return snd_play_rate (smp, NULL, 0, group, gain, flags, fade_ms, rate);
}

void sim_sound_glide (SND_HANDLE handle, double rate, double secs, int32 curve)
{
snd_glide_ex (handle, rate, secs, curve);
}

void sim_sound_extend (SND_HANDLE handle, double secs)
{
snd_extend_ex (handle, secs);
}

SND_HANDLE sim_sound_play_timed (SND_SAMPLE *smp, int32 group, double gain, uint32 flags,
                                 double rate, double delay_s, double life_s, uint32 fade_ms)
{
return snd_play_full (smp, NULL, 0, group, gain, flags, fade_ms, rate, delay_s, life_s, fade_ms);
}

/* Unit loops (see sim_sound.h) */

void sim_sound_unit_load (SND_UNITLOOP *ul, const char *dir, const char *start,
                          const char *run, const char *stop)
{
int32 i;
SND_UNITLOOP **n;

ul->start = sim_sound_load_optional (dir, start);
ul->run = sim_sound_load_optional (dir, run);
ul->stop = sim_sound_load_optional (dir, stop);
ul->handle = 0;
ul->active = ul->silenced = FALSE;
for (i = 0; i < snd_n_unitloops; i++)           /* register it (once) for */
    if (snd_unitloops[i] == ul)                 /* power off/on */
        return;
n = (SND_UNITLOOP **) realloc ((void *) snd_unitloops, (snd_n_unitloops + 1) * sizeof (*n));
if (n == NULL)
    return;
snd_unitloops = n;
snd_unitloops[snd_n_unitloops++] = ul;
}

void sim_sound_unit_start (SND_UNITLOOP *ul, int32 group, double gain)
{
sim_sound_stop (ul->handle, 0);                 /* a stopping unit restarted */
ul->active = TRUE;
ul->silenced = FALSE;
ul->group = group;
ul->gain = gain;
if (ul->start != NULL)
    ul->handle = snd_play_ex (ul->start, ul->run, SND_F_RANDPHASE, group, gain, 0, 0);
else
    ul->handle = snd_play_ex (ul->run, NULL, 0, group, gain, SND_F_LOOP | SND_F_RANDPHASE, 200);
}

void sim_sound_unit_stop (SND_UNITLOOP *ul, int32 group, double gain)
{
sim_sound_stop (ul->handle, 200);
ul->handle = 0;
ul->active = ul->silenced = FALSE;
if (ul->stop != NULL)
    snd_play_ex (ul->stop, NULL, 0, group, gain, 0, 0);
}

/* Power on: a unit that kept running while the power was off (its state is
   the device's business, power off only silences it) gets its running loop
   back, fading in. */

static void snd_unitloops_restore (void)
{
int32 i;

for (i = 0; i < snd_n_unitloops; i++) {
    SND_UNITLOOP *ul = snd_unitloops[i];

    if ((ul != NULL) && ul->active && ul->silenced) {
        ul->silenced = FALSE;
        ul->handle = snd_play_ex (ul->run, NULL, 0, ul->group, ul->gain,
                                  SND_F_LOOP | SND_F_RANDPHASE, 500);
        }
    }
}

/* Power off silences every voice; remember which unit loops were active */

static void snd_unitloops_silence (void)
{
int32 i;

for (i = 0; i < snd_n_unitloops; i++) {
    SND_UNITLOOP *ul = snd_unitloops[i];

    if ((ul != NULL) && ul->active) {
        ul->silenced = TRUE;
        ul->handle = 0;
        }
    }
}

/* Run hooks (see sim_sound.h) */

void sim_sound_register_run_hook (void (*fn)(t_bool running))
{
int32 i;
void (**n) (t_bool running);

for (i = 0; i < snd_n_runhooks; i++)
    if (snd_runhooks[i] == fn)
        return;
n = (void (**) (t_bool)) realloc ((void *) snd_runhooks, (snd_n_runhooks + 1) * sizeof (*n));
if (n == NULL)
    return;
snd_runhooks = n;
snd_runhooks[snd_n_runhooks++] = fn;
}

static void snd_timers_run_state (t_bool running);

static void snd_rt_run_state (t_bool running);

void sim_sound_run_state (t_bool running)
{
int32 i;

snd_rt_run_state (running);                     /* the realtime clock first */
snd_timers_run_state (running);
for (i = 0; i < snd_n_runhooks; i++)
    snd_runhooks[i] (running);
}

/* Realistic durations (see sim_sound.h) */

double sim_sound_inst_per_us (void)
{
double ips = sim_throt_target_ips ();

if (ips <= 0.0)
    ips = sim_timer_inst_per_sec ();
return ips / 1000000.0;
}

void sim_sound_activate_us (UNIT *uptr, double usecs)
{
double inst = usecs * sim_sound_inst_per_us ();

if (inst < 1.0)
    inst = 1.0;
if (inst > 2000000000.0)
    inst = 2000000000.0;
sim_activate_abs (uptr, (int32)inst);                  /* (an asynchronous I/O completion
                                                           has activated the unit already:
                                                           the realistic delay replaces it) */
}

/* Realtime timers (see sim_sound.h)

   A timer measures a realistic duration: simulated time while the CPU
   runs (the guest always sees the full time), wall time while the
   simulator is stopped at the sim> prompt.  Used by the spindles and the
   tape transports.

   Registry: there is no fixed limit.  Timers are registered in chunks of
   SND_TMR_CHUNK, each with its own internal timer device per kind
   (INT-SPINDLE, INT-SPINDLE2, ... for disk spindles; INT-REEL, ... for
   tape transports), so a timer unit never moves while it is on the event
   queue.  (Up to sound patch v12 there was a fixed table of 32, then 64,
   spindles: known issues case 23.)  A timer unit finds its SND_TIMER
   through up7. */

#define SND_TMR_CHUNK   32

typedef struct snd_tmr_chunk {
    SND_TIMER               *t[SND_TMR_CHUNK];          /* registered timers */
    UNIT                    units[SND_TMR_CHUNK];       /* their timer units */
    DEVICE                  dev;                        /* internal device of the units */
    char                    name[24];                   /* its name */
    const char              *kind;                      /* "SPINDLE", "REEL", ... */
    struct snd_tmr_chunk    *next;
    } SND_TMR_CHUNK_T;

static SND_TMR_CHUNK_T *snd_tmr_chunks = NULL;          /* all chunks, in order */

static const char *snd_tmr_description (DEVICE *dptr)
{
if ((dptr->name != NULL) && (strncmp (dptr->name, "INT-REEL", 8) == 0))
    return "Tape transport timers (sound realism mode)";
return "Disk spindle timers (sound realism mode)";
}

/* A free slot of the given kind: its chunk and index (a new chunk if all
   are full); FALSE only if out of memory */

static t_bool snd_tmr_slot (const char *kind, SND_TMR_CHUNK_T **chunk, int32 *index)
{
SND_TMR_CHUNK_T *c, **link = &snd_tmr_chunks;
int32 i, n = 0;

for (c = snd_tmr_chunks; c != NULL; c = c->next) {
    if (strcmp (c->kind, kind) == 0) {
        for (i = 0; i < SND_TMR_CHUNK; i++)
            if (c->t[i] == NULL) {
                *chunk = c;
                *index = i;
                return TRUE;
                }
        n = n + 1;
        }
    link = &c->next;
    }
c = (SND_TMR_CHUNK_T *) calloc (1, sizeof (*c));
if (c == NULL)
    return FALSE;
c->kind = kind;
if (n == 0)
    snprintf (c->name, sizeof (c->name), "INT-%s", kind);
else
    snprintf (c->name, sizeof (c->name), "INT-%s%d", kind, n + 1);
c->dev.name = c->name;
c->dev.units = c->units;
c->dev.numunits = SND_TMR_CHUNK;
c->dev.flags = DEV_NOSAVE;
c->dev.description = &snd_tmr_description;
*link = c;
sim_register_internal_device (&c->dev);
*chunk = c;
*index = 0;
return TRUE;
}

/* The time is over: call the owner (which may start the timer again) */

static void snd_tmr_expired (SND_TIMER *t)
{
t->busy = FALSE;
t->paused = FALSE;
t->left_us = 0.0;
if (t->done != NULL)
    t->done (t);
}

static t_stat snd_tmr_svc (UNIT *uptr)
{
SND_TIMER *t = (SND_TIMER *) uptr->up7;

if ((t != NULL) && t->busy)
    snd_tmr_expired (t);
return SCPE_OK;
}

/* At the prompt: deduct the wall time since the last look; expire the
   timer if the time is over */

static void snd_tmr_catch_up (SND_TIMER *t)
{
uint32 now;

if (!t->busy || !t->paused)
    return;
now = sim_os_msec ();
t->left_us -= 1000.0 * (double)(now - t->ref_ms);
t->ref_ms = now;
if (t->left_us < 1.0)
    snd_tmr_expired (t);
}

/* Execution starts: schedule what is left; execution stops: keep it */

static void snd_tmr_run_state (SND_TIMER *t, t_bool running)
{
if ((t == NULL) || !t->busy)
    return;
if (running) {
    if (t->paused) {
        snd_tmr_catch_up (t);
        if (t->busy && t->paused) {
            t->paused = FALSE;
            sim_cancel (t->unit);
            sim_sound_activate_us (t->unit, t->left_us);
            }
        }
    else if (!sim_is_active (t->unit))          /* lost (event queue emptied): finish it */
        snd_tmr_expired (t);
    }
else if (!t->paused) {
    t->left_us = sim_is_active (t->unit) ?
        (double)sim_activate_time (t->unit) / sim_sound_inst_per_us () : 0.0;
    sim_cancel (t->unit);
    t->paused = TRUE;
    t->ref_ms = sim_os_msec ();
    }
}

static void snd_timers_run_state (t_bool running)
{
SND_TMR_CHUNK_T *c;
int32 i;

for (c = snd_tmr_chunks; c != NULL; c = c->next)
    for (i = 0; i < SND_TMR_CHUNK; i++)
        snd_tmr_run_state (c->t[i], running);
}

t_bool sim_sound_timer_init (SND_TIMER *t, const char *kind, void (*done) (SND_TIMER *t), void *ctx)
{
SND_TMR_CHUNK_T *c;
int32 i;

if (t->unit != NULL)                            /* already initialized */
    return TRUE;
if (!snd_tmr_slot (kind, &c, &i))               /* out of memory */
    return FALSE;
memset (t, 0, sizeof (*t));
t->done = done;
t->ctx = ctx;
memset (&c->units[i], 0, sizeof (c->units[i]));
c->units[i].action = &snd_tmr_svc;
c->units[i].up7 = t;
t->unit = &c->units[i];
c->t[i] = t;
return TRUE;
}

void sim_sound_timer_start (SND_TIMER *t, double usecs)
{
if (t->unit == NULL)
    return;
sim_cancel (t->unit);
t->busy = TRUE;
t->left_us = (usecs > 1.0) ? usecs : 1.0;
if (sim_is_running) {
    t->paused = FALSE;
    sim_sound_activate_us (t->unit, t->left_us);
    }
else {
    t->paused = TRUE;
    t->ref_ms = sim_os_msec ();
    }
}

void sim_sound_timer_cancel (SND_TIMER *t)
{
if (t->unit != NULL)
    sim_cancel (t->unit);
t->busy = FALSE;
t->paused = FALSE;
t->left_us = 0.0;
}

t_bool sim_sound_timer_busy (SND_TIMER *t)
{
if (!sim_is_running)                            /* prompt: wall time passed */
    snd_tmr_catch_up (t);
return t->busy;
}

double sim_sound_timer_left_s (SND_TIMER *t)
{
if (!t->busy)
    return 0.0;
if (t->paused)
    return t->left_us / 1000000.0;
if (!sim_is_active (t->unit))
    return 0.0;
return (double)sim_activate_time (t->unit) / sim_sound_inst_per_us () / 1000000.0;
}

/* Spindles (see sim_sound.h) */

/* A phase (spin-up or spin-down) is over */

static void snd_spindle_phase_done (SND_SPINDLE *sp)
{
sim_sound_timer_cancel (&sp->tmr);
if (sp->state == SND_SPIN_UP) {
    sp->state = SND_SPIN_READY;
    if (sp->on_ready != NULL)
        sp->on_ready (sp->uptr);
    }
else if (sp->state == SND_SPIN_DOWN) {
    sp->state = SND_SPIN_STOPPED;
    if (sp->on_stopped != NULL)
        sp->on_stopped (sp->uptr);
    }
}

static void snd_spindle_tmr_done (SND_TIMER *t)
{
snd_spindle_phase_done ((SND_SPINDLE *) t->ctx);
}

void sim_spindle_init (SND_SPINDLE *sp, UNIT *uptr, int32 *spinup_ms, int32 *spindown_ms,
                       void (*on_ready) (UNIT *uptr), void (*on_stopped) (UNIT *uptr),
                       int32 group, double gain)
{
if (sp->uptr != NULL)                           /* already initialized */
    return;
memset (sp, 0, sizeof (*sp));
if (!sim_sound_timer_init (&sp->tmr, "SPINDLE", &snd_spindle_tmr_done, sp)) {
    sim_printf ("Sound: out of memory for the spindle of %s\n", sim_uname (uptr));
    return;                                     /* sp->uptr stays NULL (RQ: the */
    }                                           /* drive works without realism) */
sp->uptr = uptr;
sp->spinup_ms = spinup_ms;
sp->spindown_ms = spindown_ms;
sp->on_ready = on_ready;
sp->on_stopped = on_stopped;
sp->group = group;
sp->gain = gain;
sp->state = SND_SPIN_STOPPED;
}

void sim_spindle_load (SND_SPINDLE *sp, const char *dir, const char *prefix)
{
char up[CBUFSIZE], run[CBUFSIZE], down[CBUFSIZE];

snprintf (up, sizeof (up), "%s_spinup.wav", prefix);
snprintf (run, sizeof (run), "%s_running.wav", prefix);
snprintf (down, sizeof (down), "%s_spindown.wav", prefix);
sim_sound_unit_load (&sp->sound, dir, up, run, down);
}

void sim_spindle_start (SND_SPINDLE *sp)
{
if ((sp->uptr == NULL) || (sp->state == SND_SPIN_UP) || (sp->state == SND_SPIN_READY))
    return;
sim_sound_unit_start (&sp->sound, sp->group, sp->gain);
if (!snd_enabled) {                             /* normal mode: at once */
    sp->state = SND_SPIN_UP;
    snd_spindle_phase_done (sp);
    return;
    }
sp->state = SND_SPIN_UP;
sim_sound_timer_start (&sp->tmr, 1000.0 * ((*sp->spinup_ms > 0) ? *sp->spinup_ms : 1));
}

void sim_spindle_stop (SND_SPINDLE *sp)
{
if ((sp->uptr == NULL) || (sp->state == SND_SPIN_STOPPED) || (sp->state == SND_SPIN_DOWN))
    return;
sim_sound_unit_stop (&sp->sound, sp->group, sp->gain);
sim_sound_timer_cancel (&sp->tmr);
sp->state = SND_SPIN_DOWN;
if (!snd_enabled || (*sp->spindown_ms <= 0))    /* normal mode: at once */
    snd_spindle_phase_done (sp);
else
    sim_sound_timer_start (&sp->tmr, 1000.0 * *sp->spindown_ms);
}

void sim_spindle_off (SND_SPINDLE *sp)
{
if (sp->uptr == NULL)
    return;
if ((sp->state == SND_SPIN_UP) || (sp->state == SND_SPIN_READY))
    sim_sound_unit_stop (&sp->sound, sp->group, sp->gain);
sim_sound_timer_cancel (&sp->tmr);
sp->state = SND_SPIN_STOPPED;
}

void sim_spindle_ready (SND_SPINDLE *sp)
{
if (sp->uptr == NULL)
    return;
sim_sound_timer_cancel (&sp->tmr);
sp->state = SND_SPIN_READY;
}

int32 sim_spindle_state (SND_SPINDLE *sp)
{
if (sp->uptr != NULL)
    sim_sound_timer_busy (&sp->tmr);            /* prompt: wall time passed */
return sp->state;
}

void sim_spindle_wait (SND_SPINDLE *sp)
{
int32 st = sim_spindle_state (sp);

if (sim_is_running || ((st != SND_SPIN_UP) && (st != SND_SPIN_DOWN)))
    return;
if (st == SND_SPIN_UP)
    sim_printf ("%s: spinning up, ready in %.0f s ...\n", sim_uname (sp->uptr), sim_sound_timer_left_s (&sp->tmr));
else
    sim_printf ("%s: spinning down, at rest in %.0f s ...\n", sim_uname (sp->uptr), sim_sound_timer_left_s (&sp->tmr));
while (sim_spindle_state (sp) == st)
    sim_os_ms_sleep (50);
if (sp->state == SND_SPIN_READY)
    sim_printf ("%s: up to speed, ready\n", sim_uname (sp->uptr));
else if (sp->state == SND_SPIN_STOPPED)
    sim_printf ("%s: at rest\n", sim_uname (sp->uptr));
}

t_stat sim_spindle_show (FILE *st, SND_SPINDLE *sp)
{
switch (sim_spindle_state (sp)) {
    case SND_SPIN_UP:
        fprintf (st, "spinning up, %.1f s left", sim_sound_timer_left_s (&sp->tmr));
        break;
    case SND_SPIN_READY:
        fprintf (st, "online");
        break;
    case SND_SPIN_DOWN:
        fprintf (st, "spinning down, %.1f s left", sim_sound_timer_left_s (&sp->tmr));
        break;
    default:
        fprintf (st, "offline");
        break;
    }
return SCPE_OK;
}

/* Tape transports (see sim_sound.h) */

/* Reel geometry for the pitch of the reel motors: a reel holding len
   inches of tape has the radius sqrt (hub^2 + len * thickness / pi); its
   rotation rate at tape speed v is v / (2 pi r).  The rewind loop
   (rewind.wav) is a reel motor recorded (synthesized) at REEL_SND_REF_RPS
   revolutions per second; it is played at rate = rps / REEL_SND_REF_RPS. */

#ifndef M_PI
#define M_PI                3.14159265358979323846
#endif
#define REEL_HUB_R          1.845               /* hub radius (3.69 in hub) */
#define REEL_TAPE_T         0.0019              /* tape thickness in the pack (1.5 mil + air) */
#define REEL_SND_REF_RPS    8.0                 /* rotation rate of rewind.wav */
#define REEL_POS_UNIT       10000.0             /* r->pos is in 1/10000 in */

#define REEL_SMP_LOAD       0                   /* indices of the transport sounds */
#define REEL_SMP_UNLOAD     1
#define REEL_SMP_RSTART     2
#define REEL_SMP_REWIND     3
#define REEL_SMP_RSTOP      4
#define REEL_SMP_START      5
#define REEL_SMP_MOTION     6
#define REEL_SMP_STOP       7
#define REEL_H_REWIND2      8                   /* second reel's rewind voice */

#define REEL_BATCH          8                   /* start/stop drive: records per sound batch */

static const char *snd_reel_names[SND_REEL_NSMP] = {
    "load", "unload", "rewind_start", "rewind", "rewind_stop", "start", "motion", "stop" };

static double snd_reel_len_ft (SND_REEL *r)
{
int32 ft = (r->reel_ft != NULL) ? *r->reel_ft : 2400;

return ((ft == 600) || (ft == 1200) || (ft == 2400)) ? (double)ft : 2400.0;
}

double sim_reel_pos_in (SND_REEL *r)
{
return (double)r->pos / REEL_POS_UNIT;
}

static void snd_reel_set_pos_in (SND_REEL *r, double in)
{
double max = (snd_reel_len_ft (r) - REEL_LEADER_FT) * 12.0;

if (in < 0.0)
    in = 0.0;
if (in > max)
    in = max;
r->pos = (uint32)(in * REEL_POS_UNIT + 0.5);
}

void sim_reel_set_bot (SND_REEL *r)
{
r->pos = 0;
}

t_bool sim_reel_past_eot (SND_REEL *r)
{
return (sim_reel_pos_in (r) >= (snd_reel_len_ft (r) - REEL_LEADER_FT - REEL_EOT_FT) * 12.0);
}

t_bool sim_reel_past_end (SND_REEL *r)
{
return (sim_reel_pos_in (r) >= (snd_reel_len_ft (r) - REEL_LEADER_FT) * 12.0 - 0.5);
}

/* Rotation rates (rev/s) of the take-up and the supply reel at a tape
   position and a tape speed */

static double snd_reel_radius (double len_in)
{
return sqrt (REEL_HUB_R * REEL_HUB_R + len_in * REEL_TAPE_T / M_PI);
}

static void snd_reel_rps (SND_REEL *r, double pos_in, double ips, double *takeup, double *supply)
{
double total = snd_reel_len_ft (r) * 12.0;
double on_takeup = REEL_LEADER_FT * 12.0 + pos_in;

*takeup = ips / (2.0 * M_PI * snd_reel_radius (on_takeup));
*supply = ips / (2.0 * M_PI * snd_reel_radius (total - on_takeup));
}

static double snd_reel_rate (double rps)
{
double rate = rps / REEL_SND_REF_RPS;

return (rate < 0.25) ? 0.25 : ((rate > 4.0) ? 4.0 : rate);
}

static double snd_reel_rewind_ips (SND_REEL *r)
{
double ft = snd_reel_len_ft (r);
int32 i = (ft <= 600.0) ? 0 : ((ft <= 1200.0) ? 1 : 2);

return r->type->rewind_ips[i];
}

/* Sounds */

static double snd_reel_dur (SND_REEL *r, int32 i)
{
return sim_sound_duration (r->smp[i]);
}

static void snd_reel_silence (SND_REEL *r, uint32 fade_ms)
{
int32 i;

for (i = 0; i <= SND_REEL_NSMP; i++) {
    sim_sound_stop (r->h[i], fade_ms);
    r->h[i] = 0;
    }
}

static void snd_reel_load_sounds (SND_REEL *r)
{
char name[CBUFSIZE];
int32 i;

if ((r->type == NULL) || !snd_enabled)
    return;
for (i = 0; i < SND_REEL_NSMP; i++) {
    char dname[CBUFSIZE];

    snprintf (name, sizeof (name), "%s_%s.wav", r->type->prefix, snd_reel_names[i]);
    snprintf (dname, sizeof (dname), "%s.wav", snd_reel_names[i]);
    r->smp[i] = snd_load_with_default (r->type->dir, name, SND_DEFAULT_TAPE, dname, "tape");
    }
}

/* Rewind sound for a rewind of total seconds from pos_in to BOT (for an
   unload followed by the unload sound after it) */

static void snd_reel_rewind_sound (SND_REEL *r, double pos_in, double total)
{
double t_start = snd_reel_dur (r, REEL_SMP_RSTART);
double t_stop = snd_reel_dur (r, REEL_SMP_RSTOP);
double from, to, ips = snd_reel_rewind_ips (r);
double tu0, su0, tu1, su1;

if (total <= 0.0)
    return;
if (t_start > total / 2.0)
    t_start = total / 2.0;
if (t_stop > total / 2.0)
    t_stop = total / 2.0;
r->h[REEL_SMP_RSTART] = sim_sound_play_timed (r->smp[REEL_SMP_RSTART], r->group, r->gain, 0,
                                              1.0, 0.0, total, 50);
from = t_start * 0.6;                           /* the loops overlap the ramps */
to = total - t_stop * 0.6;
if ((to - from > 0.1) && (pos_in > 1.0)) {
    snd_reel_rps (r, pos_in, ips, &tu0, &su0);
    snd_reel_rps (r, 0.0, ips, &tu1, &su1);
    r->h[REEL_SMP_REWIND] = sim_sound_play_timed (r->smp[REEL_SMP_REWIND], r->group, r->gain,
                                                  SND_F_LOOP | SND_F_RANDPHASE, snd_reel_rate (tu0),
                                                  from, to - from, 300);
    sim_sound_glide (r->h[REEL_SMP_REWIND], snd_reel_rate (tu1), to - from, SND_GLIDE_REEL);
    r->h[REEL_H_REWIND2] = sim_sound_play_timed (r->smp[REEL_SMP_REWIND], r->group, r->gain * 0.8,
                                                 SND_F_LOOP | SND_F_RANDPHASE, snd_reel_rate (su0),
                                                 from, to - from, 300);
    sim_sound_glide (r->h[REEL_H_REWIND2], snd_reel_rate (su1), to - from, SND_GLIDE_REEL);
    }
r->h[REEL_SMP_RSTOP] = sim_sound_play_timed (r->smp[REEL_SMP_RSTOP], r->group, r->gain, 0,
                                             1.0, total - t_stop, 0.0, 50);
}

/* Timers */

static void snd_reel_finish (SND_REEL *r, int32 what)
{
if (r->on_done != NULL)
    r->on_done (r->uptr, what);
}

static void snd_reel_motion_batch (SND_REEL *r);

static void snd_reel_tmr_done (SND_TIMER *t)
{
SND_REEL *r = (SND_REEL *) t->ctx;

switch (r->state) {
    case SND_REEL_LOADING:
        r->state = SND_REEL_READY;
        r->pos = 0;
        snd_reel_finish (r, SND_REEL_DONE_LOAD);
        break;
    case SND_REEL_REWINDING:
        r->state = SND_REEL_READY;
        r->pos = 0;
        snd_reel_finish (r, SND_REEL_DONE_REWIND);
        break;
    case SND_REEL_UNLOADING:
        r->state = SND_REEL_MOUNTED;
        r->pos = 0;
        snd_reel_finish (r, SND_REEL_DONE_UNLOAD);
        break;
    case SND_REEL_MOVING:
        if (r->mot_items > 0) {                 /* start/stop drive: next batch */
            snd_reel_motion_batch (r);
            break;
            }
        r->state = SND_REEL_READY;
        if (r->type->access_ms > 0.0) {         /* streaming: coast through the gap */
            r->streaming = TRUE;
            r->stream_rev = r->mot_rev;
            sim_sound_timer_start (&r->coast, 1000000.0 * r->type->gap_in / r->type->ips);
            }
        snd_reel_finish (r, SND_REEL_DONE_MOTION);
        break;
        }
}

static void snd_reel_coast_done (SND_TIMER *t)
{
SND_REEL *r = (SND_REEL *) t->ctx;

r->streaming = FALSE;                           /* the tape has stopped */
}

void sim_reel_init (SND_REEL *r, UNIT *uptr, int32 *load_ms, int32 *unload_ms, int32 *reel_ft,
                    void (*on_done) (UNIT *uptr, int32 what), int32 group, double gain)
{
if (r->uptr != NULL)                            /* already initialized */
    return;
memset (r, 0, sizeof (*r));
if (!sim_sound_timer_init (&r->tmr, "REEL", &snd_reel_tmr_done, r) ||
    !sim_sound_timer_init (&r->coast, "REEL", &snd_reel_coast_done, r)) {
    sim_printf ("Sound: out of memory for the tape transport of %s\n", sim_uname (uptr));
    return;
    }
r->uptr = uptr;
r->load_ms = load_ms;
r->unload_ms = unload_ms;
r->reel_ft = reel_ft;
r->on_done = on_done;
r->group = group;
r->gain = gain;
r->state = SND_REEL_NOREEL;
}

void sim_reel_set_type (SND_REEL *r, const SND_REEL_TYPE *type)
{
r->type = type;
snd_reel_load_sounds (r);
}

void sim_reel_mount (SND_REEL *r)
{
if (r->uptr == NULL)
    return;
sim_sound_timer_cancel (&r->tmr);
sim_sound_timer_cancel (&r->coast);
snd_reel_load_sounds (r);
r->pos = 0;
r->streaming = FALSE;
r->state = snd_enabled ? SND_REEL_MOUNTED : SND_REEL_READY;
}

void sim_reel_remove (SND_REEL *r)
{
if (r->uptr == NULL)
    return;
sim_sound_timer_cancel (&r->tmr);
sim_sound_timer_cancel (&r->coast);
snd_reel_silence (r, 100);
r->pos = 0;
r->streaming = FALSE;
r->mot_items = 0;
r->state = SND_REEL_NOREEL;
}

t_stat sim_reel_load (SND_REEL *r)
{
double ms;

if ((r->uptr == NULL) || (r->type == NULL))
    return SCPE_IERR;
if (r->state != SND_REEL_MOUNTED)
    return SCPE_OK;                             /* loaded or busy: nothing to do */
r->pos = 0;
ms = (r->load_ms != NULL) ? (double)*r->load_ms : (double)r->type->load_ms;
if (!snd_enabled || (ms <= 0.0)) {              /* normal mode: at once */
    r->state = SND_REEL_READY;
    snd_reel_finish (r, SND_REEL_DONE_LOAD);
    return SCPE_OK;
    }
r->state = SND_REEL_LOADING;
r->h[REEL_SMP_LOAD] = sim_sound_play_timed (r->smp[REEL_SMP_LOAD], r->group, r->gain, 0,
                                            1.0, 0.0, ms / 1000.0, 300);
sim_sound_timer_start (&r->tmr, 1000.0 * ms);
return SCPE_OK;
}

/* Rewind time from the current position (seconds) */

static double snd_reel_rewind_s (SND_REEL *r)
{
return sim_reel_pos_in (r) / snd_reel_rewind_ips (r) + r->type->rewind_extra_ms / 1000.0;
}

static void snd_reel_start_rewind (SND_REEL *r, int32 state)
{
double rew = snd_reel_rewind_s (r);
double total = rew;

sim_sound_timer_cancel (&r->coast);
r->streaming = FALSE;
r->rew_from_in = sim_reel_pos_in (r);
r->rew_ips = snd_reel_rewind_ips (r);
r->rew_us = 1000000.0 * rew;
r->state = state;
if (state == SND_REEL_UNLOADING) {
    double ums = (r->unload_ms != NULL) ? (double)*r->unload_ms : (double)r->type->unload_ms;

    if (r->pos == 0)                            /* at BOT: only the unload */
        rew = 0.0;
    total = rew + ums / 1000.0;
    r->rew_us = 1000000.0 * rew;
    }
if (!snd_enabled) {                             /* normal mode: at once */
    snd_reel_tmr_done (&r->tmr);
    return;
    }
if (rew > 0.0)
    snd_reel_rewind_sound (r, r->rew_from_in, rew);
if (state == SND_REEL_UNLOADING)
    r->h[REEL_SMP_UNLOAD] = sim_sound_play_timed (r->smp[REEL_SMP_UNLOAD], r->group, r->gain, 0,
                                                  1.0, rew, total - rew, 300);
r->tot_us = 1000000.0 * total;
sim_sound_timer_start (&r->tmr, r->tot_us);
}

t_stat sim_reel_rewind (SND_REEL *r)
{
if ((r->uptr == NULL) || (r->type == NULL))
    return SCPE_IERR;
if ((r->state != SND_REEL_READY) && (r->state != SND_REEL_MOVING))
    return SCPE_OK;
sim_sound_timer_cancel (&r->tmr);
r->mot_items = 0;
snd_reel_start_rewind (r, SND_REEL_REWINDING);
return SCPE_OK;
}

/* Where a rewind has got to (realism mode): the rewind part of the action
   is r->rew_us long and starts with the action (an unload follows it) */

static void snd_reel_rewind_progress (SND_REEL *r)
{
double done_us;

if ((r->state != SND_REEL_REWINDING) && (r->state != SND_REEL_UNLOADING))
    return;
done_us = r->tot_us - 1000000.0 * sim_sound_timer_left_s (&r->tmr);
if (done_us > r->rew_us)
    done_us = r->rew_us;
if (done_us < 0.0)
    done_us = 0.0;
snd_reel_set_pos_in (r, r->rew_from_in - r->rew_ips * done_us / 1000000.0);
}

t_stat sim_reel_unload (SND_REEL *r)
{
if ((r->uptr == NULL) || (r->type == NULL))
    return SCPE_IERR;
if ((r->state == SND_REEL_NOREEL) || (r->state == SND_REEL_MOUNTED) ||
    (r->state == SND_REEL_UNLOADING) || (r->state == SND_REEL_LOADING))
    return SCPE_OK;
if (r->state == SND_REEL_REWINDING)             /* pressed while rewinding */
    snd_reel_rewind_progress (r);
sim_sound_timer_cancel (&r->tmr);
snd_reel_silence (r, 100);
r->mot_items = 0;
snd_reel_start_rewind (r, SND_REEL_UNLOADING);
return SCPE_OK;
}

void sim_reel_abort (SND_REEL *r)
{
if (r->uptr == NULL)
    return;
if (r->state == SND_REEL_REWINDING) {
    snd_reel_rewind_progress (r);
    sim_sound_timer_cancel (&r->tmr);
    snd_reel_silence (r, 100);
    r->state = SND_REEL_READY;
    }
else if (r->state == SND_REEL_MOVING) {
    sim_sound_timer_cancel (&r->tmr);
    snd_reel_silence (r, 30);
    r->mot_items = 0;
    r->state = SND_REEL_READY;
    }
sim_sound_timer_cancel (&r->coast);
r->streaming = FALSE;
}

/* Guest motion commands */

void sim_reel_motion_begin (SND_REEL *r)
{
r->mot_us = 0.0;
r->mot_in = 0.0;
r->mot_items = 0;
r->mot_rev = FALSE;
}

static void snd_reel_move (SND_REEL *r, double dist, t_bool reverse)
{
const SND_REEL_TYPE *ty = r->type;

if (ty == NULL)
    return;
snd_reel_set_pos_in (r, sim_reel_pos_in (r) + (reverse ? -dist : dist));
r->mot_in += reverse ? -dist : dist;
if (ty->access_ms > 0.0)                        /* streaming: continuous motion */
    r->mot_us += 1000000.0 * dist / ty->ips;
else {                                          /* start, move, stop */
    double run = dist - ty->ramp_in;

    r->mot_us += 1000.0 * (ty->start_ms + ty->stop_ms) +
                 ((run > 0.0) ? 1000000.0 * run / ty->ips : 0.0);
    }
r->mot_items = r->mot_items + 1;
r->mot_rev = reverse;
}

void sim_reel_record (SND_REEL *r, uint32 bytes, t_bool reverse)
{
if ((r->uptr == NULL) || (r->type == NULL))
    return;
snd_reel_move (r, (double)bytes / r->type->bpi + r->type->gap_in, reverse);
}

void sim_reel_tapemark (SND_REEL *r, t_bool reverse)
{
if ((r->uptr == NULL) || (r->type == NULL))
    return;
snd_reel_move (r, r->type->tm_in, reverse);
}

void sim_reel_erase (SND_REEL *r, double inches)
{
if ((r->uptr == NULL) || (r->type == NULL) || (inches == 0.0))
    return;
snd_reel_move (r, fabs (inches), inches < 0.0);         /* < 0: reverse */
}

/* Start/stop drive: play the sounds of the next batch of records (at most
   REEL_BATCH, each with its start and stop transient) and time it */

static void snd_reel_motion_batch (SND_REEL *r)
{
int32 k = (r->mot_items < REEL_BATCH) ? r->mot_items : REEL_BATCH;
double each = r->mot_us / (double)r->mot_items;     /* seconds per record: */
double es = each / 1000000.0;
double stop_s = r->type->stop_ms / 1000.0;
int32 i;

for (i = 0; i < k; i++) {
    double at = es * (double)i;

    sim_sound_play_timed (r->smp[REEL_SMP_START], r->group, r->gain, 0, 1.0, at, es, 4);
    sim_sound_play_timed (r->smp[REEL_SMP_MOTION], r->group, r->gain, SND_F_LOOP | SND_F_RANDPHASE,
                          1.0, at, es, 4);
    sim_sound_play_timed (r->smp[REEL_SMP_STOP], r->group, r->gain, 0, 1.0,
                          at + ((es > stop_s) ? es - stop_s : 0.0), 0.0, 4);
    }
r->mot_items -= k;
r->mot_us -= each * (double)k;
sim_sound_timer_start (&r->tmr, each * (double)k);
}

double sim_reel_motion_end (SND_REEL *r)
{
const SND_REEL_TYPE *ty = r->type;
double total;

if ((r->uptr == NULL) || (ty == NULL) || (r->mot_items == 0) || !snd_enabled) {
    r->mot_items = 0;
    return 0.0;
    }
total = r->mot_us;
r->state = SND_REEL_MOVING;
if (ty->access_ms <= 0.0) {                     /* start/stop drive */
    snd_reel_motion_batch (r);
    return total;
    }
/* Streaming drive: if the tape still moves in the same direction (the
   command came while it coasted through the gap) it goes on; otherwise it
   starts from rest and repositions (access time).  The motion loop lasts
   to the end of the coasting, the stop transient follows; a command in
   time cancels the stop and extends the loop. */
if (sim_sound_timer_busy (&r->coast) && r->streaming && (r->stream_rev == r->mot_rev)) {
    sim_sound_timer_cancel (&r->coast);
    sim_sound_stop (r->h[REEL_SMP_STOP], 0);
    }
else {
    sim_sound_timer_cancel (&r->coast);
    total += 1000.0 * ty->access_ms;
    sim_sound_stop (r->h[REEL_SMP_MOTION], 20);
    r->h[REEL_SMP_MOTION] = 0;
    sim_sound_play (r->smp[REEL_SMP_START], r->group, r->gain, 0);
    }
r->streaming = FALSE;
{
    double coast_s = ty->gap_in / ty->ips;
    double life = total / 1000000.0 + coast_s;

    if (r->h[REEL_SMP_MOTION] != 0)
        sim_sound_extend (r->h[REEL_SMP_MOTION], life);
    else
        r->h[REEL_SMP_MOTION] = sim_sound_play_timed (r->smp[REEL_SMP_MOTION], r->group, r->gain,
                                                      SND_F_LOOP | SND_F_RANDPHASE, 1.0, 0.0, life, 20);
    r->h[REEL_SMP_STOP] = sim_sound_play_timed (r->smp[REEL_SMP_STOP], r->group, r->gain, 0, 1.0,
                                                life, 0.0, 4);
    }
r->mot_items = 0;
sim_sound_timer_start (&r->tmr, total);
return total;
}

/* State, waiting, display */

int32 sim_reel_state (SND_REEL *r)
{
if (r->uptr == NULL)
    return SND_REEL_NOREEL;
sim_sound_timer_busy (&r->tmr);                 /* prompt: wall time passed */
sim_sound_timer_busy (&r->coast);
return r->state;
}

void sim_reel_wait (SND_REEL *r)
{
int32 st = sim_reel_state (r);
const char *what;

if (sim_is_running ||
    ((st != SND_REEL_LOADING) && (st != SND_REEL_REWINDING) && (st != SND_REEL_UNLOADING)))
    return;
what = (st == SND_REEL_LOADING) ? "loading, at BOT" :
       ((st == SND_REEL_REWINDING) ? "rewinding, at BOT" : "unloading, unloaded");
sim_printf ("%s: %s in %.0f s ...\n", sim_uname (r->uptr), what, sim_sound_timer_left_s (&r->tmr));
while (sim_reel_state (r) == st)
    sim_os_ms_sleep (50);
if (r->state == SND_REEL_READY)
    sim_printf ("%s: at BOT\n", sim_uname (r->uptr));
else if (r->state == SND_REEL_MOUNTED)
    sim_printf ("%s: unloaded\n", sim_uname (r->uptr));
}

t_stat sim_reel_show (FILE *st, SND_REEL *r)
{
int32 s = sim_reel_state (r);

if ((s == SND_REEL_REWINDING) || (s == SND_REEL_UNLOADING))
    snd_reel_rewind_progress (r);
switch (s) {
    case SND_REEL_NOREEL:
        fprintf (st, "no reel");
        break;
    case SND_REEL_MOUNTED:
        fprintf (st, "reel mounted, not loaded");
        break;
    case SND_REEL_LOADING:
        fprintf (st, "loading, %.1f s left", sim_sound_timer_left_s (&r->tmr));
        break;
    case SND_REEL_REWINDING:
        fprintf (st, "rewinding, %.1f s left", sim_sound_timer_left_s (&r->tmr));
        break;
    case SND_REEL_UNLOADING:
        fprintf (st, "unloading, %.1f s left", sim_sound_timer_left_s (&r->tmr));
        break;
    default:
        if (r->pos == 0)
            fprintf (st, "loaded, at BOT");
        else
            fprintf (st, "loaded, %.1f ft from BOT%s", sim_reel_pos_in (r) / 12.0,
                     sim_reel_past_eot (r) ? " (past EOT)" : "");
        if (s == SND_REEL_MOVING)
            fprintf (st, ", moving");
        break;
    }
if (s != SND_REEL_NOREEL)
    fprintf (st, ", %d ft reel", (int)snd_reel_len_ft (r));
return SCPE_OK;
}

/* Realtime clock (see sim_sound.h): simulated time while running, wall
   time at the prompt; continuous across the switches */

static t_bool snd_rt_inited = FALSE;
static t_bool snd_rt_running = FALSE;
static double snd_rt_base_us = 0.0;             /* running: rt at snd_rt_gbase */
static double snd_rt_gbase = 0.0;               /*  ... sim_gtime () then */
static double snd_rt_stop_us = 0.0;             /* stopped: rt at snd_rt_wall_ms */
static uint32 snd_rt_wall_ms = 0;

double sim_sound_rt_us (void)
{
if (!snd_rt_inited) {
    snd_rt_inited = TRUE;
    snd_rt_wall_ms = sim_os_msec ();
    snd_rt_stop_us = 0.0;
    }
if (snd_rt_running)
    return snd_rt_base_us + (sim_gtime () - snd_rt_gbase) / sim_sound_inst_per_us ();
return snd_rt_stop_us + 1000.0 * (double)(sim_os_msec () - snd_rt_wall_ms);
}

static void snd_rt_run_state (t_bool running)
{
double now = sim_sound_rt_us ();

if (running == snd_rt_running)
    return;
if (running) {
    snd_rt_base_us = now;
    snd_rt_gbase = sim_gtime ();
    }
else {
    snd_rt_stop_us = now;
    snd_rt_wall_ms = sim_os_msec ();
    }
snd_rt_running = running;
}

/* Cartridge streamers (see sim_sound.h) */

#define STRM_IDLE       0                       /* motion modes */
#define STRM_START      1
#define STRM_STREAM     2
#define STRM_REPOS      3
#define STRM_MOVE       4

#define STRM_DIR_NONE   0                       /* buffer directions */
#define STRM_DIR_READ   1
#define STRM_DIR_WRITE  2

#define STRM_S_LOAD     0                       /* sound indices */
#define STRM_S_CAL      1
#define STRM_S_UNLOAD   2
#define STRM_S_START    3
#define STRM_S_FWD      4
#define STRM_S_STOP     5
#define STRM_S_RSTART   6
#define STRM_S_REV      7
#define STRM_S_RSTOP    8

#define STRM_INF        1e300

static const char *snd_strm_names[SND_STRM_NSMP] = {
    "load", "calibrate", "unload", "start", "forward", "stop", "rstart", "reverse", "rstop" };
static const char *snd_strm_defaults[SND_STRM_NSMP] = {         /* in default_tape */
    "load.wav", NULL, "unload.wav", "start.wav", "motion.wav", "stop.wav",
    "start.wav", "motion.wav", "stop.wav" };

void sim_strm_set_defaults (SND_STRM_REGS *regs, const SND_STRM_TYPE *type)
{
regs->buffer = type->buffer;
regs->backup_in = type->backup_in;
regs->repos_ms = type->repos_ms;
regs->load_min_ms = type->load_min_ms;
regs->load_max_ms = type->load_max_ms;
regs->cal_ms = type->cal_ms;
regs->unload_ms = type->unload_ms;
}

static double strm_buf (SND_STREAMER *s)
{
return (s->regs->buffer > 0) ? (double)s->regs->buffer : 32768.0;
}

static double strm_cap_in (SND_STREAMER *s)
{
return s->type->track_in * (double)s->type->tracks;
}

static void strm_set_lpos (SND_STREAMER *s, double in)
{
double max = strm_cap_in (s) + s->type->track_in;       /* a little beyond EOT */

s->lpos = (in < 0.0) ? 0.0 : ((in > max) ? max : in);
s->pos_mil = (uint32)(s->lpos * 1000.0 + 0.5);
}

/* Physical distance from BOT of a logical position (serpentine: even
   tracks run forward, odd ones in reverse) */

static double strm_phys (SND_STREAMER *s, double lpos)
{
double tl = s->type->track_in;
int32 k = (int32)(lpos / tl);
double w = lpos - (double)k * tl;

if (k >= s->type->tracks) {
    k = s->type->tracks - 1;
    w = tl;
    }
return (k & 1) ? tl - w : w;
}

/* Rotation rate of the take-up reel relative to the loops' reference */

static double strm_rate (SND_STREAMER *s, double phys_in, double ips)
{
const SND_STRM_TYPE *ty = s->type;
double r = sqrt (ty->hub_r * ty->hub_r + phys_in * ty->tape_t / M_PI);
double rate = (ips / ty->ips) * (ty->ref_r / r);

return (rate < 0.4) ? 0.4 : ((rate > 2.5) ? 2.5 : rate);
}

static void strm_start (SND_STREAMER *s, double at)
{
const SND_STRM_TYPE *ty = s->type;
double run = ty->start_s;

if (s->backed)                                  /* run up over the backed-up tape */
    run += ((double)s->regs->backup_in - ty->stop_in) / ty->ips;
s->mode = STRM_START;
s->t = at;
s->pend = at + 1000000.0 * run;
s->backed = FALSE;
}

static void strm_repos (SND_STREAMER *s)
{
s->mode = STRM_REPOS;
s->pend = s->t + 1000.0 * (double)s->regs->repos_ms;
s->backed = TRUE;
}

/* Bring the model up to time now (nothing happens before s->t) */

static void strm_advance (SND_STREAMER *s, double now)
{
double B = strm_buf (s), rate = s->type->rate / 1000000.0;  /* bytes per us */
int32 guard;

for (guard = 0; guard < 100; guard++) {
    switch (s->mode) {

        case STRM_IDLE:
            if ((s->dir == STRM_DIR_WRITE) && (s->bufc > 0.0) &&
                (s->flush || (s->need > 0.0) || (s->bufc >= B / 2.0))) {
                strm_start (s, s->t);           /* write the buffer out */
                continue;
                }
            if ((s->dir == STRM_DIR_READ) &&
                ((s->need > s->bufc) || (s->bufc <= B / 2.0))) {
                strm_start (s, s->t);           /* read ahead */
                continue;
                }
            return;

        case STRM_START:
        case STRM_REPOS:
        case STRM_MOVE:
            if (now < s->pend)
                return;
            s->t = s->pend;
            if (s->mode == STRM_START)
                s->mode = STRM_STREAM;
            else {
                s->mode = STRM_IDLE;
                s->mv_rewind = FALSE;
                }
            continue;

        case STRM_STREAM:
            if (now <= s->t)
                return;
            if (s->dir == STRM_DIR_WRITE) {     /* buffer -> tape */
                double dt = s->bufc / rate;

                if (s->t + dt <= now) {
                    s->t += dt;
                    s->bufc = 0.0;
                    s->flush = FALSE;
                    strm_repos (s);             /* empty: stop, back up */
                    continue;
                    }
                s->bufc -= (now - s->t) * rate;
                s->t = now;
                return;
                }
            else {                              /* tape -> buffer */
                double cap = (s->need > B) ? s->need : B;
                double dt = (cap - s->bufc) / rate;

                if (dt < 0.0)
                    dt = 0.0;
                if (s->t + dt <= now) {
                    s->t += dt;
                    s->bufc = cap;
                    strm_repos (s);             /* full: stop, back up */
                    continue;
                    }
                s->bufc += (now - s->t) * rate;
                s->t = now;
                return;
                }
        }
    }
}

/* Time of the next transition of the model (STRM_INF: none) */

static double strm_next (SND_STREAMER *s)
{
double B = strm_buf (s), rate = s->type->rate / 1000000.0;

switch (s->mode) {
    case STRM_START: case STRM_REPOS: case STRM_MOVE:
        return s->pend;
    case STRM_STREAM:
        if (s->dir == STRM_DIR_WRITE)
            return s->t + s->bufc / rate;
        return s->t + ((((s->need > B) ? s->need : B) - s->bufc) / rate);
    default: {
        SND_STREAMER c = *s;

        strm_advance (&c, c.t);                 /* would it start? */
        return (c.mode != STRM_IDLE) ? s->t : STRM_INF;
        }
    }
}

/* Sounds: the predicted timeline of the model, as queued segments.  A
   segment that is already queued is kept (a loop gets its new end), a
   queued future one that is no longer in the timeline is dropped, a
   playing one is faded out.

   Crossfades (D = length of the start or stop sound): a loop begins at
   0.6 D of its start sound, from its first frame (no random phase), and
   fades in over 0.4 D; the start sound ends with the loop's own first
   0.4 D fading out, so both add up to the loop.  A loop ends where its
   stop sound begins and fades out over 0.4 D of the stop, which fades in
   over the same time.  The reverse start follows the stop when it has
   died away (a short standstill at the direction change, so the
   reposition is heard as stop - reverse - stop).  (The TK50 sounds of
   tools/make_tape_sounds.py v4 are made for this.) */

typedef struct { int32 kind; double at; double end; double rate; double rate2; } STRM_PSEG;

static int32 strm_timeline (SND_STREAMER *s, STRM_PSEG *ps, int32 max)
{
SND_STREAMER c = *s;
int32 n = 0, guard;
double d_st = sim_sound_duration (s->smp[STRM_S_START]);
double d_sp = sim_sound_duration (s->smp[STRM_S_STOP]);
double d_rst = sim_sound_duration (s->smp[STRM_S_RSTART]);
double d_rsp = sim_sound_duration (s->smp[STRM_S_RSTOP]);

#define PSEG(k,a,e,r,r2) if (n < max) { ps[n].kind = (k); ps[n].at = (a); ps[n].end = (e); \
                                        ps[n].rate = (r); ps[n].rate2 = (r2); n++; }
for (guard = 0; guard < 12; guard++) {
    double a, b, e;

    if (c.mode == STRM_IDLE) {
        double nx = strm_next (&c);

        if (nx >= STRM_INF)
            break;
        strm_advance (&c, nx);
        if (c.mode == STRM_IDLE)
            break;
        continue;
        }
    a = (c.mode == STRM_STREAM) ? c.t : c.t;
    if (c.mode == STRM_START) {                 /* start, then stream: one loop */
        double st = c.t;

        strm_advance (&c, c.pend);              /* now streaming */
        e = strm_next (&c);
        if (e >= STRM_INF)
            e = c.t;
        PSEG (STRM_S_START, st, st, 1.0, 1.0);
        PSEG (STRM_S_FWD, st + 0.6 * 1000000.0 * d_st, e, 1.0, 1.0);
        strm_advance (&c, e);                   /* -> repos */
        continue;
        }
    if (c.mode == STRM_STREAM) {                /* streaming already */
        e = strm_next (&c);
        PSEG (STRM_S_FWD, -1.0, e, 1.0, 1.0);   /* (-1: the playing loop) */
        strm_advance (&c, e);
        continue;
        }
    b = c.pend;
    if (c.mode == STRM_REPOS) {                 /* stop, back up, stop */
        double r0 = a + 1000000.0 * d_sp;       /* after the stop: a short standstill */

        PSEG (STRM_S_STOP, a, a, 1.0, 1.0);
        PSEG (STRM_S_RSTART, r0, r0, 1.0, 1.0);
        PSEG (STRM_S_REV, r0 + 0.6 * 1000000.0 * d_rst, b - 1000000.0 * d_rsp, 1.0, 1.0);
        PSEG (STRM_S_RSTOP, b - 1000000.0 * d_rsp, b, 1.0, 1.0);
        }
    else {                                      /* move: space or rewind */
        int32 ks = c.mv_rev ? STRM_S_RSTART : STRM_S_START;
        int32 kl = c.mv_rev ? STRM_S_REV : STRM_S_FWD;
        int32 kp = c.mv_rev ? STRM_S_RSTOP : STRM_S_STOP;
        double ds = c.mv_rev ? d_rst : d_st, dp = c.mv_rev ? d_rsp : d_sp;
        double r1 = 1.0, r2 = 1.0;

        if (c.mv_rewind) {                      /* reel glide */
            r1 = strm_rate (s, c.mv_from, c.type->rewind_ips);
            r2 = strm_rate (s, 0.0, c.type->rewind_ips);
            }
        PSEG (ks, a, a, 1.0, 1.0);
        PSEG (kl, a + 0.6 * 1000000.0 * ds, b - 1000000.0 * dp, r1, r2);
        PSEG (kp, b - 1000000.0 * dp, b, 1.0, 1.0);
        }
    strm_advance (&c, b);
    }
#undef PSEG
return n;
}

static void strm_sounds (SND_STREAMER *s)
{
STRM_PSEG ps[SND_STRM_NSEG];
t_bool used[SND_STRM_NSEG];
int32 n, i, j;
double now;

if (!snd_enabled || (s->type == NULL))
    return;
now = sim_sound_rt_us ();
n = strm_timeline (s, ps, SND_STRM_NSEG);
memset (used, 0, sizeof (used));
for (j = 0; j < SND_STRM_NSEG; j++) {           /* old segments */
    SND_STRM_SEG *g = &s->seg[j];
    t_bool keep = FALSE;

    if (g->kind < 0)
        continue;
    for (i = 0; i < n; i++) {
        if (used[i] || (ps[i].kind != g->kind))
            continue;
        if ((ps[i].at < 0.0) ? ((g->at <= now) && (g->end >= now - 50000.0))
                             : (fabs (ps[i].at - g->at) < 2000.0)) {
            keep = TRUE;                        /* the same segment */
            used[i] = TRUE;
            if ((ps[i].end != g->end) && (g->kind == STRM_S_FWD || g->kind == STRM_S_REV)) {
                g->end = ps[i].end;             /* new end of a loop */
                sim_sound_extend (g->h, (g->end - now) / 1000000.0);
                }
            break;
            }
        }
    if (!keep) {
        if ((g->at > now) || ((g->end > now) && (g->kind == STRM_S_FWD || g->kind == STRM_S_REV)))
            sim_sound_stop (g->h, 30);          /* drop or fade */
        g->kind = -1;
        }
    }
for (i = 0; i < n; i++) {                       /* new segments */
    double at, end, life = 0.0;
    uint32 flags = 0, fade_in = 0, fade_out = 0;

    if (used[i])
        continue;
    if (ps[i].at < 0.0)                         /* streaming, no loop yet */
        ps[i].at = now;
    for (j = 0; (j < SND_STRM_NSEG) && (s->seg[j].kind >= 0); j++) ;
    if (j >= SND_STRM_NSEG)
        break;
    at = ps[i].at;
    end = ps[i].end;
    if ((ps[i].kind == STRM_S_FWD) || (ps[i].kind == STRM_S_REV)) {
        if (end <= at)
            continue;
        int32 ks = (ps[i].kind == STRM_S_FWD) ? STRM_S_START : STRM_S_RSTART;
        int32 kp = (ps[i].kind == STRM_S_FWD) ? STRM_S_STOP : STRM_S_RSTOP;

        flags = SND_F_LOOP;                     /* from its beginning: the start sound ends
                                                   with the loop's own first 0.4 D */
        if (end < now)
            continue;
        fade_in = (uint32)(400.0 * sim_sound_duration (s->smp[ks]));   /* crossfades: */
        fade_out = (uint32)(400.0 * sim_sound_duration (s->smp[kp]));  /* 0.4 D */
        if (at < now) {                         /* streaming already, or late */
            at = now;
            flags |= SND_F_RANDPHASE;
            fade_in = 30;
            }
        life = (end - at) / 1000000.0;
        }
    else if (at < now - 20000.0)                /* a transient that is past */
        continue;
    s->seg[j].kind = ps[i].kind;
    s->seg[j].at = ps[i].at;
    s->seg[j].end = end;
    s->seg[j].h = snd_play_full (s->smp[ps[i].kind], NULL, 0, s->group, s->gain, flags, fade_in,
                                 ps[i].rate, (at > now) ? (at - now) / 1000000.0 : 0.0,
                                 life, fade_out);
    if (ps[i].rate2 != ps[i].rate)
        sim_sound_glide (s->seg[j].h, ps[i].rate2, life, SND_GLIDE_REEL);
    }
}

static void strm_silence (SND_STREAMER *s, uint32 fade_ms)
{
int32 j;

for (j = 0; j < SND_STRM_NSEG; j++) {
    if (s->seg[j].kind >= 0)
        sim_sound_stop (s->seg[j].h, fade_ms);
    s->seg[j].kind = -1;
    }
sim_sound_stop (s->hact, fade_ms);
sim_sound_stop (s->hcal, fade_ms);
s->hact = s->hcal = 0;
}

static void strm_reset_motion (SND_STREAMER *s, double at)
{
s->mode = STRM_IDLE;
s->dir = STRM_DIR_NONE;
s->bufc = s->need = 0.0;
s->flush = s->backed = s->mv_rev = s->mv_rewind = FALSE;
s->t = s->pend = at;
}

static void strm_tmr_done (SND_TIMER *t)
{
SND_STREAMER *s = (SND_STREAMER *) t->ctx;

if (s->state == SND_STRM_LOADING) {
    s->state = SND_STRM_READY;
    strm_reset_motion (s, sim_sound_rt_us ());
    strm_set_lpos (s, 0.0);
    s->newcart = FALSE;
    if (s->on_done != NULL)
        s->on_done (s->uptr, SND_STRM_DONE_LOAD);
    }
else if (s->state == SND_STRM_UNLOADING) {
    s->state = SND_STRM_INSERTED;
    strm_reset_motion (s, sim_sound_rt_us ());
    strm_set_lpos (s, 0.0);
    if (s->on_done != NULL)
        s->on_done (s->uptr, SND_STRM_DONE_UNLOAD);
    }
}

void sim_strm_init (SND_STREAMER *s, UNIT *uptr, SND_STRM_REGS *regs,
                    void (*on_done) (UNIT *uptr, int32 what), int32 group, double gain)
{
int32 j;

if (s->uptr != NULL)
    return;
memset (s, 0, sizeof (*s));
if (!sim_sound_timer_init (&s->tmr, "STREAMER", &strm_tmr_done, s)) {
    sim_printf ("Sound: out of memory for the tape drive %s\n", sim_uname (uptr));
    return;
    }
s->uptr = uptr;
s->regs = regs;
s->on_done = on_done;
s->group = group;
s->gain = gain;
s->state = SND_STRM_NOCART;
for (j = 0; j < SND_STRM_NSEG; j++)
    s->seg[j].kind = -1;
}

static void strm_load_sounds (SND_STREAMER *s)
{
char name[CBUFSIZE];
int32 i;

if ((s->type == NULL) || !snd_enabled)
    return;
for (i = 0; i < SND_STRM_NSMP; i++) {
    snprintf (name, sizeof (name), "%s_%s.wav", s->type->prefix, snd_strm_names[i]);
    if (snd_strm_defaults[i] != NULL)
        s->smp[i] = snd_load_with_default (s->type->dir, name, SND_DEFAULT_TAPE,
                                           snd_strm_defaults[i], "tape");
    else s->smp[i] = sim_sound_load_optional (s->type->dir, name);
    }
}

void sim_strm_set_type (SND_STREAMER *s, const SND_STRM_TYPE *type)
{
s->type = type;
strm_load_sounds (s);
}

void sim_strm_insert (SND_STREAMER *s, t_bool newcart)
{
if ((s->uptr == NULL) || (s->type == NULL))
    return;
sim_sound_timer_cancel (&s->tmr);
strm_silence (s, 50);
strm_load_sounds (s);
strm_reset_motion (s, sim_sound_rt_us ());
strm_set_lpos (s, 0.0);
s->newcart = newcart;
s->state = snd_enabled ? SND_STRM_INSERTED : SND_STRM_READY;
}

void sim_strm_remove (SND_STREAMER *s)
{
if (s->uptr == NULL)
    return;
sim_sound_timer_cancel (&s->tmr);
strm_silence (s, 100);
strm_reset_motion (s, sim_sound_rt_us ());
strm_set_lpos (s, 0.0);
s->state = SND_STRM_NOCART;
}

t_stat sim_strm_load (SND_STREAMER *s)
{
double ms, now;
int32 lo, hi;

if ((s->uptr == NULL) || (s->type == NULL))
    return SCPE_IERR;
if (s->state != SND_STRM_INSERTED)
    return SCPE_OK;
if (!snd_enabled) {
    s->state = SND_STRM_LOADING;
    strm_tmr_done (&s->tmr);
    return SCPE_OK;
    }
lo = s->regs->load_min_ms;                      /* a random load time: the head */
hi = s->regs->load_max_ms;                      /* finds the tape's tracks */
if (hi < lo)
    hi = lo;
ms = (double)lo + (double)(hi - lo) * ((double)(rand () & 0x7FFF) / 32767.0);
now = sim_sound_rt_us ();
s->hact = sim_sound_play_timed (s->smp[STRM_S_LOAD], s->group, s->gain, 0, 1.0, 0.0,
                                ms / 1000.0, 300);
if (s->newcart && (s->regs->cal_ms > 0)) {      /* a new cartridge: calibration */
    s->hcal = sim_sound_play_timed (s->smp[STRM_S_CAL], s->group, s->gain,
                                    SND_F_LOOP, 1.0, ms / 1000.0,
                                    (double)s->regs->cal_ms / 1000.0, 300);
    ms += (double)s->regs->cal_ms;
    }
s->state = SND_STRM_LOADING;
s->act_end = now + 1000.0 * ms;
sim_sound_timer_start (&s->tmr, 1000.0 * ms);
return SCPE_OK;
}

/* Common start of a host motion: the time base and pending writes */

static double strm_begin (SND_STREAMER *s)
{
double now = sim_sound_rt_us ();

strm_advance (s, now);
if ((s->mode == STRM_IDLE) && (s->t < now))     /* at rest: a restart is now */
    s->t = now;
return now;
}

/* Settle the buffer: write it out (write) or drop the read-ahead (read);
   the model is then idle (possibly in the future) */

static void strm_settle (SND_STREAMER *s, double now)
{
if (s->dir == STRM_DIR_WRITE) {
    if (s->bufc > 0.0) {
        s->flush = TRUE;
        strm_advance (s, STRM_INF);             /* written out, stopped */
        }
    }
else if (s->dir == STRM_DIR_READ) {
    s->need = 0.0;
    if ((s->mode == STRM_START) || (s->mode == STRM_STREAM)) {
        if (s->t < now)
            s->t = now;
        strm_repos (s);                         /* stop and back up */
        }
    strm_advance (s, STRM_INF);
    s->bufc = 0.0;
    }
else strm_advance (s, STRM_INF);
s->dir = STRM_DIR_NONE;
s->bufc = 0.0;
s->flush = FALSE;
if (s->t < now)
    s->t = now;
}

static double strm_done (SND_STREAMER *s, double now, double ready)
{
strm_sounds (s);
return (ready > now) ? ready - now : 0.0;
}

double sim_strm_read (SND_STREAMER *s, double bytes)
{
double now, ready, rate;
int32 guard;

if ((s->uptr == NULL) || (s->type == NULL) || !snd_enabled || (s->state != SND_STRM_READY)) {
    if ((s->type != NULL) && (s->uptr != NULL))
        strm_set_lpos (s, s->lpos + bytes / s->type->bytes_per_in);
    return 0.0;
    }
now = strm_begin (s);
if (s->dir != STRM_DIR_READ) {
    strm_settle (s, now);
    s->dir = STRM_DIR_READ;
    }
rate = s->type->rate / 1000000.0;
if (s->bufc + 0.5 >= bytes) {                   /* in the buffer */
    ready = (s->t > now) ? s->t : now;
    }
else {                                          /* wait for the tape */
    s->need = bytes;
    for (guard = 0; guard < 20; guard++) {
        double nx;

        if (s->mode == STRM_STREAM) {           /* the data arrives */
            strm_advance (s, s->t + (bytes - s->bufc) / rate);
            break;
            }
        nx = strm_next (s);
        if (nx >= STRM_INF)
            break;
        strm_advance (s, (nx > now) ? nx : now);
        }
    ready = (s->t > now) ? s->t : now;
    s->need = 0.0;
    }
s->bufc -= bytes;
if (s->bufc < 0.0)
    s->bufc = 0.0;
strm_set_lpos (s, s->lpos + bytes / s->type->bytes_per_in);
strm_advance (s, (s->t > now) ? s->t : now);    /* restart the read-ahead? */
return strm_done (s, now, ready);
}

double sim_strm_write (SND_STREAMER *s, double bytes)
{
double now, ready, B, rate;
int32 guard;

if ((s->uptr == NULL) || (s->type == NULL) || !snd_enabled || (s->state != SND_STRM_READY)) {
    if ((s->type != NULL) && (s->uptr != NULL))
        strm_set_lpos (s, s->lpos + bytes / s->type->bytes_per_in);
    return 0.0;
    }
now = strm_begin (s);
if (s->dir != STRM_DIR_WRITE) {
    strm_settle (s, now);
    s->dir = STRM_DIR_WRITE;
    }
B = strm_buf (s);
if (bytes > B)
    B = bytes;
rate = s->type->rate / 1000000.0;
if (B - s->bufc + 0.5 < bytes) {                /* wait for room */
    s->need = bytes;
    for (guard = 0; guard < 20; guard++) {
        double nx;

        if (s->mode == STRM_STREAM) {           /* room appears */
            strm_advance (s, s->t + (s->bufc - (B - bytes)) / rate);
            break;
            }
        nx = strm_next (s);
        if (nx >= STRM_INF)
            break;
        strm_advance (s, (nx > now) ? nx : now);
        }
    s->need = 0.0;
    }
ready = (s->t > now) ? s->t : now;
s->bufc += bytes;
strm_set_lpos (s, s->lpos + bytes / s->type->bytes_per_in);
strm_advance (s, ready);                        /* start writing? */
return strm_done (s, now, ready);
}

double sim_strm_tapemark (SND_STREAMER *s)
{
if (s->type == NULL)
    return 0.0;
return sim_strm_write (s, s->type->tm_bytes);
}

double sim_strm_flush (SND_STREAMER *s)
{
double now;

if ((s->uptr == NULL) || (s->type == NULL) || !snd_enabled || (s->state != SND_STRM_READY))
    return 0.0;
now = strm_begin (s);
if (s->dir == STRM_DIR_WRITE)
    strm_settle (s, now);
return strm_done (s, now, s->t);
}

/* A move along the tape (space, reverse read, rewind) from the settled
   model; crossing a track end costs a stop and a start */

static double strm_move (SND_STREAMER *s, double to_lpos, t_bool rewind)
{
const SND_STRM_TYPE *ty = s->type;
double now = strm_begin (s);
double from = s->lpos, p0, p1, secs;
int32 k0, k1;

strm_settle (s, now);
if (to_lpos < 0.0)
    to_lpos = 0.0;
p0 = strm_phys (s, from);
p1 = strm_phys (s, to_lpos);
k0 = (int32)(from / ty->track_in);
k1 = (int32)(to_lpos / ty->track_in);
if (rewind)
    secs = ty->start_s + p0 / ty->rewind_ips + ty->stop_s;
else {
    secs = ty->start_s + fabs (to_lpos - from) / ty->ips + ty->stop_s +
           (double)abs (k1 - k0) * (ty->start_s + ty->stop_s);
    }
if (fabs (to_lpos - from) < 0.01 && !rewind)
    secs = 0.0;
strm_set_lpos (s, to_lpos);
if (secs > 0.0) {
    s->mode = STRM_MOVE;
    s->pend = s->t + 1000000.0 * secs;
    s->mv_rewind = rewind;
    s->mv_from = p0;
    s->mv_lfrom = from;                         /* (SHOW TRANSPORT: where it is now) */
    s->mv_t0 = s->t;
    s->mv_rev = rewind ? TRUE : ((k0 & 1) ? (to_lpos > from) : (to_lpos < from));
    if (!rewind && (k0 != k1))                  /* sound: direction at the start */
        s->mv_rev = (k0 & 1) ? (to_lpos > from) : (to_lpos < from);
    (void)p1;
    }
s->backed = FALSE;
return strm_done (s, now, (s->mode == STRM_MOVE) ? s->pend : s->t);
}

double sim_strm_space (SND_STREAMER *s, double bytes)
{
if ((s->uptr == NULL) || (s->type == NULL))
    return 0.0;
if (!snd_enabled || (s->state != SND_STRM_READY)) {
    strm_set_lpos (s, s->lpos + bytes / s->type->bytes_per_in);
    return 0.0;
    }
return strm_move (s, s->lpos + bytes / s->type->bytes_per_in, FALSE);
}

double sim_strm_rewind (SND_STREAMER *s)
{
if ((s->uptr == NULL) || (s->type == NULL))
    return 0.0;
if (!snd_enabled || (s->state != SND_STRM_READY)) {
    strm_set_lpos (s, 0.0);
    return 0.0;
    }
return strm_move (s, 0.0, TRUE);
}

t_stat sim_strm_unload (SND_STREAMER *s)
{
double now, rew_end, ums;

if ((s->uptr == NULL) || (s->type == NULL))
    return SCPE_IERR;
if (s->state != SND_STRM_READY)
    return SCPE_OK;
if (!snd_enabled) {
    s->state = SND_STRM_UNLOADING;
    strm_tmr_done (&s->tmr);
    return SCPE_OK;
    }
now = sim_sound_rt_us ();
sim_strm_rewind (s);                            /* write out, rewind */
rew_end = (s->mode == STRM_MOVE) ? s->pend : s->t;
if (rew_end < now)
    rew_end = now;
ums = (double)s->regs->unload_ms;
s->hact = sim_sound_play_timed (s->smp[STRM_S_UNLOAD], s->group, s->gain, 0, 1.0,
                                (rew_end - now) / 1000000.0, ums / 1000.0, 300);
s->state = SND_STRM_UNLOADING;
s->act_end = rew_end + 1000.0 * ums;
sim_sound_timer_start (&s->tmr, s->act_end - now);
return SCPE_OK;
}

t_bool sim_strm_loaded (SND_STREAMER *s)
{
return (s->uptr != NULL) && (sim_strm_state (s) == SND_STRM_READY);
}

t_bool sim_strm_past_eot (SND_STREAMER *s)
{
return (s->type != NULL) && (s->lpos >= strm_cap_in (s));
}

int32 sim_strm_state (SND_STREAMER *s)
{
if (s->uptr == NULL)
    return SND_STRM_NOCART;
sim_sound_timer_busy (&s->tmr);                 /* prompt: wall time passed */
return s->state;
}

void sim_strm_wait (SND_STREAMER *s)
{
int32 st = sim_strm_state (s);

if (sim_is_running || ((st != SND_STRM_LOADING) && (st != SND_STRM_UNLOADING)))
    return;
sim_printf ("%s: %s in %.0f s ...\n", sim_uname (s->uptr),
            (st == SND_STRM_LOADING) ? (s->newcart ? "loading and calibrating a new cartridge, at BOT"
                                                   : "loading, at BOT")
                                     : "rewinding and unloading, unloaded",
            sim_sound_timer_left_s (&s->tmr));
while (sim_strm_state (s) == st)
    sim_os_ms_sleep (50);
if (s->state == SND_STRM_READY)
    sim_printf ("%s: at BOT\n", sim_uname (s->uptr));
else if (s->state == SND_STRM_INSERTED)
    sim_printf ("%s: unloaded, the handle can be lifted\n", sim_uname (s->uptr));
}

t_stat sim_strm_show (FILE *st, SND_STREAMER *s)
{
int32 state = sim_strm_state (s);
const char *lights = "";

switch (state) {
    case SND_STRM_NOCART:
        fprintf (st, "no cartridge");
        lights = "green on";
        break;
    case SND_STRM_INSERTED:
        fprintf (st, "cartridge inserted, not loaded%s", s->newcart ? " (new)" : "");
        lights = "green on (the handle can be lifted)";
        break;
    case SND_STRM_LOADING:
        fprintf (st, "loading, %.1f s left", sim_sound_timer_left_s (&s->tmr));
        lights = "green blinking";
        break;
    case SND_STRM_UNLOADING:
        fprintf (st, "rewinding and unloading, %.1f s left", sim_sound_timer_left_s (&s->tmr));
        lights = "red blinking, then red on";
        break;
    default: {
        double now = sim_sound_rt_us ();
        SND_STREAMER c = *s;
        int32 k = (int32)(s->lpos / s->type->track_in);
        static const char *mname[] = { "at rest", "starting", "streaming", "repositioning", "moving" };

        strm_advance (&c, now);
        if ((c.mode == STRM_MOVE) && (c.pend > c.mv_t0)) {  /* lpos is the target already: */
            double f = (now - c.mv_t0) / (c.pend - c.mv_t0);  /* show where the tape is now */

            f = (f < 0.0) ? 0.0 : ((f > 1.0) ? 1.0 : f);
            if (c.mv_rewind) {
                fprintf (st, "loaded, rewinding, %.0f ft from BOT", c.mv_from * (1.0 - f) / 12.0);
                }
            else {
                double l = c.mv_lfrom + (s->lpos - c.mv_lfrom) * f;

                fprintf (st, "loaded, track %d of %d, %.0f ft from BOT, moving",
                         (int32)(l / s->type->track_in) + 1, s->type->tracks, strm_phys (s, l) / 12.0);
                }
            }
        else {
            if (s->lpos < 0.01)
                fprintf (st, "loaded, at BOT");
            else fprintf (st, "loaded, track %d of %d, %.0f ft from BOT%s", k + 1, s->type->tracks,
                          strm_phys (s, s->lpos) / 12.0, sim_strm_past_eot (s) ? " (past EOT)" : "");
            fprintf (st, ", %s", mname[c.mode]);
            }
        if (c.dir != STRM_DIR_NONE)
            fprintf (st, ", buffer %.0f KB %s", c.bufc / 1024.0,
                     (c.dir == STRM_DIR_READ) ? "read ahead" : "to write");
        lights = (c.mode == STRM_IDLE) ? "green and red on" :
                 ((c.mode == STRM_MOVE && c.mv_rewind) ? "both blinking" : "green blinking, red on");
        break;
        }
    }
fprintf (st, "; lights: %s", lights);
return SCPE_OK;
}

/* Seek sounds (see sim_sound.h) */

void sim_sound_seek_load (SND_SAMPLE *smp[3], const char *dir, const char *prefix)
{
static const char *cls[3] = { "s", "m", "l" };
char name[CBUFSIZE];
char dname[CBUFSIZE];
int32 i;


for (i = 0; i < 3; i++) {                       /* own file, else the default */
    snprintf (name, sizeof (name), "%s-%s.wav", prefix, cls[i]);
    snprintf (dname, sizeof (dname), "seek-%s.wav", cls[i]);
    smp[i] = snd_load_with_default (dir, name, SND_DEFAULT_DISK, dname, "seek");
    }
}

int32 sim_sound_seek (SND_SAMPLE *smp[3], int32 dist, int32 s_lim, int32 m_lim,
                      t_bool force_long, int32 group, double gain)
{
int32 cls;

if (!snd_enabled || ((dist == 0) && !force_long))
    return -1;
if (force_long || (dist > m_lim))
    cls = 2;
else if (dist > s_lim)
    cls = 1;
else
    cls = 0;
sim_sound_play (smp[cls], group, gain, 0);
return cls;
}

void sim_sound_stop (SND_HANDLE handle, uint32 fade_ms)
{
if (handle != 0)
    snd_stop_ex (SND_CMD_STOP, handle, fade_ms);
}

double sim_sound_duration (const SND_SAMPLE *smp)
{
#if defined (USE_SIM_SOUND)
if ((smp != NULL) && (smp->data != NULL))
    return (double)smp->frames / (double)SND_RATE;
#endif
return 0.0;
}

/* System sounds */

static void snd_sys_load (void)
{
int32 i;

for (i = 0; i < SND_SYS_COUNT; i++) {
    const char *file = snd_sys_file (i);

    if (snd_sys_user[i][0] != '\0')              /* the user's file: no default */
        snd_sys_smp[i] = sim_sound_load (snd_sys_dir, file);
    else if (!snd_sys_use_default)              /* no default for this simulator */
        snd_sys_smp[i] = (file[0] != '\0') ? sim_sound_load (snd_sys_dir, file) : NULL;
    else if (file[0] == '\0') {                  /* no own name: the default */
        snd_sys_smp[i] = sim_sound_load (SND_DEFAULT_CPU, snd_sys_default_file[i]);
        if (snd_sys_smp[i] != NULL)
            snd_note_default (snd_sys_dir, "system", SND_DEFAULT_CPU);
        }
    else
        snd_sys_smp[i] = snd_load_with_default (snd_sys_dir, file, SND_DEFAULT_CPU,
                                                snd_sys_default_file[i], "system");
    }
}

/* Start the system sound.  With direct == FALSE the starting one-shot is
   played and the running loop follows it seamlessly; with direct == TRUE
   (sound enabled or loop changed while the power is already on) the
   running loop just fades in. */

static void snd_sys_start (t_bool direct)
{
SND_SAMPLE *starting = snd_sys_smp[SND_SYS_STARTING];
SND_SAMPLE *running = snd_sys_smp[SND_SYS_RUNNING];

if (!sim_sound_available ())
    return;
if (!direct && (starting != NULL))
    snd_sys_handle = snd_play_ex (starting, running, 0, SND_GRP_SYSTEM, 1.0, 0, 0);
else if (running != NULL)
    snd_sys_handle = snd_play_ex (running, NULL, 0, SND_GRP_SYSTEM, 1.0, SND_F_LOOP, 300);
}

void sim_sound_power_on (void)
{
if (snd_power)                                  /* already on: e.g. RUN after HALT */
    return;
snd_power = TRUE;
snd_sys_start (FALSE);
snd_unitloops_restore ();                       /* spindles that kept running */
flp_power (TRUE, FALSE);                        /* floppy AC motors start */
}

void sim_sound_power_off (t_bool wait)
{
SND_SAMPLE *stopping = snd_sys_smp[SND_SYS_STOPPING];

if (!snd_power)
    return;
snd_power = FALSE;
if (!sim_sound_available ())
    return;
/* Power off silences everything: the running loop and every unit loop
   fade out, the stopping sound plays.  Units that stay active (their state
   belongs to the device) get their loops back at the next power on. */
snd_stop_ex (SND_CMD_STOPALL, 0, 150);
snd_sys_handle = 0;
snd_unitloops_silence ();
flp_power (FALSE, FALSE);                       /* floppy AC motors run down */
if (stopping != NULL) {
    snd_play_ex (stopping, NULL, 0, SND_GRP_SYSTEM, 1.0, 0, 0);
    if (wait)                                   /* let the run-down finish before exit */
        sim_os_ms_sleep ((unsigned int)(sim_sound_duration (stopping) * 1000.0) + 100);
    }
else if (wait)
    sim_os_ms_sleep (200);                      /* let the loop fade out */
}

void sim_sound_exit (void)
{
sim_sound_power_off (TRUE);
snd_audio_close ();
}

/* Configuration checks for SET SOUND ENABLED: a device registers a
   function that returns SCPE_OK if its configuration may run in the
   realism mode, or prints why not and returns an error.  SET SOUND
   ENABLED is refused if one of them fails.  The list grows as needed;
   registering the same function twice is harmless. */

static t_stat (**snd_enable_checks) (void) = NULL;
static int32 snd_n_enable_checks = 0;

void sim_sound_register_enable_check (t_stat (*fn) (void))
{
t_stat (**n) (void);
int32 i;

for (i = 0; i < snd_n_enable_checks; i++)
    if (snd_enable_checks[i] == fn)
        return;
n = (t_stat (**) (void)) realloc ((void *) snd_enable_checks, (snd_n_enable_checks + 1) * sizeof (*n));
if (n == NULL)
    return;
snd_enable_checks = n;
snd_enable_checks[snd_n_enable_checks++] = fn;
}

/* Switch the sound (and the realism mode) on or off */

static t_stat snd_set_enabled (t_bool enable)
{
UNIT *uptr = snd_attached_storage_unit ();

if (enable == snd_enabled)
    return SCPE_OK;
if (uptr != NULL)
    return sim_messagef (SCPE_ALATT, "SET SOUND %s is only allowed while no disk or tape is attached (%s is attached)\n",
                         enable ? "ENABLED" : "DISABLED", sim_uname (uptr));
if (enable) {                                   /* the devices' rules */
    int32 i;
    t_stat r;

    for (i = 0; i < snd_n_enable_checks; i++)
        if ((r = snd_enable_checks[i] ()) != SCPE_OK)
            return r;
    }
snd_enabled = enable;
snd_apply_throttle (enable);
if (enable) {
    if (snd_audio_open ()) {
        snd_sys_load ();
        if (snd_power) {                        /* power already on: just the loops */
            snd_sys_start (TRUE);
            flp_power (TRUE, TRUE);
            }
        }
    else {
#if defined (USE_SIM_SOUND)
        sim_printf ("Sound: %s; sounds are ignored, realism mode is on\n", snd_error);
#else
        sim_printf ("Sound: built without SDL2 sound support; sounds are ignored, realism mode is on\n");
#endif
        }
    }
else {
    if (sim_sound_available ()) {
        snd_stop_ex (SND_CMD_STOPALL, 0, 100);  /* fade out everything, then close */
        sim_os_ms_sleep (150);
        }
    snd_audio_close ();
    snd_sys_handle = 0;
    memset (snd_sys_smp, 0, sizeof (snd_sys_smp));
    flp_forget_sounds ();
    }
return SCPE_OK;
}

/* Change one of the system sounds (SET SOUND STARTING/RUNNING/STOPPING) */

static t_stat snd_set_sys_file (int32 idx, const char *file)
{
SND_SAMPLE *smp;
char previous[CBUFSIZE];

strlcpy (previous, snd_sys_user[idx], sizeof (previous));
strlcpy (snd_sys_user[idx], file, CBUFSIZE);
if (!sim_sound_available ())
    return SCPE_OK;                             /* loaded when sound is enabled */
smp = sim_sound_load (snd_sys_dir, snd_sys_file (idx));
if ((smp == NULL) && (*snd_sys_file (idx) != '\0')) {
    strlcpy (snd_sys_user[idx], previous, CBUFSIZE);/* keep the previous sound */
    return SCPE_OPENERR | SCPE_NOMESSAGE;       /* the reason was reported */
    }
snd_sys_smp[idx] = smp;
if ((idx == SND_SYS_RUNNING) && snd_power) {    /* replace a playing loop */
    sim_sound_stop (snd_sys_handle, 300);
    snd_sys_handle = 0;
    snd_sys_start (TRUE);
    }
return SCPE_OK;
}

/* SET SOUND <item>[,<item>...]

   ENABLED | DISABLED          sound and realism mode on/off
   VOLUME=n | VOLUME=grp:n     master or group volume, 0..200 %
   MUTE[=grp] | NOMUTE[=grp]   (un)mute everything or one group
   POWER=ON | POWER=OFF        switch the system power by hand
   PATH=dir[;dir...]           user sound directories (rest of the line)
   STARTING=file, RUNNING=file, STOPPING=file
                               user system sounds (rest of the line);
                               an empty value restores the default

   The items taking a file or directory consume the rest of the line, so
   that names keep their case and may contain commas. */

typedef struct {
    const char *name;                           /* keyword */
    size_t min;                                 /* shortest abbreviation */
    int32 id;                                   /* item code */
    t_bool rest;                                /* value is the rest of the line */
    } SND_KW;

#define SND_KW_ENABLED  1
#define SND_KW_DISABLED 2
#define SND_KW_VOLUME   3
#define SND_KW_MUTE     4
#define SND_KW_NOMUTE   5
#define SND_KW_POWER    6
#define SND_KW_PATH     7
#define SND_KW_STARTING 8
#define SND_KW_RUNNING  9
#define SND_KW_STOPPING 10

static const SND_KW snd_keywords[] = {
    { "ENABLED",  3, SND_KW_ENABLED,  FALSE },
    { "DISABLED", 3, SND_KW_DISABLED, FALSE },
    { "VOLUME",   3, SND_KW_VOLUME,   FALSE },
    { "MUTE",     3, SND_KW_MUTE,     FALSE },
    { "NOMUTE",   3, SND_KW_NOMUTE,   FALSE },
    { "UNMUTE",   3, SND_KW_NOMUTE,   FALSE },
    { "POWER",    3, SND_KW_POWER,    FALSE },
    { "PATH",     3, SND_KW_PATH,     TRUE  },
    { "STARTING", 4, SND_KW_STARTING, TRUE  },
    { "RUNNING",  3, SND_KW_RUNNING,  TRUE  },
    { "STOPPING", 4, SND_KW_STOPPING, TRUE  },
    { NULL,       0, 0,               FALSE }
    };

/* Remove surrounding white space and quotes from a value (in place) */

static char *snd_trim_value (char *val)
{
size_t len;

while (isspace ((unsigned char)*val))
    val++;
len = strlen (val);
while ((len > 0) && isspace ((unsigned char)val[len - 1]))
    val[--len] = '\0';
if ((len >= 2) && ((val[0] == '"') || (val[0] == '\'')) && (val[len - 1] == val[0])) {
    val[len - 1] = '\0';
    val++;
    }
return val;
}

t_stat sim_sound_set (int32 flag, CONST char *cptr)
{
char kw[CBUFSIZE], valbuf[CBUFSIZE];
char *val;
const SND_KW *k;
size_t n;
t_stat r;

if ((cptr == NULL) || (*cptr == '\0'))
    return sim_messagef (SCPE_2FARG, "Missing SET SOUND argument\n");
while (*cptr != '\0') {
    while (isspace ((unsigned char)*cptr) || (*cptr == ','))
        cptr++;
    if (*cptr == '\0')
        break;
    for (n = 0; isalpha ((unsigned char)*cptr) && (n < sizeof (kw) - 1); n++)
        kw[n] = (char)toupper ((unsigned char)*cptr++);
    kw[n] = '\0';
    for (k = snd_keywords; k->name != NULL; k++) {
        if ((n >= k->min) && (strncmp (kw, k->name, n) == 0))
            break;
        }
    if (k->name == NULL)
        return sim_messagef (SCPE_ARG, "Unknown SET SOUND argument: %s\n", n ? kw : cptr);
    while (isspace ((unsigned char)*cptr))
        cptr++;
    val = NULL;
    if (*cptr == '=') {                         /* value */
        cptr++;
        if (k->rest) {                          /* the rest of the line */
            strlcpy (valbuf, cptr, sizeof (valbuf));
            cptr += strlen (cptr);
            }
        else {                                  /* up to the next comma */
            for (n = 0; (*cptr != '\0') && (*cptr != ',') && (n < sizeof (valbuf) - 1); n++)
                valbuf[n] = *cptr++;
            valbuf[n] = '\0';
            }
        val = snd_trim_value (valbuf);
        }
    else if ((*cptr != '\0') && (*cptr != ','))
        return sim_messagef (SCPE_ARG, "Unexpected SET SOUND argument: %s\n", cptr);

    switch (k->id) {

    case SND_KW_ENABLED:
    case SND_KW_DISABLED:
        if (val != NULL)
            return sim_messagef (SCPE_ARG, "SET SOUND %s takes no value\n", k->name);
        r = snd_set_enabled (k->id == SND_KW_ENABLED);
        if (r != SCPE_OK)
            return r;
        break;

    case SND_KW_VOLUME:                         /* VOLUME=n or VOLUME=group:n */
        if ((val == NULL) || (*val == '\0'))
            return sim_messagef (SCPE_ARG, "SET SOUND VOLUME needs a value\n");
        if (strchr (val, ':') != NULL) {
            char *colon = strchr (val, ':');
            int32 grp;

            *colon = '\0';
            grp = snd_find_group (snd_trim_value (val));
            if (grp < 0)
                return sim_messagef (SCPE_ARG, "Unknown sound group: %s\n", val);
            r = snd_parse_volume (snd_trim_value (colon + 1), &snd_group_vol[grp]);
            }
        else
            r = snd_parse_volume (val, &snd_master_vol);
        if (r != SCPE_OK)
            return r;
        break;

    case SND_KW_MUTE:                           /* MUTE or MUTE=group */
    case SND_KW_NOMUTE:
        if ((val == NULL) || (*val == '\0'))
            snd_master_mute = (k->id == SND_KW_MUTE);
        else {
            int32 grp = snd_find_group (val);

            if (grp < 0)
                return sim_messagef (SCPE_ARG, "Unknown sound group: %s\n", val);
            snd_group_mute[grp] = (k->id == SND_KW_MUTE);
            }
        break;

    case SND_KW_POWER:                          /* POWER=ON or POWER=OFF */
        if ((val != NULL) && (strlen (val) >= 2) && (MATCH_CMD (val, "ON") == 0))
            sim_sound_power_on ();
        else if ((val != NULL) && (strlen (val) >= 2) && (MATCH_CMD (val, "OFF") == 0))
            sim_sound_power_off (FALSE);
        else
            return sim_messagef (SCPE_ARG, "SET SOUND POWER=ON or POWER=OFF expected\n");
        break;

    case SND_KW_PATH:                           /* PATH=dir;dir... */
        strlcpy (snd_path, val ? val : "", sizeof (snd_path));
        snd_cache_forget_failures ();           /* look again for missing files */
        break;

    case SND_KW_STARTING:
    case SND_KW_RUNNING:
    case SND_KW_STOPPING:
        snd_cache_forget_failures ();
        r = snd_set_sys_file (k->id - SND_KW_STARTING, val ? val : "");
        if (r != SCPE_OK)
            return r;
        break;
        }
    }
return SCPE_OK;
}

/* SHOW SOUND */

t_stat sim_sound_show (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, CONST char *cptr)
{
int32 i;

if ((cptr != NULL) && (*cptr != '\0'))
    return SCPE_2MARG;
fprintf (st, "Sound:   %s\n", snd_enabled ? "enabled (realism mode on)" : "disabled");
snd_show_backend (st);
fprintf (st, "Power:   %s\n", snd_power ? "on" : "off");
fprintf (st, "Volume:  master %d%%%s", (int)snd_master_vol, snd_master_mute ? " (muted)" : "");
for (i = 0; i < SND_GRP_COUNT; i++)
    fprintf (st, ", %s %d%%%s", snd_group_names[i], (int)snd_group_vol[i], snd_group_mute[i] ? " (muted)" : "");
fprintf (st, "\n");
fprintf (st, "Path:    %s\n", snd_path[0] ? snd_path : "(none)");
if (snd_throttle[0] != '\0')
    fprintf (st, "CPU:     realistic speed THROTTLE %s%s\n", snd_throttle,
             snd_throttle_set ? " (set)" : (snd_enabled ? " (not used: THROTTLE set by the user)" : ""));
fprintf (st, "System sounds (directory \"%s\"):\n", snd_sys_dir);
for (i = 0; i < SND_SYS_COUNT; i++) {
    const char *file = snd_sys_file (i);

    fprintf (st, "  %-9s %s%s", snd_sys_names[i], file[0] ? file : "(none)",
             snd_sys_user[i][0] ? " (user)" : "");
#if defined (USE_SIM_SOUND)
    if ((snd_sys_smp[i] != NULL) && (snd_sys_smp[i]->path != NULL))
        fprintf (st, " = %s, %.2f s", snd_sys_smp[i]->path, sim_sound_duration (snd_sys_smp[i]));
#endif
    fprintf (st, "\n");
    }
fprintf (st, "Defaults: %s\n", snd_default_uses ? snd_default_uses : "(none used yet)");
if (sim_sound_available ())
    fprintf (st, "Voices:  %d playing\n", (int)snd_active_voices ());
return SCPE_OK;
}

/* Floppy disk drives (see sim_sound.h) */

/* Drive types.  Sources:
   RX02: RX02 Floppy Disk System Technical Manual EK-0RX02-TM-001, 1.4
         (360 rpm, 166 ms/rev, track-to-track 6 ms max., head settle
         25 ms max., 77 tracks of 26 sectors, average access 262 ms =
         77/3 x 6 + 25 + 166/2), 5.3.4.4 (head unloaded if no new command
         within 200 ms), 5.3.4.1 (ready within 2 s of power), 2.x (power
         up: the heads move one track in and back); RX02 print set MP00629
         (H771 power supply: both spin motors on the AC line).
   RX01: RX8/RX11 Floppy Disk System Maintenance Manual EK-RX01-MM-002
         (Dec 1976): 1.6 (track-to-track 10 ms max., head settle 25 ms
         max., 360 rpm), microcode STEPHD (the head is loaded after the
         steps, then a 20 ms delay), the head is unloaded "to reduce
         diskette wear" when no new command (RUN) comes within 45.87 ms,
         2.4.4 (at power up the heads move ten tracks in and back).  The
         drive mechanics are those of the RX02 (same drive): its sounds
         are used unless rx01/ has files of its own. */

const SND_FLP_TYPE sim_flp_rx01 = {
    "RX01", "rx01", 360.0, 77, 26, SND_FLP_MOTOR_AC, 10,
    10000, 20000, 20000, 46, 2000, 0, "rx02" };
const SND_FLP_TYPE sim_flp_rx02 = {
    "RX02", "rx02", 360.0, 77, 26, SND_FLP_MOTOR_AC, 1,
    6000, 25000, 25000, 200, 2000, 0, NULL };

#define FLP_S_MOTOR_START   0                   /* sound indexes */
#define FLP_S_MOTOR         1
#define FLP_S_MOTOR_STOP    2
#define FLP_S_DISK          3
#define FLP_S_HEAD          4
#define FLP_S_HEAD_LOAD     5
#define FLP_S_HEAD_UNLOAD   6
#define FLP_S_STEP          7
#define FLP_S_DOOR_CLOSE    8
#define FLP_S_DOOR_OPEN     9

static const char *flp_snd_names[SND_FLP_NSMP] = {
    "motor_start", "motor", "motor_stop", "disk", "head",
    "head_load", "head_unload", "step", "door_close", "door_open" };

#define FLP_DEFAULT_DIR     "default_floppy"
#define FLP_GAIN_DISK       0.5                 /* the loops relative to the drive gain */
#define FLP_GAIN_HEAD       0.6

static SND_FLOPPY **flp_list = NULL;            /* every floppy drive ever initialized */
static int32 flp_n = 0;
static uint32 flp_rand_state = 4711;            /* rotational phases (simulator thread) */

static double flp_random (void)                 /* 0 <= x < 1 */
{
flp_rand_state = flp_rand_state * 1103515245u + 12345u;
return (double)((flp_rand_state >> 8) & 0xFFFFFF) / 16777216.0;
}

/* The drive takes part: its unit and device are enabled */

static t_bool flp_present (SND_FLOPPY *f)
{
DEVICE *dptr;

if ((f->uptr == NULL) || (f->uptr->flags & UNIT_DIS))
    return FALSE;
dptr = find_dev_from_unit (f->uptr);
return (dptr != NULL) && ((dptr->flags & DEV_DIS) == 0);
}

static void flp_load_sounds (SND_FLOPPY *f)
{
int32 i;

if (f->loaded || !sim_sound_available ())
    return;
for (i = 0; i < SND_FLP_NSMP; i++) {
    char own[CBUFSIZE], def[CBUFSIZE];

    snprintf (own, sizeof (own), "%s_%s.wav", f->type->prefix, flp_snd_names[i]);
    snprintf (def, sizeof (def), "%s.wav", flp_snd_names[i]);
    f->smp[i] = NULL;
    if (f->type->alias != NULL) {               /* own file, else the same drive's */
        char alias[CBUFSIZE];                   /* (RX01: rx02/rx02_*.wav) */

        if ((f->smp[i] = sim_sound_load_optional (f->type->prefix, own)) == NULL) {
            snprintf (alias, sizeof (alias), "%s_%s.wav", f->type->alias, flp_snd_names[i]);
            if ((f->smp[i] = sim_sound_load_optional (f->type->alias, alias)) != NULL)
                snd_note_default (f->type->prefix, "floppy", f->type->alias);
            }
        }
    if (f->smp[i] == NULL)                      /* else the default floppy */
        f->smp[i] = snd_load_with_default (f->type->prefix, own, FLP_DEFAULT_DIR, def, "floppy");
    }
f->loaded = TRUE;
}

/* SET SOUND DISABLED closes the audio output: the samples are gone */

static void flp_forget_sounds (void)
{
int32 i;

for (i = 0; i < flp_n; i++) {
    flp_list[i]->loaded = FALSE;
    memset (flp_list[i]->smp, 0, sizeof (flp_list[i]->smp));
    flp_list[i]->motor_h = flp_list[i]->disk_h = flp_list[i]->head_h = 0;
    }
}

static t_bool flp_motor_running (SND_FLOPPY *f)
{
return snd_power;                               /* SND_FLP_MOTOR_AC (the only kind so far) */
}

static void flp_disk_start (SND_FLOPPY *f, uint32 fade_ms, double delay_s)
{
if ((f->disk_h == 0) && f->inserted && flp_motor_running (f))
    f->disk_h = snd_play_full (f->smp[FLP_S_DISK], NULL, 0, f->group, f->gain * FLP_GAIN_DISK,
                               SND_F_LOOP | SND_F_RANDPHASE, fade_ms, 1.0, delay_s, 0.0, 0);
}

static void flp_unload_done (SND_TIMER *t);

void sim_floppy_init (SND_FLOPPY *f, UNIT *uptr, const SND_FLP_TYPE *type, SND_FLP_REGS *regs,
                      int32 group, double gain)
{
SND_FLOPPY **n;

if (f->uptr != NULL)                            /* already initialized */
    return;
n = (SND_FLOPPY **) realloc (flp_list, (flp_n + 1) * sizeof (*flp_list));
if (n == NULL)
    return;
flp_list = n;
memset (f, 0, sizeof (*f));
f->uptr = uptr;
f->type = type;
f->regs = regs;
f->group = group;
f->gain = gain;
f->phase_us = flp_random () * (60.0e6 / type->rpm);
sim_sound_timer_init (&f->unload_tmr, "FLOPPY", &flp_unload_done, f);
flp_list[flp_n++] = f;
}

void sim_floppy_set_defaults (SND_FLP_REGS *regs, const SND_FLP_TYPE *type)
{
regs->step_us = type->step_us;
regs->settle_us = type->settle_us;
regs->hdload_us = type->hdload_us;
regs->hdunload_ms = type->hdunload_ms;
regs->motorup_ms = type->motorup_ms;
regs->motoroff_ms = type->motoroff_ms;
}

/* A diskette attached to a floppy drive keeps SET SOUND ENABLED/DISABLED
   from changing the timing under it (the floppy devices are not DEV_DISK) */

static UNIT *flp_attached_unit (void)
{
int32 i;

for (i = 0; i < flp_n; i++)
    if ((flp_list[i]->uptr != NULL) && (flp_list[i]->uptr->flags & UNIT_ATT))
        return flp_list[i]->uptr;
return NULL;
}

/* Power on: the AC motors start (direct: SET SOUND ENABLED with the power
   already on, just the running loops); the heads step in and back (the
   drive's power-up homing).  Power off: they run down, the heads unload. */

static void flp_power (t_bool on, t_bool direct)
{
int32 i;

if (!sim_sound_enabled ())
    return;
for (i = 0; i < flp_n; i++) {
    SND_FLOPPY *f = flp_list[i];

    if (!flp_present (f) || (f->type->motor != SND_FLP_MOTOR_AC))
        continue;
    flp_load_sounds (f);
    if (on) {
        double up_s = (double)f->regs->motorup_ms / 1000.0;

        if (direct || (f->smp[FLP_S_MOTOR_START] == NULL))
            f->motor_h = snd_play_full (f->smp[FLP_S_MOTOR], NULL, 0, f->group, f->gain,
                                        SND_F_LOOP | SND_F_RANDPHASE, 300, 1.0, 0.0, 0.0, 0);
        else
            f->motor_h = snd_play_ex (f->smp[FLP_S_MOTOR_START], f->smp[FLP_S_MOTOR], SND_F_RANDPHASE,
                                      f->group, f->gain, 0, 0);
        flp_disk_start (f, direct ? 300 : (uint32)(up_s * 1000.0), 0.0);
        if (!direct && (f->type->powerup_steps > 0)) {  /* power-up homing */
            int32 n = f->type->powerup_steps;

            snd_play_repeat (f->smp[FLP_S_STEP], f->group, f->gain, 2 * n,
                             (double)f->regs->step_us / 1.0e6, up_s);
            f->track = 0;
            }
        }
    else {                                      /* the loops were faded by power off */
        f->motor_h = f->disk_h = f->head_h = 0;
        f->head_loaded = FALSE;
        sim_sound_timer_cancel (&f->unload_tmr);
        snd_play_ex (f->smp[FLP_S_MOTOR_STOP], NULL, 0, f->group, f->gain, 0, 0);
        }
    }
}

void sim_floppy_insert (SND_FLOPPY *f)
{
f->inserted = TRUE;
f->head_loaded = FALSE;
f->phase_us = flp_random () * sim_floppy_rev_us (f);
if (!sim_sound_enabled () || (f->uptr == NULL))
    return;
flp_load_sounds (f);
snd_play_ex (f->smp[FLP_S_DOOR_CLOSE], NULL, 0, f->group, f->gain, 0, 0);
flp_disk_start (f, 400, 0.3);                   /* the cone clamps the diskette */
}

void sim_floppy_remove (SND_FLOPPY *f)
{
t_bool was = f->inserted;

f->inserted = FALSE;
if (!sim_sound_enabled () || (f->uptr == NULL)) {
    f->head_loaded = FALSE;
    return;
    }
sim_sound_timer_cancel (&f->unload_tmr);
f->head_loaded = FALSE;                         /* opening the door lifts the head */
sim_sound_stop (f->head_h, 30);
sim_sound_stop (f->disk_h, 200);
f->head_h = f->disk_h = 0;
if (was) {
    flp_load_sounds (f);
    snd_play_ex (f->smp[FLP_S_DOOR_OPEN], NULL, 0, f->group, f->gain, 0, 0);
    }
}

/* Head load: the solenoid clacks after delay_us (the steps come first);
   returns the head load time, 0 if the head was loaded already */

static double flp_head_load (SND_FLOPPY *f, double delay_us)
{
if (f->head_loaded)
    return 0.0;
f->head_loaded = TRUE;
if (f->regs->hdload_us <= 0)                    /* no solenoid */
    return 0.0;
if (sim_sound_enabled ()) {
    flp_load_sounds (f);
    snd_play_full (f->smp[FLP_S_HEAD_LOAD], NULL, 0, f->group, f->gain, 0, 0, 1.0,
                   delay_us / 1.0e6, 0.0, 0);
    if (f->inserted && flp_motor_running (f))
        f->head_h = snd_play_full (f->smp[FLP_S_HEAD], NULL, 0, f->group, f->gain * FLP_GAIN_HEAD,
                                   SND_F_LOOP | SND_F_RANDPHASE, 20, 1.0, delay_us / 1.0e6, 0.0, 0);
    }
return (double)f->regs->hdload_us;
}

void sim_floppy_unload (SND_FLOPPY *f)
{
if (f->uptr == NULL)
    return;
sim_sound_timer_cancel (&f->unload_tmr);
if (!f->head_loaded)
    return;
f->head_loaded = FALSE;
if (sim_sound_enabled () && (f->regs->hdload_us > 0)) {
    flp_load_sounds (f);
    sim_sound_stop (f->head_h, 30);
    f->head_h = 0;
    snd_play_ex (f->smp[FLP_S_HEAD_UNLOAD], NULL, 0, f->group, f->gain, 0, 0);
    }
}

static void flp_unload_done (SND_TIMER *t)
{
sim_floppy_unload ((SND_FLOPPY *)t->ctx);
}

double sim_floppy_seek (SND_FLOPPY *f, int32 track, t_bool load)
{
int32 dist;
double t, settle = 0.0, hl;

if (f->uptr == NULL)
    return 0.0;
sim_sound_timer_cancel (&f->unload_tmr);        /* a command is running */
dist = abs (track - f->track);
t = (double)dist * (double)f->regs->step_us;
if ((dist > 0) && sim_sound_enabled ()) {
    flp_load_sounds (f);
    snd_play_repeat (f->smp[FLP_S_STEP], f->group, f->gain, (uint32)dist,
                     (double)f->regs->step_us / 1.0e6, 0.0);
    }
f->track = track;
if (dist > 0)
    settle = (double)f->regs->settle_us;
if (load) {                                     /* RX02: step, then load the head */
    hl = flp_head_load (f, t);                  /* and wait 25 ms (flowchart 5-28) */
    if (hl > settle)
        settle = hl;
    }
return t + settle;
}

double sim_floppy_rev_us (SND_FLOPPY *f)
{
return (f->type != NULL) ? 60.0e6 / f->type->rpm : 0.0;
}

double sim_floppy_rotate (SND_FLOPPY *f, double after_us, int32 sector, int32 nsect)
{
double rev = sim_floppy_rev_us (f);
double sec_us, angle, wait;

if ((f->type == NULL) || (rev <= 0.0))
    return after_us;
sec_us = rev / (double)f->type->sectors;
angle = fmod (sim_sound_rt_us () + after_us + f->phase_us, rev);
wait = (double)sector * sec_us - angle;
if (wait < 0.0)
    wait += rev;
return after_us + wait + (double)nsect * sec_us;
}

void sim_floppy_end (SND_FLOPPY *f, double end_us)
{
if ((f->uptr == NULL) || !f->head_loaded || !sim_sound_enabled () || (f->regs->hdunload_ms <= 0))
    return;
sim_sound_timer_start (&f->unload_tmr, end_us + 1000.0 * (double)f->regs->hdunload_ms);
}

void sim_floppy_steps (SND_FLOPPY *f, int32 n, double period_us, double delay_us)
{
if ((n <= 0) || !sim_sound_enabled () || (f->uptr == NULL))
    return;
flp_load_sounds (f);
snd_play_repeat (f->smp[FLP_S_STEP], f->group, f->gain, (uint32)n, period_us / 1.0e6, delay_us / 1.0e6);
}

int32 sim_floppy_track (SND_FLOPPY *f)
{
return f->track;
}

t_stat sim_floppy_show (FILE *st, SND_FLOPPY *f)
{
if (f->uptr == NULL) {
    fprintf (st, "not initialized\n");
    return SCPE_OK;
    }
(void) sim_sound_timer_busy (&f->unload_tmr);   /* at the prompt: makes a due unload happen */
fprintf (st, "%s, head %s", f->inserted ? "diskette in" : "no diskette",
         f->head_loaded ? "loaded" : "unloaded");
if (f->head_loaded && sim_sound_timer_busy (&f->unload_tmr))
    fprintf (st, " (unloads in %.2f s)", sim_sound_timer_left_s (&f->unload_tmr));
fprintf (st, ", track %d, motor %s\n", (int)f->track,
         !sim_sound_enabled () ? "not simulated (sound disabled)" :
         (flp_motor_running (f) ? "running" : "stopped (power off)"));
return SCPE_OK;
}
