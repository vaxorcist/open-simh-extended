/* vax_dmb.c: DMB32 VAXBI asynchronous/synchronous multiplexer and printer port

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
   THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   dmb          DMB32 (T1012) - VAXBI eight-line asynchronous multiplexer,
                synchronous line and line printer port

   References:
     EK-DMB32-UG-001  DMB32 User Guide (registers: chapter 3)
     EK-DMB32-TD-001  DMB32 Technical Description (DMA address translation,
                      interrupt vectors: chapter 2)
     $DMBDEF          VMS V4.7 register definitions ("BICOMBO")

   The DMB32 is the VAXBI relative of the DMF32: the same three functions
   (no parallel interface) on one BI node, controlled by an on-board 68000.
   The host sees the node's 8 Kbyte register space:

     base+000  BIIC registers: DTYPE (0109), BICSR, BER, EINTRCSR, INTRDES,
               IPINTR registers, SADR/EADR, UINTRCSR; base+0F0 GPR0 (self-test
               summary)
     base+100  MAINT, ACSR, SCSR, PCSR, CONFIG, ACSR2, SCSR2, PCSR2
     base+150  SPTE, SPTS, GPTE, GPTS (page tables for DMA translation)
     base+160  printer registers
     base+180  sync line registers (indexed by SCSR<7:0>)
     base+1C0  async line registers (indexed by ACSR<7:0>)
     base+204  TBUF, SBUF, RBUF completion and receive FIFOs: a read does
               not remove the entry (VAXBI rule), a write does
     base+218  on-board RAM, visible only in maintenance mode

   Interrupts go out at BR4 (IPL 14) with a vector the board supplies itself:
   100 + type*40 + node*4, type 0 = BIIC error, 1 = sync, 2 = async,
   3 = printer.

   DMA: the board walks the VAX page tables itself (TD 2.2.2).  A buffer is
   given as a physical address, the physical address of its page table, a
   system virtual address, or the system virtual address of its process page
   table, plus a byte offset within the first page for the two page table
   forms.

   This file has the BI node, the self-test, the async lines and the DMA
   engine.  The printer port and the synchronous line use the shared
   printer formatter and DDCMP engine (vax_combo_lib.c, with the DMF32).

   Modification history:

   15-Nov-26    Ulli/Claude  First version (BI node, self-test, async lines)
*/

#if !defined (VAX_820)
#error "DMB32 is only supported on the VAX 8200/8300 (VAXBI)"
#endif

#include "vax_defs.h"
#include "vax_bi.h"
#include "sim_tmxr.h"
#include "vax_combo_lib.h"                     /* printer formatter (with DMF32) */

/* ---------------------------------------------------------------------------
   Sizes
   --------------------------------------------------------------------------- */

#define DMB_LINES       8                       /* asynchronous lines */
#define DMB_RXFIFO      256                     /* received character FIFO */
#define DMB_RXALARM     ((DMB_RXFIFO * 3) / 4)  /* "critically full": XOFF, interrupt */
#define DMB_RXEASED     (DMB_RXFIFO / 4)        /* congestion over: XON */
#define DMB_TXFIFO      32                      /* transmit completion FIFO */
#define DMB_SFIFO       16                      /* sync completion FIFO */
#define DMB_NODESPACE   0x2000                  /* node space size */
#define DMB_RAM_BASE    0x218                   /* on-board RAM (maintenance) */
#define DMB_RAM_LONGS   ((DMB_NODESPACE - DMB_RAM_BASE) / 4)
#define DMB_DEF_NODE    5                       /* default BI node (decision D2) */
#define DMB_DEF_STMS    50                      /* default self-test time, ms (D4) */
#define DMB_DTYPE       0x0109                  /* device type */
#define DMB_REVCODE     0x0001                  /* revision (DTYPE<31:16>) */

/* ---------------------------------------------------------------------------
   Register offsets (bytes from the node base)
   --------------------------------------------------------------------------- */

#define OF_DTYPE        0x000                   /* BIIC */
#define OF_BICSR        0x004
#define OF_BER          0x008
#define OF_EICR         0x00C
#define OF_IDEST        0x010
#define OF_IMSK         0x014
#define OF_FIDEST       0x018
#define OF_ISRC         0x01C
#define OF_SADR         0x020
#define OF_EADR         0x024
#define OF_BCICSR       0x028
#define OF_WSTAT        0x02C
#define OF_FIPSCMD      0x030
#define OF_UINTRCSR     0x040
#define OF_GPR0         0x0F0
#define OF_GPR3         0x0FC
#define OF_MAINT        0x100                   /* DMB32 (CASRAM) */
#define OF_ACSR         0x104
#define OF_SCSR         0x108
#define OF_PCSR         0x10C
#define OF_CONFIG       0x114
#define OF_ACSR2        0x118
#define OF_SCSR2        0x11C
#define OF_PCSR2        0x120
#define OF_SPTE         0x150
#define OF_SPTS         0x154
#define OF_GPTE         0x158
#define OF_GPTS         0x15C
#define OF_PFIX         0x160                   /* printer */
#define OF_PBUFFAD      0x164
#define OF_PBUFFCT      0x168
#define OF_PCTRL        0x16C
#define OF_PCAR         0x170
#define OF_PSIZE        0x174
#define OF_SYNC         0x180                   /* sync, 0x180-0x1BC, indexed */
#define OF_SYNC_END     0x1BC
#define OF_PREEMPT      0x1C0                   /* async, indexed by ACSR<7:0> */
#define OF_TBUFFAD      0x1C4
#define OF_TBUFFCT      0x1C8
#define OF_LPR          0x1CC
#define OF_LNCTRL       0x1D0
#define OF_LSTAT        0x1D4
#define OF_FLOWC        0x1D8
#define OF_RXCD         0x200                   /* console register: NO ACK */
#define OF_TBUF         0x204                   /* FIFOs */
#define OF_SBUF         0x208
#define OF_RBUF         0x20C

/* sync registers: index of the longword from OF_SYNC */
#define SY_TBUFFAD1     0
#define SY_TBUFFCT1     1
#define SY_RBUFFAD1     2
#define SY_RBUFFCT1     3
#define SY_TLNCTRL1     4
#define SY_RLNCTRL1     5
#define SY_LPR1         6
#define SY_LPR2         7
#define SY_TBUFFAD2     8
#define SY_TBUFFCT2     9
#define SY_RBUFFAD2     10
#define SY_RBUFFCT2     11
#define SY_TLNCTRL2     12
#define SY_RLNCTRL2     13
#define SY_LPR3         14
#define SY_BUFCTRL      15
#define SY_NREGS        16

/* ---------------------------------------------------------------------------
   Register bits (User Guide chapter 3, cross-checked with VMS $DMBDEF)
   --------------------------------------------------------------------------- */

/* MAINT (3.3.10) */
#define MT_FORCE_FAIL   0x00000001              /* act as if self-test failed */
#define MT_PROG_RESET   0x00000002              /* programmed reset (set while busy) */
#define MT_PTE_VALID    0x00000004              /* page table registers loaded */
#define MT_SKIP_ST      0x00000008              /* skip self-test */
#define MT_V_LEVEL      4                       /* maintenance level */
#define MT_LEVEL        0x00000030
#define MT_SYNC         0x00000100              /* sync line present (ro) */
#define MT_ASYNC        0x00000200              /* async lines present (ro) */
#define MT_PRINT        0x00000400              /* printer present (ro) */
#define MT_DIAG_FAIL    0x00000800              /* self-test failed (ro) */
#define MT_CABLE_KEY    0x00002000              /* cable key present (ro) */
#define MT_RW           (MT_FORCE_FAIL|MT_PROG_RESET|MT_PTE_VALID|MT_SKIP_ST|MT_LEVEL)
#define MT_PRESENT      (MT_SYNC|MT_ASYNC|MT_PRINT|MT_CABLE_KEY)

/* ACSR, SCSR, PCSR (3.3.11 - 3.3.13) */
#define CS_IND          0x000000FF              /* channel index (ACSR, SCSR) */
#define AC_RXIE         0x00000100              /* async receive interrupt enable */
#define AC_TXIE         0x00000200              /* async transmit interrupt enable */
#define AC_RW           (CS_IND|AC_RXIE|AC_TXIE)
#define SC_IE           0x00000800              /* sync interrupt enable */
#define SC_RW           (CS_IND|SC_IE)
#define PC_IE           0x00000800              /* printer interrupt enable */
#define PC_DAVFU_RDY    0x00010000              /* DAVFU ready (ro) */
#define PC_CONN_VERIFY  0x00020000              /* printer connected (ro) */
#define PC_OFFLINE      0x00040000              /* printer offline (ro) */
#define PC_RW           (PC_IE)

/* ACSR2, SCSR2, PCSR2 (3.3.15 - 3.3.17) */
#define C2_RESET        0x00000400              /* reset this function */
#define AC2_V_RXTIMER   16                      /* receive interrupt delay, ms */
#define AC2_RXTIMER     0x00FF0000
#define AC2_RW          (C2_RESET|AC2_RXTIMER)

/* async LPR (3.3.47) */
#define LP_DTR          0x00000002              /* data terminal ready */
#define LP_RTS          0x00000010              /* request to send */
#define LP_TXINTDLY     0x00000200              /* interrupt after the last character */
#define LP_RXENA        0x00000400              /* receiver enable */
#define LP_BREAK        0x00000800              /* send break */
#define LP_V_MAINT      12                      /* maintenance mode */
#define LP_MAINT        0x00003000
#define  MAINT_NORMAL    0
#define  MAINT_ECHO      1                      /* automatic echo */
#define  MAINT_LOCAL     2                      /* local loopback */
#define  MAINT_REMOTE    3                      /* remote loopback */
#define LP_RPTMODEM     0x00004000              /* report modem changes in RBUF */
#define LP_DISCFLOW     0x00008000              /* do not store received XON/XOFF */
#define LP_V_CLEN       16                      /* character length - 5 */
#define LP_PARENB       0x00040000              /* parity enable */
#define LP_EVEN         0x00080000              /* even parity */
#define LP_STOP2        0x00100000              /* two stop bits */
#define LP_USECTS       0x00200000              /* CTS controls output */
#define LP_IAUTO        0x00400000              /* send XOFF/XON when the FIFO fills */
#define LP_OAUTO        0x00800000              /* obey received XOFF/XON */
#define LP_V_RSPD       24                      /* receive speed */
#define LP_V_TSPD       28                      /* transmit speed */
#define LP_INIT         0xDD030000              /* 9600 baud, 8 bits, no parity */

/* async LNCTRL (3.3.48) and sync TLNCTRLn/RLNCTRLn, PCTRL: DMA control */
#define DC_START        0x00000001              /* start a DMA transfer */
#define DC_PTE          0x00000002              /* address is a PTE address */
#define DC_PHYS         0x00000004              /* address is physical */
#define DC_ABORT        0x00000100              /* abort the transfer */
#define DC_V_ERR        16                      /* error code */
#define DC_ERR          0x00FF0000
#define LN_RW           (DC_START|DC_PTE|DC_PHYS|DC_ABORT|DC_ERR)

/* printer PCTRL (3.3.25): DMA control as above, plus */
#define PR_FORMAT       0x00000200              /* formatting on */
#define PR_TAB          0x01000000              /* expand tabs */
#define PR_TRUNC        0x02000000              /* truncate long lines */
#define PR_AUTOCR       0x04000000              /* automatic CR insertion */
#define PR_AUTOFF       0x08000000              /* form feed to line feeds */
#define PR_NONPRINT     0x10000000              /* accept non printing */
#define PR_DAVFU        0x20000000              /* DAVFU codes */
#define PR_WRAP         0x40000000              /* line wrap */
#define PR_UPPER        0x80000000              /* to upper case */
#define PR_RW           (LN_RW|PR_FORMAT|0xFF000000)

/* buffer count/offset registers (TBUFFCT and friends) */
#define CT_OFF          0x000001FF              /* offset in the first page */
#define CT_V_COUNT      16                      /* character count */

/* async LSTAT (3.3.49) */
#define LS_TI           0x00000400              /* test indicator (ML2) */
#define LS_CTS          0x00001000              /* clear to send */
#define LS_DCD          0x00002000              /* carrier detect */
#define LS_RI           0x00004000              /* ring indicator */
#define LS_DSR          0x00008000              /* data set ready */
#define LS_MODEM        (LS_TI|LS_CTS|LS_DCD|LS_RI|LS_DSR)
#define LS_SNDOFF       0x00800000              /* send XOFF */
#define LS_TXENA        0x80000000              /* transmitter enable */
#define LS_RW           (LS_SNDOFF|LS_TXENA)

/* FLOWC (3.3.50): sent XOFF, sent XON, received XOFF, received XON */
#define FLOWC_INIT      0x11131113

/* PREEMPT (3.3.44) */
#define PE_CHAR         0x000000FF
#define PE_GO           0x00008000

/* TBUF (3.3.51) */
#define TB_PREEMPT      0x00000100              /* preempt character sent */
#define TB_V_ERR        16
#define TB_ACT          0x80000000              /* entry valid */

/* SBUF (3.3.52) */
#define SB_MODEM        0x00000100
#define SB_TXACT        0x00000200
#define SB_SECOND       0x00000400
#define SB_V_ERR        16
#define SB_ACT          0x80000000

/* RBUF (3.3.53) */
#define RB_PARERR       0x00001000
#define RB_FRMERR       0x00002000
#define RB_OVRERR       0x00004000
#define RB_NONCHAR      0x00008000              /* modem change or diagnostic */
#define RB_V_LINE       16
#define RB_VALID        0x80000000

/* error codes, bits <23:16> of the completion entries (3.4.8) */
#define ER_DMA          0x10                    /* + reason<3:2> + table<1:0> */
#define  ER_T_SYS        0                      /* a system PTE */
#define  ER_T_GBL        1                      /* a global PTE */
#define  ER_T_PRC        2                      /* a process PTE */
#define  ER_T_BUF        3                      /* the buffer itself */
#define  ER_R_NXM        (0 << 2)               /* memory did not respond */
#define  ER_R_INV        (2 << 2)               /* PTE not valid */
#define  ER_R_ACC        (3 << 2)               /* PTE does not allow the access */
#define ER_ABORT        0x61                    /* aborted by the host */
#define ER_NOPRINTER    0x81                    /* printer not connected */

/* BI interrupt vector types (TD 2.2.3) */
#define IT_ERR          0
#define IT_SYNC         1
#define IT_ASYNC        2
#define IT_PRINT        3
#define DMB_IPL         0x14                    /* BR4 */

/* VAX page table entries (TD 2.2.2.3) */
#define DPTE_V           0x80000000              /* valid */
#define DPTE_V_PROT      27
#define DPTE_TYP0        0x04000000              /* bit 26 */
#define DPTE_TYP1        0x00400000              /* bit 22 */
#define DPTE_PFN         0x001FFFFF
#define DPTE_GPTX        0x003FFFFF              /* global page table index */

/* debug flags */
#define DBG_REG         0x0001                  /* register accesses */
#define DBG_INT         0x0002                  /* interrupts */
#define DBG_DMA         0x0004                  /* DMA transfers, translation */
#define DBG_INIT        0x0008                  /* self-test, resets */
#define DBG_LPT         0x0010                  /* printer port */
#define DBG_SYNC        0x0020                  /* synchronous line */
#define DBG_XMT         TMXR_DBG_XMT
#define DBG_RCV         TMXR_DBG_RCV
#define DBG_MDM         TMXR_DBG_MDM
#define DBG_CON         TMXR_DBG_CON
#define DBG_TRC         TMXR_DBG_TRC

/* ---------------------------------------------------------------------------
   DMA descriptor: what the host wrote into the address, count/offset and
   control registers of one channel.  The DMA engine moves through it; the
   registers are updated from it when the transfer ends (the manual says
   they are undefined while DMA.START is set).
   --------------------------------------------------------------------------- */

typedef struct {
    uint32  addr;                               /* buffer or PTE address */
    uint32  off;                                /* offset in the first page (PTE modes) */
    uint32  count;                              /* characters left */
    uint32  ctrl;                               /* DMA.PTE, DMA.PHYS */
    uint32  pos;                                /* characters done */
    uint32  page;                               /* translated page (pos >> 9) + 1, 0 = none */
    uint32  pbase;                              /* its physical base */
    uint32  err_addr;                           /* address reported with an error */
    } DMB_DMA;

/* ---------------------------------------------------------------------------
   State
   --------------------------------------------------------------------------- */

typedef struct {
    uint32  lpr;                                /* line parameters */
    uint32  lnctrl;                             /* line control (DMA, abort, error) */
    uint32  lstat;                              /* line status (modem in, SNDOFF, TX.ENA) */
    uint32  flowc;                              /* flow control characters */
    uint32  preempt;                            /* preempt character and GO */
    uint32  tbuffad;                            /* DMA buffer address */
    uint32  tbuffct;                            /* DMA count and offset */
    DMB_DMA dma;                                /* transfer in progress */
    t_bool  dma_active;                         /* DMA.START seen, not yet reported */
    t_bool  xoff_sent;                          /* we sent XOFF (IAUTO, SNDOFF) */
    int32   flow_out;                           /* flow character to send, -1 = none */
    t_bool  overrun;                            /* characters lost: flag the next one */
    t_bool  rx_toggle;                          /* IAUTO: XOFF to every other character */
    } DMB_LINE;

static BIIC dmb_biic;                           /* BIIC registers */
static uint32 dmb_maint;
static uint32 dmb_acsr, dmb_scsr, dmb_pcsr;
static uint32 dmb_acsr2, dmb_scsr2, dmb_pcsr2;
static uint32 dmb_spte, dmb_spts, dmb_gpte, dmb_gpts;
static uint32 dmb_pfix, dmb_pbuffad, dmb_pbuffct, dmb_pctrl, dmb_pcar, dmb_psize;
static COMBO_LP dmb_lp;                         /* printer formatter state */
static DMB_DMA dmb_pdma;                        /* printer DMA in progress */
static t_bool dmb_p_active;                     /* printer DMA.START seen */
static uint32 dmb_sreg[SY_NREGS];               /* sync line registers */
static uint32 dmb_ram[DMB_RAM_LONGS];           /* on-board RAM (maintenance) */
static DMB_LINE dmb_line[DMB_LINES];

static uint32 dmb_rxfifo[DMB_RXFIFO];           /* received character FIFO */
static int32 dmb_rxcnt;
static uint32 dmb_txfifo[DMB_TXFIFO];           /* transmit completion FIFO */
static int32 dmb_txcnt;
static uint32 dmb_sfifo[DMB_SFIFO];             /* sync completion FIFO */
static int32 dmb_scnt;
static t_bool dmb_congested;                    /* RX FIFO critically full (IAUTO) */

/* Interrupts.  An event sets its condition (async receive, async
   transmit, sync, printer); a condition becomes a request of its vector
   type when its own enable bit is set (at once, or later when the host
   sets the bit: TD 2.2.3).  The acknowledge takes one request. */
#define CB_ARX          0                       /* condition bits */
#define CB_ATX          1
#define CB_SYNC         2
#define CB_PRINT        3
static uint32 dmb_cond;                         /* conditions waiting for their enable */
static uint32 dmb_req;                          /* requests, by vector type */
static t_bool dmb_tx_posted;                    /* TX interrupt given for this TBUF batch */
static t_bool dmb_s_posted;                     /* sync interrupt given for this batch */

static int32 dmb_st_ms = DMB_DEF_STMS;          /* self-test time, ms */
static t_bool dmb_testing;                      /* self-test running */
static int32 dmb_mctl;                          /* modem control (ATTACH -M) */

static TMLN dmb_ldsc[DMB_LINES];
static TMXR dmb_desc = { DMB_LINES, 0, 0, dmb_ldsc };

extern int32 tmxr_poll;
extern uint32 nexus_req[NEXUS_HLVL];

t_stat dmb_rd (int32 *val, int32 pa, int32 lnt);
t_stat dmb_wr (int32 val, int32 pa, int32 lnt);
int32 dmb_ack (void);
t_stat dmb_reset (DEVICE *dptr);
t_stat dmb_svc (UNIT *uptr);
t_stat dmb_lp_svc (UNIT *uptr);
t_stat dmb_sync_svc (UNIT *uptr);
t_stat dmb_st_svc (UNIT *uptr);
t_stat dmb_rxt_svc (UNIT *uptr);
t_stat dmb_attach (UNIT *uptr, CONST char *cptr);
t_stat dmb_detach (UNIT *uptr);
t_stat dmb_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *dmb_description (DEVICE *dptr);
static t_stat dmb_set_node (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat dmb_show_node (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
static t_stat dmb_set_st (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
static t_stat dmb_show_st (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
static void dmb_update_int (void);
static void dmb_start_selftest (const char *why);
static void dmb_async_init (t_bool power);
static void dmb_sync_init (void);
static void dmb_lp_init (void);
static void dmb_transmit (int32 ln);

/* ---------------------------------------------------------------------------
   Data structures
   --------------------------------------------------------------------------- */

/* ba = BI node number.  ack[0] supplies the vector (vax820_bi.c get_vector
   asks the device for nodes that have one). */
DIB dmb_dib = { DMB_DEF_NODE, 0, &dmb_rd, &dmb_wr, 0, 0, 0, { &dmb_ack } };

UNIT dmb_unit[5] = {
    { UDATA (&dmb_svc, UNIT_IDLE|UNIT_ATTABLE|TT_MODE_8B, 0) },       /* async lines */
    { UDATA (&dmb_lp_svc, UNIT_SEQ|UNIT_ATTABLE|UNIT_TEXT, 0), 200 },  /* printer */
    { UDATA (&dmb_sync_svc, UNIT_IDLE|UNIT_ATTABLE, 0) },              /* sync line */
    { UDATA (&dmb_st_svc, UNIT_DIS, 0) },                              /* self-test */
    { UDATA (&dmb_rxt_svc, UNIT_IDLE|UNIT_DIS, 0) }                    /* RX.TIMER */
    };

#define DMB_MUX_UNIT    (&dmb_unit[0])
#define DMB_LP_UNIT     (&dmb_unit[1])
#define DMB_SYNC_UNIT   (&dmb_unit[2])
#define DMB_ST_UNIT     (&dmb_unit[3])
#define DMB_RXT_UNIT    (&dmb_unit[4])

REG dmb_reg[] = {
    { HRDATAD (BICSR,   dmb_biic.csr,   32, "BI control and status") },
    { HRDATAD (BER,     dmb_biic.ber,   32, "BI bus error") },
    { HRDATAD (EINTRCSR, dmb_biic.eicr, 32, "BI error interrupt control") },
    { HRDATAD (INTRDES, dmb_biic.idest, 16, "BI interrupt destination") },
    { HRDATAD (UINTRCSR, dmb_biic.uiic, 32, "BI user interrupt control") },
    { HRDATAD (GPR0,    dmb_biic.gpr0,  32, "self-test summary") },
    { HRDATAD (MAINT,   dmb_maint,      32, "maintenance register") },
    { HRDATAD (ACSR,    dmb_acsr,       32, "async control and status") },
    { HRDATAD (SCSR,    dmb_scsr,       32, "sync control and status") },
    { HRDATAD (PCSR,    dmb_pcsr,       32, "printer control and status") },
    { HRDATAD (ACSR2,   dmb_acsr2,      32, "2nd async control and status") },
    { HRDATAD (SPTE,    dmb_spte,       32, "system page table, physical address") },
    { HRDATAD (SPTS,    dmb_spts,       32, "system page table size") },
    { HRDATAD (GPTE,    dmb_gpte,       32, "global page table, virtual address") },
    { HRDATAD (GPTS,    dmb_gpts,       32, "global page table size") },
    { HRDATAD (PCTRL,   dmb_pctrl,      32, "printer control") },
    { BRDATAD (SREG,    dmb_sreg, 16, 32, SY_NREGS, "sync line registers") },
    { DRDATAD (RXCNT,   dmb_rxcnt, 16, "received character FIFO entries") },
    { BRDATAD (RXFIFO,  dmb_rxfifo, 16, 32, DMB_RXFIFO, "received character FIFO") },
    { DRDATAD (TXCNT,   dmb_txcnt, 16, "transmit completion FIFO entries") },
    { BRDATAD (TXFIFO,  dmb_txfifo, 16, 32, DMB_TXFIFO, "transmit completion FIFO") },
    { DRDATAD (SCNT,    dmb_scnt, 16, "sync completion FIFO entries") },
    { HRDATAD (COND,    dmb_cond, 4, "interrupt conditions waiting for enable") },
    { HRDATAD (REQ,     dmb_req, 4, "interrupt requests") },
    { DRDATAD (STTIME,  dmb_st_ms, 16, "self-test time (ms)"), PV_LEFT },
    { FLDATAD (MDMCTL,  dmb_mctl, 0, "modem control enabled") },
    { NULL }
    };

static t_stat dmb_show_summ (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
return tmxr_show_summ (st, uptr, val, (void *) &dmb_desc);
}

MTAB dmb_mod[] = {
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "NODE", "NODE=n",
        &dmb_set_node, &dmb_show_node, NULL, "VAXBI node number (default 5)" },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "SELFTEST", "SELFTEST=ms",
        &dmb_set_st, &dmb_show_st, NULL, "Self-test time in milliseconds (default 50)" },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 1, NULL, "DISCONNECT",
        &tmxr_dscln, NULL, &dmb_desc, "Disconnect a specific line" },
    { UNIT_ATT, UNIT_ATT, "summary", NULL,
        NULL, &dmb_show_summ, NULL, "Display a summary of line states" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 1, "CONNECTIONS", NULL,
        NULL, &tmxr_show_cstat, (void *) &dmb_desc, "Display current connections" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "STATISTICS", NULL,
        NULL, &tmxr_show_cstat, (void *) &dmb_desc, "Display multiplexer statistics" },
    { 0 }
    };

DEBTAB dmb_debug[] = {
    { "REG",  DBG_REG,  "register accesses" },
    { "INT",  DBG_INT,  "interrupts" },
    { "DMA",  DBG_DMA,  "DMA transfers and address translation" },
    { "INIT", DBG_INIT, "self-test and resets" },
    { "LPT",  DBG_LPT,  "printer port" },
    { "SYNC", DBG_SYNC, "synchronous line" },
    { "XMT",  DBG_XMT,  "transmitted data" },
    { "RCV",  DBG_RCV,  "received data" },
    { "MDM",  DBG_MDM,  "modem signals" },
    { "CON",  DBG_CON,  "connection activities" },
    { "TRC",  DBG_TRC,  "trace routine calls" },
    { 0 }
    };

DEVICE dmb_dev = {
    "DMB", dmb_unit, dmb_reg, dmb_mod,
    5, 16, 16, 1, 16, 8,
    NULL, NULL, &dmb_reset,
    NULL, &dmb_attach, &dmb_detach,
    &dmb_dib, DEV_NEXUS | DEV_DISABLE | DEV_DIS | DEV_DEBUG | DEV_MUX,
    0, dmb_debug, NULL, NULL,
    &dmb_help, NULL, (void *)&dmb_desc,
    &dmb_description
    };

/* ---------------------------------------------------------------------------
   Interrupts

   The four vector types share the node's BR4 request bit in nexus_req.
   dmb_update_int turns conditions into requests where the enable bit is
   set and keeps the request bit in step; dmb_ack (called by get_vector)
   hands out one request per acknowledge.  The order is the one of TD 2.2.3:
   error, sync, async, printer.
   --------------------------------------------------------------------------- */

static int32 dmb_node (void)
{
return (int32)(dmb_dib.ba & 0xF);
}

/* Enable bit and vector type of each condition. */
static const struct { uint32 *csr; uint32 bit; int32 type; } dmb_cb[4] = {
    { &dmb_acsr, AC_RXIE, IT_ASYNC },           /* CB_ARX */
    { &dmb_acsr, AC_TXIE, IT_ASYNC },           /* CB_ATX */
    { &dmb_scsr, SC_IE,   IT_SYNC },            /* CB_SYNC */
    { &dmb_pcsr, PC_IE,   IT_PRINT }            /* CB_PRINT */
    };

static void dmb_update_int (void)
{
int32 c;

for (c = CB_ARX; c <= CB_PRINT; c++) {
    if ((dmb_cond & (1u << c)) && (*dmb_cb[c].csr & dmb_cb[c].bit)) {
        int32 t = dmb_cb[c].type;

        dmb_cond &= ~(1u << c);
        if (!(dmb_req & (1u << t)))
            sim_debug (DBG_INT, &dmb_dev, "request type %d (vector %03X)\n",
                       t, 0x100 | (t << 6) | (dmb_node () << 2));
        dmb_req |= (1u << t);
        }
    }
if (dmb_req)                                    /* to the primary; see INTRDES note */
    nexus_req[DMB_IPL - IPL_HMIN] |= (1u << dmb_node ());
else
    nexus_req[DMB_IPL - IPL_HMIN] &= ~(1u << dmb_node ());
}

/* An event: interrupt now if its enable bit is set, otherwise when it is. */
static void dmb_event (int32 c)
{
dmb_cond |= (1u << c);
dmb_update_int ();
}

int32 dmb_ack (void)
{
int32 t;

for (t = IT_ERR; t <= IT_PRINT; t++) {
    if (dmb_req & (1u << t)) {
        dmb_req &= ~(1u << t);
        dmb_update_int ();                      /* more requests keep the bit set */
        sim_debug (DBG_INT, &dmb_dev, "acknowledge type %d, vector %03X\n",
                   t, 0x100 | (t << 6) | (dmb_node () << 2));
        return 0x100 | (t << 6) | (dmb_node () << 2);
        }
    }
dmb_update_int ();
return 0;                                       /* passive release */
}

/* ---------------------------------------------------------------------------
   FIFOs.  A read returns the oldest entry with its valid bit, or 0; a write
   removes it.  A write to an empty FIFO is ignored (the manual forbids it).
   --------------------------------------------------------------------------- */

static void dmb_rx_check_timer (void);

/* Put an entry into the received character FIFO.  A character that goes
   into an empty FIFO starts the receive interrupt (after RX.TIMER); a
   modem change interrupts at once. */
static t_bool dmb_rx_put (uint32 entry, t_bool urgent)
{
t_bool was_empty = (dmb_rxcnt == 0);

if (dmb_rxcnt >= DMB_RXFIFO)
    return FALSE;
dmb_rxfifo[dmb_rxcnt++] = entry | RB_VALID;
if (urgent || (dmb_rxcnt >= DMB_RXALARM)) {     /* modem change, FIFO filling */
    sim_cancel (DMB_RXT_UNIT);
    dmb_event (CB_ARX);
    }
else if (was_empty)
    dmb_rx_check_timer ();
if (dmb_rxcnt >= DMB_RXALARM)
    dmb_congested = TRUE;
return TRUE;
}

/* First character in an empty FIFO: RX.TIMER 1 = interrupt now, 0 = only
   when 3/4 full or on a modem change, n = after n milliseconds. */
static void dmb_rx_check_timer (void)
{
int32 tmr = (dmb_acsr2 & AC2_RXTIMER) >> AC2_V_RXTIMER;

if (!(dmb_acsr & AC_RXIE))                      /* not enabled: when it is */
    dmb_cond |= (1u << CB_ARX);
else if (tmr == 1)
    dmb_event (CB_ARX);
else if ((tmr > 1) && !sim_is_active (DMB_RXT_UNIT))
    sim_activate_after (DMB_RXT_UNIT, tmr * 1000);
}

t_stat dmb_rxt_svc (UNIT *uptr)
{
if (dmb_rxcnt)
    dmb_event (CB_ARX);
return SCPE_OK;
}

static void dmb_rx_pop (void)
{
int32 ln;

if (dmb_rxcnt == 0)
    return;
memmove (&dmb_rxfifo[0], &dmb_rxfifo[1], (dmb_rxcnt - 1) * sizeof (uint32));
dmb_rxcnt--;
if (dmb_congested && (dmb_rxcnt <= DMB_RXEASED)) {  /* congestion over: XON */
    dmb_congested = FALSE;
    for (ln = 0; ln < DMB_LINES; ln++) {
        DMB_LINE *l = &dmb_line[ln];

        if (l->xoff_sent && !(l->lstat & LS_SNDOFF)) {
            l->flow_out = (l->flowc >> 8) & 0xFF;       /* sent XON character */
            l->xoff_sent = FALSE;
            dmb_transmit (ln);
            }
        }
    }
}

/* Transmit completion entry for line ln. */
static void dmb_tx_report (int32 ln, t_bool preempt, int32 err)
{
if (dmb_txcnt >= DMB_TXFIFO) {                  /* cannot happen: one per line */
    sim_debug (DBG_INT, &dmb_dev, "TBUF full, line %d report lost\n", ln);
    return;
    }
dmb_txfifo[dmb_txcnt++] = TB_ACT | (preempt ? TB_PREEMPT : 0) |
                          ((uint32)err << TB_V_ERR) | (uint32)ln;
sim_debug (DBG_INT, &dmb_dev, "TBUF: line %d %s, error %02X\n",
           ln, preempt ? "preempt done" : "DMA done", err);
if (!dmb_tx_posted) {                           /* one interrupt per batch: the */
    dmb_tx_posted = TRUE;                       /* next one after TBUF has emptied */
    dmb_event (CB_ATX);
    }
}

static void dmb_tx_pop (void)
{
if (dmb_txcnt == 0)
    return;
memmove (&dmb_txfifo[0], &dmb_txfifo[1], (dmb_txcnt - 1) * sizeof (uint32));
if (--dmb_txcnt == 0)
    dmb_tx_posted = FALSE;
}

/* Sync completion entry (used by the sync line). */
static void dmb_s_report (uint32 entry)
{
if (dmb_scnt >= DMB_SFIFO)
    return;
dmb_sfifo[dmb_scnt++] = entry | SB_ACT;
if (!dmb_s_posted) {
    dmb_s_posted = TRUE;
    dmb_event (CB_SYNC);
    }
}

static void dmb_s_pop (void)
{
if (dmb_scnt == 0)
    return;
memmove (&dmb_sfifo[0], &dmb_sfifo[1], (dmb_scnt - 1) * sizeof (uint32));
if (--dmb_scnt == 0)
    dmb_s_posted = FALSE;
}

/* ---------------------------------------------------------------------------
   DMA engine: VAX address translation done by the board (TD 2.2.2)

   dmb_xlate returns the physical address of character pos of the buffer,
   or an error code (3.4.8.2) with the address to report.  The
   translation of the current page is kept in the descriptor.
   --------------------------------------------------------------------------- */

static t_bool dmb_mem_rl (uint32 pa, uint32 *val)
{
if (!ADDR_IS_MEM (pa))
    return FALSE;
*val = M[pa >> 2];
return TRUE;
}

/* Protection check of the first PTE that describes a page (kernel mode). */
static t_bool dmb_prot_ok (uint32 pte, t_bool wr)
{
uint32 prot = (pte >> DPTE_V_PROT) & 0xF;

if ((prot == 0) || (prot == 1))
    return FALSE;
if (wr && ((prot == 3) || (prot == 7) || (prot == 11) || (prot == 15)))
    return FALSE;
return TRUE;
}

/* System PTE of system virtual address va: physical address of the page,
   or an error (table ER_T_SYS or the table of the caller). */
static int32 dmb_sys_xlate (uint32 va, t_bool wr, t_bool chk, uint32 *pa, uint32 *eaddr)
{
uint32 vpn = (va >> 9) & 0x1FFFFF;
uint32 pteaddr, pte;

if (((va & 0xC0000000) != 0x80000000) || (vpn >= dmb_spts)) {
    *eaddr = va;
    return ER_DMA | ER_R_INV | ER_T_SYS;        /* not a system address, too long */
    }
pteaddr = dmb_spte + (vpn << 2);
if (!dmb_mem_rl (pteaddr, &pte)) {
    *eaddr = pteaddr;
    return ER_DMA | ER_R_NXM | ER_T_SYS;
    }
if (!(pte & DPTE_V) && ((pte & (DPTE_TYP0|DPTE_TYP1)) != 0)) {  /* valid or transition */
    *eaddr = pteaddr;
    return ER_DMA | ER_R_INV | ER_T_SYS;
    }
if (chk && !dmb_prot_ok (pte, wr)) {
    *eaddr = pteaddr;
    return ER_DMA | ER_R_ACC | ER_T_SYS;
    }
*pa = ((pte & DPTE_PFN) << 9) | (va & 0x1FF);
return 0;
}

/* Interpret a process PTE (or a physical-mode PTE): physical page base.
   A global page is looked up once in the global page table. */
static int32 dmb_prc_pte (uint32 pte, uint32 pteaddr, t_bool wr, uint32 *pbase, uint32 *eaddr)
{
int32 r;
uint32 gva, gpa, gpte;

if (!(pte & DPTE_V) && (pte & DPTE_TYP0) && !(pte & DPTE_TYP1)) {      /* global page */
    uint32 gptx = pte & DPTE_GPTX;

    if (gptx >= dmb_gpts) {
        *eaddr = pteaddr;
        return ER_DMA | ER_R_INV | ER_T_GBL;
        }
    gva = dmb_gpte + (gptx << 2);
    if ((r = dmb_sys_xlate (gva, FALSE, FALSE, &gpa, eaddr)) != 0)
        return r;
    if (!dmb_mem_rl (gpa, &gpte)) {
        *eaddr = gpa;
        return ER_DMA | ER_R_NXM | ER_T_GBL;
        }
    if (!(gpte & DPTE_V) && ((gpte & (DPTE_TYP0|DPTE_TYP1)) != 0)) {
        *eaddr = gva;
        return ER_DMA | ER_R_INV | ER_T_GBL;
        }
    if (!dmb_prot_ok (pte, wr)) {               /* protection of the first PTE */
        *eaddr = pteaddr;
        return ER_DMA | ER_R_ACC | ER_T_PRC;
        }
    *pbase = (gpte & DPTE_PFN) << 9;
    return 0;
    }
if (!(pte & DPTE_V) && ((pte & (DPTE_TYP0|DPTE_TYP1)) != 0)) {
    *eaddr = pteaddr;
    return ER_DMA | ER_R_INV | ER_T_PRC;
    }
if (!dmb_prot_ok (pte, wr)) {
    *eaddr = pteaddr;
    return ER_DMA | ER_R_ACC | ER_T_PRC;
    }
*pbase = (pte & DPTE_PFN) << 9;
return 0;
}

/* Physical address of character d->pos; 0 = ok, else an error code. */
static int32 dmb_xlate (DMB_DMA *d, t_bool wr, uint32 *pa)
{
uint32 byte, page, pte, pteaddr, ptepa;
int32 r;

if ((d->ctrl & (DC_PHYS|DC_PTE)) == DC_PHYS) {  /* physical buffer */
    *pa = d->addr + d->pos;
    return 0;
    }
if (!(d->ctrl & DC_PHYS) && !(dmb_maint & MT_PTE_VALID)) {
    d->err_addr = d->addr;                      /* virtual without page tables */
    return ER_DMA | ER_R_INV | ER_T_SYS;
    }
if (!(d->ctrl & DC_PTE)) {                      /* system virtual buffer */
    uint32 va = d->addr + d->pos;

    page = (va >> 9) + 1;
    if (d->page != page) {
        if ((r = dmb_sys_xlate (va, wr, TRUE, &d->pbase, &d->err_addr)) != 0)
            return r;
        d->pbase &= ~0x1FF;
        d->page = page;
        }
    *pa = d->pbase | (va & 0x1FF);
    return 0;
    }
byte = (d->off & CT_OFF) + d->pos;              /* PTE modes */
page = (byte >> 9) + 1;
if (d->page != page) {
    pteaddr = d->addr + ((byte >> 9) << 2);
    if (d->ctrl & DC_PHYS) {                    /* physical page table */
        if (!dmb_mem_rl (pteaddr, &pte)) {
            d->err_addr = pteaddr;
            return ER_DMA | ER_R_NXM | ER_T_PRC;
            }
        d->pbase = (pte & DPTE_PFN) << 9;        /* other bits ignored (2.2.2.2) */
        }
    else {                                      /* virtual process page table */
        if ((r = dmb_sys_xlate (pteaddr, FALSE, FALSE, &ptepa, &d->err_addr)) != 0)
            return r;
        if (!dmb_mem_rl (ptepa, &pte)) {
            d->err_addr = ptepa;
            return ER_DMA | ER_R_NXM | ER_T_PRC;
            }
        if ((r = dmb_prc_pte (pte, pteaddr, wr, &d->pbase, &d->err_addr)) != 0)
            return r;
        }
    d->page = page;
    }
*pa = d->pbase | (byte & 0x1FF);
return 0;
}

/* Read or write up to n characters at the descriptor's position.  Returns
   the number moved; *err is the error code that stopped it (0 = none). */
static int32 dmb_dma_xfer (DMB_DMA *d, uint8 *buf, int32 n, t_bool wr, int32 *err)
{
int32 i;
uint32 pa;

*err = 0;
for (i = 0; i < n; i++) {
    if ((*err = dmb_xlate (d, wr, &pa)) != 0)
        break;
    if (!ADDR_IS_MEM (pa)) {
        d->err_addr = pa & ~0xF;                /* rounded to the octaword */
        *err = ER_DMA | ER_R_NXM | ER_T_BUF;
        break;
        }
    if (wr) {
        int32 sc = (pa & 3) << 3;

        M[pa >> 2] = (M[pa >> 2] & ~(0xFFu << sc)) | ((uint32)buf[i] << sc);
        }
    else
        buf[i] = (uint8)(M[pa >> 2] >> ((pa & 3) << 3));
    d->pos++;
    }
if (*err)
    sim_debug (DBG_DMA, &dmb_dev, "DMA error %02X at %08X (addr %08X, pos %d)\n",
               *err, d->err_addr, d->addr, d->pos);
return i;
}

/* Set up a descriptor from the registers. */
static void dmb_dma_setup (DMB_DMA *d, uint32 addr, uint32 ct, uint32 ctrl)
{
d->addr = addr;
d->off = ct & CT_OFF;
d->count = (ct >> CT_V_COUNT) & 0xFFFF;
d->ctrl = ctrl & (DC_PTE|DC_PHYS);
d->pos = 0;
d->page = 0;
d->pbase = 0;
d->err_addr = 0;
}

/* Registers after the end of a transfer: the rest of the buffer, so that a
   restart continues it (3.3.48 TX.OUT.ABORT); after an error the address
   register holds the failing address (3.4.8.2). */
static void dmb_dma_regs (DMB_DMA *d, int32 err, uint32 *addr, uint32 *ct)
{
uint32 left = (d->count > d->pos) ? (d->count - d->pos) : 0;

if (err & ER_DMA) {
    *addr = d->err_addr;
    *ct = (left << CT_V_COUNT) | (*ct & CT_OFF);
    return;
    }
if (d->ctrl & DC_PTE) {                         /* PTE address, offset */
    uint32 byte = (d->off & CT_OFF) + d->pos;

    *addr = d->addr + ((byte >> 9) << 2);
    *ct = (left << CT_V_COUNT) | (byte & CT_OFF);
    }
else {
    *addr = d->addr + d->pos;
    *ct = (left << CT_V_COUNT) | (*ct & CT_OFF);
    }
}

/* ---------------------------------------------------------------------------
   Asynchronous lines
   --------------------------------------------------------------------------- */

/* Modem inputs from the connection.  Without modem control a connected
   line looks like an answered call: DSR, carrier and CTS on. */
static void dmb_update_modem (int32 ln)
{
DMB_LINE *l = &dmb_line[ln];
TMLN *lp = &dmb_ldsc[ln];
uint32 ms = 0;

if (dmb_mctl) {
    int32 bits;

    tmxr_set_get_modem_bits (lp, 0, 0, &bits);
    ms = ((bits & TMXR_MDM_DSR) ? LS_DSR : 0) | ((bits & TMXR_MDM_DCD) ? LS_DCD : 0) |
         ((bits & TMXR_MDM_CTS) ? LS_CTS : 0) | ((bits & TMXR_MDM_RNG) ? LS_RI : 0);
    }
else if (lp->conn)
    ms = LS_DSR | LS_DCD | LS_CTS;
if (ms != (l->lstat & LS_MODEM)) {
    sim_debug (DBG_MDM, &dmb_dev, "line %d modem status %04X -> %04X\n",
               ln, l->lstat & LS_MODEM, ms);
    l->lstat = (l->lstat & ~LS_MODEM) | ms;
    if (l->lpr & LP_RPTMODEM)                   /* modem change entry, RBUF<7:0> */
        dmb_rx_put (RB_NONCHAR | ((uint32)ln << RB_V_LINE) |
                    ((ms & LS_TI) ? 0x04 : 0) | ((ms & LS_CTS) ? 0x10 : 0) |
                    ((ms & LS_DCD) ? 0x20 : 0) | ((ms & LS_RI) ? 0x40 : 0) |
                    ((ms & LS_DSR) ? 0x80 : 0), TRUE);
    }
}

/* Modem outputs (LPR DTR, RTS) to the connection. */
static void dmb_set_modem_out (int32 ln, uint32 old)
{
DMB_LINE *l = &dmb_line[ln];
int32 set = 0, clr = 0;

if (!dmb_mctl || !((old ^ l->lpr) & (LP_DTR|LP_RTS)))
    return;
if (l->lpr & LP_DTR) set |= TMXR_MDM_DTR; else clr |= TMXR_MDM_DTR;
if (l->lpr & LP_RTS) set |= TMXR_MDM_RTS; else clr |= TMXR_MDM_RTS;
tmxr_set_get_modem_bits (&dmb_ldsc[ln], set, clr, NULL);
sim_debug (DBG_MDM, &dmb_dev, "line %d DTR %s RTS %s\n", ln,
           (l->lpr & LP_DTR) ? "on" : "off", (l->lpr & LP_RTS) ? "on" : "off");
}

/* Line parameters to a serial port (Telnet lines ignore them). */
static void dmb_set_line_config (int32 ln)
{
static const char *speeds[16] = { "50", "75", "110", "134", "150", "300", "600", "1200",
                                  "1800", "2000", "2400", "4800", "7200", "9600", "19200", "38400" };
DMB_LINE *l = &dmb_line[ln];
char config[40];

dmb_ldsc[ln].rcve = (l->lpr & LP_RXENA) ? 1 : 0;
if (!dmb_ldsc[ln].serport)
    return;
snprintf (config, sizeof (config), "%s-%d%c%d", speeds[(l->lpr >> LP_V_TSPD) & 0xF],
          (int)((l->lpr >> LP_V_CLEN) & 3) + 5,
          (l->lpr & LP_PARENB) ? ((l->lpr & LP_EVEN) ? 'E' : 'O') : 'N',
          (l->lpr & LP_STOP2) ? 2 : 1);
tmxr_set_config_line (&dmb_ldsc[ln], config);
}

/* A received character (or a break) for line ln. */
static void dmb_receive (int32 ln, int32 c, t_bool brk)
{
DMB_LINE *l = &dmb_line[ln];
int32 len = (int32)((l->lpr >> LP_V_CLEN) & 3) + 5;
int32 maint = (int32)((l->lpr & LP_MAINT) >> LP_V_MAINT);
int32 rxoff = (l->flowc >> 16) & 0xFF, rxon = (l->flowc >> 24) & 0xFF;
t_bool flow = FALSE;

c &= (1 << len) - 1;
if (!brk && (l->lpr & LP_OAUTO)) {             /* obey XON/XOFF */
    if (c == rxoff) {
        l->lstat &= ~LS_TXENA;
        flow = TRUE;
        }
    else if (c == rxon) {
        l->lstat |= LS_TXENA;
        flow = TRUE;
        dmb_transmit (ln);
        }
    }
if ((maint == MAINT_ECHO) || (maint == MAINT_REMOTE))
    tmxr_putc_ln (&dmb_ldsc[ln], c);
if (maint == MAINT_REMOTE)                      /* not stored */
    return;
if (flow && (l->lpr & LP_DISCFLOW))             /* flow characters not stored */
    return;
if (dmb_rx_put ((brk ? RB_FRMERR : (uint32)c) | ((uint32)ln << RB_V_LINE) |
                (l->overrun ? RB_OVRERR : 0), FALSE))
    l->overrun = FALSE;
else
    l->overrun = TRUE;
if (dmb_congested && ((l->lpr & LP_IAUTO) || (l->lstat & LS_SNDOFF))) {
    l->rx_toggle = !l->rx_toggle;               /* XOFF to every other character */
    if (l->rx_toggle || !l->xoff_sent) {
        l->flow_out = l->flowc & 0xFF;          /* sent XOFF character */
        l->xoff_sent = TRUE;
        dmb_transmit (ln);
        }
    }
}

/* End of a DMA output: update the registers, report it in TBUF. */
static void dmb_tx_dma_done (int32 ln, int32 err)
{
DMB_LINE *l = &dmb_line[ln];

dmb_dma_regs (&l->dma, err, &l->tbuffad, &l->tbuffct);
l->lnctrl = (l->lnctrl & ~(DC_START|DC_ERR)) | ((uint32)err << DC_V_ERR);
l->dma_active = FALSE;
sim_debug (DBG_DMA, &dmb_dev, "line %d DMA end: %d of %d sent, error %02X\n",
           ln, l->dma.pos, l->dma.count, err);
dmb_tx_report (ln, FALSE, err);
}

/* Send what a line has to send: a flow control character, the preempt
   character, then the DMA buffer.  Output goes as fast as the multiplexer
   takes it; a line without a connection discards its output, as a real
   line without a terminal would. */
static void dmb_transmit (int32 ln)
{
DMB_LINE *l = &dmb_line[ln];
TMLN *lp = &dmb_ldsc[ln];
int32 maint = (int32)((l->lpr & LP_MAINT) >> LP_V_MAINT);

for (;;) {
    int32 c, err = 0;
    t_bool is_dma = FALSE, is_pre = FALSE;
    uint8 b;

    if (l->flow_out >= 0)
        c = l->flow_out;
    else if (l->preempt & PE_GO) {
        c = l->preempt & PE_CHAR;
        is_pre = TRUE;
        }
    else if (l->dma_active && !(l->lnctrl & DC_ABORT)) {
        if (!(l->lstat & LS_TXENA) || (maint == MAINT_REMOTE) ||
            ((l->lpr & LP_USECTS) && !(l->lstat & LS_CTS)))
            break;
        if (l->dma.pos >= l->dma.count) {       /* all characters sent */
            dmb_tx_dma_done (ln, 0);
            continue;
            }
        if (dmb_dma_xfer (&l->dma, &b, 1, FALSE, &err) != 1) {
            dmb_tx_dma_done (ln, err);
            continue;
            }
        l->dma.pos--;                           /* counted when sent */
        c = b;
        is_dma = TRUE;
        }
    else
        break;
    if (maint == MAINT_LOCAL)                   /* looped back to the receiver */
        dmb_receive (ln, c, FALSE);
    else if (lp->conn && (maint != MAINT_REMOTE)) {
        if (tmxr_putc_ln (lp, c) == SCPE_STALL) /* output buffer full: later */
            break;
        }
    if (is_dma) {
        l->dma.pos++;
        if (l->dma.pos >= l->dma.count)
            dmb_tx_dma_done (ln, 0);
        }
    else if (is_pre) {
        l->preempt &= ~PE_GO;
        dmb_tx_report (ln, TRUE, 0);
        }
    else
        l->flow_out = -1;
    }
}

/* LNCTRL write: start or abort a DMA output. */
static void dmb_write_lnctrl (int32 ln, uint32 data, uint32 bytes)
{
DMB_LINE *l = &dmb_line[ln];
uint32 old = l->lnctrl;

l->lnctrl = data & LN_RW;
if ((bytes & 1) && (data & DC_START) && !l->dma_active && !(data & DC_ABORT)) {
    dmb_dma_setup (&l->dma, l->tbuffad, l->tbuffct, data);
    l->dma_active = TRUE;
    l->lnctrl &= ~DC_ERR;
    sim_debug (DBG_DMA, &dmb_dev, "line %d DMA start: %s %08X offset %03X count %d\n",
               ln, (data & DC_PHYS) ? ((data & DC_PTE) ? "PTE(phys)" : "phys") :
               ((data & DC_PTE) ? "PTE(virt)" : "virt"),
               l->dma.addr, l->dma.off, l->dma.count);
    if (l->dma.count == 0)
        dmb_tx_dma_done (ln, 0);
    }
if ((data & DC_ABORT) && !(old & DC_ABORT)) {   /* abort: stop, report the rest */
    if (l->dma_active)
        dmb_tx_dma_done (ln, ER_ABORT);
    else
        l->lnctrl &= ~DC_START;
    }
if (l->dma_active)                              /* START stays set while active */
    l->lnctrl |= DC_START;
dmb_transmit (ln);
}

/* LPR write. */
static void dmb_write_lpr (int32 ln, uint32 data)
{
DMB_LINE *l = &dmb_line[ln];
uint32 old = l->lpr;

l->lpr = data;
dmb_set_modem_out (ln, old);
dmb_set_line_config (ln);
if ((data & LP_BREAK) && !(old & LP_BREAK) && dmb_ldsc[ln].conn)
    sim_debug (DBG_XMT, &dmb_dev, "line %d break\n", ln);
}

/* LSTAT write: SNDOFF and TX.ENA. */
static void dmb_write_lstat (int32 ln, uint32 data)
{
DMB_LINE *l = &dmb_line[ln];
uint32 old = l->lstat;

l->lstat = (l->lstat & ~LS_RW) | (data & LS_RW);
if ((l->lstat & LS_SNDOFF) && !(old & LS_SNDOFF)) {     /* force XOFF */
    l->flow_out = l->flowc & 0xFF;
    l->xoff_sent = TRUE;
    }
else if (!(l->lstat & LS_SNDOFF) && (old & LS_SNDOFF) && l->xoff_sent) {
    l->flow_out = (l->flowc >> 8) & 0xFF;               /* XON after it */
    l->xoff_sent = FALSE;
    }
dmb_transmit (ln);
}

/* Initialize the async lines (self-test, ASYNC.RESET): 3.4.1. */
static void dmb_async_init (t_bool power)
{
int32 ln;

dmb_rxcnt = 0;
dmb_txcnt = 0;
dmb_congested = FALSE;
dmb_tx_posted = FALSE;
sim_cancel (DMB_RXT_UNIT);
for (ln = 0; ln < DMB_LINES; ln++) {
    DMB_LINE *l = &dmb_line[ln];
    uint32 old = l->lpr;

    l->lpr = LP_INIT;
    l->lnctrl = 0;
    l->lstat = (l->lstat & LS_MODEM) | LS_TXENA;
    l->flowc = FLOWC_INIT;
    l->preempt = 0;
    l->tbuffad = 0;
    l->tbuffct = 0;
    l->dma_active = FALSE;
    l->xoff_sent = FALSE;
    l->flow_out = -1;
    l->overrun = FALSE;
    l->rx_toggle = FALSE;
    dmb_ldsc[ln].rcve = 0;                      /* receivers disabled */
    if (!power)
        dmb_set_modem_out (ln, old);            /* DTR and RTS off */
    }
}

/* ---------------------------------------------------------------------------
   Printer port.  A print buffer is moved by DMA through the formatter
   (vax_combo_lib.c) to the file attached to DMB-PRINT, with the prefix
   and suffix characters of PFIX.  Without a file the printer is not
   connected: a transfer ends at once with error 81 (3.4.8.8).
   --------------------------------------------------------------------------- */

/* Formatter output: the character goes to the file. */
static t_bool dmb_lp_put (COMBO_LP *lp, int32 c)
{
if (DMB_LP_UNIT->fileref)
    fputc (c, DMB_LP_UNIT->fileref);
return TRUE;
}

static void dmb_lp_init (void)
{
dmb_pfix = dmb_pbuffad = dmb_pbuffct = dmb_pctrl = dmb_pcar = dmb_psize = 0;
dmb_p_active = FALSE;
memset (&dmb_lp, 0, sizeof (dmb_lp));
dmb_lp.put = &dmb_lp_put;
combo_lp_reset (&dmb_lp);                       /* paper at top of form */
sim_cancel (DMB_LP_UNIT);
}

static uint32 dmb_pcsr_status (void)
{
if (DMB_LP_UNIT->flags & UNIT_ATT)
    return PC_CONN_VERIFY | PC_DAVFU_RDY;
return PC_OFFLINE;
}

/* End of a print transfer: registers, counters, interrupt. */
static void dmb_lp_done (int32 err)
{
dmb_dma_regs (&dmb_pdma, err, &dmb_pbuffad, &dmb_pbuffct);
dmb_pcar = ((dmb_lp.chars & 0xFFFF) << 16) | (dmb_lp.lines & 0xFFFF);
dmb_pctrl = (dmb_pctrl & ~(DC_START|DC_ERR)) | ((uint32)err << DC_V_ERR);
dmb_p_active = FALSE;
if (DMB_LP_UNIT->fileref) {
    fflush (DMB_LP_UNIT->fileref);
    DMB_LP_UNIT->pos = (t_addr)sim_ftell (DMB_LP_UNIT->fileref);
    }
sim_debug (DBG_LPT, &dmb_dev, "print done: %d of %d bytes, %d characters, %d lines, error %02X\n",
           dmb_pdma.pos, dmb_pdma.count, dmb_lp.chars, dmb_lp.lines, err);
dmb_event (CB_PRINT);
}

/* Print cycle (runs a short while after DMA.START, like the real board). */
t_stat dmb_lp_svc (UNIT *uptr)
{
uint8 buf[512];
int32 err = 0, i, n;
t_bool fmt = (dmb_pctrl & PR_FORMAT) != 0;

if (!dmb_p_active)
    return SCPE_OK;
if (!(uptr->flags & UNIT_ATT)) {                /* no printer */
    dmb_lp_done (ER_NOPRINTER);
    return SCPE_OK;
    }
dmb_lp.flags = ((dmb_pctrl & PR_TAB) ? CLP_TAB : 0) |
               ((dmb_pctrl & PR_TRUNC) ? CLP_TRUNC : 0) |
               ((dmb_pctrl & PR_AUTOCR) ? CLP_AUTOCR : 0) |
               ((dmb_pctrl & PR_AUTOFF) ? CLP_FFCONV : 0) |
               ((dmb_pctrl & PR_NONPRINT) ? CLP_NONPRINT : 0) |
               ((dmb_pctrl & PR_DAVFU) ? CLP_DAVFU : 0) |
               ((dmb_pctrl & PR_WRAP) ? CLP_WRAP : 0) |
               ((dmb_pctrl & PR_UPPER) ? CLP_UPPER : 0);
dmb_lp.width = (int32)(dmb_psize & 0xFFFF);
dmb_lp.lpp = (int32)((dmb_psize >> 16) & 0xFFFF);
dmb_lp.chars = dmb_lp.lines = 0;
sim_debug (DBG_LPT, &dmb_dev, "print cycle: %d bytes, format %s, PCTRL %08X, PFIX %08X, PSIZE %08X\n",
           dmb_pdma.count, fmt ? "on" : "off", dmb_pctrl, dmb_pfix, dmb_psize);
if (fmt)                                        /* prefix */
    combo_lp_fix (&dmb_lp, dmb_pfix & 0xFF, (dmb_pfix >> 8) & 0xFF);
while (dmb_pdma.pos < dmb_pdma.count) {
    n = (int32)(dmb_pdma.count - dmb_pdma.pos);
    if (n > (int32)sizeof (buf))
        n = (int32)sizeof (buf);
    n = dmb_dma_xfer (&dmb_pdma, buf, n, FALSE, &err);
    for (i = 0; i < n; i++) {
        if (fmt)
            combo_lp_format (&dmb_lp, buf[i]);
        else
            combo_lp_out (&dmb_lp, buf[i]);
        }
    if (err)
        break;
    }
if (fmt && !err)                                /* suffix */
    combo_lp_fix (&dmb_lp, (dmb_pfix >> 16) & 0xFF, (dmb_pfix >> 24) & 0xFF);
dmb_lp_done (err);
return SCPE_OK;
}

/* PCTRL write: start or abort a print transfer. */
static void dmb_write_pctrl (uint32 data, uint32 bytes)
{
dmb_pctrl = data & PR_RW;
if ((bytes & 1) && (data & DC_START) && !dmb_p_active && !(data & DC_ABORT)) {
    dmb_dma_setup (&dmb_pdma, dmb_pbuffad, dmb_pbuffct, data);
    dmb_p_active = TRUE;
    dmb_pctrl &= ~DC_ERR;
    sim_activate (DMB_LP_UNIT, DMB_LP_UNIT->wait);
    }
if ((data & DC_ABORT) && dmb_p_active) {        /* abort */
    sim_cancel (DMB_LP_UNIT);
    dmb_lp_done (ER_ABORT);
    }
if (dmb_p_active)
    dmb_pctrl |= DC_START;
}

/* ---------------------------------------------------------------------------
   Synchronous line: registers only in this version (comes with P4)
   --------------------------------------------------------------------------- */

static void dmb_sync_init (void)
{
int32 i;

dmb_scnt = 0;
dmb_s_posted = FALSE;
for (i = 0; i < SY_NREGS; i++)                  /* all fields 0 but BUFCTRL<31:16> */
    dmb_sreg[i] = (i == SY_BUFCTRL) ? (dmb_sreg[i] & 0xFFFF0000) : 0;
}

t_stat dmb_sync_svc (UNIT *uptr)
{
return SCPE_OK;
}

static void dmb_write_sync (int32 idx, uint32 data, uint32 bytes)
{
dmb_sreg[idx] = data;
}

/* ---------------------------------------------------------------------------
   Self-test and resets (3.4.1).  Bus reset, BICSR NRST and MAINT
   PROG.RESET run the self-test; it takes SET DMB SELFTEST=ms of simulated
   time (1 ms with SKIP.SELF.TEST).  The BIIC's own test (STS) passes at
   once; BROKE stays set while the module tests itself and is cleared when
   it passes, which is what VMS waits for (a timed wait on BICSR<12>).
   PROG.RESET and INIT read as set meanwhile; afterwards the lines are in
   their initial state.
   --------------------------------------------------------------------------- */

static void dmb_init_all (t_bool power)
{
dmb_acsr = dmb_scsr = dmb_pcsr = 0;
dmb_acsr2 = dmb_scsr2 = dmb_pcsr2 = 0;
dmb_cond = dmb_req = 0;
dmb_async_init (power);
dmb_sync_init ();
dmb_lp_init ();
dmb_update_int ();
}

static void dmb_start_selftest (const char *why)
{
int32 ms = (dmb_maint & MT_SKIP_ST) ? 1 : dmb_st_ms;

sim_debug (DBG_INIT, &dmb_dev, "self-test started (%s), %d ms\n", why, ms);
dmb_testing = TRUE;
dmb_maint |= MT_PROG_RESET;
dmb_biic.csr = dmb_biic.csr | BICSR_STS | BICSR_BRK | BICSR_INI;
dmb_init_all (FALSE);
sim_cancel (DMB_ST_UNIT);
if (ms <= 0)
    dmb_st_svc (DMB_ST_UNIT);
else
    sim_activate_after (DMB_ST_UNIT, ms * 1000);
}

t_stat dmb_st_svc (UNIT *uptr)
{
t_bool fail = (dmb_maint & MT_FORCE_FAIL) != 0;

dmb_testing = FALSE;
dmb_maint &= ~(MT_PROG_RESET|MT_FORCE_FAIL|MT_SKIP_ST|MT_DIAG_FAIL);
dmb_biic.csr = (dmb_biic.csr & ~(BICSR_INI|BICSR_BRK)) | BICSR_STS;
if (fail) {                                     /* FORCE.FAIL: as if a test failed */
    dmb_maint |= MT_DIAG_FAIL;
    dmb_biic.csr |= BICSR_BRK;
    dmb_biic.gpr0 = 0x80000001;                 /* valid, one error bit */
    dmb_rx_put (RB_NONCHAR | 0x01, FALSE);      /* a diagnostic report */
    }
else
    dmb_biic.gpr0 = 0x80000000;                 /* valid, no errors */
sim_debug (DBG_INIT, &dmb_dev, "self-test %s\n", fail ? "failed (forced)" : "passed");
return SCPE_OK;
}

/* ---------------------------------------------------------------------------
   Register access

   Reads return the whole longword (the CPU picks the byte or word); writes
   arrive right justified and are merged into the longword, and "bytes"
   tells which bytes were written (bit n = byte n), so that the action bits
   of a byte (DMA.START, ABORT) act only when their byte is written.
   --------------------------------------------------------------------------- */

static int32 dmb_aind (void)
{
return (int32)(dmb_acsr & CS_IND);
}

static uint32 dmb_reg_read (int32 ofs, t_bool *ok)
{
DMB_LINE *l;

*ok = TRUE;
if ((ofs >= OF_SYNC) && (ofs <= OF_SYNC_END))
    return dmb_sreg[(ofs - OF_SYNC) >> 2];
if ((ofs >= OF_PREEMPT) && (ofs <= OF_FLOWC)) {
    if (dmb_aind () >= DMB_LINES)
        return 0;
    l = &dmb_line[dmb_aind ()];
    switch (ofs) {
        case OF_PREEMPT:  return l->preempt;
        case OF_TBUFFAD:  return l->tbuffad;
        case OF_TBUFFCT:  return l->tbuffct;
        case OF_LPR:      return l->lpr;
        case OF_LNCTRL:   return l->lnctrl;
        case OF_LSTAT:    return l->lstat;
        case OF_FLOWC:    return l->flowc;
        }
    }
if (ofs >= DMB_RAM_BASE) {
    if (!(dmb_maint & MT_LEVEL)) {              /* only in maintenance mode */
        *ok = FALSE;
        return 0;
        }
    return dmb_ram[(ofs - DMB_RAM_BASE) >> 2];
    }
switch (ofs) {
    case OF_DTYPE:    return (DMB_REVCODE << 16) | DMB_DTYPE;
    case OF_BICSR:    return dmb_biic.csr & BICSR_RD;
    case OF_BER:      return dmb_biic.ber;
    case OF_EICR:     return dmb_biic.eicr & BIECR_RD;
    case OF_IDEST:    return dmb_biic.idest & BIID_RD;
    case OF_IMSK:     return dmb_biic.imsk & 0xFFFF0000;
    case OF_FIDEST:   return dmb_biic.fidest & 0xFFFF0000;
    case OF_ISRC:     return dmb_biic.isrc & 0xFFFF0000;
    case OF_SADR:     return dmb_biic.sa & 0x3FFC0000;
    case OF_EADR:     return dmb_biic.ea & 0x3FFC0000;
    case OF_UINTRCSR: return (dmb_biic.uiic & BIICR_RD) | BIICR_EXV;
    case OF_GPR0:     return dmb_biic.gpr0;
    case OF_MAINT:    return dmb_maint | MT_PRESENT;
    case OF_ACSR:     return dmb_acsr;
    case OF_SCSR:     return dmb_scsr;
    case OF_PCSR:     return dmb_pcsr | dmb_pcsr_status ();
    case OF_CONFIG:   return 0x00010108;        /* 1 printer, 1 sync, 8 async */
    case OF_ACSR2:    return dmb_acsr2;
    case OF_SCSR2:    return dmb_scsr2;
    case OF_PCSR2:    return dmb_pcsr2;
    case OF_SPTE:     return dmb_spte;
    case OF_SPTS:     return dmb_spts;
    case OF_GPTE:     return dmb_gpte;
    case OF_GPTS:     return dmb_gpts;
    case OF_PFIX:     return dmb_pfix;
    case OF_PBUFFAD:  return dmb_pbuffad;
    case OF_PBUFFCT:  return dmb_pbuffct;
    case OF_PCTRL:    return dmb_pctrl;
    case OF_PCAR:     return dmb_pcar;
    case OF_PSIZE:    return dmb_psize;
    case OF_RXCD:     *ok = FALSE; return 0;    /* console register: NO ACK */
    case OF_TBUF:     return dmb_txcnt ? dmb_txfifo[0] : 0;
    case OF_SBUF:     return dmb_scnt ? dmb_sfifo[0] : 0;
    case OF_RBUF:     return dmb_rxcnt ? dmb_rxfifo[0] : 0;
    default:          return 0;                 /* unused, BCI registers */
    }
}

t_stat dmb_rd (int32 *val, int32 pa, int32 lnt)
{
int32 ofs = pa & (DMB_NODESPACE - 4);
t_bool ok;

*val = (int32)dmb_reg_read (ofs, &ok);
if (!ok)
    return SCPE_NXM;
sim_debug (DBG_REG, &dmb_dev, "read  %03X = %08X%s\n", ofs, (uint32)*val,
           ((ofs >= OF_PREEMPT) && (ofs <= OF_FLOWC)) ? " (async line)" : "");
return SCPE_OK;
}

t_stat dmb_wr (int32 val, int32 pa, int32 lnt)
{
int32 ofs = pa & (DMB_NODESPACE - 4);
int32 sc = (pa & 3) << 3;
uint32 bytes, mask, old, data;
t_bool ok;

switch (lnt) {                                  /* merge into the longword */
    case L_BYTE: mask = 0xFFu << sc;   bytes = 1u << (pa & 3);  break;
    case L_WORD: mask = 0xFFFFu << sc; bytes = 3u << (pa & 2);  break;
    default:     mask = 0xFFFFFFFFu;   bytes = 0xF; sc = 0;     break;
    }
old = dmb_reg_read (ofs, &ok);
if (!ok)
    return SCPE_NXM;
data = (old & ~mask) | (((uint32)val << sc) & mask);
sim_debug (DBG_REG, &dmb_dev, "write %03X = %08X (bytes %X)\n", ofs, data, bytes);
if (dmb_testing && (ofs >= OF_ACSR) && (ofs < DMB_RAM_BASE)) {
    sim_debug (DBG_INIT, &dmb_dev, "write during self-test ignored\n");
    return SCPE_OK;                             /* host must wait (3.3.10) */
    }
if ((ofs >= OF_SYNC) && (ofs <= OF_SYNC_END)) {
    dmb_write_sync ((ofs - OF_SYNC) >> 2, data, bytes);
    return SCPE_OK;
    }
if ((ofs >= OF_PREEMPT) && (ofs <= OF_FLOWC)) {
    int32 ln = dmb_aind ();
    DMB_LINE *l;

    if (ln >= DMB_LINES)
        return SCPE_OK;
    l = &dmb_line[ln];
    switch (ofs) {
        case OF_PREEMPT:
            l->preempt = data & (PE_CHAR|PE_GO);
            dmb_transmit (ln);
            break;
        case OF_TBUFFAD:
            if (!l->dma_active)
                l->tbuffad = data;
            break;
        case OF_TBUFFCT:
            if (!l->dma_active)
                l->tbuffct = data & ((0xFFFFu << CT_V_COUNT) | CT_OFF);
            break;
        case OF_LPR:
            dmb_write_lpr (ln, data);
            break;
        case OF_LNCTRL:
            dmb_write_lnctrl (ln, data, bytes);
            break;
        case OF_LSTAT:
            dmb_write_lstat (ln, data);
            break;
        case OF_FLOWC:
            l->flowc = data;
            break;
        }
    return SCPE_OK;
    }
if (ofs >= DMB_RAM_BASE) {
    dmb_ram[(ofs - DMB_RAM_BASE) >> 2] = data;
    return SCPE_OK;
    }
switch (ofs) {
    case OF_BICSR:
        if (data & BICSR_RST) {                 /* node reset: self-test */
            dmb_biic.csr = (dmb_biic.csr & ~BICSR_RW) | (data & BICSR_RW);
            dmb_start_selftest ("BICSR node reset");
            break;
            }
        dmb_biic.csr = (dmb_biic.csr & ~BICSR_RW) | (data & BICSR_RW);
        dmb_biic.csr &= ~(data & (BICSR_UWP|BICSR_BRK));    /* write 1 to clear */
        break;
    case OF_BER:
        dmb_biic.ber &= ~(data & BIBER_W1C);
        break;
    case OF_EICR:
        dmb_biic.eicr = (dmb_biic.eicr & ~BIECR_RW) | (data & BIECR_RW);
        dmb_biic.eicr &= ~(data & BIECR_W1C);
        break;
    case OF_IDEST:
        dmb_biic.idest = data & BIID_RW;
        if (dmb_biic.idest && !(dmb_biic.idest & 1))
            sim_debug (DBG_INT, &dmb_dev, "INTRDES %04X does not include KA0: "
                       "interrupts still go to the primary\n", dmb_biic.idest);
        break;
    case OF_IMSK:
        dmb_biic.imsk = data & 0xFFFF0000;
        break;
    case OF_FIDEST:
        dmb_biic.fidest = data & 0xFFFF0000;
        break;
    case OF_ISRC:
        dmb_biic.isrc &= ~(data & 0xFFFF0000);
        break;
    case OF_SADR:
        dmb_biic.sa = data & 0x3FFC0000;
        break;
    case OF_EADR:
        dmb_biic.ea = data & 0x3FFC0000;
        break;
    case OF_UINTRCSR:
        dmb_biic.uiic = ((dmb_biic.uiic & ~BIICR_RW) | (data & BIICR_RW)) & ~(data & BIICR_W1C);
        break;
    case OF_MAINT:
        dmb_maint = (dmb_maint & ~MT_RW) | (data & MT_RW);
        if ((data & MT_PROG_RESET) && !dmb_testing)
            dmb_start_selftest ("programmed reset");
        break;
    case OF_ACSR:
        dmb_acsr = data & AC_RW;
        dmb_update_int ();                      /* enabling with a condition waiting */
        break;
    case OF_SCSR:
        dmb_scsr = data & SC_RW;
        dmb_update_int ();
        break;
    case OF_PCSR:
        dmb_pcsr = data & PC_RW;
        dmb_update_int ();
        break;
    case OF_ACSR2:
        dmb_acsr2 = data & AC2_RW;
        if (data & C2_RESET) {                  /* async port reset */
            int32 ln;

            sim_debug (DBG_INIT, &dmb_dev, "async port reset\n");
            for (ln = 0; ln < DMB_LINES; ln++)
                dmb_line[ln].dma_active = FALSE;
            dmb_async_init (FALSE);
            dmb_acsr2 &= ~C2_RESET;             /* done (at once) */
            }
        break;
    case OF_SCSR2:
        dmb_scsr2 = data & C2_RESET;
        if (data & C2_RESET) {
            sim_debug (DBG_INIT, &dmb_dev, "sync port reset\n");
            dmb_sync_init ();
            dmb_scsr2 &= ~C2_RESET;
            }
        break;
    case OF_PCSR2:
        dmb_pcsr2 = data & C2_RESET;
        if (data & C2_RESET) {
            sim_debug (DBG_INIT, &dmb_dev, "printer port reset\n");
            dmb_lp_init ();
            dmb_pcsr2 &= ~C2_RESET;
            }
        break;
    case OF_SPTE:     dmb_spte = data; break;
    case OF_SPTS:     dmb_spts = data; break;
    case OF_GPTE:     dmb_gpte = data; break;
    case OF_GPTS:     dmb_gpts = data; break;
    case OF_PFIX:     dmb_pfix = data; break;
    case OF_PBUFFAD:  dmb_pbuffad = data; break;
    case OF_PBUFFCT:  dmb_pbuffct = data; break;
    case OF_PCTRL:    dmb_write_pctrl (data, bytes); break;
    case OF_PCAR:     dmb_pcar = data; break;
    case OF_PSIZE:    dmb_psize = data; break;
    case OF_TBUF:     dmb_tx_pop (); break;     /* FIFOs: a write removes */
    case OF_SBUF:     dmb_s_pop (); break;
    case OF_RBUF:     dmb_rx_pop (); break;
    default:          break;                    /* read only, unused */
    }
return SCPE_OK;
}

/* ---------------------------------------------------------------------------
   Multiplexer poll
   --------------------------------------------------------------------------- */

t_stat dmb_svc (UNIT *uptr)
{
int32 ln;

ln = tmxr_poll_conn (&dmb_desc);                /* new connection */
if (ln >= 0) {
    dmb_ldsc[ln].rcve = (dmb_line[ln].lpr & LP_RXENA) ? 1 : 0;
    sim_debug (DBG_CON, &dmb_dev, "line %d connected\n", ln);
    }
tmxr_poll_rx (&dmb_desc);
for (ln = 0; ln < DMB_LINES; ln++) {
    DMB_LINE *l = &dmb_line[ln];
    TMLN *lp = &dmb_ldsc[ln];
    int32 c;

    dmb_update_modem (ln);
    while ((l->lpr & LP_RXENA) && lp->conn && (dmb_rxcnt < DMB_RXFIFO) &&
           ((c = tmxr_getc_ln (lp)) & TMXR_VALID))
        dmb_receive (ln, c & 0xFF, (c & SCPE_BREAK) != 0);
    }
tmxr_poll_tx (&dmb_desc);
for (ln = 0; ln < DMB_LINES; ln++)
    dmb_transmit (ln);
sim_clock_coschedule (uptr, tmxr_poll);
return SCPE_OK;
}

/* ---------------------------------------------------------------------------
   SET/SHOW
   --------------------------------------------------------------------------- */

static t_stat dmb_set_node (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
t_stat r;
int32 node;

if (cptr == NULL)
    return SCPE_ARG;
node = (int32) get_uint (cptr, 10, 15, &r);
if (r != SCPE_OK)
    return r;
if (node < 2)                                   /* 0, 1: the KA820s */
    return sim_messagef (SCPE_ARG, "Node %d is a KA820 node\n", node);
nexus_req[DMB_IPL - IPL_HMIN] &= ~(1u << dmb_node ());
dmb_dib.ba = (uint32)node;
dmb_update_int ();
return SCPE_OK;
}

static t_stat dmb_show_node (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "node=%d", dmb_node ());
return SCPE_OK;
}

static t_stat dmb_set_st (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
t_stat r;
int32 ms;

if (cptr == NULL)
    return SCPE_ARG;
ms = (int32) get_uint (cptr, 10, 60000, &r);
if (r != SCPE_OK)
    return r;
dmb_st_ms = ms;
return SCPE_OK;
}

static t_stat dmb_show_st (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "selftest=%dms", dmb_st_ms);
return SCPE_OK;
}

/* ---------------------------------------------------------------------------
   Reset, attach, detach
   --------------------------------------------------------------------------- */

t_stat dmb_reset (DEVICE *dptr)
{
sim_set_uname (&dmb_unit[0], "DMB-ASYNC");
sim_set_uname (&dmb_unit[1], "DMB-PRINT");
sim_set_uname (&dmb_unit[2], "DMB-SYNC");
dmb_desc.dptr = dptr;
/* bus reset: the self-test has run by the time software looks at the
   node (power-up), so the board is ready at once */
sim_cancel (DMB_ST_UNIT);
dmb_testing = FALSE;
dmb_maint &= MT_PTE_VALID;                      /* PTE.VALID: bus reset only (A) */
if (sim_switches & SWMASK ('P'))
    dmb_maint = 0;
memset (&dmb_biic, 0, sizeof (dmb_biic));
dmb_biic.csr = (1u << BICSR_V_IF) | BICSR_STS | (dmb_node () & BICSR_NODE);
dmb_biic.uiic = BIICR_EXV;
dmb_biic.gpr0 = 0x80000000;
dmb_init_all ((sim_switches & SWMASK ('P')) != 0);
if (DMB_MUX_UNIT->flags & UNIT_ATT)
    sim_clock_coschedule (DMB_MUX_UNIT, tmxr_poll);
else
    sim_cancel (DMB_MUX_UNIT);
return SCPE_OK;
}

t_stat dmb_attach (UNIT *uptr, CONST char *cptr)
{
t_stat r;

if (uptr == DMB_LP_UNIT)
    return attach_unit (uptr, cptr);
if (uptr == DMB_SYNC_UNIT)
    return sim_messagef (SCPE_NOFNC, "DMB-SYNC: the synchronous line is not implemented yet\n");
if (uptr != DMB_MUX_UNIT)
    return SCPE_NOATT;
if (sim_switches & SWMASK ('M'))                /* modem control on all lines */
    tmxr_set_modem_control_passthru (&dmb_desc);
r = tmxr_attach (&dmb_desc, uptr, cptr);
if (r != SCPE_OK) {
    tmxr_clear_modem_control_passthru (&dmb_desc);
    return r;
    }
dmb_mctl = (sim_switches & SWMASK ('M')) ? 1 : 0;
sim_clock_coschedule (uptr, tmxr_poll);
return SCPE_OK;
}

t_stat dmb_detach (UNIT *uptr)
{
t_stat r;

if (uptr == DMB_LP_UNIT)
    return detach_unit (uptr);
if (uptr != DMB_MUX_UNIT)
    return SCPE_NOATT;
r = tmxr_detach (&dmb_desc, uptr);
sim_cancel (uptr);
dmb_mctl = 0;
tmxr_clear_modem_control_passthru (&dmb_desc);
return r;
}

/* ---------------------------------------------------------------------------
   Help
   --------------------------------------------------------------------------- */

const char *dmb_description (DEVICE *dptr)
{
return "DMB32 VAXBI multiplexer, synchronous line and printer port";
}

t_stat dmb_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
fprintf (st, "DMB32 VAXBI Communications Interface (DMB)\n\n");
fprintf (st, "The DMB32 is a VAXBI module with an eight-line asynchronous multiplexer,\n");
fprintf (st, "a synchronous line and a line printer port: the VAXBI relative of the\n");
fprintf (st, "DMF32.  It is a BI node of its own (default node 5):\n\n");
fprintf (st, "   sim> SET DMB ENABLE\n");
fprintf (st, "   sim> SET DMB NODE=6           (another BI node, 2-15)\n");
fprintf (st, "   sim> SET DMB SELFTEST=50      (self-test time in ms after a reset)\n\n");
fprintf (st, "VMS configures the functions as TXA0-TXA7 (TXB.. if another TX controller\n");
fprintf (st, "comes first), SIA0 (DECnet circuit DMB-0) and LIA0.\n\n");
fprintf (st, "The functions have their own units:\n\n");
fprintf (st, "   DMB-ASYNC   asynchronous multiplexer, lines 0-7\n");
fprintf (st, "   DMB-PRINT   line printer port\n");
fprintf (st, "   DMB-SYNC    synchronous line\n\n");
fprintf (st, "Terminal lines: attach the multiplexer (DMB-ASYNC, or just DMB) to a Telnet\n");
fprintf (st, "port, or lines to serial ports, as for the other terminal multiplexers:\n\n");
fprintf (st, "   sim> ATTACH DMB-ASYNC 2323\n");
fprintf (st, "   sim> ATTACH -M DMB-ASYNC 2323  (modem control)\n\n");
fprintf (st, "Without -M a Telnet connection appears as data set ready, carrier and\n");
fprintf (st, "clear to send; with -M the modem signals follow the connection and DTR.\n\n");
fprintf (st, "Line printer: attach DMB-PRINT to an output file:\n\n");
fprintf (st, "   sim> ATTACH DMB-PRINT printer.txt\n\n");
fprintf (st, "Without a file the printer is offline (not connected).\n\n");
fprint_set_help (st, dptr);
fprint_show_help (st, dptr);
fprint_reg_help (st, dptr);
return SCPE_OK;
}
