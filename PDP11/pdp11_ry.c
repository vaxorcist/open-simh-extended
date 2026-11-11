/* pdp11_ry.c: RX211/RXV21/RX02 floppy disk simulator

   Copyright (c) 1993-2013, Robert M Supnik

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
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   ry           RX211/RXV21/RX02 floppy disk

   11-Nov-26    Ulli    Sound realism mode (SET SOUND ENABLED): realistic
                        timing from the RX02 technical manual (stepping,
                        head load and unload, rotational position from
                        time, serial buffer transfers), drive sounds,
                        SHOW RYn DRIVE, DETACH during a command ends it
                        with an error; Set Media Density on an empty
                        drive no longer crashes (known issues case 45)
   23-Oct-13    RMS     Revised for new boot setup routine
   03-Sep-13    RMS     Added explicit void * cast
   15-May-06    RMS     Fixed bug in autosize attach (David Gesswein)
   07-Jul-05    RMS     Removed extraneous externs
   18-Feb-05    RMS     Fixed bug in boot code (Graham Toal)
   30-Sep-04    RMS     Revised Unibus interface
   21-Mar-04    RMS     Added VAX support
   29-Dec-03    RMS     Added RXV21 support
   19-May-03    RMS     Revised for new conditional compilation scheme
   25-Apr-03    RMS     Revised for extended file support
   14-Mar-03    RMS     Fixed variable size interaction with save/restore
   03-Mar-03    RMS     Fixed autosizing
   12-Oct-02    RMS     Added autoconfigure support

   An RX02 diskette consists of 77 tracks, each with 26 sectors of 256B.
   Tracks are numbered 0-76, sectors 1-26.
*/

#if defined (VM_PDP10)                                  /* PDP10 version */
#include "pdp10_defs.h"
#define DEV_DISI        DEV_DIS

#elif defined (VM_VAX)                                  /* VAX version */
#include "vax_defs.h"
#define DEV_DISI        0

#else                                                   /* PDP-11 version */
#include "pdp11_defs.h"
#define DEV_DISI        DEV_DIS
#endif

#include "sim_sound.h"

#define RX_NUMTR        77                              /* tracks/disk */
#define RX_M_TRACK      0377
#define RX_NUMSC        26                              /* sectors/track */
#define RX_M_SECTOR     0177
#define RX_NUMBY        128
#define RX_SIZE         (RX_NUMTR * RX_NUMSC * RX_NUMBY)
#define RY_NUMBY        256                             /* bytes/sector */
#define RY_SIZE         (RX_NUMTR * RX_NUMSC * RY_NUMBY)
#define RX_NUMDR        2                               /* drives/controller */
#define RX_M_NUMDR      01
#define UNIT_V_DEN      (UNIT_V_UF + 0)                 /* double density */
#define UNIT_V_AUTO     (UNIT_V_UF + 1)                 /* autosize */
#define UNIT_DEN        (1u << UNIT_V_DEN)
#define UNIT_AUTO       (1u << UNIT_V_AUTO)

#define IDLE            0                               /* idle state */
#define RWDS            1                               /* rw, sect next */
#define RWDT            2                               /* rw, track next */
#define RWXFR           3                               /* rw, transfer */
#define FEWC            4                               /* fill empty, wc next */
#define FEBA            5                               /* fill empty, ba next */
#define FEXFR           6                               /* fill empty, transfer */
#define SDCNF           7                               /* set dens, conf next */
#define SDXFR           8                               /* set dens, transfer */
#define ESBA            9                               /* ext sta, ba next */
#define ESXFR           10                              /* ext sta, transfer */
#define CMD_COMPLETE    11                              /* set done next */
#define INIT_COMPLETE   12                              /* init compl next */

#define RYCS_V_FUNC     1                               /* function */
#define RYCS_M_FUNC     7
#define  RYCS_FILL      0                               /* fill buffer */
#define  RYCS_EMPTY     1                               /* empty buffer */
#define  RYCS_WRITE     2                               /* write sector */
#define  RYCS_READ      3                               /* read sector */
#define  RYCS_SDEN      4                               /* set density */
#define  RYCS_RYES      5                               /* read status */
#define  RYCS_WRDEL     6                               /* write del data */
#define  RYCS_ESTAT     7                               /* read ext status */
#define RYCS_V_DRV      4                               /* drive select */
#define RYCS_V_DONE     5                               /* done */
#define RYCS_V_IE       6                               /* int enable */
#define RYCS_V_TR       7                               /* xfer request */
#define RYCS_V_DEN      8                               /* density select */
#define RYCS_V_RY       11                              /* RX02 flag */
#define RYCS_V_UAE      12                              /* addr ext */
#define RYCS_M_UAE      03
#define RYCS_V_INIT     14                              /* init */
#define RYCS_V_ERR      15                              /* error */
#define RYCS_FUNC       (RYCS_M_FUNC << RYCS_V_FUNC)
#define RYCS_DRV        (1u << RYCS_V_DRV)
#define RYCS_DONE       (1u << RYCS_V_DONE)
#define RYCS_IE         (1u << RYCS_V_IE)
#define RYCS_TR         (1u << RYCS_V_TR)
#define RYCS_DEN        (1u << RYCS_V_DEN)
#define RYCS_RY         (1u << RYCS_V_RY)
#define RYCS_UAE        (RYCS_M_UAE << RYCS_V_UAE)
#define RYCS_INIT       (1u << RYCS_V_INIT)
#define RYCS_ERR        (1u << RYCS_V_ERR)
#define RYCS_IMP        (RYCS_ERR+RYCS_UAE+RYCS_DEN+RYCS_TR+RYCS_IE+\
                         RYCS_DONE+RYCS_DRV+RYCS_FUNC)
#define RYCS_RW         (RYCS_UAE+RYCS_DEN+RYCS_IE+RYCS_DRV+RYCS_FUNC)
#define RYCS_GETFNC(x)  (((x) >> RYCS_V_FUNC) & RYCS_M_FUNC)
#define RYCS_GETUAE(x)  (((x) >> RYCS_V_UAE) & RYCS_M_UAE)

#define RYES_CRC        00001                           /* CRC error NI */
#define RYES_ID         00004                           /* init done */
#define RYES_ACLO       00010                           /* ACLO NI */
#define RYES_DERR       00020                           /* density err */
#define RYES_DDEN       00040                           /* drive density */
#define RYES_DD         00100                           /* deleted data */
#define RYES_DRDY       00200                           /* drive ready */
#define RYES_USEL       00400                           /* unit selected */
#define RYES_WCO        02000                           /* wc overflow */
#define RYES_NXM        04000                           /* nxm */
#define RYES_ERR        (RYES_NXM|RYES_WCO|RYES_DERR|RYES_ACLO|RYES_CRC)

#define TRACK u3                                        /* current track */
#define CALC_DA(t,s,b)  (((t) * RX_NUMSC) + ((s) - 1)) * b

int32 ry_csr = 0;                                       /* control/status */
int32 ry_dbr = 0;                                       /* data buffer */
int32 ry_esr = 0;                                       /* error status */
uint8 ry_ecode = 0;                                     /* error code */
uint8 ry_track = 0;                                     /* desired track */
uint8 ry_sector = 0;                                    /* desired sector */
int32 ry_ba = 0;                                        /* bus addr */
uint8 ry_wc = 0;                                        /* word count */
int32 ry_state = IDLE;                                  /* controller state */
int32 ry_stopioe = 1;                                   /* stop on error */
int32 ry_cwait = 100;                                   /* command time */
int32 ry_swait = 10;                                    /* seek, per track */
int32 ry_xwait = 1;                                     /* tr set time */
uint8 rx2xb[RY_NUMBY] = { 0 };                          /* sector buffer */

/* Sound realism mode (SET SOUND ENABLED): the drive mechanics of each
   RX02 drive (sim_sound SND_FLOPPY) and their adjustable times, shared by
   both drives (registers STEP, SETTLE, HDLOAD, HDUNLOAD, MOTORUP). */

static SND_FLOPPY ry_flp[RX_NUMDR];
static SND_FLP_REGS ry_flp_regs;
static t_bool ry_flp_inited = FALSE;

#define RY_REAL         (sim_sound_enabled ())
#define RY_BIT_US       1.2                             /* RX02 <-> interface: 1.2 us per bit */
#define RYDEB_OPS       0001                            /* debug: commands with their times */

DEBTAB ry_deb[] = {
    { "OPS",  RYDEB_OPS,  "commands and their realistic times (realism mode)" },
    { NULL, 0 }
    };

t_stat ry_rd (int32 *data, int32 PA, int32 access);
t_stat ry_wr (int32 data, int32 PA, int32 access);
t_stat ry_svc (UNIT *uptr);
t_stat ry_reset (DEVICE *dptr);
t_stat ry_boot (int32 unitno, DEVICE *dptr);
void ry_done (int32 esr_flags, uint8 new_ecode);
t_stat ry_set_size (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ry_attach (UNIT *uptr, CONST char *cptr);
t_stat ry_detach (UNIT *uptr);
t_stat ry_show_drive (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
static double ry_rw_us (UNIT *uptr, int32 bps);
static double ry_init_us (void);
static double ry_sden_us (UNIT *uptr);
t_stat ry_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *ry_description (DEVICE *dptr);


/* RY11 data structures

   ry_dev       RY device descriptor
   ry_unit      RY unit list
   ry_reg       RY register list
   ry_mod       RY modifier list
*/

#define IOLN_RY         004

DIB ry_dib = {
    IOBA_AUTO, IOLN_RY, &ry_rd, &ry_wr,
    1, IVCL (RY), VEC_AUTO, { NULL }, IOLN_RY,
    };

UNIT ry_unit[] = {
    { UDATA (&ry_svc, UNIT_DEN+UNIT_FIX+UNIT_ATTABLE+UNIT_BUFABLE+UNIT_MUSTBUF,
             RY_SIZE) },
    { UDATA (&ry_svc, UNIT_DEN+UNIT_FIX+UNIT_ATTABLE+UNIT_BUFABLE+UNIT_MUSTBUF,
             RY_SIZE) }
    };

REG ry_reg[] = {
    { GRDATAD (RYCS,            ry_csr, DEV_RDX, 16, 0, "status") },
    { GRDATAD (RYBA,             ry_ba, DEV_RDX, 16, 0, "buffer address") },
    { GRDATAD (RYWC,             ry_wc, DEV_RDX,  8, 0, "word count") },
    { GRDATAD (RYDB,            ry_dbr, DEV_RDX, 16, 0, "data buffer") },
    { GRDATAD (RYES,            ry_esr, DEV_RDX, 12, 0, "error status") },
    { GRDATAD (RYERR,         ry_ecode, DEV_RDX,  8, 0, "error code") },
    { GRDATAD (RYTA,          ry_track, DEV_RDX,  8, 0, "current track") },
    { GRDATAD (RYSA,         ry_sector, DEV_RDX,  8, 0, "current sector") },
    { DRDATAD (STAPTR,        ry_state,       4,        "controller state"), REG_RO },
    { FLDATAD (INT,          IREQ (RY), INT_V_RY,       "interrupt pending flag") },
    { FLDATAD (ERR,             ry_csr, RYCS_V_ERR,     "error flag") },
    { FLDATAD (TR,              ry_csr, RYCS_V_TR,      "transfer ready flag ") },
    { FLDATAD (IE,              ry_csr, RYCS_V_IE,      "interrupt enable flag ") },
    { FLDATAD (DONE,            ry_csr, RYCS_V_DONE,    "device done flag") },
    { DRDATAD (CTIME,         ry_cwait, 24,             "command completion time"), PV_LEFT },
    { DRDATAD (STIME,         ry_swait, 24,             "seek time, per track"), PV_LEFT },
    { DRDATAD (XTIME,         ry_xwait, 24,             "transfer ready delay"), PV_LEFT },
    { BRDATAD (SBUF,             rx2xb, 8, 8, RY_NUMBY, "sector buffer array") },
    { FLDATAD (STOP_IOE,    ry_stopioe, 0,              "stop on I/O error") },
    { DRDATAD (STEP,  ry_flp_regs.step_us,     24,      "realism mode: step time per track (us)"), PV_LEFT },
    { DRDATAD (SETTLE, ry_flp_regs.settle_us,  24,      "realism mode: head settle time (us)"), PV_LEFT },
    { DRDATAD (HDLOAD, ry_flp_regs.hdload_us,  24,      "realism mode: head load time (us)"), PV_LEFT },
    { DRDATAD (HDUNLOAD, ry_flp_regs.hdunload_ms, 24,   "realism mode: head unloaded after (ms) without a command"), PV_LEFT },
    { DRDATAD (MOTORUP, ry_flp_regs.motorup_ms, 24,     "realism mode: motor run-up at power on (ms)"), PV_LEFT },
    { URDATA  (CAPAC, ry_unit[0].capac, 10, T_ADDR_W, 0,
              RX_NUMDR, REG_HRO | PV_LEFT) },
    { GRDATA  (DEVADDR,     ry_dib.ba, DEV_RDX, 32, 0), REG_HRO },
    { GRDATA  (DEVVEC, ry_dib.vec, DEV_RDX, 16, 0), REG_HRO },
    { NULL }
    };

MTAB ry_mod[] = {
    { MTAB_XTD|MTAB_VUN,                    0, "write enabled", "WRITEENABLED", 
        &set_writelock, &show_writelock,   NULL, "Write enable floppy drive" },
    { MTAB_XTD|MTAB_VUN,                    1, NULL, "LOCKED", 
        &set_writelock, NULL,   NULL, "Write lock floppy drive" },
    { (UNIT_DEN+UNIT_ATT),           UNIT_ATT, "single density",    NULL, NULL },
    { (UNIT_DEN+UNIT_ATT), (UNIT_DEN+UNIT_ATT), "double density",   NULL, NULL },
    { (UNIT_AUTO+UNIT_DEN+UNIT_ATT),         0, "single density",   NULL, NULL },
    { (UNIT_AUTO+UNIT_DEN+UNIT_ATT),  UNIT_DEN, "double density",   NULL, NULL },
    { (UNIT_AUTO+UNIT_ATT),          UNIT_AUTO, "autosize",         NULL, NULL },
    { UNIT_AUTO,                     UNIT_AUTO, NULL,               "AUTOSIZE", 
        NULL, NULL, NULL, "set density based on file size at ATTACH" },
    { (UNIT_AUTO+UNIT_DEN),                  0, NULL,               "SINGLE", 
        &ry_set_size, NULL, NULL, "Set to Single density (256Kb)" },
    { (UNIT_AUTO+UNIT_DEN),           UNIT_DEN, NULL,               "DOUBLE", 
        &ry_set_size, NULL, NULL, "Set to Double density (512Kb)" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO,          0, "DRIVE", NULL,
        NULL, &ry_show_drive, NULL, "Display the drive mechanics (sound realism mode)" },
#if defined (VM_PDP11)
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 004, "ADDRESS", "ADDRESS",
      &set_addr, &show_addr, NULL, "Bus Address" },
    { MTAB_XTD | MTAB_VDV, 0, NULL, "AUTOCONFIGURE",
      &set_addr_flt, NULL, NULL, "Enable autoconfiguration of address & vector" },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "VECTOR", "VECTOR",
      &set_vec, &show_vec, NULL, "Interrupt vector" },
#else
    { MTAB_XTD|MTAB_VDV, 004, "ADDRESS", NULL,
      NULL, &show_addr, NULL, "Display Bus Address" },
    { MTAB_XTD|MTAB_VDV, 0, "VECTOR", NULL,
      NULL, &show_vec, NULL, "Display Interrupt vector" },
#endif
    { 0 }
    };

DEVICE ry_dev = {
    "RY", ry_unit, ry_reg, ry_mod,
    RX_NUMDR, DEV_RDX, 20, 1, DEV_RDX, 8,
    NULL, NULL, &ry_reset,
    &ry_boot, &ry_attach, &ry_detach,
    &ry_dib, DEV_DISABLE | DEV_DISI | DEV_UBUS | DEV_Q18 | DEV_DEBUG, 0,
    ry_deb, NULL, NULL, &ry_help, NULL, NULL,
    &ry_description
    };

/* I/O dispatch routine, I/O addresses 17777170 - 17777172

   17777170             floppy CSR
   17777172             floppy data register
*/

t_stat ry_rd (int32 *data, int32 PA, int32 access)
{
switch ((PA >> 1) & 1) {                                /* decode PA<1> */

    case 0:                                             /* RYCS */
        ry_csr = (ry_csr & RYCS_IMP) | RYCS_RY;         /* clear junk */
        *data = ry_csr;
        break;

    case 1:                                             /* RYDB */
        *data = ry_dbr;                                 /* return data */
        break;
        }                                               /* end switch PA */

return SCPE_OK;
}

t_stat ry_wr (int32 data, int32 PA, int32 access)
{
int32 drv;

switch ((PA >> 1) & 1) {                                /* decode PA<1> */

/* Writing RYCS, three cases:
   1. Writing INIT, reset device
   2. Idle and writing new function
        - clear error, done, transfer ready, int req
        - save int enable, function, drive
        - start new function
   3. Otherwise, write IE and update interrupts
*/

    case 0:                                             /* RYCS */
        ry_csr = (ry_csr & RYCS_IMP) | RYCS_RY;         /* clear junk */
        if (access == WRITEB) data = (PA & 1)?          /* write byte? */
            (ry_csr & 0377) | (data << 8): (ry_csr & ~0377) | data;
        if (data & RYCS_INIT) {                         /* initialize? */
            ry_reset (&ry_dev);                         /* reset device */
            return SCPE_OK;                             /* end if init */
            }
        if ((data & CSR_GO) && (ry_state == IDLE)) {    /* new function? */
            ry_csr = (data & RYCS_RW) | RYCS_RY;
            drv = ((ry_csr & RYCS_DRV)? 1: 0);          /* reselect drv */
            switch (RYCS_GETFNC (data)) {

            case RYCS_FILL: case RYCS_EMPTY:
                ry_state = FEWC;                        /* state = get wc */
                ry_csr = ry_csr | RYCS_TR;              /* xfer is ready */
                break;

            case RYCS_SDEN:
                ry_state = SDCNF;                       /* state = get conf */
                ry_csr = ry_csr | RYCS_TR;              /* xfer is ready */
                break;

            case RYCS_ESTAT:
                ry_state = ESBA;                        /* state = get ba */
                ry_csr = ry_csr | RYCS_TR;              /* xfer is ready */
                break;

            case RYCS_READ: case RYCS_WRITE: case RYCS_WRDEL:
                ry_state = RWDS;                        /* state = get sector */
                ry_csr = ry_csr | RYCS_TR;              /* xfer is ready */
                ry_esr = ry_esr & RYES_ID;              /* clear errors */
                ry_ecode = 0;
                break;

            default:                                    /* read status */
                ry_state = CMD_COMPLETE;                /* state = cmd compl */
                if (RY_REAL) {                          /* load the head, count index */
                    SND_FLOPPY *f = &ry_flp[drv];       /* pulses: one revolution */
                    double t;

                    sim_floppy_unload (&ry_flp[drv ^ 1]);
                    t = sim_floppy_seek (f, sim_floppy_track (f), TRUE);
                    t = sim_floppy_rotate (f, t, 0, RX_NUMSC);  /* to the index, then once round */
                    sim_debug (RYDEB_OPS, &ry_dev, ">>RY%d read status: %.1f ms\n", drv, t / 1000.0);
                    sim_sound_activate_us (&ry_unit[drv], t);
                    }
                else
                    sim_activate (&ry_unit[drv], ry_cwait);
                break;
                }                                       /* end switch func */
            return SCPE_OK;
            }                                           /* end if GO */
        if ((data & RYCS_IE) == 0)
            CLR_INT (RY);
        else if ((ry_csr & (RYCS_DONE + RYCS_IE)) == RYCS_DONE)
            SET_INT (RY);
        ry_csr = (ry_csr & ~RYCS_RW) | (data & RYCS_RW);
        break;                                          /* end case RYCS */

/* Accessing RYDB, two cases:
   1. Write idle, write
   2. Write not idle and TR set, state dependent
*/

    case 1:                                             /* RYDB */
        if ((PA & 1) || ((ry_state != IDLE) && ((ry_csr & RYCS_TR) == 0)))
            return SCPE_OK;                             /* if ~IDLE, need tr */
        ry_dbr = data;                                  /* save data */
        if (ry_state != IDLE) {
            drv = ((ry_csr & RYCS_DRV)? 1: 0);          /* select drv */
            sim_activate (&ry_unit[drv], ry_xwait);     /* sched event */
            ry_csr = ry_csr & ~RYCS_TR;                 /* clear xfer */
            }
        break;                                          /* end case RYDB */
        }                                               /* end switch PA */

return SCPE_OK;
}

/* Unit service; the action to be taken depends on the transfer state:

   IDLE         Should never get here
   FEWC         Save word count, set TR, set FEBA
   FEBA         Save bus address, set FEXFR
   FEXFR        Fill/empty buffer
   RWDS         Save sector, set TR, set RWDT
   RWDT         Save track, set RWXFR
   RWXFR        Read/write buffer
   SDCNF        Check confirmation, set SDXFR
   SDXFR        Erase disk
   CMD_COMPLETE copy requested data to ir, finish command
   INIT_COMPLETE read drive 0, track 1, sector 1 to buffer, finish command
*/

t_stat ry_svc (UNIT *uptr)
{
int32 i, t, func, bps;
static uint8 estat[8];
uint32 ba, da;
int8 *fbuf = (int8 *) uptr->filebuf;

func = RYCS_GETFNC (ry_csr);                            /* get function */
bps = (ry_csr & RYCS_DEN)? RY_NUMBY: RX_NUMBY;          /* get sector size */
ba = (RYCS_GETUAE (ry_csr) << 16) | ry_ba;              /* get mem addr */
switch (ry_state) {                                     /* case on state */

    case IDLE:                                          /* idle */
        return SCPE_IERR;

    case FEWC:                                          /* word count */
        ry_wc = ry_dbr & 0377;                          /* save WC */
        ry_csr = ry_csr | RYCS_TR;                      /* set TR */
        ry_state = FEBA;                                /* next state */
        return SCPE_OK;

    case FEBA:                                          /* buffer address */
        ry_ba = ry_dbr;                                 /* save buf addr */
        ry_state = FEXFR;                               /* next state */
        if (RY_REAL)                                    /* serial RX02 <-> interface */
            sim_sound_activate_us (uptr, (double)(ry_wc << 1) * 8.0 * RY_BIT_US);
        else
            sim_activate (uptr, ry_cwait);              /* schedule xfer */
        return SCPE_OK;

    case FEXFR:                                         /* transfer */
        if ((ry_wc << 1) > bps) {                       /* wc too big? */
            ry_done (RYES_WCO, 0230);                   /* error */
            break;
            }
        if (func == RYCS_FILL) {                        /* fill? read */
            for (i = 0; i < RY_NUMBY; i++)
                rx2xb[i] = 0;
            t = Map_ReadB (ba, ry_wc << 1, rx2xb);
            }
        else t = Map_WriteB (ba, ry_wc << 1, rx2xb);
        ry_wc = (uint8)(t >> 1);                        /* adjust wc */
        ry_done (t? RYES_NXM: 0, 0);                    /* done */
        break;

    case RWDS:                                          /* wait for sector */
        ry_sector = ry_dbr & RX_M_SECTOR;               /* save sector */
        ry_csr = ry_csr | RYCS_TR;                      /* set xfer */
        ry_state = RWDT;                                /* advance state */
        return SCPE_OK;

    case RWDT:                                          /* wait for track */
        ry_track = ry_dbr & RX_M_TRACK;                 /* save track */
        ry_state = RWXFR;                               /* next state */
        if (RY_REAL)
            sim_sound_activate_us (uptr, ry_rw_us (uptr, bps));
        else
            sim_activate (uptr,                         /* sched xfer */
                ry_swait * abs (ry_track - uptr->TRACK));
        return SCPE_OK;

    case RWXFR:                                         /* read/write */
        if ((uptr->flags & UNIT_BUF) == 0) {            /* not buffered? */
            ry_done (0, 0110);                          /* done, error */
            if (RY_REAL)                                /* no diskette (or the door was */
                break;                                  /* opened): an error, as on the drive */
            return IORETURN (ry_stopioe, SCPE_UNATT);
            }
        if (ry_track >= RX_NUMTR) {                     /* bad track? */
            ry_done (0, 0040);                          /* done, error */
            break;
            }
        uptr->TRACK = ry_track;                         /* now on track */
        if ((ry_sector == 0) || (ry_sector > RX_NUMSC)) { /* bad sect? */
            ry_done (0, 0070);                          /* done, error */
            break;
            }
        if (((uptr->flags & UNIT_DEN) != 0) ^
            ((ry_csr & RYCS_DEN) != 0)) {               /* densities agree? */
            ry_done (RYES_DERR, 0240);                  /* no, error */
            break;
            }
        da = CALC_DA (ry_track, ry_sector, bps);        /* get disk address */
        if (func == RYCS_WRDEL)                         /* del data? */
            ry_esr = ry_esr | RYES_DD;
        if (func == RYCS_READ) {                        /* read? */
            for (i = 0; i < bps; i++)
                rx2xb[i] = fbuf[da + i];
             }
        else {
            if (uptr->flags & UNIT_WPRT) {              /* write and locked? */
                ry_done (0, 0100);                      /* done, error */
                break;
                }
            for (i = 0; i < bps; i++)                   /* write */
                fbuf[da + i] = rx2xb[i];
            da = da + bps;
            if (da > uptr->hwmark)
                uptr->hwmark = da;
            }
        ry_done (0, 0);                                 /* done */
        break;

    case SDCNF:                                         /* confirm set density */
        if ((ry_dbr & 0377) != 0111) {                  /* confirmed? */
            ry_done (0, 0250);                          /* no, error */
            break;
            }
        ry_state = SDXFR;                               /* next state */
        if (RY_REAL)
            sim_sound_activate_us (uptr, ry_sden_us (uptr));
        else
            sim_activate (uptr, ry_cwait * 100);        /* schedule operation */
        break;

    case SDXFR:                                         /* erase disk */
        if ((uptr->flags & UNIT_BUF) == 0) {            /* no diskette: the buffer is */
            ry_done (0, 0110);                          /* not there (known issues 45) */
            break;
            }
        for (i = 0; i < (int32) uptr->capac; i++)
            fbuf[i] = 0;
        uptr->hwmark = (uint32) uptr->capac;
        if (ry_csr & RYCS_DEN)
            uptr->flags = uptr->flags | UNIT_DEN;
        else uptr->flags = uptr->flags & ~UNIT_DEN;
        ry_done (0, 0);
        break;


    case ESBA:
        ry_ba = ry_dbr;                                 /* save WC */
        ry_state = ESXFR;                               /* next state */
        sim_activate (uptr, ry_cwait);                  /* schedule xfer */
        return SCPE_OK;

    case ESXFR:
        estat[0] = ry_ecode;                            /* fill 8B status */
        estat[1] = ry_wc;
        estat[2] = (uint8)ry_unit[0].TRACK;
        estat[3] = (uint8)ry_unit[1].TRACK;
        estat[4] = ry_track;
        estat[5] = ry_sector;
        estat[6] = ((ry_csr & RYCS_DRV)? 0200: 0) |
                   ((ry_unit[1].flags & UNIT_DEN)? 0100: 0) |
                   ((uptr->flags & UNIT_ATT)? 0040: 0) |
                   ((ry_unit[0].flags & UNIT_DEN)? 0020: 0) |
                   ((ry_csr & RYCS_DEN)? 0001: 0);
        estat[7] = (uint8)uptr->TRACK;
        t = Map_WriteB (ba, 8, estat);                  /* DMA to memory */
        ry_done (t? RYES_NXM: 0, 0);                    /* done */
        break;

    case CMD_COMPLETE:                                  /* command complete */
        ry_done (0, 0);
        break;

    case INIT_COMPLETE:                                 /* init complete */
        ry_unit[0].TRACK = 1;                           /* drive 0 to trk 1 */
        ry_unit[1].TRACK = 0;                           /* drive 1 to trk 0 */
        if ((uptr->flags & UNIT_BUF) == 0) {            /* not buffered? */
            ry_done (RYES_ID, 0010);                    /* init done, error */
            break;
            }
        da = CALC_DA (1, 1, bps);                       /* track 1, sector 1 */
        for (i = 0; i < bps; i++)                       /* read sector */
            rx2xb[i] = fbuf[da + i];
        ry_done (RYES_ID, 0);                           /* set done */
        if ((ry_unit[1].flags & UNIT_ATT) == 0)
            ry_ecode = 0020;
        break;
        }                                               /* end case state */

return SCPE_OK;
}

/* Command complete.  Set done and put final value in interface register,
   request interrupt if needed, return to IDLE state.
*/

void ry_done (int32 esr_flags, uint8 new_ecode)
{
int32 drv = (ry_csr & RYCS_DRV)? 1: 0;

ry_state = IDLE;                                        /* now idle */
ry_csr = ry_csr | RYCS_DONE;                            /* set done */
if (ry_csr & CSR_IE)                                    /* if ie, intr */
    SET_INT (RY);
ry_esr = (ry_esr | esr_flags) & ~(RYES_USEL|RYES_DDEN|RYES_DRDY);
if (drv)                                                /* updates RYES */
    ry_esr = ry_esr | RYES_USEL;
if (ry_unit[drv].flags & UNIT_ATT) {
    ry_esr = ry_esr | RYES_DRDY;
    if (ry_unit[drv].flags & UNIT_DEN)
        ry_esr = ry_esr | RYES_DDEN;
    }
if ((new_ecode > 0) || (ry_esr & RYES_ERR))             /* test for error */
    ry_csr = ry_csr | RYCS_ERR;
ry_ecode = new_ecode;                                   /* update ecode */
ry_dbr = ry_esr;                                        /* update RYDB */
if (RY_REAL)                                            /* the head unloads if no new */
    sim_floppy_end (&ry_flp[drv], 0.0);                 /* command comes within HDUNLOAD */
return;
}

/* Device initialization.  The RY is one of the few devices that schedules
   an I/O transfer as part of its initialization.
*/

t_stat ry_reset (DEVICE *dptr)
{
int32 i;

if (!ry_flp_inited) {                                   /* drive mechanics, once */
    ry_flp_inited = TRUE;
    sim_floppy_set_defaults (&ry_flp_regs, &sim_flp_rx02);
    }
for (i = 0; i < RX_NUMDR; i++)
    sim_floppy_init (&ry_flp[i], &ry_unit[i], &sim_flp_rx02, &ry_flp_regs, SND_GRP_DISK, 1.0);
ry_csr = ry_dbr = 0;                                    /* clear registers */
ry_esr = ry_ecode = 0;                                  /* clear error */
ry_ba = ry_wc = 0;                                      /* clear wc, ba */
ry_track = ry_sector = 0;                               /* clear trk, sector */
ry_state = IDLE;                                        /* ctrl idle */
CLR_INT (RY);                                           /* clear int req */
sim_cancel (&ry_unit[1]);                               /* cancel drive 1 */
if (dptr->flags & UNIT_DIS)                             /* disabled? */
    sim_cancel (&ry_unit[0]);
else if (RY_REAL) {                                     /* realism mode: */
    ry_state = INIT_COMPLETE;
    sim_sound_activate_us (&ry_unit[0], ry_init_us ());
    }
else if (ry_unit[0].flags & UNIT_BUF)  {                /* attached? */
    ry_state = INIT_COMPLETE;                           /* yes, sched init */
    sim_activate (&ry_unit[0], ry_swait * abs (1 - ry_unit[0].TRACK));
    }
else ry_done (RYES_ID, 0010);                           /* no, error */
return auto_config (dptr->name, 1);                     /* run autoconfig */
}

/* Attach routine */

t_stat ry_attach (UNIT *uptr, CONST char *cptr)
{
uint32 sz;
t_stat r;

if ((uptr->flags & UNIT_AUTO) && (sz = sim_fsize_name (cptr))) {
    if (sz > RX_SIZE)
        uptr->flags = uptr->flags | UNIT_DEN;
    else uptr->flags = uptr->flags & ~UNIT_DEN;
    }
uptr->capac = (uptr->flags & UNIT_DEN)? RY_SIZE: RX_SIZE;
r = attach_unit (uptr, cptr);
if (r == SCPE_OK)
    sim_floppy_insert (&ry_flp[uptr - ry_unit]);        /* realism: door closed */
return r;
}

/* Detach: opening the door.  A command running on the drive ends with an
   error when it reaches the diskette (realism mode; known issue in the
   normal mode: STOP_IOE). */

t_stat ry_detach (UNIT *uptr)
{
if (uptr->flags & UNIT_ATT)
    sim_floppy_remove (&ry_flp[uptr - ry_unit]);
return detach_unit (uptr);
}

/* Realistic times (sound realism mode), from the RX02 Floppy Disk System
   Technical Manual EK-0RX02-TM-001:
   - Read/Write Sector: step 6 ms per track, then head load / settle
     25 ms, then wait until the sector comes round (360 rpm, 26 sectors
     evenly spaced after the index), then one sector; a sector that does
     not exist (or no diskette) is given up after two revolutions (error
     0070/0110); a track above 76 is refused at once (0040).
   - Initialize: both heads step home, drive 0 reads track 1 sector 1
     (at most 1.8 s in the manual).
   - Set Media Density: about 15 s: a revolution per track plus the step.
   - Read Status: head load plus counting index pulses (about 250 ms).
   - Fill/Empty Buffer: 1.2 us per bit between the RX02 and the
     interface (the RX211 does the DMA). */

static double ry_rw_us (UNIT *uptr, int32 bps)
{
int32 drv = (int32)(uptr - ry_unit);
SND_FLOPPY *f = &ry_flp[drv];
double t;

sim_floppy_unload (&ry_flp[drv ^ 1]);                   /* one head load line */
if (ry_track >= RX_NUMTR) {                             /* refused before stepping */
    sim_debug (RYDEB_OPS, &ry_dev, ">>RY%d track %d refused\n", drv, ry_track);
    return 100.0;
    }
t = sim_floppy_seek (f, ry_track, TRUE);                /* steps, head load, settle */
if (((uptr->flags & UNIT_BUF) == 0) ||                  /* no diskette or no such sector: */
    (ry_sector == 0) || (ry_sector > RX_NUMSC))         /* two revolutions */
    t += 2.0 * sim_floppy_rev_us (f);
else
    t = sim_floppy_rotate (f, t, ry_sector - 1, 1);
sim_debug (RYDEB_OPS, &ry_dev, ">>RY%d %s track %d sector %d: %.2f ms\n", drv,
           (RYCS_GETFNC (ry_csr) == RYCS_READ) ? "read" : "write", ry_track, ry_sector, t / 1000.0);
return t;
}

static double ry_init_us (void)
{
SND_FLOPPY *f0 = &ry_flp[0];
double t0, t1, t;

sim_floppy_unload (&ry_flp[0]);
sim_floppy_unload (&ry_flp[1]);
t0 = sim_floppy_seek (&ry_flp[0], 0, FALSE);            /* both heads home */
t1 = sim_floppy_seek (&ry_flp[1], 0, FALSE);
t = (t0 > t1) ? t0 : t1;
t += sim_floppy_seek (f0, 1, TRUE);                     /* drive 0: track 1 */
if (ry_unit[0].flags & UNIT_BUF)
    t = sim_floppy_rotate (f0, t, 0, 1);                /* sector 1 */
else
    t += 2.0 * sim_floppy_rev_us (f0);                  /* no diskette: no sector */
sim_debug (RYDEB_OPS, &ry_dev, ">>RY initialize: %.2f ms\n", t / 1000.0);
return t;
}

static double ry_sden_us (UNIT *uptr)
{
int32 drv = (int32)(uptr - ry_unit);
SND_FLOPPY *f = &ry_flp[drv];
double rev = sim_floppy_rev_us (f);
double per = (double)ry_flp_regs.step_us + (double)ry_flp_regs.settle_us + rev;
double t;

sim_floppy_unload (&ry_flp[drv ^ 1]);
t = sim_floppy_seek (f, 0, TRUE);                       /* track 0, head loaded */
if ((uptr->flags & UNIT_BUF) == 0)                      /* no diskette: no header */
    return t + 2.0 * rev;
t = sim_floppy_rotate (f, t, 0, 0);                     /* to the index */
sim_floppy_steps (f, RX_NUMTR - 1, per, t + rev);       /* a track per revolution */
t += (double)RX_NUMTR * rev + (double)(RX_NUMTR - 1) * (per - rev);
(void) sim_floppy_seek (f, RX_NUMTR - 1, FALSE);        /* (no sound: played above) */
sim_debug (RYDEB_OPS, &ry_dev, ">>RY%d set density: %.1f s\n", drv, t / 1.0e6);
return t;
}

t_stat ry_show_drive (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
return sim_floppy_show (st, &ry_flp[uptr - ry_unit]);
}

/* Set size routine */

t_stat ry_set_size (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
uptr->capac = val? RY_SIZE: RX_SIZE;
return SCPE_OK;
}

/* Device bootstrap */

#if defined (VM_PDP11)

#define BOOT_START      02000                           /* start */
#define BOOT_ENTRY      (BOOT_START + 002)              /* entry */
#define BOOT_UNIT       (BOOT_START + 010)              /* unit number */
#define BOOT_CSR        (BOOT_START + 026)              /* CSR */
#define BOOT_LEN        (sizeof (boot_rom) / sizeof (int16))

static const uint16 boot_rom[] = {
    042131,                         /* "YD" */
    0012706, BOOT_START,            /*  MOV #boot_start, SP */
    0012700, 0000000,               /*  MOV #unit, R0       ; unit number */
    0010003,                        /*  MOV R0, R3 */
    0006303,                        /*  ASL R3 */
    0006303,                        /*  ASL R3 */
    0006303,                        /*  ASL R3 */
    0006303,                        /*  ASL R3 */
    0012701, 0177170,               /*  MOV #RYCS, R1       ; csr */
    0005002,                        /*  CLR R2              ; ba */
    0005004,                        /*  CLR R4              ; density */
    0012705, 0000001,               /*  MOV #1, R5          ; sector */
    0005104,                        /* DN: COM R4           ; compl dens */
    0042704, 0177377,               /*  BIC #177377, R4     ; clr rest */
    0032711, 0000040,               /* RD: BIT #40, (R1)    ; ready? */
    0001775,                        /*  BEQ .-4 */
    0012746, 0000007,               /*  MOV #READ+GO, -(SP) */
    0050316,                        /*  BIS R3, (SP)        ; or unit */
    0050416,                        /*  BIS R4, (SP)        ; or density */
    0012611,                        /*  MOV (SP)+, (R1)     ; read & go */
    0105711,                        /*  TSTB (R1)           ; xfr ready? */
    0100376,                        /*  BPL .-2 */
    0010561, 0000002,               /*  MOV R5, 2(R1)       ; sector */
    0105711,                        /*  TSTB (R1)           ; xfr ready? */
    0100376,                        /*  BPL .-2 */
    0012761, 0000001, 0000002,      /*  MOV #1, 2(R1)       ; track */
    0032711, 0000040,               /*  BIT #40, (R1)       ; ready? */
    0001775,                        /*  BEQ .-4 */
    0005711,                        /*  TST (R1)            ; error? */
    0100003,                        /*  BEQ OK */
    0005704,                        /*  TST R4              ; single? */
    0001345,                        /*  BNE DN              ; no, try again */
    0000000,                        /*  HALT                ; dead */
    0012746, 0000003,               /* OK: MOV #EMPTY+GO, -(SP); empty & go */
    0050416,                        /*  BIS R4, (SP)        ; or density */
    0012611,                        /*  MOV (SP)+, (R1)     ; read & go */
    0105711,                        /*  TSTB (R1)           ; xfr, done? */
    0001776,                        /*  BPL .-2 */
    0012746, 0000100,               /*  MOV #100, -(SP)     ; assume sd */
    0005704,                        /*  TST R4              ; test dd */
    0001401,                        /*  BEQ .+4 */
    0006316,                        /*  ASL (SP)            ; dd, double */
    0011661, 0000002,               /*  MOV (SP), 2(R1)     ; wc */
    0105711,                        /*  TSTB (R1)           ; xfr, done? */
    0001776,                        /*  BPL .-2 */
    0010261, 0000002,               /*  MOV R2, 2(R1)       ; ba */
    0032711, 0000040,               /*  BIT #40, (R1)       ; ready? */
    0001775,                        /*  BEQ .-4 */
    0061602,                        /*  ADD (SP), R2        ; cvt wd to byte */
    0062602,                        /*  ADD (SP)+, R2       ; adv buf addr */
    0122525,                        /*  CMPB (R5)+, (R5)+   ; sect += 2 */
    0020527, 0000007,               /*  CMP R5, #7          ; end? */
    0101715,                        /*  BLOS RD             ; read next */
    0005002,                        /*  CLR R2 */
    0005003,                        /*  CLR R3 */
    0012704, BOOT_START+020,        /*  MOV #START+20, R4 */
    0005005,                        /*  CLR R5 */
    0005007                         /*  CLR R7 */
    };

t_stat ry_boot (int32 unitno, DEVICE *dptr)
{
size_t i;

if ((ry_unit[unitno & RX_M_NUMDR].flags & UNIT_DEN) == 0)
    return SCPE_NOFNC;
for (i = 0; i < BOOT_LEN; i++)
    WrMemW (BOOT_START + (2 * i), boot_rom[i]);
WrMemW (BOOT_UNIT, unitno & RX_M_NUMDR);
WrMemW (BOOT_CSR, ry_dib.ba & DMASK);
cpu_set_boot (BOOT_ENTRY);
return SCPE_OK;
}

#else

t_stat ry_boot (int32 unitno, DEVICE *dptr)
{
return SCPE_NOFNC;
}

#endif

t_stat ry_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
fprintf (st, "RX211/RX02 Floppy Disk\n\n");
fprintf (st, "RX211 options include the ability to set units write enabled or write locked,\n");
fprintf (st, "single or double density, or autosized:\n\n");
fprint_set_help (st, dptr);
fprint_show_help (st, dptr);
fprintf (st, "\n");
#if defined (VM_PDP11)
fprintf (st, "The RX211 supports the BOOT command.\n\n");
#endif
fprintf (st, "The RX211 is disabled in a Qbus system with more than 256KB of memory.\n\n");
fprint_reg_help (st, dptr);
fprintf (st, "\nError handling is as follows:\n\n");
fprintf (st, "    error         STOP_IOE   processed as\n");
fprintf (st, "    not attached  1          report error and stop\n");
fprintf (st, "                  0          disk not ready\n\n");
fprintf (st, "RX02 data files are buffered in memory; therefore, end of file and OS I/O\n");
fprintf (st, "errors cannot occur.\n\n");
fprintf (st, "Sound realism mode (SET SOUND ENABLED, see HELP SET SOUND):\n\n");
fprintf (st, "  - The RX02 drive motors are AC motors on the power line: they run while\n");
fprintf (st, "    the system power is on (RUN/BOOT, SET SOUND POWER=ON), with or without\n");
fprintf (st, "    a diskette.  ATTACH is inserting a diskette and closing the door, DETACH\n");
fprintf (st, "    opening the door; a command running on the drive then ends with an\n");
fprintf (st, "    error (it does not stop the simulator).\n");
fprintf (st, "  - Times from the RX02 Technical Manual: 6 ms per track (STEP), 25 ms head\n");
fprintf (st, "    load and settle (HDLOAD, SETTLE), 360 rpm; a sector is read or written\n");
fprintf (st, "    when it comes round under the head, so the interleave of the software\n");
fprintf (st, "    has its real effect; the head unloads HDUNLOAD (200) ms after the last\n");
fprintf (st, "    command.  Initialize, Set Media Density (about 15 s) and Read Status\n");
fprintf (st, "    take their real time, Fill/Empty Buffer 1.2 us per bit.\n");
fprintf (st, "  - Sounds: motor and fan, a diskette turning, head load and unload, one\n");
fprintf (st, "    click per track, the door (sound/rx02, else sound/default_floppy).\n");
fprintf (st, "  - SHOW RYn DRIVE shows diskette, head, track and motor; SET RY DEBUG=OPS\n");
fprintf (st, "    traces every command with its time.\n");
return SCPE_OK;
}

const char *ry_description (DEVICE *dptr)
{
return (UNIBUS) ? "RX211 floppy disk controller" : 
                  "RXV21 floppy disk controller";
}
