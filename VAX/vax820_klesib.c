/* vax820_klesib.c: KLESI-B VAXBI tape adapter (TB), adapter part

   Incorporates the DWBUA model of vax820_uba.c, Copyright (c) 2019,
   Matt Burke, and code from SimH, Copyright (c) 2004-2008, Robert M Supnik

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

   tb           KLESI-B VAXBI tape adapter

   16-Nov-26    Ulli/Claude  First version (VAX 8200/8300)

   The KLESI-B (BI device type 0103) is a VAXBI node with a DWBUA-like
   adapter in front of a KLESI TMSCP port on a private, internal Unibus
   (ULTRIX V2.0 sys/vaxbi/bla.c: "looks alot like BUA").  The node has the
   DWBUA's registers (BIIC, control/status at 0x720, vector offset at 0x724,
   failed Unibus address, data path registers, 512 map registers at 0x800),
   its node window is the internal Unibus, and the TMSCP port's IP and SA
   registers are at Unibus address 774500 in it (the only device there).

   This file is the adapter: the BIIC and DWBUA registers, the map for the
   port's transfers (with the DWBUA's buffer routines, vax820_uba.c), the
   window, and the interrupts.  The TMSCP port and its drives (TU81) are
   the TQ code (pdp11_tq.c, controller 1, device TB), which calls kb_rb,
   kb_rw, kb_wb, kb_ww for memory and kb_setint, kb_clrint for its
   interrupt, and calls kb_reset from the TB reset.

   Interrupts: the port's interrupt is requested at IPL 15 (BR5) for the
   adapter's node; the vector is the vector offset register ORed with the
   port's vector (TMSCP step 1), as on the DWBUA.  The adapter's error
   interrupt uses the BIIC's error interrupt control register.

   The KLESI-B works with or without the DWBUA (SET UBA DISABLE: the
   VAXBI-only mode); the two adapters have separate windows and maps.
*/

#include "vax_defs.h"

#define DTYPE_KLESIB    0x0103                          /* KLESI-B (BI_BLA) */
#define KB_DEF_NODE     6                               /* default node (below the DEBNA's) */
#define KB_NDPATH       6                               /* data paths */
#define KB_NMAPR        512                             /* map registers */
#define KB_NMAPU        496                             /* usable */
#define KB_IPL          (0x15 - IPL_HMIN)               /* port: BR5 */
#define KB_PORT_OFS     (0x3E000 + 0x1940)              /* 774500 in the window */

/* Register offsets (longwords) and bits, as the DWBUA (vax820_uba.c) */

#define BIGPR0_UPU      0x00000001                      /* Unibus power up */
#define KBCSR_OF        0x1C8
#define KBCSR_ERR       0x80000000
#define KBCSR_BIF       0x10000000
#define KBCSR_TO        0x08000000
#define KBCSR_UIE       0x04000000
#define KBCSR_IMR       0x02000000
#define KBCSR_BDP       0x01000000
#define KBCSR_EIE       0x00100000
#define KBCSR_UPI       0x00020000
#define KBCSR_MBO       0x00008000
#define KBCSR_WR        (KBCSR_EIE)
#define KBCSR_ERRS      (KBCSR_BIF | KBCSR_TO | KBCSR_UIE | KBCSR_IMR | KBCSR_BDP)
#define KBCSR_W1C       (KBCSR_ERRS)
#define KBVO_OF         0x1C9
#define KBVO_VEC        0x00003E00
#define KBFUBAR_OF      0x1CA
#define KBBIFA_OF       0x1CB
#define KBDR_OF         0x1CC
#define KBDPR_OF        0x1D4
#define KBDPR_V_SEL     21
#define KBDPR_M_SEL     0x7
#define KBDPR_RD        (KBDPR_M_SEL << KBDPR_V_SEL)
#define KBBDPS_OF       0x1E4
#define KBMAP_OF        0x200
#define KBMAP_VLD       0x80000000
#define KBMAP_IOAD      0x40000000
#define KBMAP_LWAE      0x04000000
#define KBMAP_ODD       0x02000000
#define KBMAP_V_DP      21
#define KBMAP_M_DP      0x7
#define KBMAP_DP        (KBMAP_M_DP << KBMAP_V_DP)
#define KBMAP_PAG       0x001FFFFF
#define KBMAP_RW        (0xC6000000 | KBMAP_DP | KBMAP_PAG)

/* Debug: the TB device's flags (the TQ code uses 0x01-0x10 and the top
   four bits) */

#define DBG_KREG        0x00000100                      /* adapter registers */
#define DBG_KMAP        0x00000200                      /* map registers */
#define DBG_KINT        0x00000400                      /* interrupts */
#define DBG_KERR        0x00000800                      /* errors */

typedef t_bool (*BUA_MAPF) (uint32 ua, uint32 *ma);
extern int32 bua_rb (BUA_MAPF mapf, DEVICE *dptr, uint32 ba, int32 bc, uint8 *buf);
extern int32 bua_rw (BUA_MAPF mapf, DEVICE *dptr, uint32 ba, int32 bc, uint16 *buf);
extern int32 bua_wb (BUA_MAPF mapf, DEVICE *dptr, uint32 ba, int32 bc, const uint8 *buf);
extern int32 bua_ww (BUA_MAPF mapf, DEVICE *dptr, uint32 ba, int32 bc, const uint16 *buf);
extern uint32 nexus_req[NEXUS_HLVL];
extern DEVICE tb_dev;                                   /* pdp11_tq.c */
extern t_stat tb_rd (int32 *data, int32 PA, int32 access);
extern t_stat tb_wr (int32 data, int32 PA, int32 access);
extern int32 tb_inta (void);
extern t_stat tb_port_reset (void);

t_stat kb_rdreg (int32 *val, int32 pa, int32 lnt);
t_stat kb_wrreg (int32 val, int32 pa, int32 lnt);
int32 kb_ack (void);
t_stat kb_reset (DEVICE *dptr);
void kb_clrint (void);

/* Adapter state */

DIB tb_dib = { KB_DEF_NODE, 0, &kb_rdreg, &kb_wrreg, 0, 0, 0, { &kb_ack } };
uint32 kb_winbase = 0;                                  /* window, 0 = disabled */
BIIC kb_biic;                                           /* BIIC registers */
uint32 kb_csr = 0;                                      /* control/status */
uint32 kb_vo = 0;                                       /* vector offset */
uint32 kb_int = 0;                                      /* adapter (error) int */
uint32 kb_pint = 0;                                     /* port int request */
uint32 kb_fubar = 0;                                    /* failed Unibus addr */
uint32 kb_bifa = 0;                                     /* failed BI addr */
uint32 kb_dpr[KB_NDPATH] = { 0 };                       /* data paths */
uint32 kb_map[KB_NMAPR] = { 0 };                        /* map registers */
static int32 kb_node_req = -1;                          /* node of the requests */

static t_bool kb_map_addr (uint32 ua, uint32 *ma);

/* Node, interrupts */

static int32 kb_node (void)
{
return (int32) (tb_dib.ba & 0xF);
}

/* Present the requests: the adapter interrupt at the levels in the BIIC's
   error interrupt control register, the port's at IPL 15 */

static void kb_eval_int (void)
{
int32 i, lvl, node = kb_node ();

for (i = 0; i < NEXUS_HLVL; i++) {                      /* clear our requests */
    if (kb_node_req >= 0)
        nexus_req[i] &= ~(1u << kb_node_req);
    nexus_req[i] &= ~(1u << node);
    }
kb_node_req = node;
if (tb_dev.flags & DEV_DIS)
    return;
if (kb_int) {
    lvl = (kb_biic.eicr >> BIECR_V_LVL) & BIECR_M_LVL;
    for (i = 0; i < NEXUS_HLVL; i++)
        if (lvl & (1u << i))
            nexus_req[i] |= (1u << node);
    }
if (kb_pint)
    nexus_req[KB_IPL] |= (1u << node);
}

/* Interrupt acknowledge (nexusA): the request at the highest level wins */

int32 kb_ack (void)
{
int32 lvl, i, elvl = -1;
int32 vec;

if (kb_int) {
    lvl = (kb_biic.eicr >> BIECR_V_LVL) & BIECR_M_LVL;
    for (i = NEXUS_HLVL - 1; i >= 0; i--)
        if (lvl & (1u << i)) {
            elvl = i;
            break;
            }
    }
if (kb_int && (!kb_pint || (elvl >= KB_IPL))) {         /* adapter error */
    vec = kb_biic.eicr & BIECR_VEC;
    kb_int = 0;
    }
else if (kb_pint) {                                     /* the port */
    kb_pint = 0;
    vec = (kb_vo & KBVO_VEC) | tb_inta ();
    }
else vec = 0;
sim_debug (DBG_KINT, &tb_dev, "interrupt acknowledge, vector %03X\n", vec);
kb_eval_int ();                                         /* others stay */
return vec;
}

void kb_setint (void)
{
kb_pint = 1;
kb_eval_int ();
}

void kb_clrint (void)
{
kb_pint = 0;
kb_eval_int ();
}

static void kb_adap_set_int (void)
{
if (kb_csr & KBCSR_EIE) {
    kb_int = 1;
    kb_eval_int ();
    }
}

/* Memory access of the port through the map */

int32 kb_rb (uint32 ba, int32 bc, uint8 *buf)
{
return bua_rb (&kb_map_addr, &tb_dev, ba, bc, buf);
}

int32 kb_rw (uint32 ba, int32 bc, uint16 *buf)
{
return bua_rw (&kb_map_addr, &tb_dev, ba, bc, buf);
}

int32 kb_wb (uint32 ba, int32 bc, const uint8 *buf)
{
return bua_wb (&kb_map_addr, &tb_dev, ba, bc, buf);
}

int32 kb_ww (uint32 ba, int32 bc, const uint16 *buf)
{
return bua_ww (&kb_map_addr, &tb_dev, ba, bc, buf);
}

/* Map a Unibus address (as the DWBUA, uba_map_addr) */

static t_bool kb_map_addr (uint32 ua, uint32 *ma)
{
uint32 ublk = ua >> VA_V_VPN, umap;

if (ublk >= KB_NMAPR)
    return FALSE;
umap = kb_map[ublk];
if (umap == 0xFFFFFFFF)
    return FALSE;
if (umap & KBMAP_VLD) {
    *ma = ((umap & KBMAP_PAG) << VA_V_VPN) + VA_GETOFF (ua);
    if ((umap & KBMAP_DP) && (umap & KBMAP_ODD))        /* buffered dp, odd */
        *ma = *ma + 1;
    if (ADDR_IS_MEM (*ma))
        return TRUE;
    if ((kb_biic.ber & BIBER_BTO) == 0) {
        kb_biic.ber |= BIBER_BTO;
        kb_bifa = *ma;
        kb_adap_set_int ();
        }
    sim_debug (DBG_KERR, &tb_dev, "BI nxm, ba = %X\n", *ma);
    return FALSE;
    }
if ((kb_csr & KBCSR_IMR) == 0) {
    kb_csr |= KBCSR_IMR;
    kb_adap_set_int ();
    }
sim_debug (DBG_KERR, &tb_dev, "invalid map register, ua = %X\n", ua);
return FALSE;
}

/* The window: the internal Unibus.  Only the port's IP (774500) and SA
   (774502) respond; byte or aligned word accesses, as the DWBUA. */

static void kb_ub_nxm (uint32 ua)
{
if ((kb_csr & KBCSR_TO) == 0) {
    kb_csr |= KBCSR_TO;
    kb_fubar = (ua >> 2) & 0xFFFF;
    kb_adap_set_int ();
    }
sim_debug (DBG_KERR, &tb_dev, "Unibus nxm, ua = %X\n", ua);
}

int32 kb_rdio (uint32 pa, int32 lnt)
{
uint32 ua = pa - kb_winbase;
int32 val = 0;

if ((lnt == L_BYTE) || ((lnt == L_WORD) && ((pa & 1) == 0))) {
    if ((ua & ~3u) == KB_PORT_OFS)
        tb_rd (&val, (int32) ua, READ);
    else {
        kb_biic.ber |= BIBER_RDS;
        kb_ub_nxm (ua);
        }
    if (pa & 2)
        val = val << 16;
    }
else sim_debug (DBG_KERR, &tb_dev, "invalid read, pa = %X, lnt = %d\n", pa, lnt);
SET_IRQL;
return val;
}

void kb_wrio (uint32 pa, int32 val, int32 lnt)
{
uint32 ua = pa - kb_winbase;
int32 mode;

if (lnt == L_BYTE)
    mode = WRITEB;
else if (((lnt == L_WORD) || (lnt == L_LONG)) && ((pa & 1) == 0))
    mode = WRITE;
else {
    sim_debug (DBG_KERR, &tb_dev, "invalid write, pa = %X, lnt = %d\n", pa, lnt);
    return;
    }
if ((ua & ~3u) == KB_PORT_OFS)
    tb_wr (val, (int32) ua, mode);
else kb_ub_nxm (ua);
SET_IRQL;
}

/* Adapter registers (aligned longwords), as the DWBUA */

t_stat kb_rdreg (int32 *val, int32 pa, int32 lnt)
{
int32 idx, ofs = NEXUS_GETOFS (pa);

if ((ofs >= KBBDPS_OF) && (ofs < KBBDPS_OF + 0x10)) {
    *val = 0;
    return SCPE_OK;
    }
if (ofs >= KBMAP_OF) {
    idx = ofs - KBMAP_OF;
    if (idx >= KB_NMAPR)
        return SCPE_NXM;
    *val = kb_map[idx] & KBMAP_RW;
    sim_debug (DBG_KMAP, &tb_dev, "map %d read, value = %X\n", idx, *val);
    return SCPE_OK;
    }
switch (ofs) {

    case BI_DTYPE:
        *val = DTYPE_KLESIB;
        break;

    case BI_CSR:
        *val = kb_biic.csr & BICSR_RD;
        break;

    case BI_BER:
        *val = kb_biic.ber & BIBER_RD;
        break;

    case BI_EICR:
        *val = kb_biic.eicr & BIECR_RD;
        break;

    case BI_IDEST:
        *val = kb_biic.idest & BIID_RD;
        break;

    case BI_IMSK:
    case BI_FIDEST:
    case BI_ISRC:
    case BI_GPR1:
    case BI_GPR2:
    case BI_GPR3:
        *val = 0;
        break;

    case BI_SA:
        *val = kb_biic.sa;
        break;

    case BI_EA:
        *val = kb_biic.ea;
        break;

    case BI_BCIC:
        *val = kb_biic.bcic & BIBCI_RD;
        break;

    case BI_UIIC:
        *val = kb_biic.uiic;
        break;

    case BI_GPR0:
        *val = kb_biic.gpr0;
        break;

    case KBCSR_OF:
        *val = kb_csr | KBCSR_MBO;
        if (kb_csr & KBCSR_ERRS)
            *val |= KBCSR_ERR;
        break;

    case KBVO_OF:
        *val = kb_vo & KBVO_VEC;
        break;

    case KBFUBAR_OF:
        *val = kb_fubar & 0xFFFF;
        break;

    case KBBIFA_OF:
        *val = kb_bifa;
        break;

    case KBDR_OF + 0: case KBDR_OF + 1: case KBDR_OF + 2:
    case KBDR_OF + 3: case KBDR_OF + 4:
        *val = 0;
        break;

    case KBDPR_OF + 0: case KBDPR_OF + 1: case KBDPR_OF + 2:
    case KBDPR_OF + 3: case KBDPR_OF + 4: case KBDPR_OF + 5:
        *val = kb_dpr[ofs - KBDPR_OF] & KBDPR_RD;
        break;

    default:
        return SCPE_NXM;
        }
sim_debug (DBG_KREG, &tb_dev, "reg %X read, value = %X\n", ofs, *val);
return SCPE_OK;
}

t_stat kb_wrreg (int32 val, int32 pa, int32 lnt)
{
int32 idx, ofs = NEXUS_GETOFS (pa);

if (ofs >= KBMAP_OF) {
    idx = ofs - KBMAP_OF;
    if (idx >= KB_NMAPR)
        return SCPE_NXM;
    kb_map[idx] = val & KBMAP_RW;
    sim_debug (DBG_KMAP, &tb_dev, "map %d write, value = %X\n", idx, val);
    return SCPE_OK;
    }
switch (ofs) {

    case BI_CSR:
        /* Start self-test (ULTRIX bisst, buainit): the adapter and the
           port are initialized, as the DWBUA does with its Unibus */
        if (val & BICSR_RST) {
            kb_reset (&tb_dev);
            tb_port_reset ();
            }
        kb_biic.csr = (kb_biic.csr & ~BICSR_RW) | (val & BICSR_RW);
        break;

    case BI_BER:
        kb_biic.ber = kb_biic.ber & ~(val & BIBER_W1C);
        break;

    case BI_EICR:
        kb_biic.eicr = (kb_biic.eicr & ~BIECR_RW) | (val & BIECR_RW);
        kb_biic.eicr = kb_biic.eicr & ~(val & BIECR_W1C);
        kb_eval_int ();
        break;

    case BI_IDEST:
        kb_biic.idest = val & BIID_RW;
        break;

    case BI_BCIC:
        kb_biic.bcic = val & BIBCI_RW;
        break;

    case BI_UIIC:
    case BI_GPR0: case BI_GPR1: case BI_GPR2: case BI_GPR3:
        break;

    case KBCSR_OF:
        if (val & KBCSR_UPI)                            /* Unibus power init */
            tb_port_reset ();
        kb_csr = (kb_csr & ~KBCSR_WR) | (val & KBCSR_WR);
        kb_csr = kb_csr & ~(val & KBCSR_W1C);
        if (!(kb_csr & KBCSR_EIE))
            kb_int = 0;
        kb_eval_int ();
        break;

    case KBVO_OF:
        kb_vo = val & KBVO_VEC;
        break;

    case KBDPR_OF + 0: case KBDPR_OF + 1: case KBDPR_OF + 2:
    case KBDPR_OF + 3: case KBDPR_OF + 4: case KBDPR_OF + 5:
        break;

    default:
        return SCPE_NXM;
        }
sim_debug (DBG_KREG, &tb_dev, "reg %X write, value = %X\n", ofs, val);
return SCPE_OK;
}

/* Reset the adapter (TB reset, self-test); the window follows the node
   and the enable state */

t_stat kb_reset (DEVICE *dptr)
{
int32 i, node = kb_node ();

kb_int = kb_pint = 0;
for (i = 0; i < KB_NMAPR; i++)
    kb_map[i] = (i < KB_NMAPU) ? 0 : 0xFFFFFFFF;
for (i = 0; i < KB_NDPATH; i++)
    kb_dpr[i] = (i << KBDPR_V_SEL);
kb_csr = 0;
kb_fubar = kb_bifa = 0;
kb_biic.csr = (1u << BICSR_V_IF) | BICSR_STS | (node & BICSR_NODE);
kb_biic.ber = 0;
kb_biic.eicr = 0;
kb_biic.idest = 0;
kb_biic.sa = WINADDR (node);
kb_biic.ea = WINADDR (node) + WINSIZE;
kb_biic.uiic = BIICR_EXV;
kb_biic.gpr0 = BIGPR0_UPU;
kb_winbase = (dptr->flags & DEV_DIS) ? 0 : WINADDR (node);
kb_eval_int ();
return SCPE_OK;
}

/* SET/SHOW TB NODE */

t_stat kb_set_node (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
t_stat r;
int32 node;

if ((r = bi_get_node (cptr, &tb_dev, -1, &node)) != SCPE_OK)
    return r;                                           /* 0-15, free */
if ((r = bi_set_node_rules (&tb_dev, node)) != SCPE_OK)
    return r;                                           /* realism: below the DEBNA */
kb_clrint ();
tb_dib.ba = (uint32) node;
return kb_reset (&tb_dev);
}

t_stat kb_show_node (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "node=%d", kb_node ());
return SCPE_OK;
}

t_stat kb_show_vec (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "vector offset=%X", kb_vo & KBVO_VEC);
return SCPE_OK;
}
