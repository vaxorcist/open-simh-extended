/* pdp11_rx.c: RX11/RX01 floppy disk simulator

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

   rx           RX11/RX01 floppy disk

   11-Nov-26    Ulli    Sound realism mode (SET SOUND ENABLED): realistic
                        timing from the RX01 maintenance manual (stepping,
                        head load after the steps, head unload, rotational
                        position from time, 18 us per byte on the interface),
                        drive sounds, SHOW RXn DRIVE, DETACH during a command
                        ends it with an error; debug flags OPS, CMD, REG;
                        HELP RX
   23-Oct-13    RMS     Revised for new boot setup routine
   03-Sep-13    RMS     Added explicit void * cast
   07-Jul-05    RMS     Removed extraneous externs
   12-Oct-02    RMS     Added autoconfigure support
   08-Oct-02    RMS     Added variable address support to bootstrap
                        Added vector change/display support
                        Revised state machine based on RX211
                        New data structures
                        Fixed reset of disabled device
   26-Jan-02    RMS     Revised bootstrap to conform to M9312
   06-Jan-02    RMS     Revised enable/disable support
   30-Nov-01    RMS     Added read only unit, extended SET/SHOW support
   24-Nov-01    RMS     Converted FLG to array
   07-Sep-01    RMS     Revised device disable and interrupt mechanisms
   17-Jul-01    RMS     Fixed warning from VC++ 6.0
   26-Apr-01    RMS     Added device enable/disable support
   13-Apr-01    RMS     Revised for register arrays
   15-Feb-01    RMS     Corrected bootstrap string
   14-Apr-99    RMS     Changed t_addr to unsigned

   An RX01 diskette consists of 77 tracks, each with 26 sectors of 128B.
   Tracks are numbered 0-76, sectors 1-26.
*/

#include "pdp11_defs.h"
#include "sim_sound.h"

#define RX_NUMTR        77                              /* tracks/disk */
#define RX_M_TRACK      0377
#define RX_NUMSC        26                              /* sectors/track */
#define RX_M_SECTOR     0177
#define RX_NUMBY        128                             /* bytes/sector */
#define RX_SIZE         (RX_NUMTR * RX_NUMSC * RX_NUMBY)        /* bytes/disk */
#define RX_NUMDR        2                               /* drives/controller */
#define RX_M_NUMDR      01

#define IDLE            0                               /* idle state */
#define RWDS            1                               /* rw, sect next */
#define RWDT            2                               /* rw, track next */
#define RWXFR           3                               /* rw, transfer */
#define FILL            4                               /* fill buffer */
#define EMPTY           5                               /* empty buffer */
#define CMD_COMPLETE    6                               /* set done next */
#define INIT_COMPLETE   7                               /* init compl next */

#define RXCS_V_FUNC     1                               /* function */
#define RXCS_M_FUNC     7
#define  RXCS_FILL      0                               /* fill buffer */
#define  RXCS_EMPTY     1                               /* empty buffer */
#define  RXCS_WRITE     2                               /* write sector */
#define  RXCS_READ      3                               /* read sector */
#define  RXCS_RXES      5                               /* read status */
#define  RXCS_WRDEL     6                               /* write del data */
#define  RXCS_ECODE     7                               /* read error code */
#define RXCS_V_DRV      4                               /* drive select */
#define RXCS_V_DONE     5                               /* done */
#define RXCS_V_IE       6                               /* intr enable */
#define RXCS_V_TR       7                               /* xfer request */
#define RXCS_V_INIT     14                              /* init */
#define RXCS_V_ERR      15                              /* error */
#define RXCS_FUNC       (RXCS_M_FUNC << RXCS_V_FUNC)
#define RXCS_DRV        (1u << RXCS_V_DRV)
#define RXCS_DONE       (1u << RXCS_V_DONE)
#define RXCS_IE         (1u << RXCS_V_IE)
#define RXCS_TR         (1u << RXCS_V_TR)
#define RXCS_INIT       (1u << RXCS_V_INIT)
#define RXCS_ERR        (1u << RXCS_V_ERR)
#define RXCS_ROUT       (RXCS_ERR+RXCS_TR+RXCS_IE+RXCS_DONE)
#define RXCS_IMP        (RXCS_ROUT+RXCS_DRV+RXCS_FUNC)
#define RXCS_RW         (RXCS_IE)                       /* read/write */
#define RXCS_GETFNC(x)  (((x) >> RXCS_V_FUNC) & RXCS_M_FUNC)

#define RXES_CRC        0001                            /* CRC error */
#define RXES_PAR        0002                            /* parity error */
#define RXES_ID         0004                            /* init done */
#define RXES_WLK        0010                            /* write protect */
#define RXES_DD         0100                            /* deleted data */
#define RXES_DRDY       0200                            /* drive ready */

#define TRACK u3                                        /* current track */
#define CALC_DA(t,s) (((t) * RX_NUMSC) + ((s) - 1)) * RX_NUMBY

int32 rx_csr = 0;                                       /* control/status */
int32 rx_dbr = 0;                                       /* data buffer */
int32 rx_esr = 0;                                       /* error status */
int32 rx_ecode = 0;                                     /* error code */
int32 rx_track = 0;                                     /* desired track */
int32 rx_sector = 0;                                    /* desired sector */
int32 rx_state = IDLE;                                  /* controller state */
int32 rx_stopioe = 1;                                   /* stop on error */
int32 rx_cwait = 100;                                   /* command time */
int32 rx_swait = 10;                                    /* seek, per track */
int32 rx_xwait = 1;                                     /* tr set time */
uint8 rx_buf[RX_NUMBY] = { 0 };                         /* sector buffer */
int32 rx_bptr = 0;                                      /* buffer pointer */
int32 rx_enb = 1;                                       /* device enable */

/* Sound realism mode (SET SOUND ENABLED): the drive mechanics of each
   RX01 drive (sim_sound SND_FLOPPY) and their adjustable times, shared by
   both drives (registers STEP, SETTLE, HDLOAD, HDUNLOAD, MOTORUP). */

static SND_FLOPPY rx_flp[RX_NUMDR];
static SND_FLP_REGS rx_flp_regs;
static t_bool rx_flp_inited = FALSE;

#define RX_REAL         (sim_sound_enabled ())
#define RX_BYTE_US      18.0                            /* RX01 <-> RX11: 18 us per byte */
#define RXDEB_OPS       0001                            /* debug: commands with their times */
#define RXDEB_CMD       0002                            /* debug: function starts and ends */
#define RXDEB_REG       0004                            /* debug: register accesses */

DEBTAB rx_deb[] = {
    { "OPS",  RXDEB_OPS,  "commands and their realistic times (realism mode)" },
    { "CMD",  RXDEB_CMD,  "function starts, INIT and completions with status (both modes)" },
    { "REG",  RXDEB_REG,  "register reads and writes (both modes)" },
    { NULL, 0 }
    };

t_stat rx_rd (int32 *data, int32 PA, int32 access);
t_stat rx_wr (int32 data, int32 PA, int32 access);
t_stat rx_svc (UNIT *uptr);
t_stat rx_reset (DEVICE *dptr);
t_stat rx_boot (int32 unitno, DEVICE *dptr);
void rx_done (int32 esr_flags, int32 new_ecode);
t_stat rx_attach (UNIT *uptr, CONST char *cptr);
t_stat rx_detach (UNIT *uptr);
t_stat rx_show_drive (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat rx_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *rx_description (DEVICE *dptr);
static void rx_xfer (UNIT *uptr);
static double rx_rw_us (UNIT *uptr);
static double rx_init_us (void);
static double rx_status_us (int32 drv);

/* RX11 data structures

   rx_dev       RX device descriptor
   rx_unit      RX unit list
   rx_reg       RX register list
   rx_mod       RX modifier list
*/

#define IOLN_RX         004

DIB rx_dib = {
    IOBA_AUTO, IOLN_RX, &rx_rd, &rx_wr,
    1, IVCL (RX), VEC_AUTO, { NULL }, IOLN_RX,
    };

UNIT rx_unit[] = {
    { UDATA (&rx_svc,
             UNIT_FIX+UNIT_ATTABLE+UNIT_BUFABLE+UNIT_MUSTBUF, RX_SIZE) },
    { UDATA (&rx_svc,
             UNIT_FIX+UNIT_ATTABLE+UNIT_BUFABLE+UNIT_MUSTBUF, RX_SIZE) }
    };

REG rx_reg[] = {
    { ORDATA (RXCS, rx_csr, 16) },
    { ORDATA (RXDB, rx_dbr, 8) },
    { ORDATA (RXES, rx_esr, 8) },
    { ORDATA (RXERR, rx_ecode, 8) },
    { ORDATA (RXTA, rx_track, 8) },
    { ORDATA (RXSA, rx_sector, 8) },
    { DRDATA (STAPTR, rx_state, 3), REG_RO },
    { DRDATA (BUFPTR, rx_bptr, 7)  },
    { FLDATA (INT, IREQ (RX), INT_V_RX) },
    { FLDATA (ERR, rx_csr, RXCS_V_ERR) },
    { FLDATA (TR, rx_csr, RXCS_V_TR) },
    { FLDATA (IE, rx_csr, RXCS_V_IE) },
    { FLDATA (DONE, rx_csr, RXCS_V_DONE) },
    { DRDATA (CTIME, rx_cwait, 24), PV_LEFT },
    { DRDATA (STIME, rx_swait, 24), PV_LEFT },
    { DRDATA (XTIME, rx_xwait, 24), PV_LEFT },
    { FLDATA (STOP_IOE, rx_stopioe, 0) },
    { BRDATA (SBUF, rx_buf, 8, 8, RX_NUMBY) },
    { DRDATAD (STEP,  rx_flp_regs.step_us,     24,      "realism mode: step time per track (us)"), PV_LEFT },
    { DRDATAD (SETTLE, rx_flp_regs.settle_us,  24,      "realism mode: head settle time (us)"), PV_LEFT },
    { DRDATAD (HDLOAD, rx_flp_regs.hdload_us,  24,      "realism mode: head load time (us)"), PV_LEFT },
    { DRDATAD (HDUNLOAD, rx_flp_regs.hdunload_ms, 24,   "realism mode: head unloaded after (ms) without a command"), PV_LEFT },
    { DRDATAD (MOTORUP, rx_flp_regs.motorup_ms, 24,     "realism mode: motor run-up at power on (ms)"), PV_LEFT },
    { ORDATA (DEVADDR, rx_dib.ba, 32), REG_HRO },
    { ORDATA (DEVVEC, rx_dib.vec, 16), REG_HRO },
    { NULL }
    };

MTAB rx_mod[] = {
    { MTAB_XTD|MTAB_VUN, 0, "write enabled", "WRITEENABLED", 
        &set_writelock, &show_writelock,   NULL, "Write enable floppy drive" },
    { MTAB_XTD|MTAB_VUN, 1, NULL, "LOCKED", 
        &set_writelock, NULL,   NULL, "Write lock floppy drive" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "DRIVE", NULL,
        NULL, &rx_show_drive, NULL, "Display the drive mechanics (sound realism mode)" },
#if defined (VM_PDP11)
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 004, "ADDRESS", "ADDRESS",
      &set_addr, &show_addr, NULL },
    { MTAB_XTD|MTAB_VDV, 0, NULL, "AUTOCONFIGURE",
      &set_addr_flt, NULL, NULL },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "VECTOR", "VECTOR",
      &set_vec, &show_vec, NULL },
#else
    { MTAB_XTD|MTAB_VDV, 004, "ADDRESS", NULL,
      NULL, &show_addr, NULL },
    { MTAB_XTD|MTAB_VDV, 0, "VECTOR", NULL,
      NULL, &show_vec, NULL },
#endif
    { 0 }
    };

DEVICE rx_dev = {
    "RX", rx_unit, rx_reg, rx_mod,
    RX_NUMDR, 8, 20, 1, 8, 8,
    NULL, NULL, &rx_reset,
    &rx_boot, &rx_attach, &rx_detach,
    &rx_dib, DEV_DISABLE | DEV_UBUS | DEV_QBUS | DEV_DEBUG, 0,
    rx_deb, NULL, NULL, &rx_help, NULL, NULL,
    &rx_description
    };

/* I/O dispatch routine, I/O addresses 17777170 - 17777172

   17777170             floppy CSR
   17777172             floppy data register
*/

t_stat rx_rd (int32 *data, int32 PA, int32 access)
{
switch ((PA >> 1) & 1) {                                /* decode PA<1> */

    case 0:                                             /* RXCS */
        rx_csr = rx_csr & RXCS_IMP;                     /* clear junk */
        *data = rx_csr & RXCS_ROUT;
        sim_debug (RXDEB_REG, &rx_dev, ">>RX rd RXCS %06o\n", *data);
        break;

    case 1:                                             /* RXDB */
        if ((rx_state == EMPTY) && (rx_csr & RXCS_TR)) {/* empty? */
            rx_xfer (&rx_unit[0]);
            rx_csr = rx_csr & ~RXCS_TR;                 /* clear xfer */
            }
        *data = rx_dbr;                                 /* return data */
        sim_debug (RXDEB_REG, &rx_dev, ">>RX rd RXDB %06o\n", *data);
        break;
        }                                               /* end switch PA */

return SCPE_OK;
}

t_stat rx_wr (int32 data, int32 PA, int32 access)
{
int32 drv;

switch ((PA >> 1) & 1) {                                /* decode PA<1> */

/* Writing RXCS, three cases:
   1. Writing INIT, reset device
   2. Idle and writing new function
        - clear error, done, transfer ready, int req
        - save int enable, function, drive
        - start new function
   3. Otherwise, write IE and update interrupts
*/

    case 0:                                             /* RXCS */
        rx_csr = rx_csr & RXCS_IMP;                     /* clear junk */
        if (access == WRITEB) data = (PA & 1)?          /* write byte? */
            (rx_csr & 0377) | (data << 8): (rx_csr & ~0377) | data;
        sim_debug (RXDEB_REG, &rx_dev, ">>RX wr RXCS %06o (state %d)\n", data, rx_state);
        if (data & RXCS_INIT) {                         /* initialize? */
            sim_debug (RXDEB_CMD, &rx_dev, ">>RX INIT (state was %d)\n", rx_state);
            rx_reset (&rx_dev);                         /* reset device */
            return SCPE_OK;                             /* end if init */
            }
        if ((data & CSR_GO) && (rx_state != IDLE))
            sim_debug (RXDEB_CMD, &rx_dev, ">>RX GO %06o ignored: busy (state %d)\n", data, rx_state);
        if ((data & CSR_GO) && (rx_state == IDLE)) {    /* new function? */
            rx_csr = data & (RXCS_IE + RXCS_DRV + RXCS_FUNC);
            drv = ((rx_csr & RXCS_DRV)? 1: 0);          /* reselect drive */
            rx_bptr = 0;                                /* clear buf pointer */
            sim_debug (RXDEB_CMD, &rx_dev, ">>RX%d function %d%s\n", drv, RXCS_GETFNC (data),
                       (data & RXCS_IE) ? ", IE" : "");
            switch (RXCS_GETFNC (data)) {               /* case on func */

            case RXCS_FILL:
                rx_state = FILL;                        /* state = fill */
                rx_csr = rx_csr | RXCS_TR;              /* xfer is ready */
                break;

            case RXCS_EMPTY:
                rx_state = EMPTY;                       /* state = empty */
                rx_xfer (&rx_unit[drv]);
                break;

            case RXCS_READ: case RXCS_WRITE: case RXCS_WRDEL:
                rx_state = RWDS;                        /* state = get sector */
                rx_csr = rx_csr | RXCS_TR;              /* xfer is ready */
                rx_esr = rx_esr & RXES_ID;              /* clear errors */
                break;

            default:
                rx_state = CMD_COMPLETE;                /* state = cmd compl */
                if (RX_REAL && (RXCS_GETFNC (data) == RXCS_RXES))
                    sim_sound_activate_us (&rx_unit[drv], rx_status_us (drv));
                else
                    sim_activate (&rx_unit[drv], rx_cwait);
                break;
                }                                       /* end switch func */
            return SCPE_OK;
            }                                           /* end if GO */
        if ((data & RXCS_IE) == 0)
            CLR_INT (RX);
        else if ((rx_csr & (RXCS_DONE + RXCS_IE)) == RXCS_DONE)
            SET_INT (RX);
        rx_csr = (rx_csr & ~RXCS_RW) | (data & RXCS_RW);
        break;                                          /* end case RXCS */

/* Accessing RXDB, two cases:
   1. Write idle, write
   2. Write not idle and TR set, state dependent
*/

    case 1:                                             /* RXDB */
        if ((PA & 1) || ((rx_state != IDLE) && ((rx_csr & RXCS_TR) == 0)))
            return SCPE_OK;                             /* if ~IDLE, need tr */
        rx_dbr = data & 0377;                           /* save data */
        sim_debug (RXDEB_REG, &rx_dev, ">>RX wr RXDB %06o (state %d)\n", data, rx_state);
        if ((rx_state != IDLE) && (rx_state != EMPTY)) {
            drv = ((rx_csr & RXCS_DRV)? 1: 0);          /* select drive */
            rx_xfer (&rx_unit[drv]);                    /* sched event */
            rx_csr = rx_csr & ~RXCS_TR;                 /* clear xfer */
            }
        break;                                          /* end case RXDB */
        }                                               /* end switch PA */

return SCPE_OK;
}

/* Unit service; the action to be taken depends on the transfer state:

   IDLE         Should never get here
   RWDS         Save sector, set TR, set RWDT
   RWDT         Save track, set RWXFR
   RWXFR        Read/write buffer
   FILL         copy ir to rx_buf[rx_bptr], advance ptr
                if rx_bptr > max, finish command, else set tr
   EMPTY        if rx_bptr > max, finish command, else
                copy rx_buf[rx_bptr] to ir, advance ptr, set tr
   CMD_COMPLETE copy requested data to ir, finish command
   INIT_COMPLETE read drive 0, track 1, sector 1 to buffer, finish command

   For RWDT and CMD_COMPLETE, the input argument is the selected drive;
   otherwise, it is drive 0.
*/

t_stat rx_svc (UNIT *uptr)
{
int32 i, func;
uint32 da;
int8 *fbuf = (int8 *) uptr->filebuf;

func = RXCS_GETFNC (rx_csr);                            /* get function */
switch (rx_state) {                                     /* case on state */

    case IDLE:                                          /* idle */
        return SCPE_IERR;                               /* done */

    case EMPTY:                                         /* empty buffer */
        if (rx_bptr >= RX_NUMBY)                        /* done all? */
            rx_done (0, 0);
        else {
            rx_dbr = rx_buf[rx_bptr];                   /* get next */
            rx_bptr = rx_bptr + 1;
            rx_csr = rx_csr | RXCS_TR;                  /* set xfer */
            }
        break;

    case FILL:                                          /* fill buffer */
        rx_buf[rx_bptr] = (uint8)rx_dbr;                /* write next */
        rx_bptr = rx_bptr + 1;
        if (rx_bptr < RX_NUMBY)                         /* more? set xfer */
            rx_csr = rx_csr | RXCS_TR;
        else rx_done (0, 0);                            /* else done */
        break;

    case RWDS:                                          /* wait for sector */
        rx_sector = rx_dbr & RX_M_SECTOR;               /* save sector */
        rx_csr = rx_csr | RXCS_TR;                      /* set xfer */
        rx_state = RWDT;                                /* advance state */
        return SCPE_OK;

    case RWDT:                                          /* wait for track */
        rx_track = rx_dbr & RX_M_TRACK;                 /* save track */
        rx_state = RWXFR;
        if (RX_REAL)
            sim_sound_activate_us (uptr, rx_rw_us (uptr));
        else
            sim_activate (uptr,                         /* sched done */
                    rx_swait * abs (rx_track - uptr->TRACK));
        return SCPE_OK;

    case RWXFR:
        if ((uptr->flags & UNIT_BUF) == 0) {            /* not buffered? */
            rx_done (0, 0110);                          /* done, error */
            if (RX_REAL)                                /* no diskette (or the door was */
                break;                                  /* opened): an error, as on the drive */
            return IORETURN (rx_stopioe, SCPE_UNATT);
            }
        if (rx_track >= RX_NUMTR) {                     /* bad track? */
            rx_done (0, 0040);                          /* done, error */
            break;
            }
        uptr->TRACK = rx_track;                         /* now on track */
        if ((rx_sector == 0) || (rx_sector > RX_NUMSC)) { /* bad sect? */
            rx_done (0, 0070);                          /* done, error */
            break;
            }
        da = CALC_DA (rx_track, rx_sector);             /* get disk address */
        if (func == RXCS_WRDEL)                         /* del data? */
            rx_esr = rx_esr | RXES_DD;
        if (func == RXCS_READ) {                        /* read? */
            for (i = 0; i < RX_NUMBY; i++)
                rx_buf[i] = fbuf[da + i];
            }
        else {
            if (uptr->flags & UNIT_WPRT) {              /* write and locked? */
                rx_done (RXES_WLK, 0100);               /* done, error */
                break;
                }
            for (i = 0; i < RX_NUMBY; i++)              /* write */
                fbuf[da + i] = rx_buf[i];
            da = da + RX_NUMBY;
            if (da > uptr->hwmark)
                uptr->hwmark = da;
            }
        rx_done (0, 0);                                 /* done */
        break;

    case CMD_COMPLETE:                                  /* command complete */
        if (func == RXCS_ECODE) {                       /* read ecode? */
            rx_dbr = rx_ecode;                          /* set dbr */
            rx_done (0, -1);                            /* don't update */
            }
        else rx_done (0, 0);
        break;

    case INIT_COMPLETE:                                 /* init complete */
        rx_unit[0].TRACK = 1;                           /* drive 0 to trk 1 */
        rx_unit[1].TRACK = 0;                           /* drive 1 to trk 0 */
        if ((rx_unit[0].flags & UNIT_BUF) == 0) {       /* not buffered? */
            rx_done (RXES_ID, 0010);                    /* init done, error */
            break;
            }
        da = CALC_DA (1, 1);                            /* track 1, sector 1 */
        for (i = 0; i < RX_NUMBY; i++)                  /* read sector */
            rx_buf[i] = fbuf[da + i];
        rx_done (RXES_ID, 0);                           /* set done */
        if ((rx_unit[1].flags & UNIT_ATT) == 0)
            rx_ecode = 0020;
        break;
        }                                               /* end case state */

return SCPE_OK;
}

/* Command complete.  Set done and put final value in interface register,
   request interrupt if needed, return to IDLE state.
*/

void rx_done (int32 esr_flags, int32 new_ecode)
{
int32 drv = (rx_csr & RXCS_DRV)? 1: 0;

rx_state = IDLE;                                        /* now idle */
rx_csr = rx_csr | RXCS_DONE;                            /* set done */
if (rx_csr & RXCS_IE) SET_INT (RX);                     /* if ie, intr */
rx_esr = (rx_esr | esr_flags) & ~RXES_DRDY;
if (rx_unit[drv].flags & UNIT_ATT)
    rx_esr = rx_esr | RXES_DRDY;
if (new_ecode > 0)                                      /* test for error */
    rx_csr = rx_csr | RXCS_ERR;
if (new_ecode < 0) {                                    /* don't update? */
    sim_debug (RXDEB_CMD, &rx_dev, ">>RX%d done: RXCS %06o error code %04o returned\n",
               drv, rx_csr, rx_ecode);
    if (RX_REAL)
        sim_floppy_end (&rx_flp[drv], 0.0);
    return;
    }
rx_ecode = new_ecode;                                   /* update ecode */
rx_dbr = rx_esr;                                        /* update RXDB */
sim_debug (RXDEB_CMD, &rx_dev, ">>RX%d done: RXCS %06o RXES %03o error code %04o, track %d sector %d\n",
           drv, rx_csr, rx_esr, rx_ecode, rx_track, rx_sector);
if (RX_REAL)                                            /* the head unloads if no new */
    sim_floppy_end (&rx_flp[drv], 0.0);                 /* command comes within HDUNLOAD */
return;
}

/* Device initialization.  The RX is one of the few devices that schedules
   an I/O transfer as part of its initialization.
*/

t_stat rx_reset (DEVICE *dptr)
{
int32 i;

if (!rx_flp_inited) {                                   /* drive mechanics, once */
    rx_flp_inited = TRUE;
    sim_floppy_set_defaults (&rx_flp_regs, &sim_flp_rx01);
    }
for (i = 0; i < RX_NUMDR; i++)
    sim_floppy_init (&rx_flp[i], &rx_unit[i], &sim_flp_rx01, &rx_flp_regs, SND_GRP_DISK, 1.0);
rx_csr = rx_dbr = 0;                                    /* clear regs */
rx_esr = rx_ecode = 0;                                  /* clear error */
rx_track = rx_sector = 0;                               /* clear addr */
rx_state = IDLE;                                        /* ctrl idle */
CLR_INT (RX);                                           /* clear int req */
sim_cancel (&rx_unit[1]);                               /* cancel drive 1 */
if (dptr->flags & DEV_DIS)                              /* disabled? */
    sim_cancel (&rx_unit[0]);
else if (RX_REAL) {                                     /* realism mode: drive 1 home, */
    rx_state = INIT_COMPLETE;                           /* drive 0 reads track 1 sector 1 */
    sim_sound_activate_us (&rx_unit[0], rx_init_us ());
    }
else if (rx_unit[0].flags & UNIT_BUF)  {                /* attached? */
    rx_state = INIT_COMPLETE;                           /* yes, sched init */
    sim_activate (&rx_unit[0], rx_swait * abs (1 - rx_unit[0].TRACK));
    }
else rx_done (0, 0010);                                 /* no, error */
return auto_config (0, 0);                              /* run autoconfig */
}

/* Attach: inserting a diskette and closing the door */

t_stat rx_attach (UNIT *uptr, CONST char *cptr)
{
t_stat r = attach_unit (uptr, cptr);

if (r == SCPE_OK)
    sim_floppy_insert (&rx_flp[uptr - rx_unit]);        /* realism: door closed */
return r;
}

/* Detach: opening the door.  A command running on the drive ends with an
   error when it reaches the diskette (realism mode; in the normal mode
   STOP_IOE decides, as before). */

t_stat rx_detach (UNIT *uptr)
{
if (uptr->flags & UNIT_ATT)
    sim_floppy_remove (&rx_flp[uptr - rx_unit]);
return detach_unit (uptr);
}

/* Schedule the next byte of Fill/Empty Buffer (and the sector and track
   bytes of Read/Write Sector): XTIME instructions in the normal mode; in
   the realism mode 18 us, the RX01 <-> RX11 transfer of one byte
   (EK-RX01-MM-002 1.6: "CPU interface to I/O bus 18 us/8-bit byte") */

static void rx_xfer (UNIT *uptr)
{
if (RX_REAL)
    sim_sound_activate_us (uptr, RX_BYTE_US);
else
    sim_activate (uptr, rx_xwait);
}

/* Realistic times (sound realism mode), from the RX8/RX11 Floppy Disk
   System Maintenance Manual EK-RX01-MM-002:
   - Read/Write Sector: the other drive's head unloads (one head load line),
     the head steps 10 ms per track, is loaded and the microcode waits
     20 ms (STEPHD), then the sector comes round (360 rpm, 26 sectors evenly
     spaced after the index) and is read or written (6.41 ms); no diskette
     or a sector 0 or > 26: given up after two revolutions (0110/0070); a
     track > 76: refused at once (0040).
   - Initialize: drive 1 to track 0, drive 0 reads track 1 sector 1 (up to
     1.8 s).
   - Read Status: "between one and two diskette revolutions or about 250
     ms" (it checks drive ready by the index pulses).
   - The head unloads when no new command comes within 45.87 ms
     (HDUNLOAD 46). */

static double rx_rw_us (UNIT *uptr)
{
int32 drv = (int32)(uptr - rx_unit);
SND_FLOPPY *f = &rx_flp[drv];
double t;

sim_floppy_unload (&rx_flp[drv ^ 1]);                   /* one head load line */
if (rx_track >= RX_NUMTR) {                             /* refused before stepping */
    sim_debug (RXDEB_OPS, &rx_dev, ">>RX%d track %d refused\n", drv, rx_track);
    return 100.0;
    }
t = sim_floppy_seek (f, rx_track, TRUE);                /* steps, head load, 20 ms */
if (((uptr->flags & UNIT_BUF) == 0) ||                  /* no diskette or no such sector: */
    (rx_sector == 0) || (rx_sector > RX_NUMSC))         /* two revolutions */
    t += 2.0 * sim_floppy_rev_us (f);
else
    t = sim_floppy_rotate (f, t, rx_sector - 1, 1);
sim_debug (RXDEB_OPS, &rx_dev, ">>RX%d %s track %d sector %d: %.2f ms\n", drv,
           (RXCS_GETFNC (rx_csr) == RXCS_READ) ? "read" : "write", rx_track, rx_sector, t / 1000.0);
return t;
}

static double rx_init_us (void)
{
SND_FLOPPY *f0 = &rx_flp[0];
double t0, t1, t;

sim_floppy_unload (&rx_flp[0]);
sim_floppy_unload (&rx_flp[1]);
t0 = sim_floppy_seek (&rx_flp[0], 0, FALSE);            /* both heads home */
t1 = sim_floppy_seek (&rx_flp[1], 0, FALSE);
t = (t0 > t1) ? t0 : t1;
t += sim_floppy_seek (f0, 1, TRUE);                     /* drive 0: track 1 */
if (rx_unit[0].flags & UNIT_BUF)
    t = sim_floppy_rotate (f0, t, 0, 1);                /* sector 1 */
else
    t += 2.0 * sim_floppy_rev_us (f0);                  /* no diskette: no sector */
sim_debug (RXDEB_OPS, &rx_dev, ">>RX initialize: %.2f ms\n", t / 1000.0);
return t;
}

static double rx_status_us (int32 drv)
{
SND_FLOPPY *f = &rx_flp[drv];
double t = sim_floppy_rotate (f, 0.0, 0, RX_NUMSC);     /* to the index, then once round */

sim_debug (RXDEB_OPS, &rx_dev, ">>RX%d read status: %.1f ms\n", drv, t / 1000.0);
return t;
}

t_stat rx_show_drive (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
return sim_floppy_show (st, &rx_flp[uptr - rx_unit]);
}

const char *rx_description (DEVICE *dptr)
{
return "RX11/RX01 floppy disk controller";
}

t_stat rx_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
fprintf (st, "RX11/RX01 Floppy Disk (RX)\n\n");
fprintf (st, "The RX device simulates the RX11 controller (RXV11 on the Qbus) with two\n");
fprintf (st, "RX01 drives, single density only: 77 tracks of 26 sectors of 128 bytes\n");
fprintf (st, "(256,256 bytes per diskette).  The RX occupies the standard floppy address\n");
fprintf (st, "17777170 shared with the RY (RX211/RXV21): enable only one of them, or move\n");
fprintf (st, "one with SET ADDRESS and SET VECTOR (an RT-11 boot block expects 17777170).\n\n");
fprint_set_help (st, dptr);
fprint_show_help (st, dptr);
fprint_reg_help (st, dptr);
fprintf (st, "\nError handling:\n\n");
fprintf (st, "    error         STOP_IOE   processed as\n");
fprintf (st, "    not attached  1          report error and stop\n");
fprintf (st, "                  0          disk not ready\n\n");
fprintf (st, "RX01 data files are buffered in memory; therefore, end of file and OS I/O\n");
fprintf (st, "errors cannot occur.\n\n");
fprintf (st, "Sound realism mode (SET SOUND ENABLED, see HELP SET SOUND):\n\n");
fprintf (st, "  - The RX01 drive motors are AC motors on the power line: they run while\n");
fprintf (st, "    the system power is on (RUN/BOOT, SET SOUND POWER=ON), with or without\n");
fprintf (st, "    a diskette; at power on the heads move ten tracks in and back.  ATTACH\n");
fprintf (st, "    is inserting a diskette and closing the door, DETACH opening the door;\n");
fprintf (st, "    a command running on the drive then ends with an error (it does not\n");
fprintf (st, "    stop the simulator).\n");
fprintf (st, "  - Times from the RX01 maintenance manual: 10 ms per track (STEP), then the\n");
fprintf (st, "    head is loaded and the drive waits 20 ms (HDLOAD, SETTLE), 360 rpm; a\n");
fprintf (st, "    sector is read or written when it comes round under the head, so the\n");
fprintf (st, "    interleave of the software has its real effect; the head unloads when\n");
fprintf (st, "    no new command comes within HDUNLOAD (46) ms.  Initialize (up to 1.8 s)\n");
fprintf (st, "    and Read Status (one to two revolutions) take their real time; Fill and\n");
fprintf (st, "    Empty Buffer 18 us per byte.\n");
fprintf (st, "  - Sounds: motor and fan, a diskette turning, head load and unload, one\n");
fprintf (st, "    click per track, the door (sound/rx01, else the RX02's sound/rx02 (the\n");
fprintf (st, "    same drive mechanics), else sound/default_floppy).\n");
fprintf (st, "  - SHOW RXn DRIVE shows diskette, head, track and motor; SET RX DEBUG=OPS\n");
fprintf (st, "    traces every command with its time, CMD and REG (both modes) the\n");
fprintf (st, "    functions and the register accesses.\n");
return SCPE_OK;
}

/* Device bootstrap */

#define BOOT_START      02000                           /* start */
#define BOOT_ENTRY      (BOOT_START + 002)              /* entry */
#define BOOT_UNIT       (BOOT_START + 010)              /* unit number */
#define BOOT_CSR        (BOOT_START + 026)              /* CSR */
#define BOOT_LEN        (sizeof (boot_rom) / sizeof (int16))

static const uint16 boot_rom[] = {
    042130,                         /* "XD" */
    0012706, BOOT_START,            /* MOV #boot_start, SP */
    0012700, 0000000,               /* MOV #unit, R0        ; unit number */
    0010003,                        /* MOV R0, R3 */
    0006303,                        /* ASL R3 */
    0006303,                        /* ASL R3 */
    0006303,                        /* ASL R3 */
    0006303,                        /* ASL R3 */
    0012701, 0177170,               /* MOV #RXCS, R1        ; csr */
    0032711, 0000040,               /* BITB #40, (R1)       ; ready? */
    0001775,                        /* BEQ .-4 */
    0052703, 0000007,               /* BIS #READ+GO, R3 */
    0010311,                        /* MOV R3, (R1)         ; read & go */
    0105711,                        /* TSTB (R1)            ; xfr ready? */
    0100376,                        /* BPL .-2 */
    0012761, 0000001, 0000002,      /* MOV #1, 2(R1)        ; sector */
    0105711,                        /* TSTB (R1)            ; xfr ready? */
    0100376,                        /* BPL .-2 */
    0012761, 0000001, 0000002,      /* MOV #1, 2(R1)        ; track */
    0005003,                        /* CLR R3 */
    0032711, 0000040,               /* BITB #40, (R1)       ; ready? */
    0001775,                        /* BEQ .-4 */
    0012711, 0000003,               /* MOV #EMPTY+GO, (R1)  ; empty & go */
    0105711,                        /* TSTB (R1)            ; xfr, done? */
    0001776,                        /* BEQ .-2 */
    0100003,                        /* BPL .+010 */
    0116123, 0000002,               /* MOVB 2(R1), (R3)+    ; move byte */
    0000772,                        /* BR .-012 */
    0005002,                        /* CLR R2 */
    0005003,                        /* CLR R3 */
    0012704, BOOT_START+020,        /* MOV #START+20, R4 */
    0005005,                        /* CLR R5 */
    0005007                         /* CLR R7 */
    };

t_stat rx_boot (int32 unitno, DEVICE *dptr)
{
size_t i;

for (i = 0; i < BOOT_LEN; i++)
    WrMemW (BOOT_START + (2 * i), boot_rom[i]);
WrMemW (BOOT_UNIT, unitno & RX_M_NUMDR);
WrMemW (BOOT_CSR, rx_dib.ba & DMASK);
cpu_set_boot (BOOT_ENTRY);
return SCPE_OK;
}
