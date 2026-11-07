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
   - tapes: sound/default_tape/ (the first tape sounds, still to come);
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
return NULL;
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

typedef struct {
    int32 type;                                 /* SND_CMD_xxx */
    SND_HANDLE handle;                          /* voice handle */
    SND_SAMPLE *smp;                            /* sample to play */
    SND_SAMPLE *chain;                          /* loop that follows a one-shot (or NULL) */
    int32 group;                                /* mixer group */
    float gain;                                 /* voice gain */
    uint32 flags;                               /* SND_F_xxx */
    uint32 chain_flags;                         /* SND_F_xxx of the chained loop */
    uint32 fade;                                /* fade-in (PLAY) or fade-out (STOP) in frames */
    } SND_CMD;

/* A voice: one sample being played.  Only the audio thread touches the
   voices while the device is open. */

typedef struct {
    t_bool active;                              /* voice in use */
    SND_HANDLE handle;                          /* handle (0 after a chain handed it on) */
    SND_SAMPLE *smp;                            /* sample */
    SND_SAMPLE *chain;                          /* loop to start near the end (or NULL) */
    uint32 chain_flags;                         /* flags of that loop */
    uint32 pos;                                 /* next frame */
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

static void snd_voice_start (SND_HANDLE handle, SND_SAMPLE *smp, SND_SAMPLE *chain,
                             uint32 chain_flags, int32 group, float gain, uint32 flags, uint32 fade)
{
int32 i;
SND_VOICE *v;

if ((smp == NULL) || (smp->data == NULL) || (smp->frames == 0))
    return;
for (i = 0; i < SND_VOICES; i++) {
    if (!snd_voices[i].active)
        break;
    }
if (i == SND_VOICES)                            /* all voices busy: drop the request */
    return;
v = &snd_voices[i];
memset (v, 0, sizeof (*v));
v->active = TRUE;
v->handle = handle;
v->smp = smp;
v->chain = ((chain != NULL) && (chain->data != NULL)) ? chain : NULL;
v->chain_flags = chain_flags | SND_F_LOOP;
v->loop = (flags & SND_F_LOOP) ? TRUE : FALSE;
v->pos = ((flags & SND_F_RANDPHASE) && v->loop) ? (snd_random () % smp->frames) : 0;
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

/* Execute the queued commands (audio thread, queue mutex held) */

static void snd_process_queue (void)
{
while (snd_qhead != snd_qtail) {
    SND_CMD *c = &snd_queue[snd_qhead];
    int32 i;

    snd_qhead = (snd_qhead + 1) % SND_QSIZE;
    switch (c->type) {
        case SND_CMD_PLAY:
            snd_voice_start (c->handle, c->smp, c->chain, c->chain_flags, c->group, c->gain, c->flags, c->fade);
            break;
        case SND_CMD_STOP:
            for (i = 0; i < SND_VOICES; i++) {
                if (snd_voices[i].active && (snd_voices[i].handle == c->handle))
                    snd_voice_fade_out (&snd_voices[i], c->fade);
                }
            break;
        case SND_CMD_STOPALL:
            for (i = 0; i < SND_VOICES; i++) {
                if (snd_voices[i].active)
                    snd_voice_fade_out (&snd_voices[i], c->fade);
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
    /* A one-shot followed by a loop (system starting -> running): when the
       crossfade region is reached within this block, start the loop with a
       fade-in, hand the handle on to it and fade the one-shot out. */
    if ((v->chain != NULL) && !v->loop) {
        uint32 xfade = SND_CHAIN_XFADE;

        if (xfade > v->smp->frames / 2)
            xfade = v->smp->frames / 2;
        if (xfade == 0)
            xfade = 1;
        if (v->pos + (uint32)n >= v->smp->frames - xfade) {
            snd_voice_start (v->handle, v->chain, NULL, 0, v->group, v->gain, v->chain_flags, xfade);
            v->handle = 0;
            v->chain = NULL;
            v->fade_step = -1.0f / (float)xfade;
            }
        }
    data = v->smp->data;
    frames = v->smp->frames;
    g = v->gain * group_gain[v->group];
    for (i = 0; i < n; i++) {
        if (v->pos >= frames) {
            if (!v->loop) {                     /* one-shot finished */
                snd_voice_end (v);
                break;
                }
            v->pos = 0;
            }
        out[i] += data[v->pos++] * g * v->fade;
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

static SND_HANDLE snd_play_ex (SND_SAMPLE *smp, SND_SAMPLE *chain, uint32 chain_flags,
                               int32 group, double gain, uint32 flags, uint32 fade_ms)
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
return snd_queue_cmd (&cmd) ? cmd.handle : 0;
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

static void snd_spindles_run_state (t_bool running);

void sim_sound_run_state (t_bool running)
{
int32 i;

snd_spindles_run_state (running);
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
sim_activate (uptr, (int32)inst);
}

/* Spindles (see sim_sound.h) */

/* Spindle registry.  There is no fixed limit: spindles are registered in
   chunks of SND_SPIN_CHUNK, each with its own internal timer device
   (INT-SPINDLE, INT-SPINDLE2, ...), so a timer unit never moves while it
   is on the event queue.  The number of drives is limited only by the
   devices (controllers and their units).  (Up to sound patch v12 there
   was a fixed table of 32, then 64, spindles: known issues case 23.)
   A timer unit finds its spindle through up7. */

#define SND_SPIN_CHUNK  32

typedef struct snd_spin_chunk {
    SND_SPINDLE             *sp[SND_SPIN_CHUNK];        /* registered spindles */
    UNIT                    units[SND_SPIN_CHUNK];      /* their timers */
    DEVICE                  dev;                        /* internal device of the timers */
    char                    name[24];                   /* its name */
    struct snd_spin_chunk   *next;
    } SND_SPIN_CHUNK_T;

static SND_SPIN_CHUNK_T *snd_spin_chunks = NULL;        /* the chunks, in order */

static const char *snd_spindle_description (DEVICE *dptr)
{
return "Disk spindle timers (sound realism mode)";
}

/* The next free slot: its chunk and index (a new chunk if all are full);
   FALSE only if out of memory */

static t_bool snd_spindle_slot (SND_SPIN_CHUNK_T **chunk, int32 *index)
{
SND_SPIN_CHUNK_T *c, **link = &snd_spin_chunks;
int32 i, n = 0;

for (c = snd_spin_chunks; c != NULL; c = c->next) {
    for (i = 0; i < SND_SPIN_CHUNK; i++)
        if (c->sp[i] == NULL) {
            *chunk = c;
            *index = i;
            return TRUE;
            }
    link = &c->next;
    n = n + 1;
    }
c = (SND_SPIN_CHUNK_T *) calloc (1, sizeof (*c));
if (c == NULL)
    return FALSE;
if (n == 0)
    snprintf (c->name, sizeof (c->name), "INT-SPINDLE");
else
    snprintf (c->name, sizeof (c->name), "INT-SPINDLE%d", n + 1);
c->dev.name = c->name;
c->dev.units = c->units;
c->dev.numunits = SND_SPIN_CHUNK;
c->dev.flags = DEV_NOSAVE;
c->dev.description = &snd_spindle_description;
*link = c;
sim_register_internal_device (&c->dev);
*chunk = c;
*index = 0;
return TRUE;
}

/* A phase (spin-up or spin-down) is over */

static void snd_spindle_phase_done (SND_SPINDLE *sp)
{
sp->paused = FALSE;
sp->left_us = 0.0;
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

static t_stat snd_spindle_svc (UNIT *uptr)
{
SND_SPINDLE *sp = (SND_SPINDLE *) uptr->up7;

if (sp != NULL)
    snd_spindle_phase_done (sp);
return SCPE_OK;
}

/* Start timing a phase of usecs: scheduled now while the CPU runs, kept
   in the structure (wall time) at the prompt */

static void snd_spindle_schedule (SND_SPINDLE *sp, double usecs)
{
sim_cancel (sp->timer);
sp->left_us = usecs;
if (sim_is_running) {
    sp->paused = FALSE;
    sim_sound_activate_us (sp->timer, usecs);
    }
else {
    sp->paused = TRUE;
    sp->ref_ms = sim_os_msec ();
    }
}

/* At the prompt: deduct the wall time since the last look; finish the
   phase if it is over */

static void snd_spindle_catch_up (SND_SPINDLE *sp)
{
uint32 now;

if (!sp->paused || ((sp->state != SND_SPIN_UP) && (sp->state != SND_SPIN_DOWN)))
    return;
now = sim_os_msec ();
sp->left_us -= 1000.0 * (double)(now - sp->ref_ms);
sp->ref_ms = now;
if (sp->left_us < 1.0)
    snd_spindle_phase_done (sp);
}

/* Execution starts: schedule what is left; execution stops: keep it */

static void snd_spindle_run_state (SND_SPINDLE *sp, t_bool running)
{
if ((sp == NULL) || ((sp->state != SND_SPIN_UP) && (sp->state != SND_SPIN_DOWN)))
    return;
if (running) {
    if (sp->paused) {
        snd_spindle_catch_up (sp);
        if ((sp->state == SND_SPIN_UP) || (sp->state == SND_SPIN_DOWN)) {
            sp->paused = FALSE;
            sim_cancel (sp->timer);
            sim_sound_activate_us (sp->timer, sp->left_us);
            }
        }
    else if (!sim_is_active (sp->timer))        /* lost: finish it */
        snd_spindle_phase_done (sp);
    }
else if (!sp->paused) {
    sp->left_us = sim_is_active (sp->timer) ?
        (double)sim_activate_time (sp->timer) / sim_sound_inst_per_us () : 0.0;
    sim_cancel (sp->timer);
    sp->paused = TRUE;
    sp->ref_ms = sim_os_msec ();
    }
}

static void snd_spindles_run_state (t_bool running)
{
SND_SPIN_CHUNK_T *c;
int32 i;

for (c = snd_spin_chunks; c != NULL; c = c->next)
    for (i = 0; i < SND_SPIN_CHUNK; i++)
        snd_spindle_run_state (c->sp[i], running);
}

void sim_spindle_init (SND_SPINDLE *sp, UNIT *uptr, int32 *spinup_ms, int32 *spindown_ms,
                       void (*on_ready) (UNIT *uptr), void (*on_stopped) (UNIT *uptr),
                       int32 group, double gain)
{
SND_SPIN_CHUNK_T *c;
int32 i;

if (sp->uptr != NULL)                           /* already initialized */
    return;
if (!snd_spindle_slot (&c, &i)) {               /* out of memory: sp->uptr */
    sim_printf ("Sound: out of memory for the spindle of %s\n", sim_uname (uptr));
    return;                                     /* stays NULL (RQ: the drive */
    }                                           /* works without realism) */
memset (sp, 0, sizeof (*sp));
sp->uptr = uptr;
sp->spinup_ms = spinup_ms;
sp->spindown_ms = spindown_ms;
sp->on_ready = on_ready;
sp->on_stopped = on_stopped;
sp->group = group;
sp->gain = gain;
sp->state = SND_SPIN_STOPPED;
memset (&c->units[i], 0, sizeof (c->units[i]));
c->units[i].action = &snd_spindle_svc;
c->units[i].up7 = sp;
sp->timer = &c->units[i];
c->sp[i] = sp;
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
    sim_cancel (sp->timer);
    sp->state = SND_SPIN_UP;
    snd_spindle_phase_done (sp);
    return;
    }
sp->state = SND_SPIN_UP;
snd_spindle_schedule (sp, 1000.0 * ((*sp->spinup_ms > 0) ? *sp->spinup_ms : 1));
}

void sim_spindle_stop (SND_SPINDLE *sp)
{
if ((sp->uptr == NULL) || (sp->state == SND_SPIN_STOPPED) || (sp->state == SND_SPIN_DOWN))
    return;
sim_sound_unit_stop (&sp->sound, sp->group, sp->gain);
sim_cancel (sp->timer);
sp->state = SND_SPIN_DOWN;
if (!snd_enabled || (*sp->spindown_ms <= 0))    /* normal mode: at once */
    snd_spindle_phase_done (sp);
else
    snd_spindle_schedule (sp, 1000.0 * *sp->spindown_ms);
}

void sim_spindle_off (SND_SPINDLE *sp)
{
if (sp->uptr == NULL)
    return;
if ((sp->state == SND_SPIN_UP) || (sp->state == SND_SPIN_READY))
    sim_sound_unit_stop (&sp->sound, sp->group, sp->gain);
sim_cancel (sp->timer);
sp->state = SND_SPIN_STOPPED;
sp->paused = FALSE;
}

void sim_spindle_ready (SND_SPINDLE *sp)
{
if (sp->uptr == NULL)
    return;
sim_cancel (sp->timer);
sp->state = SND_SPIN_READY;
sp->paused = FALSE;
}

int32 sim_spindle_state (SND_SPINDLE *sp)
{
if (!sim_is_running)                            /* prompt: wall time passed */
    snd_spindle_catch_up (sp);
return sp->state;
}

static double snd_spindle_left_s (SND_SPINDLE *sp)
{
if (sp->paused)
    return sp->left_us / 1000000.0;
if (!sim_is_active (sp->timer))
    return 0.0;
return (double)sim_activate_time (sp->timer) / sim_sound_inst_per_us () / 1000000.0;
}

void sim_spindle_wait (SND_SPINDLE *sp)
{
int32 st = sim_spindle_state (sp);

if (sim_is_running || ((st != SND_SPIN_UP) && (st != SND_SPIN_DOWN)))
    return;
if (st == SND_SPIN_UP)
    sim_printf ("%s: spinning up, ready in %.0f s ...\n", sim_uname (sp->uptr), snd_spindle_left_s (sp));
else
    sim_printf ("%s: spinning down, at rest in %.0f s ...\n", sim_uname (sp->uptr), snd_spindle_left_s (sp));
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
        fprintf (st, "spinning up, %.1f s left", snd_spindle_left_s (sp));
        break;
    case SND_SPIN_READY:
        fprintf (st, "online");
        break;
    case SND_SPIN_DOWN:
        fprintf (st, "spinning down, %.1f s left", snd_spindle_left_s (sp));
        break;
    default:
        fprintf (st, "offline");
        break;
    }
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
        if (snd_power)                          /* power already on: just the loop */
            snd_sys_start (TRUE);
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
