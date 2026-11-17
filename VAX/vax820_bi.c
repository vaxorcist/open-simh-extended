/* vax820_bi.c: VAX 8200 BI

   Copyright (c) 2019, Matt Burke
   This module incorporates code from SimH, Copyright (c) 2004-2008, Robert M Supnik

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

   This module contains the VAX 8200 system-specific registers and devices.

   bi                   bus controller
*/

#include "vax_defs.h"
#include "sim_sound.h"

#ifdef DONT_USE_INTERNAL_ROM
#define BOOT_CODE_FILENAME "vmb.exe"
#else /* !DONT_USE_INTERNAL_ROM */
#include "vax_vmb_exe.h" /* Defines BOOT_CODE_FILENAME and BOOT_CODE_ARRAY, etc */
#endif /* DONT_USE_INTERNAL_ROM */

/* KA820 specific IPRs */

/* Writeable control store */

#define WCSA_RW         0x3FFFFF                        /* writeable */
#define WCSD_RD_VAL     0xFF                            /* fixed read val */
#define WCSD_WR         0xFFFFFFFF                      /* write */
#define MBRK_RW         0x1FFF                          /* microbreak */

/* Accelerator control/status register */

#define ACCS_ON         0x1                             /* enable F-chip */ 
#define ACCS_RW         (ACCS_ON)

/* KA820 boot device definitions */

struct boot_dev {
    const char          *name;
    int32               code;
    int32               let;
    };

uint32 wcs_addr = 0;
uint32 wcs_data = 0;
uint32 nexus_req[NEXUS_HLVL];                           /* nexus int req */
int32 ipr_int = 0;
int32 rxcd_int = 0;
int32 ipir = 0;
int32 accs = 0;
int32 sys_model = 0;
int32 mchk_flag[KA_NUM] = { 0 };
char cpu_boot_cmd[CBUFSIZE]  = { 0 };                   /* boot command */

static t_stat (*nexusR[NEXUS_NUM])(int32 *dat, int32 ad, int32 md);
static t_stat (*nexusW[NEXUS_NUM])(int32 dat, int32 ad, int32 md);
static int32 (*nexusA[NEXUS_NUM])(void);                /* BI node supplies its vector */

static struct boot_dev boot_tab[] = {
    { "HK", BOOT_HK, 0 },
    { "RL", BOOT_RL, 0 },
    { "RQ", BOOT_UDA, 1 << 24 },
    { "RQB", BOOT_UDA, 1 << 24 },
    { "RQC", BOOT_UDA, 1 << 24 },
    { "RQD", BOOT_UDA, 1 << 24 },
    { "CS", BOOT_CS, 0 },
    { "KDB", BOOT_BDA, 0 },                             /* KDB50: its BI node in R1 */
    { NULL }
    };

extern int32 tmr_int, tti_int, tto_int, fl_int;
extern int32 cur_cpu;

t_stat bi_reset (DEVICE *dptr);
t_stat bi_show_nodes (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
static t_stat bi_count_rules (const char *prefix);
static t_stat bi_rlz_enable_check (t_bool apply);
extern DEVICE mctl_dev;
extern void mctl_board_text (int32 i, char *buf, size_t lnt);
t_stat vax820_boot (int32 flag, CONST char *ptr);
t_stat vax820_boot_parse (int32 flag, CONST char *ptr);
t_stat cpu_boot (int32 unitno, DEVICE *dptr);

extern void uba_eval_int (void);
extern int32 uba_get_ubvector (int32 lvl);
extern int32 iccs_rd (void);
extern int32 nicr_rd (void);
extern int32 icr_rd (void);
extern int32 todr_rd (void);
extern int32 rxcs_rd (void);
extern int32 rxdb_rd (void);
extern int32 txcs_rd (void);
extern int32 rxcd_rd (void);
extern int32 pcsr_rd (int32 pa);
extern int32 fl_rd (int32 pa);
extern void iccs_wr (int32 dat);
extern void nicr_wr (int32 dat);
extern void todr_wr (int32 dat);
extern void rxcs_wr (int32 dat);
extern void txcs_wr (int32 dat);
extern void txdb_wr (int32 dat);
extern void rxcd_wr (int32 val);
extern void pcsr_wr (int32 pa, int32 val, int32 lnt);
extern void fl_wr (int32 pa, int32 val, int32 lnt);
extern void init_ubus_tab (void);
extern t_stat build_ubus_tab (DEVICE *dptr, DIB *dibp);

/* BI data structures

   bi_dev       BI device descriptor
   bi_unit      BI unit
   bi_reg       BI register list
*/

UNIT bi_unit = { UDATA (NULL, 0, 0) };

REG bi_reg[] = {
    { HRDATA (NREQ14, nexus_req[0], 16) },
    { HRDATA (NREQ15, nexus_req[1], 16) },
    { HRDATA (NREQ16, nexus_req[2], 16) },
    { HRDATA (NREQ17, nexus_req[3], 16) },
    { HRDATA (WCSA, wcs_addr, 21) },
    { HRDATA (WCSD, wcs_data, 32) },
    { NULL }
    };

MTAB bi_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "NODES", NULL,
      NULL, &bi_show_nodes, NULL, "Display the VAXBI nodes" },
    { 0 }
    };

DEVICE bi_dev = {
    "BI", &bi_unit, bi_reg, bi_mod,
    1, 16, 16, 1, 16, 8,
    NULL, NULL, &bi_reset,
    NULL, NULL, NULL,
    NULL, 0
    };

/* Special boot command, overrides regular boot */

CTAB vax820_cmd[] = {
    { "BOOT", &vax820_boot, RU_BOOT,
      "bo{ot} <device>{/R5:flg} boot device\n"
      "                         type HELP CPU to see bootable devices\n", NULL, &run_cmd_message },
    { NULL }
    };

/* The VAX 8200 has three sources of interrupts

   - internal device interrupts (CPU, console, clock)
   - nexus interupts (e.g., memory controller, MBA, UBA)
   - external device interrupts (Unibus)

   Internal devices vector to fixed SCB locations.

   Nexus interrupts vector to an SCB location based on this
   formula: SCB_NEXUS + ((IPL - 0x14) * 0x40) + (TR# * 0x4)

   External device interrupts do not vector directly.
   Instead, the interrupt handler for a given UBA IPL
   reads a vector register that contains the Unibus vector
   for that IPL.
*/
/* Find highest priority vectorable interrupt */

int32 eval_int (void)
{
int32 ipl = PSL_GETIPL (PSL);
int32 cpu_msk = (1u << cur_cpu);
int32 i, t;

static const int32 sw_int_mask[IPL_SMAX] = {
    0xFFFE, 0xFFFC, 0xFFF8, 0xFFF0,                     /* 0 - 3 */
    0xFFE0, 0xFFC0, 0xFF80, 0xFF00,                     /* 4 - 7 */
    0xFE00, 0xFC00, 0xF800, 0xF000,                     /* 8 - B */
    0xE000, 0xC000, 0x8000                              /* C - E */
    };

if (hlt_pin)                                            /* hlt pin int */
    return IPL_HLTPIN;
if ((ipl < IPL_CRDERR) && crd_err)                      /* crd err int */
    return IPL_CRDERR;
if ((ipl < IPL_CLKINT) && (tmr_int & cpu_msk))          /* its clock int */
    return IPL_CLKINT;
if (cur_cpu != 0) {                                     /* attached processor: */
    if ((ipl < IPL_IPRINT) && (ipir & cpu_msk))         /* only its IPI, its */
        return IPL_IPRINT;                              /* console output and */
    if ((ipl < IPL_TTINT) && (tto_int & cpu_msk))       /* software interrupts; */
        return IPL_TTINT;                               /* device, RXCD, floppy */
    goto swint;                                         /* and terminal interrupts */
    }                                                   /* go to the primary */
uba_eval_int ();                                        /* update UBA */
for (i = IPL_HMAX; i >= IPL_HMIN; i--) {                /* chk hwre int */
    if (i <= ipl)                                       /* at ipl? no int */
        return 0;
    if (nexus_req[i - IPL_HMIN])                        /* req != 0? int */
        return i;
    }
if ((ipl < IPL_RXCDINT) && rxcd_int)                    /* rxcd int */
    return IPL_RXCDINT;
if ((ipl < IPL_IPRINT) && (ipir & cpu_msk))             /* ipr int */
    return IPL_IPRINT;
if ((ipl < IPL_FLINT) && fl_int)                        /* console floppy int */
    return IPL_FLINT;
if ((ipl < IPL_TTINT) && (tti_int && (cur_cpu == 0)))   /* console int */
    return IPL_TTINT;
if ((ipl < IPL_TTINT) && (tto_int & cpu_msk))           /* console int */
    return IPL_TTINT;
swint:
if (ipl >= IPL_SMAX)                                    /* ipl >= sw max? */
    return 0;
if ((t = SISR & sw_int_mask[ipl]) == 0)
    return 0;                                           /* eligible req */
for (i = IPL_SMAX; i > ipl; i--) {                      /* check swre int */
    if ((t >> i) & 1)                                   /* req != 0? int */
        return i;
    }
return 0;
}

/* Return vector for highest priority hardware interrupt at IPL lvl */

int32 get_vector (int32 lvl)
{
int32 i, l;
int32 vec;
int32 cpu_msk = (1u << cur_cpu);

if (lvl == IPL_CRDERR) {                                /* CRD error? */
    crd_err = 0;
    return SCB_CRDERR;
    }
if (lvl == IPL_CLKINT) {                                /* clock? */
    tmr_int = tmr_int & ~(1u << cur_cpu);               /* clear req */
    return SCB_INTTIM;                                  /* return vector */
    }
if (lvl > IPL_HMAX) {                                   /* error req lvl? */
    ABORT (STOP_UIPL);                                  /* unknown intr */
    }
if ((lvl <= IPL_HMAX) && (lvl >= IPL_HMIN)) {           /* nexus? */
    l = lvl - IPL_HMIN;
    if (nexus_req[l] & (1u << uba_node ())) {           /* unibus int? */
        nexus_req[l] = nexus_req[l] & ~(1u << uba_node ());
        vec = uba_get_ubvector(l);
        return vec;                                     /* return vector */
        }
    for (i = 0; nexus_req[l] && (i < NEXUS_NUM); i++) {
        if ((nexus_req[l] >> i) & 1) {
            nexus_req[l] = nexus_req[l] & ~(1u << i);
            if (nexusA[i])                              /* BI node (DMB32): */
                return nexusA[i] ();                    /* its own vector */
            vec = SCB_NEXUS + (l << 6) + (i << 2);
            return vec;                                 /* return vector */
            }
        }
    }
if (lvl == IPL_RXCDINT) {
    if (rxcd_int) {
        rxcd_int = 0;                                   /* clear req */
        return SCB_RXCD;                                /* return vector */
        }
    }
if (lvl == IPL_IPRINT) {                                /* inter-processor? */
    if (ipir & (1u << cur_cpu)) {
        ipir = ipir & ~(1u << cur_cpu);                 /* clear req */
        return SCB_IPRINT;                              /* return vector */
        }
    }
if (lvl == IPL_FLINT) {                                 /* console floppy? */
    if (fl_int) {
        fl_int = 0;                                     /* clear req */
        return SCB_FLINT;                               /* return vector */
        }
    }
if (lvl == IPL_TTINT) {                                 /* console? */
    if (tti_int && (cur_cpu == 0)) {                    /* input? */
        tti_int = 0;                                    /* clear req */
        return SCB_TTI;                                 /* return vector */
        }
    if (tto_int & cpu_msk) {                            /* output? */
        tto_int = tto_int & ~cpu_msk;                   /* clear its req */
        return SCB_TTO;                                 /* return vector */
        }
    }
return 0;
}

/* Read 8200-specific IPR's */

int32 ReadIPR (int32 rg)
{
int32 val;

switch (rg) {

    case MT_ICCS:                                       /* ICCS */
        val = iccs_rd ();
        break;

    case MT_NICR:                                       /* NICR */
        val = nicr_rd ();
        break;

    case MT_ICR:                                        /* ICR */
        val = icr_rd ();
        break;

    case MT_TODR:                                       /* TODR */
        val = todr_rd ();
        break;

    case MT_RXCS:                                       /* RXCS */
        if (cur_cpu == 0)
            val = rxcs_rd ();
        else
            val = 0;
        break;

    case MT_RXDB:                                       /* RXDB */
        if (cur_cpu == 0)
            val = rxdb_rd ();
        else
            val = 0;
        break;

    case MT_TXCS:                                       /* TXCS */
        val = txcs_rd ();
        break;

    case MT_TBDR:                                       /* TBDR (not impl) */
        val = 0;
        break;

    case MT_CADR:                                       /* CADR (not impl) */
        val = 0;
        break;

    case MT_ACCS:                                       /* ACCS */
        val = accs & ACCS_RW;
        break;

    case MT_WCSA:                                       /* WCSA */
        val = wcs_addr & WCSA_RW;
        break;

    case MT_WCSD:                                       /* WCSD */
        val = WCSD_RD_VAL;
        break;

    case MT_SID:                                        /* SID */
        if (sys_model)
            val = VAX820_SID | VAX820_REV | VAX820_PATCH | VAX825_TYP | VAX820_UCODE;
        else
            val = VAX820_SID | VAX820_REV | VAX820_PATCH | VAX820_TYP | VAX820_UCODE;
        break;

    case MT_RXCS1:                                      /* RXCS1 */
        val = 0;
        break;

    case MT_RXDB1:                                      /* RXDB1 */
        val = 0;
        break;

    case MT_TXCS1:                                      /* TXCS1 */
        val = CSR_DONE;
        break;

    case MT_RXCS2:                                      /* RXCS2 */
        val = 0;
        break;

    case MT_RXDB2:                                      /* RXDB2 */
        val = 0;
        break;

    case MT_TXCS2:                                      /* TXCS2 */
        val = CSR_DONE;
        break;

    case MT_RXCS3:                                      /* RXCS3 */
        val = 0;
        break;

    case MT_RXDB3:                                      /* RXDB3 */
        val = 0;
        break;

    case MT_TXCS3:                                      /* TXCS3 */
        val = CSR_DONE;
        break;

    case MT_BINID:                                      /* BINID */
        val = ka_node (cur_cpu);
        break;

    case MT_RXCD:                                       /* RXCD */
        val = rxcd_rd ();
        break;

    default:
        RSVD_OPND_FAULT(ReadIPR);
        }

return val;
}

/* Write 8200-specific IPR's */

void WriteIPR (int32 rg, int32 val)
{
switch (rg) {

    case MT_IPIR:                                       /* IPIR: a node mask */
        ipir = 0;                                       /* (as processor mask) */
        if ((val >> ka_node (0)) & 1)
            ipir |= 1;
        if ((val >> ka_node (1)) & 1)
            ipir |= 2;
        break;

    case MT_ICCS:                                       /* ICCS */
        iccs_wr (val);
        break;

    case MT_NICR:                                       /* NICR */
        nicr_wr (val);
        break;

    case MT_TODR:                                       /* TODR */
        todr_wr (val);
        break;

    case MT_RXCS:                                       /* RXCS */
        if (cur_cpu == 0)
            rxcs_wr (val);
        break;

    case MT_TXCS:                                       /* TXCS */
        txcs_wr (val);
        break;

    case MT_TXDB:                                       /* TXDB */
        txdb_wr (val);
        break;

    case MT_TBDR:                                       /* TBDR (not impl) */
        break;

    case MT_CADR:                                       /* CADR (not impl) */
        break;

    case MT_MCESR:                                      /* MCESR */
        mchk_flag[cur_cpu] = 0;
        break;

    case MT_ACCS:                                       /* ACCS */
        accs = val & ACCS_RW;
        break;

    case MT_WCSA:                                       /* WCSA */
        wcs_addr = val & WCSA_RW;
        break;

    case MT_WCSL:                                       /* WCSL */
        wcs_data = val & WCSD_WR;
        break;

    case MT_RXCS1:                                      /* RXCS1 */
    case MT_TXCS1:                                      /* TXCS1 */
    case MT_TXDB1:                                      /* TXDB1 */
    case MT_RXCS2:                                      /* RXCS2 */
    case MT_TXCS2:                                      /* TXCS2 */
    case MT_TXDB2:                                      /* TXDB2 */
    case MT_RXCS3:                                      /* RXCS3 */
    case MT_TXCS3:                                      /* TXCS3 */
    case MT_TXDB3:                                      /* TXDB3 */
    case MT_CACHEX:                                     /* CACHEX */
    case MT_BISTOP:                                     /* BISTOP */
        break;

    case MT_RXCD:                                       /* RXCD */
        rxcd_wr (val);
        break;

    default:
        RSVD_OPND_FAULT(WriteIPR);
        }

return;
}

struct reglink {                                        /* register linkage */
    uint32      low;                                    /* low addr */
    uint32      high;                                   /* high addr */
    int32       (*read)(int32 pa);                      /* read routine */
    void        (*write)(int32 pa, int32 val, int32 lnt); /* write routine */
    };

extern int32 ka_priv_rd (int32 pa);
extern void ka_priv_wr (int32 pa, int32 val, int32 lnt);

struct reglink regtable[] = {
    { WATCHBASE, WATCHBASE+WATCHSIZE, &wtc_rd_pa, &wtc_wr_pa },
    { 0x20080000, 0x20080100, &ka_priv_rd, &ka_priv_wr },  /* local BIIC */
    { 0x20088000, 0x20088004, &pcsr_rd, &pcsr_wr },
    { 0x200B0000, 0x200B0020, &fl_rd, &fl_wr },
    { 0, 0, NULL, NULL }
    };

/* ReadReg - read register space

   Inputs:
        pa      =       physical address
        lnt     =       length (BWLQ)
   Output:
        longword of data
*/

int32 ReadReg (uint32 pa, int32 lnt)
{
int32 nexus, val;
struct reglink *p;

if (ADDR_IS_REG (pa)) {                                 /* reg space? */
    nexus = NEXUS_GETNEX (pa);                          /* get nexus */
    if (nexusR[nexus] &&                                /* valid? */
        (nexusR[nexus] (&val, pa, lnt) == SCPE_OK)) {
        SET_IRQL;
        return val;
        }
    MACH_CHECK (MCHK_BIERR);                            /* machine check */
    return 0;
    }
for (p = &regtable[0]; p->low != 0; p++) {
    if ((pa >= p->low) && (pa < p->high) && p->read)
        return p->read (pa);
    }
MACH_CHECK (MCHK_BIERR);                                /* machine check */
return 0;
} 

/* WriteReg - write register space

   Inputs:
        pa      =       physical address
        val     =       data to write, right justified in 32b longword
        lnt     =       length (BWLQ)
   Outputs:
        none
*/

void WriteReg (uint32 pa, int32 val, int32 lnt)
{
int32 nexus;
struct reglink *p;

if (ADDR_IS_REG (pa)) {                                 /* reg space? */
    nexus = NEXUS_GETNEX (pa);                          /* get nexus */
    if (nexusW[nexus] &&                                /* valid? */
        (nexusW[nexus] (val, pa, lnt) == SCPE_OK)) {
        SET_IRQL;
        return;
        }
    }
for (p = &regtable[0]; p->low != 0; p++) {
    if ((pa >= p->low) && (pa < p->high) && p->write) {
        p->write (pa, val, lnt);  
        return;
        }
    }
return;
}

/* Machine check

   Error status word format
   <2:0> =      ASTLVL
   <3> =        PME
   <6:4> =      arith trap code
   Rest will be zero
*/

int32 machine_check (int32 p1, int32 opc, int32 cc, int32 delta)
{
int32 acc;

if (mchk_flag[cur_cpu])                                 /* double error? */
    ABORT (STOP_INIE);                                  /* halt */
mchk_flag[cur_cpu] = 1;
if (in_ie)                                              /* in exc? panic */
    ABORT (STOP_INIE);
cc = intexc (SCB_MCHK, cc, 0, IE_SVE);                  /* take exception */
acc = ACC_MASK (KERN);                                  /* in kernel mode */
in_ie = 1;
SP = SP - 36;                                           /* push 8 words */
Write (SP, 32, L_LONG, WA);                             /* # bytes */
Write (SP + 4, p1, L_LONG, WA);                         /* mcheck type */
Write (SP + 8, 0, L_LONG, WA);                          /* parameter 1 */
Write (SP + 12, mchk_va, L_LONG, WA);                   /* VA */
Write (SP + 16, mchk_va, L_LONG, WA);                   /* VA prime */
Write (SP + 20, 0, L_LONG, WA);                         /* memory address */
Write (SP + 24, 0x00400000, L_LONG, WA);                /* status word */
Write (SP + 28, PC, L_LONG, WA);                        /* PC at failure */
Write (SP + 32, 0, L_LONG, WA);                         /* uPC at failure */
in_ie = 0;
return cc;
}

/* Console entry - only reached if CONHALT is set (AUTORESTART is set */

int32 con_halt (int32 code, int32 cc)
{
#if defined (VAX_MP)
if (cur_cpu != 0) {                                     /* attached processor: */
    extern t_bool cpu_mp_stop (int32 cpu, int32 code);  /* only it halts (console */
    cpu_mp_stop (cur_cpu, code);                        /* mode); never a reboot */
    return cc;
    }
#endif
if ((cpu_boot_cmd[0] == 0) ||                           /* saved boot cmd? */
    (vax820_boot_parse (0, cpu_boot_cmd) != SCPE_OK) || /* reparse the boot cmd */ 
    (reset_all (0) != SCPE_OK) ||                       /* reset the world */
    (cpu_boot (0, NULL) != SCPE_OK))                    /* set up boot code */
    ABORT (STOP_BOOT);                                  /* any error? */
sim_printf ("Rebooting...\n");
return cc;
}

/* Special boot command - linked into SCP by initial reset

   Syntax: BOOT <device>{/R5:val}

   Sets up R0-R5, calls SCP boot processor with effective BOOT CPU
*/

t_stat vax820_boot (int32 flag, CONST char *ptr)
{
t_stat r;

r = vax820_boot_parse (flag, ptr);                      /* parse the boot cmd */
if (r != SCPE_OK) {                                     /* error? */
    if (r >= SCPE_BASE) {                               /* message available? */
        sim_printf ("%s\n", sim_error_text (r));
        r |= SCPE_NOMESSAGE;
        }
    return r;
    }
strncpy (cpu_boot_cmd, ptr, CBUFSIZE-1);                /* save for reboot */
return run_cmd (flag, "CPU");
}

t_stat vax820_boot_parse (int32 flag, CONST char *ptr)
{
char gbuf[CBUFSIZE];
char *slptr;
const char *regptr;
int32 i, r5v, unitno;
uint32 ba;
DEVICE *dptr;
UNIT *uptr;
DIB *dibp;
t_stat r;

if (!ptr || !*ptr)
    return SCPE_2FARG;
if ((r = bi_count_rules ("BOOT: ")) != SCPE_OK)        /* VAXBI configuration */
    return r;
if (sim_sound_enabled () &&
    ((r = bi_realism_rules (NULL, 0, "BOOT: ")) != SCPE_OK))
    return r;
if ((ptr = get_sim_sw (ptr)) == NULL)                   /* get switches */
    return SCPE_INVSW;
regptr = get_glyph (ptr, gbuf, 0);                      /* get glyph */
if ((slptr = strchr (gbuf, '/'))) {                     /* found slash? */
    regptr = strchr (ptr, '/');                         /* locate orig */
    *slptr = 0;                                         /* zero in string */
    }
dptr = find_unit (gbuf, &uptr);                         /* find device */
if ((dptr == NULL) || (uptr == NULL))
    return SCPE_ARG;
dibp = (DIB *) dptr->ctxt;                              /* get DIB */
if (dibp == NULL)                                       /* none (console */
    ba = 0;                                             /* floppy CS): as on */
else                                                    /* the 780 and 8600 */
    ba = dibp->ba;
unitno = (int32) (uptr - dptr->units);
r5v = 0;
/* coverity[NULL_RETURNS] */
if ((strncmp (regptr, "/R5:", 4) == 0) ||
    (strncmp (regptr, "/R5=", 4) == 0) ||
    (strncmp (regptr, "/r5:", 4) == 0) ||
    (strncmp (regptr, "/r5=", 4) == 0)) {
    r5v = (int32) get_uint (regptr + 4, 16, LMASK, &r);
    if (r != SCPE_OK)
        return r;
    }
else if (*regptr != 0)
    return SCPE_ARG;
for (i = 0; boot_tab[i].name != NULL; i++) {
    if (strcmp (dptr->name, boot_tab[i].name) == 0) {
        R[0] = boot_tab[i].code;
        if (boot_tab[i].code == BOOT_BDA) {             /* BI disk adapter: */
            R[1] = (int32) (ba & 0xF);                  /* its node; no */
            R[2] = 0;                                   /* Unibus address */
            }
        else {
            R[1] = uba_node ();
            R[2] = boot_tab[i].let | (ba & UBADDRMASK);
            }
        R[3] = unitno;
        R[4] = 0;
        R[5] = r5v;
        return SCPE_OK;
        }
    }
return SCPE_NOFNC;
}

/* Bootstrap - finish up bootstrap process */

t_stat cpu_boot (int32 unitno, DEVICE *dptr)
{
t_stat r;

r = cpu_load_bootcode (BOOT_CODE_FILENAME, BOOT_CODE_ARRAY, BOOT_CODE_SIZE, FALSE, 0x200);
if (r != SCPE_OK)
    return r;
SP = PC = 512;
return SCPE_OK;
}

/* BI reset */

t_stat bi_reset (DEVICE *dptr)
{
accs = ACCS_ON;                                         /* enabled by default */
wcs_addr = 0;
wcs_data = 0;
ipr_int = 0;
rxcd_int = 0;
ipir = 0;
sim_vm_cmd = vax820_cmd;
sim_sound_register_enable_check (&bi_rlz_enable_check); /* realism: BI rules */
return SCPE_OK;
}

/* Show nexus */

t_stat show_nexus (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "nexus=%d", val);
return SCPE_OK;
}

/* Init nexus tables */

void init_nexus_tab (void)
{
uint32 i;

for (i = 0; i < NEXUS_NUM; i++) {
    nexusR[i] = NULL;
    nexusW[i] = NULL;
    nexusA[i] = NULL;
    }
return;
}

/* Build nexus tables

   Inputs:
        dptr    =       pointer to device
        dibp    =       pointer to DIB
   Outputs:
        status
*/

t_stat build_nexus_tab (DEVICE *dptr, DIB *dibp)
{
uint32 idx;

if ((dptr == NULL) || (dibp == NULL))
    return SCPE_IERR;
idx = dibp->ba;
if (idx >= NEXUS_NUM)
    return SCPE_IERR;
if ((nexusR[idx] && dibp->rd &&                         /* conflict? */
    (nexusR[idx] != dibp->rd)) ||
    (nexusW[idx] && dibp->wr &&
    (nexusW[idx] != dibp->wr))) {
    sim_printf ("Nexus %s conflict at %d\n", sim_dname (dptr), dibp->ba);
    return SCPE_STOP;
    }
if (dibp->rd)                                           /* set rd dispatch */
    nexusR[idx] = dibp->rd;
if (dibp->wr)                                           /* set wr dispatch */
    nexusW[idx] = dibp->wr;
if (dibp->ack[0])                                       /* device vector */
    nexusA[idx] = dibp->ack[0];
return SCPE_OK;
}

/* Build dib_tab from device list */

t_stat build_dib_tab (void)
{
uint32 i;
DEVICE *dptr;
DIB *dibp;
t_stat r;

init_nexus_tab ();
init_ubus_tab ();
for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {     /* loop thru dev */
    dibp = (DIB *) dptr->ctxt;                          /* get DIB */
    if (dibp && !(dptr->flags & DEV_DIS)) {             /* defined, enabled? */
        if (dptr->flags & DEV_NEXUS) {                  /* Nexus? */
            if ((r = build_nexus_tab (dptr, dibp)))     /* add to dispatch table */
                return r;
            }
        else {                                          /* no, Unibus device */
            if ((r = build_ubus_tab (dptr, dibp)))      /* add to dispatch tab */
                return r;
            }                                           /* end else */
        }                                               /* end if enabled */
    }                                                   /* end for */
return mctl_build_nexus ();                             /* the memory boards */
}

/* VAXBI configuration

   Every BI device keeps its node in its DIB's base address (SET dev
   NODE=n); the memory boards are the units of the MCTL device, with their
   own nodes (vax820_mem.c).  The helpers below are shared by all of them.

   bi_node_owner        the enabled device (or present memory board, unit
                        returned) on a node; skip/skip_unit: the one being
                        changed */

DEVICE *bi_node_owner (int32 node, DEVICE *skip, int32 skip_unit, int32 *unit)
{
int32 i, n, bn;
DEVICE *dptr;
DIB *dibp;

if (unit)
    *unit = -1;
for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
    if (dptr == &mctl_dev) {                            /* memory boards */
        for (n = 0; n < MCTL_NUM; n++) {
            if ((dptr == skip) && (n == skip_unit))
                continue;
            if (mctl_board_node (n, &bn) && (bn == node)) {
                if (unit)
                    *unit = n;
                return dptr;
                }
            }
        continue;
        }
    dibp = (DIB *) dptr->ctxt;
    if ((dptr == skip) || !(dptr->flags & DEV_NEXUS) ||
        (dptr->flags & DEV_DIS) || (dibp == NULL))
        continue;
    if ((int32) (dibp->ba & 0xF) == node)
        return dptr;
    }
return NULL;
}

/* bi_get_node: SET dev NODE=n (decimal 0-15, or a hex digit A-F as in
   DEC's documents); refused when an enabled device or a present memory
   board has the node */

t_stat bi_get_node (CONST char *cptr, DEVICE *dptr, int32 unit, int32 *node)
{
char who[16];
int32 n, ou;
DEVICE *owner;
t_stat r;

if (unit >= 0)
    snprintf (who, sizeof (who), "%s%d", dptr->name, unit);
else
    snprintf (who, sizeof (who), "%s", dptr->name);
if ((cptr == NULL) || (*cptr == 0))
    return sim_messagef (SCPE_ARG, "%s: NODE=n expected (VAXBI node 0-15)\n", who);
if ((cptr[1] == 0) && isxdigit ((unsigned char) cptr[0]) && isalpha ((unsigned char) cptr[0]))
    n = 10 + (toupper ((unsigned char) cptr[0]) - 'A');
else {
    n = (int32) get_uint (cptr, 10, NEXUS_NUM - 1, &r);
    if (r != SCPE_OK)
        return sim_messagef (SCPE_ARG, "%s: the VAXBI node is 0 to 15 (or A to F)\n", who);
    }
owner = bi_node_owner (n, dptr, unit, &ou);
if (owner != NULL) {
    if (ou >= 0)
        return sim_messagef (SCPE_ARG, "%s: VAXBI node %d is in use by %s%d\n",
                             who, n, owner->name, ou);
    return sim_messagef (SCPE_ARG, "%s: VAXBI node %d is in use by %s\n", who, n, owner->name);
    }
*node = n;
return SCPE_OK;
}

/* A node's BI device type (DTYPE <15:0>), read from the device itself;
   -1 if it has no register access */

static int32 bi_dtype (DEVICE *dptr, int32 node)
{
DIB *dibp = (DIB *) dptr->ctxt;
int32 val = 0;

if (dptr == &mctl_dev)
    return DTYPE_MS820;
if ((dibp == NULL) || (dibp->rd == NULL))
    return -1;
if (dibp->rd (&val, REGBASE + (node << REG_V_NEXUS) + (BI_DTYPE << 2), L_LONG) != SCPE_OK)
    return -1;
return val & 0xFFFF;
}

#define BI_IS_KLESIB(t) ((t) == 0x0103)                 /* KLESI-B */
#define BI_IS_DEBNA(t)  (((t) & 0x3FFF) == 0x010F)      /* DEBNT 010F, DEBNA 410F */
#define BI_IS_STOR(t)   (((t) == 0x010E) || ((t) == 0x0108) || ((t) == 0x010B))
                                                        /* KDB50, CIBCA, CIBCI */
typedef struct {
    int32       n_klesi, n_debna, n_stor;
    int32       klesi_max, debna_min;                   /* nodes */
    DEVICE      *klesi_dev, *debna_dev;
    } BI_CENSUS;

/* The enabled BI devices by kind; chg (if not NULL) counts at chg_node */

static void bi_census (DEVICE *chg, int32 chg_node, BI_CENSUS *c)
{
int32 i, t, node;
DEVICE *dptr;
DIB *dibp;

memset (c, 0, sizeof (*c));
c->klesi_max = -1;
c->debna_min = NEXUS_NUM;
for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
    dibp = (DIB *) dptr->ctxt;
    if ((dptr == &mctl_dev) || !(dptr->flags & DEV_NEXUS) ||
        ((dptr->flags & DEV_DIS) && (dptr != chg)) ||   /* (the one being */
        (dibp == NULL))                                 /* changed counts) */
        continue;
    t = bi_dtype (dptr, (int32) (dibp->ba & 0xF));
    node = (dptr == chg) ? chg_node : (int32) (dibp->ba & 0xF);
    if (BI_IS_KLESIB (t)) {
        c->n_klesi++;
        if (node > c->klesi_max) {
            c->klesi_max = node;
            c->klesi_dev = dptr;
            }
        }
    if (BI_IS_DEBNA (t)) {
        c->n_debna++;
        if (node < c->debna_min) {
            c->debna_min = node;
            c->debna_dev = dptr;
            }
        }
    if (BI_IS_STOR (t))
        c->n_stor++;
    }
}

/* Limits of the 24-slot VAXBI configuration (DEC field service rules):
   at most 3 KLESI-B, 2 DEBNT/DEBNA, 4 storage options; 16 nodes */

static t_stat bi_count_rules (const char *prefix)
{
BI_CENSUS c;

bi_census (NULL, 0, &c);
if (c.n_klesi > 3)
    return sim_messagef (SCPE_ARG, "%sat most 3 KLESI-B on the VAXBI (%d enabled)\n", prefix, c.n_klesi);
if (c.n_debna > 2)
    return sim_messagef (SCPE_ARG, "%sat most 2 DEBNT/DEBNA on the VAXBI (%d enabled)\n", prefix, c.n_debna);
if (c.n_stor > 4)
    return sim_messagef (SCPE_ARG, "%sat most 4 storage options (KDB50, CIBCA, CIBCI) on the VAXBI (%d enabled)\n",
                         prefix, c.n_stor);
return SCPE_OK;
}

/* Realism mode: every DEBNT/DEBNA on a higher node than any KLESI-B */

t_stat bi_realism_rules (DEVICE *chg, int32 chg_node, const char *prefix)
{
BI_CENSUS c;

bi_census (chg, chg_node, &c);
if ((c.n_klesi > 0) && (c.n_debna > 0) && (c.debna_min <= c.klesi_max))
    return sim_messagef (SCPE_ARG, "%sthe DEBNA %s (node %d) needs a higher VAXBI node than the "
                         "KLESI-B %s (node %d) (realism mode)\n", prefix, c.debna_dev->name,
                         c.debna_min, c.klesi_dev->name, c.klesi_max);
return SCPE_OK;
}

/* SET dev NODE=n of a BI device: the realism rule with the new node */

t_stat bi_set_node_rules (DEVICE *dptr, int32 node)
{
char prefix[32];

if (!sim_sound_enabled ())
    return SCPE_OK;
snprintf (prefix, sizeof (prefix), "%s: ", dptr->name);
return bi_realism_rules (dptr, node, prefix);
}

/* SET REALISM ENABLED: refused while the realism rule is broken (no
   changes, so the dry run and the real run are the same) */

static t_stat bi_rlz_enable_check (t_bool apply)
{
return bi_realism_rules (NULL, 0, "SET REALISM ENABLED: ");
}

/* SHOW BI: the VAXBI nodes */

static const char *bi_type_name (int32 t, DEVICE *dptr)
{
switch (t) {
    case 0x0001: return "MS820 memory";
    case 0x0102: return "DWBUA Unibus adapter";
    case 0x0103: return "KLESI-B tape adapter";
    case 0x0105: return "KA820 processor";
    case 0x0108: return "CIBCA CI adapter";
    case 0x0109: return "DMB32 communications interface";
    case 0x010B: return "CIBCI CI adapter";
    case 0x010E: return "KDB50 disk controller";
    case 0x010F: return "DEBNT Ethernet adapter";
    case 0x410F: return "DEBNA Ethernet adapter";
    default:
        break;
    }
return (dptr->description != NULL) ? dptr->description (dptr) : "";
}

t_stat bi_show_nodes (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
int32 node, i, n, cnt, used = 0, t, bn;
DEVICE *dptr, *owner;
DIB *dibp;
char buf[96], extra[96];
t_stat r;

fprintf (st, "VAXBI nodes:\n");
for (node = 0; node < NEXUS_NUM; node++) {
    owner = bi_node_owner (node, NULL, -1, &n);
    if (owner == NULL) {
        fprintf (st, "  %2d (%X)  -\n", node, node);
        continue;
        }
    used++;
    t = bi_dtype (owner, node);
    extra[0] = 0;
    if (owner == &mctl_dev) {
        mctl_board_text (n, extra, sizeof (extra));
        snprintf (buf, sizeof (buf), "MCTL%d", n);
        }
    else {
        snprintf (buf, sizeof (buf), "%s", owner->name);
        if (t == 0x0105)
            snprintf (extra, sizeof (extra), "%s", (ka_cpu_of_node (node) == 0) ? "primary" : "attached");
        if (t == 0x0102)
            snprintf (extra, sizeof (extra), "Unibus window %08X", uba_winbase);
        }
    cnt = 0;                                            /* conflicts */
    for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
        dibp = (DIB *) dptr->ctxt;
        if (dptr == &mctl_dev) {
            for (bn = 0; bn < MCTL_NUM; bn++) {
                int32 nn;
                if (mctl_board_node (bn, &nn) && (nn == node))
                    cnt++;
                }
            continue;
            }
        if ((dptr->flags & DEV_NEXUS) && !(dptr->flags & DEV_DIS) &&
            (dibp != NULL) && ((int32) (dibp->ba & 0xF) == node))
            cnt++;
        }
    fprintf (st, "  %2d (%X)  %-7s ", node, node, buf);
    if (t >= 0)
        fprintf (st, "%04X  ", t);
    else
        fprintf (st, "      ");
    fprintf (st, "%s%s%s%s\n", bi_type_name (t, owner), extra[0] ? ", " : "", extra,
             (cnt > 1) ? "  ** CONFLICT: more than one device on this node **" : "");
    }
fprintf (st, "  %d of %d nodes in use\n", used, NEXUS_NUM);
buf[0] = 0;                                             /* disabled BI devices */
for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
    dibp = (DIB *) dptr->ctxt;
    if ((dptr == &mctl_dev) || !(dptr->flags & DEV_NEXUS) ||
        !(dptr->flags & DEV_DIS) || (dibp == NULL))
        continue;
    snprintf (extra, sizeof (extra), "%s%s (node %d)", buf[0] ? ", " : "",
              dptr->name, (int32) (dibp->ba & 0xF));
    if ((strlen (buf) + strlen (extra)) < sizeof (buf))
        strcat (buf, extra);
    }
if (buf[0])
    fprintf (st, "  disabled: %s\n", buf);
r = bi_count_rules ("  rule broken: ");
if ((r == SCPE_OK) && sim_sound_enabled ())
    r = bi_realism_rules (NULL, 0, "  realism rule broken: ");
if (r == SCPE_OK)
    fprintf (st, "  configuration rules: met%s", sim_sound_enabled () ? " (realism mode)" : "");
return SCPE_OK;
}

t_stat cpu_set_model (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
if (cptr == NULL)
    return SCPE_ARG;
if (strcmp(cptr, "8200") == 0) {
   sys_model = 0;
   strcpy (sim_name, "VAX 8200 (KA820)");
   }
else if (strcmp(cptr, "8250") == 0) {
   sys_model = 1;
   strcpy (sim_name, "VAX 8250 (KA825)");
   }
else
   return SCPE_ARG;
return SCPE_OK;
}

t_stat cpu_print_model (FILE *st)
{
extern DEVICE ka_dev[];
t_bool dual = ((ka_dev[1].flags & DEV_DIS) == 0);       /* KA1 enabled: 8300/8350 */

fprintf (st, "VAX %s", (sys_model ? (dual ? "8350" : "8250") : (dual ? "8300" : "8200")));
return SCPE_OK;
}

t_stat cpu_model_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
fprintf (st, "Initial memory size is 4MB (two MS820-AA boards).  SET CPU nM sets the\n");
fprintf (st, "memory boards from a table (HELP MCTL); SET MCTLn TYPE=... configures each\n");
fprintf (st, "of the twelve boards by hand.  SHOW BI shows the VAXBI nodes; every BI\n");
fprintf (st, "device has SET dev NODE=n (KA0 2, KA1 1, UBA 0, KDB 4, DMB 5, TB 6, ET 7,\n");
fprintf (st, "MCTL0-MCTL7 15-8 by default).\n\n");
fprintf (st, "The simulator is booted with the BOOT command:\n\n");
fprintf (st, "   sim> BO{OT} <device>{/R5:flags}\n\n");
fprintf (st, "where <device> is one of:\n\n");
fprintf (st, "   HKn        to boot from hkn\n");
fprintf (st, "   RLn        to boot from rln\n");
fprintf (st, "   RQn        to boot from rqn\n");
fprintf (st, "   RQBn       to boot from rqbn\n");
fprintf (st, "   RQCn       to boot from rqcn\n");
fprintf (st, "   RQDn       to boot from rqdn\n");
fprintf (st, "   TQn        to boot from tqn\n");
fprintf (st, "   CS         to boot from console RL\n\n");
return SCPE_OK;
}
