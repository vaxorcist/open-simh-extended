/* vax730_rb.c: RB730 disk simulator

   Copyright (c) 2010-2011, Matt Burke
   This module incorporates code from SimH, Copyright (c) 1993-2008, Robert M Supnik

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
   THE AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name(s) of the author(s) shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author(s).

   rb           RB730 - RB02/RB80 disk controller

   04-Nov-2026  Ulli    Sound realism mode (SET SOUND ENABLED): SET RBn
                        ONLINE/LOAD and OFFLINE/UNLOAD are the LOAD button of
                        the RB02 (an RL02), ATTACH waits for ONLINE, spin-up/
                        down (SPINUP, SPINDOWN); realistic RB02 and RB80 seek,
                        latency and transfer times, seek sounds (sound/rl02,
                        sound/rm80); SHOW RBn SPINDLE
   04-Nov-2026  Ulli    A finishing seek only raises the drive's attention (it
                        decoded the controller's current command, which may
                        belong to another drive); RB80 with 31 sectors per
                        track, as DEC's drivers use it (242,606 blocks, as
                        the RM80); the old layout keeps 32
   04-Nov-2026  Ulli    Range checks: a seek stops at the last cylinder, a
                        transfer to a track beyond the last head fails
   04-Nov-2026  Ulli    Status: GET STATUS reports the RB02 status bits (was
                        an operator precedence error), READ HEADER returns the
                        current cylinder and head, a drive without a cartridge
                        is not ready and ends commands with drive error
   04-Nov-2026  Ulli    Uses the SIMH disk library (sim_disk): SIMH, VHD and
                        RAW formats (SET RBn FORMAT), autosize (RB02/RB80),
                        metadata, DISK debug, bad block table on new images
   04-Nov-2026  Ulli    Disk images in DEC logical block order (cylinder,
                        head, sector), as the RL11 and DEC's drivers; the old
                        head-major layout with SET RBn LAYOUT=OLD (temporary)
   29-Mar-2011  MB      First Version

   The RB730 is a four drive disk subsystem consisting of up to three RL02
   drives (known as RB02) and one optional RA80 drive (known as RB80).

   Unlike the RL11 controller seeks are not done relative to the current
   disk address.

   The RB730 has two regiter address spaces:
   
   - One dummy 16-bit register in unibus I/O space to allow the controller to
     be detected by SYSGEN autoconfigure (and others).
   - Eight 32-bit registers in the unibus controller space for the actual
     device control.
*/

#include "vax_defs.h"
#include "sim_disk.h"
#include "sim_sound.h"
#include <math.h>

/* Constants */

#define RB02_NUMWD      128                             /* words/sector */
#define RB02_NUMSC      40                              /* sectors/track */
#define RB02_NUMSF      2                               /* tracks/cylinder */
#define RB02_NUMCY      512                             /* cylinders/drive */
#define RB02_SIZE       (RB02_NUMCY * RB02_NUMSF * \
                         RB02_NUMSC * RB02_NUMWD)       /* words/drive */
#define RB80_NUMWD      256                             /* words/sector */
#define RB80_NUMSC      31                              /* sectors/track used (DEC) */
#define RB80_NUMSC_OLD  32                              /* ... in the old layout */
#define RB80_NUMSF      14                              /* tracks/cylinder */
#define RB80_NUMCY      559                             /* cylinders/drive */
#define RB80_SIZE       (RB80_NUMCY * RB80_NUMSF * \
                         RB80_NUMSC * RB80_NUMWD)       /* words/drive */
#define RB80_SIZE_OLD   (RB80_NUMCY * RB80_NUMSF * \
                         RB80_NUMSC_OLD * RB80_NUMWD)   /* words/drive, old layout */

#define RB_NUMWD(u)     ((u->flags & UNIT_RB80) ? RB80_NUMWD : RB02_NUMWD)
#define RB_NUMSC(u)     ((u->flags & UNIT_RB80) ? \
                         ((u->flags & UNIT_OLDL) ? RB80_NUMSC_OLD : RB80_NUMSC) : RB02_NUMSC)
#define RB_NUMSF(u)     ((u->flags & UNIT_RB80) ? RB80_NUMSF : RB02_NUMSF)
#define RB_NUMCY(u)     ((u->flags & UNIT_RB80) ? RB80_NUMCY : RB02_NUMCY)
#define RB_SIZE(u)      ((u->flags & UNIT_RB80) ? \
                         ((u->flags & UNIT_OLDL) ? RB80_SIZE_OLD : RB80_SIZE) : RB02_SIZE)

#define RB_NUMDR        4                               /* drives/controller */
#define RB_MAXFR        (1 << 16)                       /* max transfer */

/* Flags in the unit flags word */

#define UNIT_V_RB80     (DKUF_V_UF + 0)                 /* RB02 vs RB80 */
#define UNIT_V_DUMMY    (DKUF_V_UF + 1)                 /* dummy flag */
#define UNIT_V_OLDL     (DKUF_V_UF + 2)                 /* old image layout */
#define UNIT_NOAUTO     DKUF_NOAUTOSIZE                 /* autosize disable */
#define UNIT_DUMMY      (1 << UNIT_V_DUMMY)
#define UNIT_OLDL       (1u << UNIT_V_OLDL)
#define UNIT_RB80       (1u << UNIT_V_RB80)

/* Parameters in the unit descriptor */

#define TRK             u3                              /* current track */
#define STAT            u4                              /* status */
#define SIP             u5                              /* seek in progress */

/* RBDS, NI = not implemented, * = kept in STAT, ^ = kept in TRK */

#define RB02DS_LOAD     0                               /* no cartridge */
#define RB02DS_LOCK     5                               /* lock on */
#define RB02DS_BHO      0000010                         /* brushes home NI */
#define RB02DS_HDO      0000020                         /* heads out NI */
#define RB02DS_CVO      0000040                         /* cover open NI */
#define RB02DS_HD       0000100                         /* head select ^ */
#define RB02DS_DSE      0000400                         /* drv sel err NI */
#define RB02DS_VCK      0001000                         /* vol check * */
#define RB02DS_WGE      0002000                         /* wr gate err * */
#define RB02DS_SPE      0004000                         /* spin err * */
#define RB02DS_STO      0010000                         /* seek time out NI */
#define RB02DS_WLK      0020000                         /* wr locked */
#define RB02DS_HCE      0040000                         /* hd curr err NI */
#define RB02DS_WDE      0100000                         /* wr data err NI */
#define RB02DS_ATT      (RB02DS_HDO+RB02DS_BHO+RB02DS_LOCK)   /* att status */
#define RB02DS_UNATT    (RB02DS_CVO+RB02DS_LOAD)            /* unatt status */
#define RB02DS_ERR      (RB02DS_WDE+RB02DS_HCE+RB02DS_STO+RB02DS_SPE+RB02DS_WGE+ \
                         RB02DS_VCK+RB02DS_DSE)             /* errors bits */

#define RB80DS_SCNT     0x0000000F
#define RB80DS_FLT      0x00000100
#define RB80DS_PLV      0x00000200
#define RB80DS_SKE      0x00000400
#define RB80DS_OCY      0x00000800
#define RB80DS_RDY      0x00001000
#define RB80DS_WLK      0x00002000

/* RBCS */

#define RBCS_DRDY       0x00000001                      /* drive ready */
#define RBCS_M_FUNC     0x7                             /* function */
#define  RBCS_NOP       0
#define  RBCS_WCHK      1
#define  RBCS_GSTA      2
#define  RBCS_SEEK      3
#define  RBCS_RHDR      4
#define  RBCS_WRITE     5
#define  RBCS_READ      6
#define  RBCS_RNOHDR    7
#define RBCS_V_FUNC     1
#define RBCS_M_DRIVE    0x3
#define RBCS_V_DRIVE    8
#define RBCS_INCMP      0x00000400                      /* incomplete */
#define RBCS_CRC        0x00000800                      /* CRC error */
#define RBCS_DLT        0x00001000                      /* data late */
#define RBCS_HDE        0x00001400                      /* header error */
#define RBCS_NXM        0x00002000                      /* non-exist memory */
#define RBCS_DRE        0x00004000                      /* drive error */
#define RBCS_ERR        0x00008000                      /* error summary */
#define RBCS_ALLERR (RBCS_ERR+RBCS_DRE+RBCS_NXM+RBCS_CRC+RBCS_INCMP)
#define RBCS_M_ATN      0xF
#define RBCS_V_ATN      16
#define RBCS_ATN        (RBCS_M_ATN << RBCS_V_ATN)
#define RBCS_M_ECC      0x2
#define RBCS_V_ECC      20
#define RBCS_SSI        0x00400000
#define RBCS_SSE        0x00800000
#define RBCS_IRQ        0x01000000
#define RBCS_MTN        0x02000000
#define RBCS_R80        0x04000000
#define RBCS_ASI        0x08000000
#define RBCS_TOI        0x10000000
#define RBCS_FMT        0x20000000
#define RBCS_MATN       0x80000000
//#define RBCS_RW         0001716                         /* read/write */
#define RBCS_RW         ((RBCS_M_FUNC << RBCS_V_FUNC) + \
                        CSR_IE + CSR_DONE + \
                        (RBCS_M_DRIVE << RBCS_V_DRIVE) + \
                        RBCS_SSI + RBCS_MTN + RBCS_ASI + \
                        RBCS_TOI + RBCS_FMT + RBCS_MATN)
#define RBCS_C0         RBCS_SSE
#define RBCS_C1         (rbcs & RBCS_MATN) ? RBCS_IRQ : \
                        ((RBCS_M_ATN << RBCS_V_ATN) + RBCS_IRQ)
#define GET_FUNC(x)     (((x) >> RBCS_V_FUNC) & RBCS_M_FUNC)
#define GET_DRIVE(x)    (((x) >> RBCS_V_DRIVE) & RBCS_M_DRIVE)

/* RBBA */

#define RBBA_RW         0x0003FFFF

/* RBBC */

/* RBMP */

#define RBMP_MRK        0x00000001
#define RBMP_GST        0x00000002
#define RBMP_RST        0x00000008

/* RBDA */

#define RBDA_V_SECT     0                               /* sector */
#define RBDA_M_SECT     0xFF
#define RBDA_V_TRACK    8                               /* track */
#define RBDA_M_TRACK    0xFF
#define RBDA_V_CYL      16                              /* cylinder */
#define RBDA_M_CYL      0xFFFFu
#define RBDA_TRACK      (RBDA_M_TRACK << RBDA_V_TRACK)
#define RBDA_CYL        (RBDA_M_CYL << RBDA_V_CYL)
#define GET_SECT(x)     (((x) >> RBDA_V_SECT) & RBDA_M_SECT)
#define GET_CYL(x)      (((x) >> RBDA_V_CYL) & RBDA_M_CYL)
#define GET_TRACK(x)    (((x) >> RBDA_V_TRACK) & RBDA_M_TRACK)
/* Disk address -> word offset in the image file.

   Standard layout: DEC logical block order, cylinder by cylinder, within a
   cylinder head by head, within a track sector by sector.  This is how
   DEC's drivers number the logical blocks of an RL02/RB02 (VMS DLDRIVER on
   the RL11 and DQDRIVER on the RB730 put a block on the same cylinder, head
   and sector), so an RB02 image is the same as an RL02 image of the RL11
   (pdp11_rl.c) and a binary copy of a real cartridge.

   Old layout (SET RBn LAYOUT=OLD, temporary, for images made by earlier
   versions of this simulator): head by head, all cylinders of a surface
   before the next surface. */

#define GET_DA_STD(x,u) ((((GET_CYL (x) * RB_NUMSF(u)) + GET_TRACK (x)) * RB_NUMSC(u) + \
                          GET_SECT (x)) * RB_NUMWD(u))
#define GET_DA_OLD(x,u) ((GET_TRACK (x) * RB_NUMCY(u) * RB_NUMSC(u) * RB_NUMWD(u)) + \
                         (GET_CYL(x) * RB_NUMSC(u) * RB_NUMWD(u)) + \
                         (GET_SECT (x) * RB_NUMWD(u)))
#define GET_DA(x,u)     (((u)->flags & UNIT_OLDL) ? GET_DA_OLD (x,u) : GET_DA_STD (x,u))

#define DBG_REG         0x0001                          /* registers */
#define DBG_CMD         0x0002                          /* commands */
#define DBG_RD          0x0004                          /* disk reads */
#define DBG_WR          0x0008                          /* disk writes */
#define DBG_DSK         0x0010                          /* sim_disk activities */

uint16 *rbxb = NULL;                                     /* xfer buffer */
int32 rbcs = 0;                                         /* control/status */
int32 rbba = 0;                                         /* memory address */
int32 rbbc = 0;                                         /* bytes count */
int32 rbda = 0;                                         /* disk addr */
int32 rbmp = 0, rbmp1 = 0, rbmp2 = 0;                   /* mp register queue */
int32 rb_swait = 150;                                   /* seek wait */
int32 rb_mwait = 300;                                   /* seek wait */
int32 rb_cwait = 50;                                    /* seek wait */

/* Sound realism mode (SET SOUND ENABLED)

   The RB02 is an RL02 drive on the RB730 controller: its LOAD button is
   SET RBn ONLINE (or LOAD) and SET RBn OFFLINE (or UNLOAD).  A spindle
   (SND_SPINDLE, common support in sim_sound) times it with the registers
   SPINUP and SPINDOWN (defaults as the RL02: 40 s and 30 s); at the sim>
   prompt the commands wait unless SET -N.  While the drive is not up to
   speed it is not ready, GET STATUS shows the drive state (load, spin-up
   or spin-down), and seeks and transfers end with drive error and spin
   error, as with no cartridge, so VMS sees the medium offline.  When it
   is up to speed, the drive is locked on with volume check set, as after
   an ATTACH.  The RB80 (an R80, the RM80's drive) is a fixed disk: it is
   ready as soon as it is attached.

   - Realism mode: ATTACH of an RB02 only loads the cartridge; the drive
     waits for SET RBn ONLINE.  Seeks and transfers take realistic times:
     seek t(d) = a + b * d^e (d cylinders; RB02 as the RL02: 15 ms
     track-to-track, 100 ms maximum, e = 0.67, head switch 7 ms; RB80 as
     the RM80: 6/25/50 ms track-to-track/average/maximum, e fitted),
     rotational latency (0 .. one revolution: RB02 2400 rpm, RB80 3600
     rpm) and one sector time per sector (RB80: 32 sector slots per
     revolution, 31 used).  Seeks play the short, medium or long seek
     sound of the drive (sound/rl02 for the RB02, sound/rm80 for the RB80;
     registers SEEK02S/SEEK02M, SEEK80S/SEEK80M).  Optional spindle sounds
     rl02_spinup.wav, rl02_running.wav, rl02_spindown.wav (the RB02 is an
     RL02).
   - Normal mode: ATTACH spins up at once and ONLINE/OFFLINE take effect at
     once; the timing is unchanged. */

#define RB02_SND        "rl02"                          /* the RB02 is an RL02 */
#define RB80_SND        "rm80"                          /* the RB80 is an R80 */
#define RB_SEEK_GAIN    0.3
#define RB_SPIN_GAIN    1.0
#define RB02_REV_US     25000.0                         /* 2400 rpm */
#define RB02_T2T_MS     15.0
#define RB02_MAX_MS     100.0
#define RB02_EXP        0.67
#define RB02_HDSW_MS    7.0                             /* head switch only */
#define RB80_REV_US     (60000000.0 / 3600.0)           /* 3600 rpm */
#define RB80_SLOTS      32                              /* sector slots per revolution */
#define RB80_T2T_MS     6.0
#define RB80_AVG_MS     25.0
#define RB80_MAX_MS     50.0

int32 rb_spinup_ms = 40000;                             /* realism: RB02 spin-up time (ms) */
int32 rb_spindown_ms = 30000;                           /* realism: RB02 spin-down time (ms) */
int32 rb_seek02s = 30;                                  /* RB02 short seek up to n cylinders */
int32 rb_seek02m = 190;                                 /* RB02 medium seek up to n cylinders */
int32 rb_seek80s = 35;                                  /* RB80 short seek up to n cylinders */
int32 rb_seek80m = 205;                                 /* RB80 medium seek up to n cylinders */
static SND_SPINDLE rb_spindle[4];                       /* the drives' spindles */
static SND_SAMPLE *rb_seek_smp[4][3];                   /* seek sounds per drive */
static uint32 rb_rand_state = 1;                        /* rotational latency */

t_stat rb_rd16 (int32 *data, int32 PA, int32 access);
t_stat rb_wr16 (int32 data, int32 PA, int32 access);
t_stat rb_rd32 (int32 *data, int32 PA, int32 access);
t_stat rb_wr32 (int32 data, int32 PA, int32 access);
t_stat rb_svc (UNIT *uptr);
t_stat rb_reset (DEVICE *dptr);
const char *rb_description (DEVICE *dptr);
void rb_set_done (int32 error);
t_stat rb_attach (UNIT *uptr, CONST char *cptr);
t_stat rb_detach (UNIT *uptr);
t_stat rb_set_size (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat rb_set_bad (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat rb_set_layout (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat rb_show_layout (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat rb_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
t_stat rb_set_online (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat rb_show_spindle (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
static t_bool rb_ready (UNIT *uptr);
static void rb_spindles_init (void);
static double rb_seek_us (UNIT *uptr, int32 dist);
static double rb_xfer_us (UNIT *uptr);
static void rb_seek_sound (UNIT *uptr, int32 dist);

/* RB730 data structures

   rb_dev       RB device descriptor
   rb_unit      RB unit list
   rb_reg       RB register list
   rb_mod       RB modifier list
*/

#define IOLN_RB         002

DIB rb_dib = {
    IOBA_AUTO, IOLN_RB, &rb_rd16, &rb_wr16,
    1, IVCL (RB), VEC_AUTO, { NULL }, IOLN_RB };

UNIT rb_unit[] = {
    { UDATA (&rb_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+
             UNIT_ROABLE+UNIT_RB80, RB80_SIZE) },
    { UDATA (&rb_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+
             UNIT_ROABLE, RB02_SIZE) },
    { UDATA (&rb_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+
             UNIT_ROABLE, RB02_SIZE) },
    { UDATA (&rb_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+
             UNIT_ROABLE, RB02_SIZE) },
    };

REG rb_reg[] = {
    { DRDATAD (SPINUP,   rb_spinup_ms,   24, "RB02 spin-up time in ms (sound realism mode)"), PV_LEFT },
    { DRDATAD (SPINDOWN, rb_spindown_ms, 24, "RB02 spin-down time in ms (sound realism mode)"), PV_LEFT },
    { DRDATAD (SEEK02S,  rb_seek02s,     10, "RB02 short seek sound up to n cylinders"), PV_LEFT },
    { DRDATAD (SEEK02M,  rb_seek02m,     10, "RB02 medium seek sound up to n cylinders"), PV_LEFT },
    { DRDATAD (SEEK80S,  rb_seek80s,     10, "RB80 short seek sound up to n cylinders"), PV_LEFT },
    { DRDATAD (SEEK80M,  rb_seek80m,     10, "RB80 medium seek sound up to n cylinders"), PV_LEFT },
    { NULL }
    };

DEBTAB rb_debug[] = {
    {"REG", DBG_REG},
    {"CMD", DBG_CMD},
    {"RD",  DBG_RD},
    {"WR",  DBG_WR},
    {"DISK", DBG_DSK},
    {0}
};

MTAB rb_mod[] = {
    { MTAB_XTD|MTAB_VUN, 0, "write enabled", "WRITEENABLED", 
        &set_writelock, &show_writelock,   NULL, "Write enable disk drive" },
    { MTAB_XTD|MTAB_VUN, 1, NULL, "LOCKED", 
        &set_writelock, NULL,   NULL, "Write lock disk drive" },
    { UNIT_DUMMY,      0, NULL,            "BADBLOCK", 
        &rb_set_bad, NULL, NULL, "write bad block table on last track" },
    { MTAB_XTD|MTAB_VUN, 1, NULL, "ONLINE",
        &rb_set_online, NULL, NULL, "RB02 LOAD button in: spin the drive up" },
    { MTAB_XTD|MTAB_VUN, 1, NULL, "LOAD",
        &rb_set_online, NULL, NULL, "RB02 LOAD button in: spin the drive up" },
    { MTAB_XTD|MTAB_VUN, 0, NULL, "OFFLINE",
        &rb_set_online, NULL, NULL, "RB02 LOAD button out: spin the drive down" },
    { MTAB_XTD|MTAB_VUN, 0, NULL, "UNLOAD",
        &rb_set_online, NULL, NULL, "RB02 LOAD button out: spin the drive down" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "SPINDLE", NULL,
        NULL, &rb_show_spindle, NULL, "Display the spindle state" },
    { UNIT_NOAUTO,           0, "autosize", "AUTOSIZE",
        NULL, NULL, NULL, "Set type based on file size at attach" },
    { UNIT_NOAUTO, UNIT_NOAUTO, "noautosize", "NOAUTOSIZE",
        NULL, NULL, NULL, "Disable disk autosize on attach" },
    { MTAB_XTD|MTAB_VUN|MTAB_VALR, 0, "FORMAT", "FORMAT={AUTO|SIMH|VHD|RAW}",
        &sim_disk_set_fmt, &sim_disk_show_fmt, NULL, "Set/Display disk format" },
    { MTAB_XTD|MTAB_VUN|MTAB_VALR, 0, "LAYOUT", "LAYOUT={STANDARD|OLD}",
        &rb_set_layout, &rb_show_layout, NULL, "Set/Display the image layout (OLD: images of earlier versions)" },
    { (UNIT_RB80+UNIT_ATT),             UNIT_ATT, "RB02", NULL, NULL },
    { (UNIT_RB80+UNIT_ATT), (UNIT_RB80+UNIT_ATT), "RB80", NULL, NULL },
    { (UNIT_RB80+UNIT_ATT),                    0, "RB02", NULL, NULL },
    { (UNIT_RB80+UNIT_ATT),            UNIT_RB80, "RB80", NULL, NULL },
    { (UNIT_RB80),                             0, NULL, "RB02", 
        &rb_set_size, NULL, NULL, "Set type to RB02" },
    { (UNIT_RB80),                     UNIT_RB80, NULL, "RB80", 
        &rb_set_size, NULL, NULL, "Set type to RB80" },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0010, "ADDRESS", "ADDRESS",
      &set_addr, &show_addr, NULL, "Bus address" },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR,    0, "VECTOR", "VECTOR",
      &set_vec,  &show_vec,  NULL, "Interrupt vector" },
    { 0 }
    };

DEVICE rb_dev = {
    "RB", rb_unit, rb_reg, rb_mod,
    RB_NUMDR, DEV_RDX, T_ADDR_W, 1, DEV_RDX, 16,
    NULL, NULL, &rb_reset,
    NULL, &rb_attach, &rb_detach,
    &rb_dib, DEV_DISABLE | DEV_UBUS | DEV_DEBUG | DEV_DISK, 0,
    rb_debug, NULL, NULL, &rb_help, NULL, NULL, 
    &rb_description
    };

/* I/O dispatch routines

   17775606     RBDCS    dummy csr to trigger sysgen
*/

t_stat rb_rd16 (int32 *data, int32 PA, int32 access)
{
*data = 0;
return SCPE_OK;
}

t_stat rb_wr16 (int32 data, int32 PA, int32 access)
{
return SCPE_OK;
}

t_stat rb_rd32 (int32 *data, int32 PA, int32 access)
{
UNIT *uptr;

switch ((PA >> 2) & 07) {

    case 0:                                             /* RBCS */
        if (rbcs & RBCS_ALLERR)
            rbcs = rbcs | RBCS_ERR;
        uptr = rb_dev.units + GET_DRIVE (rbcs);
        if ((sim_is_active (uptr)) || (uptr->flags & UNIT_DIS) ||
            !rb_ready (uptr))                           /* no cartridge, not up to speed */
            rbcs = rbcs & ~RBCS_DRDY;
        else rbcs = rbcs | RBCS_DRDY;                   /* see if ready */
        if (uptr->flags & UNIT_RB80)
            rbcs = rbcs | RBCS_R80;
        else rbcs = rbcs & ~RBCS_R80;
        *data = rbcs;
        break;

    case 1:                                             /* RBBA */
        *data = rbba & RBBA_RW;
        break;

    case 2:                                             /* RBBC */
        *data = rbbc;
        break;

    case 3:                                             /* RBDA */
        *data = rbda;
        break;

    case 4:                                             /* RBMP */
        *data = rbmp;
        rbmp = rbmp1;                                   /* ripple data */
        rbmp1 = rbmp2;
        break;

    case 5:                                             /* ECCPS */
    case 6:                                             /* ECCPT */
    case 7:                                             /* INIT */
        *data = 0;
        break;
    }

sim_debug(DBG_REG, &rb_dev, "reg %d read, value = %X\n", (PA >> 2) & 07, *data);

return SCPE_OK;
}

t_stat rb_wr32 (int32 data, int32 PA, int32 access)
{
UNIT *uptr;

sim_debug(DBG_REG, &rb_dev, "reg %d write, value = %X\n", (PA >> 2) & 07, data);

switch ((PA >> 2) & 07) {

    case 0:                                             /* CSR */
        if (rbcs & RBCS_ALLERR)
            rbcs = rbcs | RBCS_ERR;
        uptr = rb_dev.units + GET_DRIVE (data);
        if ((sim_is_active (uptr)) || (uptr->flags & UNIT_DIS) ||
            !rb_ready (uptr))                           /* no cartridge, not up to speed */
            rbcs = rbcs & ~RBCS_DRDY;
        else rbcs = rbcs | RBCS_DRDY;                   /* see if ready */
        if (uptr->flags & UNIT_RB80)
            rbcs = rbcs | RBCS_R80;
        else rbcs = rbcs & ~RBCS_R80;

        rbcs = rbcs & ~(data & RBCS_C1);
        rbcs = rbcs & ~(~data & RBCS_C0);
        rbcs = (rbcs & ~RBCS_RW) | (data & RBCS_RW);
        if (data & RBCS_ATN) CLR_INT (RB);
        
        if ((data & CSR_DONE) || (sim_is_active (uptr))) /* ready set? */
            return SCPE_OK;

        CLR_INT (RB);                                   /* clear interrupt */
        rbcs = rbcs & ~RBCS_ALLERR;                     /* clear errors */
        uptr->SIP = 0;
        if (uptr->flags & UNIT_DIS) {
            rbcs = rbcs | (1u << (RBCS_V_ATN + GET_DRIVE (rbcs)));
            rb_set_done (RBCS_ERR | RBCS_INCMP);
            break;
            }
        switch (GET_FUNC (rbcs)) {                      /* case on RBCS<3:1> */
        case RBCS_NOP:                                  /* nop */
            rb_set_done (0);
            break;
        case RBCS_SEEK:                                 /* seek */
            sim_activate (uptr, rb_swait);
            break;
        default:                                        /* data transfer */
            if (sim_sound_enabled () && rb_ready (uptr) &&  /* realism: latency */
                (GET_FUNC (rbcs) != RBCS_GSTA))         /* and sector times */
                sim_sound_activate_us (uptr, rb_xfer_us (uptr));
            else
                sim_activate (uptr, rb_cwait);          /* activate unit */
            break;
            }                                           /* end switch func */
        break;

    case 1:                                             /* BAR */
        rbba = data & RBBA_RW;
        break;

    case 2:                                             /* BCR */
        rbbc = data;
        break;

    case 3:                                             /* DAR */
        rbda = data;
        break;

    case 4:                                             /* MPR */
        rbmp = rbmp1 = rbmp2 = data;
        break;

    case 5:                                             /* ECCPS */
    case 6:                                             /* ECCPT */
        break;

    case 7:                                             /* INIT */
        return rb_reset(&rb_dev);
    }

return SCPE_OK;
}

/* Service unit timeout

   If seek in progress, complete seek command
   Else complete data transfer command

   The unit control block contains the function and cylinder for
   the current command.
*/

t_stat rb_svc (UNIT *uptr)
{
int32 curr, newc, swait;
int32 err, wc, maxwc, t;
int32 i, func, da, awc;
t_seccnt sectsread;
t_lba lba;
uint32 ma;
uint16 comp;

if (uptr->SIP) {                                        /* seek finishing? */
    int32 drv = (int32) (uptr - rb_dev.units);

    sim_debug(DBG_CMD, &rb_dev, "Seek done, drive %d\n", drv);
    uptr->SIP = 0;
    rbcs = rbcs | (1u << (RBCS_V_ATN + drv));           /* drive attention */
    if ((rbcs & CSR_DONE) || (GET_DRIVE (rbcs) != drv)) {
        if (rbcs & CSR_DONE)                            /* controller idle: */
            rb_set_done (0);                            /* interrupt now */
        return SCPE_OK;                                 /* (else another drive's */
        }                                               /* command ends it) */
    /* A command for this drive was written while it was seeking: the
       controller held it until the seek was done; do it now (below). */
    }

func = GET_FUNC (rbcs);                                 /* get function */
if (func == RBCS_GSTA) {                                /* get status */
    sim_debug(DBG_CMD, &rb_dev, "Get Status\n");
    if (uptr->flags & UNIT_RB80) {
        rbmp = uptr->STAT | RB80DS_PLV;
        if (uptr->flags & UNIT_ATT)
            rbmp = rbmp | RB80DS_RDY | RB80DS_OCY;
        if (uptr->flags & UNIT_WPRT)
            rbmp = rbmp | RB80DS_WLK;
        }
    else if ((uptr->flags & UNIT_ATT) && !rb_ready (uptr)) {  /* realism: not */
        int32 st = sim_spindle_state (&rb_spindle[uptr - rb_dev.units]);  /* up to speed */

        if (rbmp & RBMP_RST)
            uptr->STAT = uptr->STAT & ~RB02DS_ERR;
        rbmp = (uptr->STAT & RB02DS_ERR) |              /* load, spin-up (1), */
            ((st == SND_SPIN_UP) ? 1 : ((st == SND_SPIN_DOWN) ? 7 : RB02DS_LOAD));/* spin-down (7) */
        if (uptr->flags & UNIT_WPRT)
            rbmp = rbmp | RB02DS_WLK;
        }
    else {
        if (rbmp & RBMP_RST)
            uptr->STAT = uptr->STAT & ~RB02DS_ERR;
        rbmp = uptr->STAT | ((uptr->flags & UNIT_ATT)? RB02DS_ATT: RB02DS_UNATT);
        if (uptr->flags & UNIT_WPRT)
            rbmp = rbmp | RB02DS_WLK;
        }
    rbmp2 = rbmp1 = rbmp;
    rb_set_done (0);                                    /* done */
    return SCPE_OK;
    }

if (func == RBCS_RHDR) {                                /* read header? */
    sim_debug(DBG_CMD, &rb_dev, "Read Header\n");
    rbmp = (uptr->TRK & (RBDA_CYL | RBDA_TRACK)) | GET_SECT (rbda);  /* cyl, head, sector */
    rbmp1 = rbmp2 = 0;
    rbcs = rbcs | (1 << (RBCS_V_ATN + GET_DRIVE (rbcs)));
    rb_set_done (0);                                    /* done */
    return SCPE_OK;
    }

if (!rb_ready (uptr)) {                                 /* attached, spun up? */
    rbcs = rbcs & ~RBCS_DRDY;                           /* clear drive ready */
    rbcs = rbcs | (1u << (RBCS_V_ATN + GET_DRIVE (rbcs)));
    if ((uptr->flags & UNIT_RB80) == 0)
        uptr->STAT = uptr->STAT | RB02DS_SPE;           /* spin error */
    rb_set_done (RBCS_ERR | RBCS_DRE | RBCS_INCMP);     /* drive error */
    //return IORETURN (rl_stopioe, SCPE_UNATT);
    return SCPE_OK;
    }

if ((func == RBCS_WRITE) && (uptr->flags & UNIT_WPRT)) {
    if ((uptr->flags & UNIT_RB80) == 0)
        uptr->STAT = uptr->STAT | RB02DS_WGE;           /* write and locked */
    rb_set_done (RBCS_ERR | RBCS_DRE);
    return SCPE_OK;
    }

if (func == RBCS_SEEK) {                                /* seek? */
    if (uptr->SIP == 0) {
        sim_debug(DBG_CMD, &rb_dev, "Seek, CYL=%d, TRK=%d, SECT=%d\n", GET_CYL(rbda), GET_TRACK(rbda), GET_SECT(rbda));
        uptr->SIP = 1;
        if ((uint32)rbda == 0xFFFFFFFF) swait = rb_swait;
        else {
            curr = GET_CYL (uptr->TRK);                     /* current cylinder */
            newc = GET_CYL (rbda);                          /* offset */
            if (newc >= RB_NUMCY (uptr))                    /* beyond the last */
                newc = RB_NUMCY (uptr) - 1;                 /* cylinder: stop there */
            uptr->TRK = (newc << RBDA_V_CYL) |              /* put on track */
                (rbda & RBDA_TRACK);                        /* (head selected) */
            swait = rb_cwait * abs (newc - curr);
            if (swait < rb_mwait) swait = rb_mwait;
            if (sim_sound_enabled ()) {                 /* realism: seek time, sound */
                rb_seek_sound (uptr, abs (newc - curr));
                sim_sound_activate_us (uptr, rb_seek_us (uptr, abs (newc - curr)));
                swait = -1;
                }
            }
        if (swait >= 0)
            sim_activate (uptr, swait);
        rbcs = rbcs | (1 << (RBCS_V_ATN + GET_DRIVE (rbcs)));
        rbcs = rbcs | RBCS_IRQ;
        rb_set_done(0);
        return SCPE_OK;
        }
    }

if (((func != RBCS_RNOHDR) && ((uptr->TRK & RBDA_CYL) != (rbda & RBDA_CYL)))
   || (GET_SECT (rbda) >= RB_NUMSC(uptr))                   /* bad cyl or sector? */
   || (GET_TRACK (rbda) >= RB_NUMSF(uptr))) {               /* or track? */
    sim_debug(DBG_CMD, &rb_dev, "Invalid cylinder, track or sector, CYL=%d, TRK=%d, SECT=%d\n", GET_CYL(rbda), GET_TRACK(rbda), GET_SECT(rbda));
    rb_set_done (RBCS_ERR | RBCS_HDE | RBCS_INCMP);     /* wrong cylinder? */
    return SCPE_OK;
    }

ma = rbba;                                              /* get mem addr */
da = GET_DA (rbda, uptr);                               /* get disk addr */
wc = ((rbbc * -1) >> 1);                                /* get true wc */

maxwc = (RB_NUMSC(uptr) - GET_SECT (rbda)) * RB_NUMWD(uptr);    /* max transfer */
if (wc > maxwc)                                         /* track overrun? */
    wc = maxwc;
lba = (t_lba) (da / RB_NUMWD(uptr));                    /* sector address */
err = 0;

if ((func >= RBCS_READ) && (err == 0)) {                /* read (no hdr)? */
    sim_debug(DBG_CMD, &rb_dev, "Read, CYL=%d, TRK=%d, SECT=%d, WC=%d, DA=%d\n", GET_CYL(rbda), GET_TRACK(rbda), GET_SECT(rbda), wc, da);
    err = sim_disk_rdsect (uptr, lba, (uint8 *) rbxb, &sectsread,
                           (wc + RB_NUMWD(uptr) - 1) / RB_NUMWD(uptr));
    sim_disk_data_trace (uptr, (uint8 *) rbxb, lba, sectsread * RB_NUMWD(uptr) * sizeof (uint16),
                         "sim_disk_rdsect", DBG_DSK & rb_dev.dctrl, DBG_RD);
    for (i = sectsread * RB_NUMWD(uptr); i < wc; i++)   /* fill buffer */
        rbxb[i] = 0;
    if ((t = Map_WriteW (ma, wc << 1, rbxb))) {         /* store buffer */
        rbcs = rbcs | RBCS_ERR | RBCS_NXM;              /* nxm */
        wc = wc - t;                                    /* adjust wc */
        }
    }                                                   /* end read */

if ((func == RBCS_WRITE) && (err == 0)) {               /* write? */
    sim_debug(DBG_CMD, &rb_dev, "Write, CYL=%d, TRK=%d, SECT=%d, WC=%d, DA=%d\n", GET_CYL(rbda), GET_TRACK(rbda), GET_SECT(rbda), wc, da);
    if ((t = Map_ReadW (ma, wc << 1, rbxb))) {          /* fetch buffer */
        rbcs = rbcs | RBCS_ERR | RBCS_NXM;              /* nxm */
        wc = wc - t;                                    /* adj xfer lnt */
        }
    if (wc) {                                           /* any xfer? */
        awc = (wc + (RB_NUMWD(uptr) - 1)) & ~(RB_NUMWD(uptr) - 1);  /* clr to */
        for (i = wc; i < awc; i++)                      /* end of blk */
            rbxb[i] = 0;
        sim_disk_data_trace (uptr, (uint8 *) rbxb, lba, awc * sizeof (uint16),
                             "sim_disk_wrsect", DBG_DSK & rb_dev.dctrl, DBG_WR);
        err = sim_disk_wrsect (uptr, lba, (uint8 *) rbxb, NULL, awc / RB_NUMWD(uptr));
        }
    }                                                   /* end write */

if ((func == RBCS_WCHK) && (err == 0)) {                /* write check? */
    sim_debug(DBG_CMD, &rb_dev, "WCheck, CYL=%d, TRK=%d, SECT=%d, WC=%d, DA=%d\n", GET_CYL(rbda), GET_TRACK(rbda), GET_SECT(rbda), wc, da);
    err = sim_disk_rdsect (uptr, lba, (uint8 *) rbxb, &sectsread,
                           (wc + RB_NUMWD(uptr) - 1) / RB_NUMWD(uptr));
    for (i = sectsread * RB_NUMWD(uptr); i < wc; i++)   /* fill buffer */
        rbxb[i] = 0;
    awc = wc;                                           /* save wc */
    for (wc = 0; (err == 0) && (wc < awc); wc++)  {     /* loop thru buf */
        if (Map_ReadW (ma + (wc << 1), 2, &comp)) {     /* mem wd */
            rbcs = rbcs | RBCS_ERR | RBCS_NXM;          /* nxm */
            break;
            }
        if (comp != rbxb[wc])                           /* check to buf */
            rbcs = rbcs | RBCS_ERR | RBCS_CRC;
        }                                               /* end for */
    }                                                   /* end wcheck */

rbbc = (rbbc + (wc << 1));                              /* final byte count */
if (rbbc != 0) {                                        /* completed? */
    rbcs = rbcs | RBCS_ERR | RBCS_INCMP;
    }
ma = ma + (wc << 1);                                    /* final byte addr */
rbba = ma & RBBA_RW;
uptr->TRK = (uptr->TRK & RBDA_CYL) | (rbda & RBDA_TRACK);  /* head used */
rbda = rbda + ((wc + (RB_NUMWD(uptr) - 1)) / RB_NUMWD(uptr));
rb_set_done (0);

if (err != 0) {                                         /* error? */
    sim_disk_perror (uptr, "RB I/O error");
    sim_disk_clearerr (uptr);
    return SCPE_IOERR;
    }
return SCPE_OK;
}

/* Set done and possibly errors */

void rb_set_done (int32 status)
{
rbcs = rbcs | status | CSR_DONE;                        /* set done */
rbcs = rbcs | RBCS_IRQ;
if (rbcs & CSR_IE) {
    sim_debug(DBG_CMD, &rb_dev, "Done, INT\n");
    SET_INT (RB);
    }
else {
    sim_debug(DBG_CMD, &rb_dev, "Done, no INT\n");
    CLR_INT (RB);
    }
return;
}

/* Device reset */

t_stat rb_reset (DEVICE *dptr)
{
int32 i;
UNIT *uptr;

rbcs = CSR_DONE;
rbda = rbba = rbbc = rbmp = 0;
CLR_INT (RB);
rb_spindles_init ();                                    /* (once) */
for (i = 0; i < RB_NUMDR; i++) {
    uptr = rb_dev.units + i;
    sim_cancel (uptr);
    sim_disk_reset (uptr);
    uptr->STAT = 0;
    uptr->SIP = 0;
    }
if (rbxb == NULL)
    rbxb = (uint16 *) calloc (RB_MAXFR, sizeof (uint16));
if (rbxb == NULL)
    return SCPE_MEM;
return SCPE_OK;
}

const char *rb_description (DEVICE *dptr)
{
return "RB730 disk controller";
}

/* Attach routine */

t_stat rb_attach (UNIT *uptr, CONST char *cptr)
{
t_stat r;
static const char *drives[] = {"RB02", "RB80", NULL};

uptr->capac = RB_SIZE (uptr);
r = sim_disk_attach_ex (uptr, cptr, RB_NUMWD(uptr) * sizeof (uint16), /* sector size */
                        sizeof (uint16), TRUE, DBG_DSK,
                        (uptr->flags & UNIT_RB80) ? "RB80" : "RB02",
                        RB_NUMSC(uptr), 0,              /* bad block table on new images */
                        (uptr->flags & UNIT_NOAUTO) ? NULL : drives);
if (r != SCPE_OK)                                       /* error? */
    return r;
if (uptr->flags & UNIT_OLDL)
    sim_messagef (SCPE_OK, "%s: old RB730 image layout (SET %s LAYOUT=OLD, see HELP RB)\n",
                  sim_uname (uptr), sim_uname (uptr));
uptr->TRK = 0;                                          /* cylinder 0 */
if ((uptr->flags & UNIT_RB80) == 0)
    uptr->STAT = RB02DS_VCK;                            /* new volume */
if (sim_sound_enabled ()) {                             /* realism: load the sounds */
    int32 drv = (int32) (uptr - rb_dev.units);          /* now, not in the I/O path */
    const char *snd = (uptr->flags & UNIT_RB80) ? RB80_SND : RB02_SND;

    sim_sound_seek_load (rb_seek_smp[drv], snd, snd);
    if ((uptr->flags & UNIT_RB80) == 0) {               /* cartridge loaded, the */
        sim_spindle_load (&rb_spindle[drv], RB02_SND, RB02_SND);
        sim_spindle_off (&rb_spindle[drv]);             /* drive waits for ONLINE */
        }
    else
        sim_spindle_ready (&rb_spindle[drv]);
    }
else
    sim_spindle_ready (&rb_spindle[uptr - rb_dev.units]);   /* ATTACH spins up */
return SCPE_OK;
}

/* Detach routine */

t_stat rb_detach (UNIT *uptr)
{
sim_cancel (uptr);
if (uptr->flags & UNIT_ATT)
    sim_spindle_off (&rb_spindle[uptr - rb_dev.units]); /* the cartridge is gone */
return sim_disk_detach (uptr);
}

/* Set size routine */

t_stat rb_set_size (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
uptr->capac = (val & UNIT_RB80) ?
    ((uptr->flags & UNIT_OLDL) ? RB80_SIZE_OLD : RB80_SIZE) : RB02_SIZE;
return SCPE_OK;
}

/* Set bad block routine */

t_stat rb_set_bad (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
return sim_disk_pdp11_bad_block (uptr, RB_NUMSC(uptr), RB_NUMWD(uptr));
}

/* Set/show the image layout: STANDARD (DEC logical block order, default)
   or OLD (the head-major layout of earlier versions; temporary) */

t_stat rb_set_layout (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
char gbuf[CBUFSIZE];

if ((cptr == NULL) || (*cptr == 0))
    return SCPE_MISVAL;
if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
get_glyph (cptr, gbuf, 0);
if ((strcmp (gbuf, "STANDARD") == 0) || (strcmp (gbuf, "STD") == 0))
    uptr->flags &= ~UNIT_OLDL;
else if (strcmp (gbuf, "OLD") == 0)
    uptr->flags |= UNIT_OLDL;
else
    return sim_messagef (SCPE_ARG, "Unknown layout: %s (STANDARD or OLD)\n", gbuf);
uptr->capac = RB_SIZE (uptr);                           /* (RB80: 31 or 32 sectors) */
return SCPE_OK;
}

t_stat rb_show_layout (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "layout=%s", (uptr->flags & UNIT_OLDL) ? "OLD" : "STANDARD");
return SCPE_OK;
}

/* Help */

t_stat rb_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
fprintf (st, "RB730 Disk Controller (RB)\n\n");
fprintf (st, "The RB730 is the integrated disk controller of the VAX-11/730.  Unit 0 is\n");
fprintf (st, "an RB80 (an R80 fixed disk, the RM80's drive) by default, units 1-3 are RB02\n");
fprintf (st, "drives (RL02 cartridge drives); SET RBn RB02/RB80 changes the type.\n\n");
fprint_set_help (st, dptr);
fprint_show_help (st, dptr);
fprintf (st, "\nImage layout: the disk images are in DEC logical block order (cylinder by\n");
fprintf (st, "cylinder, head by head, sector by sector).  This is the order DEC's drivers\n");
fprintf (st, "use, so an RB02 image is the same as an RL02 image of the RL11 (RL device)\n");
fprintf (st, "and a binary copy of a real cartridge.  Earlier versions of this simulator\n");
fprintf (st, "stored the images head by head (all cylinders of surface 0 first).  Such\n");
fprintf (st, "images can still be used with SET RBn LAYOUT=OLD before the ATTACH (a\n");
fprintf (st, "temporary option); converting them once is better.\n");
fprintf (st, "The RB80 has 31 sectors per track as DEC's drivers use it (242,606 blocks,\n");
fprintf (st, "like the RM80, which has the same R80 drive); the old layout has 32.\n");
fprintf (st, "\nSET RBn ONLINE (or LOAD) and SET RBn OFFLINE (or UNLOAD) are the LOAD button\n");
fprintf (st, "of an RB02: OFFLINE spins the drive down (the cartridge stays attached),\n");
fprintf (st, "ONLINE spins it up.  SHOW RBn SPINDLE shows the spindle state.  The RB80 is\n");
fprintf (st, "a fixed disk without this button.\n\n");
fprintf (st, "With SET SOUND ENABLED (the sound realism mode, see HELP SET SOUND):\n");
fprintf (st, "  - ATTACH of an RB02 only loads the cartridge; the drive stays stopped until\n");
fprintf (st, "    SET RBn ONLINE (spin-up time register SPINUP, default 40000 ms, spin-down\n");
fprintf (st, "    SPINDOWN, default 30000 ms); at the sim> prompt the commands wait until\n");
fprintf (st, "    the drive is ready or at rest (SET -N RBn ... returns at once);\n");
fprintf (st, "  - seeks and transfers take realistic times (RB02 as the RL02: 2400 rpm,\n");
fprintf (st, "    seek 15 ms track-to-track, 100 ms maximum; RB80 as the RM80: 3600 rpm,\n");
fprintf (st, "    6/25/50 ms track-to-track/average/maximum);\n");
fprintf (st, "  - seeks play rl02-s/m/l.wav (RB02) or rm80-s/m/l.wav (RB80) from the sound\n");
fprintf (st, "    directories rl02 and rm80 for short, medium and long seeks (limits in the\n");
fprintf (st, "    registers SEEK02S/SEEK02M, default 30/190, and SEEK80S/SEEK80M, default\n");
fprintf (st, "    35/205 cylinders); optional spindle sounds rl02_spinup.wav,\n");
fprintf (st, "    rl02_running.wav and rl02_spindown.wav are used if found.\n");
fprintf (st, "Without the realism mode an RB02 spins up at ATTACH, ONLINE and OFFLINE take\n");
fprintf (st, "effect at once, and the timing is unchanged.\n");
fprint_reg_help (st, dptr);
return SCPE_OK;
}

/* Sound realism mode (see the description at rb_spinup_ms) */

/* Is the drive ready: attached and (RB02) spun up? */

static t_bool rb_ready (UNIT *uptr)
{
if (!(uptr->flags & UNIT_ATT))
    return FALSE;
if (uptr->flags & UNIT_RB80)                            /* fixed disk */
    return TRUE;
return (sim_spindle_state (&rb_spindle[uptr - rb_dev.units]) == SND_SPIN_READY);
}

/* The spindle is up to speed: heads loaded, locked on, new volume */

static void rb_on_ready (UNIT *uptr)
{
if (!(uptr->flags & UNIT_ATT))
    return;
sim_debug (DBG_CMD, &rb_dev, "RB%d up to speed\n", (int32) (uptr - rb_dev.units));
uptr->TRK = 0;                                          /* heads at cylinder 0 */
uptr->STAT = RB02DS_VCK;                                /* volume check */
}

static void rb_spindles_init (void)
{
int32 i;

for (i = 0; i < RB_NUMDR; i++)
    sim_spindle_init (&rb_spindle[i], &rb_unit[i], &rb_spinup_ms, &rb_spindown_ms,
                      &rb_on_ready, NULL, SND_GRP_DISK, RB_SPIN_GAIN);
}

/* SET RBn ONLINE/LOAD, OFFLINE/UNLOAD: the LOAD button of the RB02.
   Realism mode at the sim> prompt: wait until ready or at rest unless
   SET -N.  The RB80 is a fixed disk without such a button. */

t_stat rb_set_online (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
int32 drv = (int32) (uptr - rb_dev.units);
int32 st;

if (cptr != NULL)
    return SCPE_ARG;
if (uptr->flags & UNIT_RB80)
    return sim_messagef (SCPE_NOFNC, "%s: the RB80 is a fixed disk (no LOAD button)\n", sim_uname (uptr));
if (!(uptr->flags & UNIT_ATT))
    return sim_messagef (SCPE_UNATT, "%s: no cartridge loaded (not attached)\n", sim_uname (uptr));
st = sim_spindle_state (&rb_spindle[drv]);
if (val) {
    if ((st != SND_SPIN_UP) && (st != SND_SPIN_READY)) {
        sim_debug (DBG_CMD, &rb_dev, "RB%d spin up\n", drv);
        sim_spindle_start (&rb_spindle[drv]);
        }
    }
else if ((st != SND_SPIN_STOPPED) && (st != SND_SPIN_DOWN)) {
    sim_debug (DBG_CMD, &rb_dev, "RB%d spin down\n", drv);
    sim_spindle_stop (&rb_spindle[drv]);                /* (a transfer still */
    }                                                   /* queued ends with spin error) */
if (!(sim_switches & SWMASK ('N')))
    sim_spindle_wait (&rb_spindle[drv]);                /* (realism mode, prompt) */
return SCPE_OK;
}

/* SHOW RBn SPINDLE.  MTAB_NMO: SCP adds no line end, so this does. */

t_stat rb_show_spindle (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
t_stat r = SCPE_OK;

if (uptr->flags & UNIT_RB80)
    fprintf (st, "fixed disk (no LOAD button)");
else if (!(uptr->flags & UNIT_ATT))
    fprintf (st, "no cartridge");
else
    r = sim_spindle_show (st, &rb_spindle[uptr - rb_dev.units]);
fputc ('\n', st);
return r;
}

/* Realistic seek time over dist cylinders (us): t(d) = a + b * d^e with
   t(1) = track-to-track.  RB02 (the RL02): through the track-to-track and
   the maximum seek, e = 0.67, a head switch only takes 7 ms.  RB80 (the
   R80): e fitted so the curve also passes through the average seek at a
   third of the stroke. */

static double rb_seek_us (UNIT *uptr, int32 dist)
{
static double e80, a80, b80;
static t_bool done80 = FALSE;

if (uptr->flags & UNIT_RB80) {
    if (!done80) {
        double d = (double) (RB80_NUMCY - 1);
        double want = (RB80_AVG_MS - RB80_T2T_MS) / (RB80_MAX_MS - RB80_T2T_MS);
        double lo = 0.2, hi = 1.5, e = 0.69;
        int32 i;

        for (i = 0; i < 60; i++) {                      /* ratio falls with e */
            e = (lo + hi) / 2.0;
            if (((pow (d / 3.0, e) - 1.0) / (pow (d, e) - 1.0)) > want)
                lo = e;
            else
                hi = e;
            }
        b80 = (RB80_MAX_MS - RB80_T2T_MS) / (pow (d, e) - 1.0);
        a80 = RB80_T2T_MS - b80;
        e80 = e;
        done80 = TRUE;
        }
    if (dist <= 0)
        return 100.0;
    return (a80 + b80 * pow ((double) dist, e80)) * 1000.0;
    }
else {
    double maxd = (double) (RB02_NUMCY - 1);
    double b = (RB02_MAX_MS - RB02_T2T_MS) / (pow (maxd, RB02_EXP) - 1.0);
    double a = RB02_T2T_MS - b;

    if (dist <= 0)
        return RB02_HDSW_MS * 1000.0;
    return (a + b * pow ((double) dist, RB02_EXP)) * 1000.0;
    }
}

/* Realistic time of a transfer (us): rotational latency and one sector
   time per sector (the transfer stops at the end of the track; RB80: 32
   sector slots per revolution, of which 31 are used); reading a header
   waits for the next sector. */

static double rb_xfer_us (UNIT *uptr)
{
double rev = (uptr->flags & UNIT_RB80) ? RB80_REV_US : RB02_REV_US;
double sect = rev / ((uptr->flags & UNIT_RB80) ? RB80_SLOTS : RB_NUMSC (uptr));
int32 wc = ((rbbc * -1) >> 1);
int32 nsect = (wc + RB_NUMWD (uptr) - 1) / RB_NUMWD (uptr);
int32 left = RB_NUMSC (uptr) - (int32) GET_SECT (rbda);

if (GET_FUNC (rbcs) == RBCS_RHDR)
    return sect;
if (left < 1)
    left = 1;
if (nsect > left)
    nsect = left;
if (nsect < 1)
    nsect = 1;
rb_rand_state = rb_rand_state * 1103515245u + 12345u;
return fmod ((double) (rb_rand_state >> 8), rev) + nsect * sect;
}

static void rb_seek_sound (UNIT *uptr, int32 dist)
{
int32 drv = (int32) (uptr - rb_dev.units);
int32 rb80 = (uptr->flags & UNIT_RB80) != 0;
int32 cls = sim_sound_seek (rb_seek_smp[drv], dist, rb80 ? rb_seek80s : rb_seek02s,
                            rb80 ? rb_seek80m : rb_seek02m, FALSE, SND_GRP_DISK, RB_SEEK_GAIN);

if (cls >= 0)
    sim_debug (DBG_CMD, &rb_dev, "RB%d seek sound %s (%d cylinders)\n", drv,
               (cls == 0) ? "short" : ((cls == 1) ? "medium" : "long"), dist);
}
