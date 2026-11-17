/* vax820_mem.c: VAX 8200 memory boards (MS820)

   Copyright (c) 2019, Matt Burke

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

   mctl                 MS820 memory boards MCTL0-MCTL11

   17-Nov-26    Ulli    Twelve MS820 boards (units MCTL0-MCTL11 of the MCTL
                        device), each with its type (MS820-AA 2MB, -BA 4MB,
                        -CA 16MB, SIMH-128MB outside the realism mode) and
                        its VAXBI node; SET CPU nM sets them from a table;
                        contiguous addresses in board order, as the KA820
                        console assigns them; CSR1 with the board's size
                        and RAM type; SHOW MCTL, SHOW CPU MEMORY (before:
                        two controllers at nodes 2 and 3, each half of the
                        memory)

   The board facts:
   - option, module and BI device type (all 0001): MS820-AA T1008 2MB,
     MS820-BA T1019 4MB, MS820-CA T1040 16MB (DEC field service handbook,
     chapter "82/83x0 and VAXBI", 1993);
   - the console self-test writes contiguous start and end addresses into
     the memory nodes (same source);
   - CSR1 at node space +100, CSR2 at +104, their bits (NetBSD ka820.c; the
     MS820 technical manual is not available): CSR1 <28:18> the size in
     256KB units, <17:16> the RAM type (0 64K, 1 256K, 2 1M chips), <13>
     the interlock (memory lock, cleared by an unlock write), error bits;
     CSR2 the error flags, address and syndrome.
   The RAM type of each option is provisional (MS820-AA and -BA 256K chips,
   MS820-CA 1M chips); the DTYPE revision field stays 0.
*/

#include "vax_defs.h"
#include "sim_sound.h"

/* MS820 CSR1 (longword 0x40 of the node space, byte offset 100) */

#define MCSR1_OF        0x40
#define MS1_ERRSUM      0x80000000                      /* error summary */
#define MS1_ECCDIAG     0x40000000                      /* ECC diagnostic */
#define MS1_ECCDIS      0x20000000                      /* ECC disable */
#define MS1_V_SIZE      18                              /* size, 256KB units */
#define MS1_M_SIZE      0x7FF
#define MS1_RAMTY256K   0x00010000                      /* 256K chips */
#define MS1_RAMTY1M     0x00020000                      /* 1M chips */
#define MS1_CRDINH      0x00008000                      /* CRD interrupt inhibit */
#define MS1_INTLK       0x00002000                      /* interlock (memory lock) */
#define MS1_BROKE       0x00001000                      /* broke */
#define MS1_MWRITEERR   0x00000400                      /* masked write error */
#define MS1_CNTLERR     0x00000200                      /* controller error */
#define MS1_DIAGC       0x0000007F                      /* ECC diagnostic bits */
#define MS1_RW          (MS1_ECCDIAG | MS1_ECCDIS | MS1_CRDINH | MS1_BROKE | MS1_DIAGC)
#define MS1_W1C         (MS1_MWRITEERR | MS1_CNTLERR | MS1_INTLK)

/* MS820 CSR2 (longword 0x41, byte offset 104) */

#define MCSR2_OF        0x41
#define MS2_W1C         0xF0000000                      /* RDS, high, CRD, address errors */

#define MCTL_MB         (1u << 20)
#define MCTL_NONODE     16                              /* board without a node */

/* Board types */

typedef struct {
    const char          *name;
    uint32              size;                           /* bytes */
    uint32              ramty;                          /* CSR1 RAM type */
    t_bool              real;                           /* a DEC option */
    const char          *module;
    } MS_TYPE;

static const MS_TYPE ms_types[] = {
    { "MS820-AA",     2 * MCTL_MB, MS1_RAMTY256K, TRUE,  "T1008" },
    { "MS820-BA",     4 * MCTL_MB, MS1_RAMTY256K, TRUE,  "T1019" },
    { "MS820-CA",    16 * MCTL_MB, MS1_RAMTY1M,   TRUE,  "T1040" },
    { "SIMH-128MB", 128 * MCTL_MB, MS1_RAMTY1M,   FALSE, "none (SIMH only)" },
    { NULL }
    };

#define MST_AA          0
#define MST_BA          1
#define MST_CA          2
#define MST_X128        3
#define MST_NUM         4

/* SET CPU nM: the boards for each size (Ulli, 2026-11-17); 256M and 512M
   only outside the realism mode, with SIMH-only boards (twelve MS820-CA
   give 192MB) */

static const struct {
    uint32      size;
    int32       n;
    int32       type;
    } mctl_tab[] = {
    {   4 * MCTL_MB, 2, MST_AA },
    {   8 * MCTL_MB, 2, MST_BA },
    {  16 * MCTL_MB, 4, MST_BA },
    {  32 * MCTL_MB, 2, MST_CA },
    {  48 * MCTL_MB, 3, MST_CA },
    {  64 * MCTL_MB, 4, MST_CA },
    { 128 * MCTL_MB, 8, MST_CA },
    { 256 * MCTL_MB, 2, MST_X128 },
    { 512 * MCTL_MB, 4, MST_X128 },
    { 0 }
    };

uint32 mctl_type[MCTL_NUM];                             /* board type (MST_xx) */
uint32 mctl_pres[MCTL_NUM];                             /* board present */
uint32 mctl_nodes[MCTL_NUM];                            /* VAXBI node, 16 = none */
uint32 mctl_manual = 0;                                 /* set by SET MCTLn */
uint32 mcsr_1[MCTL_NUM];                                /* CSR1: the writable bits */
uint32 mcsr_2[MCTL_NUM];
uint32 mctl_bcsr[MCTL_NUM];                             /* BIIC CSR, BER, EICR, INTRDES */
uint32 mctl_ber[MCTL_NUM];
uint32 mctl_eicr[MCTL_NUM];
uint32 mctl_idest[MCTL_NUM];
int32 mctl_resize_tag = 0;                              /* (MCTL_RESIZE) */
static t_bool mctl_init_done = FALSE;
static DIB mctl_dib[MCTL_NUM];

extern t_stat build_nexus_tab (DEVICE *dptr, DIB *dibp);
extern t_stat cpu_set_size (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
extern UNIT cpu_unit;

t_stat mctl_reset (DEVICE *dptr);
const char *mctl_description (DEVICE *dptr);
t_stat mctl_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
t_stat mctl_rdreg (int32 *val, int32 pa, int32 mode);
t_stat mctl_wrreg (int32 val, int32 pa, int32 mode);
t_stat mctl_set_type (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat mctl_show_type (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat mctl_set_node (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat mctl_show_node (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat mctl_set_enb (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat mctl_show_board (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat mctl_show_sum (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
static t_stat mctl_rlz_enable_check (t_bool apply);

/* MCTL data structures

   mctl_dev     MCTL device descriptor
   mctl_unit    the boards
   mctl_reg     MCTL register list
*/

UNIT mctl_unit[MCTL_NUM] = {
    { UDATA (NULL, 0, 0) }, { UDATA (NULL, 0, 0) }, { UDATA (NULL, 0, 0) },
    { UDATA (NULL, 0, 0) }, { UDATA (NULL, 0, 0) }, { UDATA (NULL, 0, 0) },
    { UDATA (NULL, 0, 0) }, { UDATA (NULL, 0, 0) }, { UDATA (NULL, 0, 0) },
    { UDATA (NULL, 0, 0) }, { UDATA (NULL, 0, 0) }, { UDATA (NULL, 0, 0) }
    };

REG mctl_reg[] = {
    { BRDATAD (TYPE,    mctl_type,  10,  8, MCTL_NUM, "board type (0 MS820-AA, 1 -BA, 2 -CA, 3 SIMH-128MB)") },
    { BRDATAD (PRESENT, mctl_pres,  10,  1, MCTL_NUM, "board present") },
    { BRDATAD (NODE,    mctl_nodes, 10,  8, MCTL_NUM, "VAXBI node (16 = none)") },
    { FLDATAD (MANUAL,  mctl_manual, 0, "boards set by SET MCTLn (not by SET CPU nM)") },
    { BRDATAD (CSR1,    mcsr_1,     16, 32, MCTL_NUM, "CSR1 (writable bits)") },
    { BRDATAD (CSR2,    mcsr_2,     16, 32, MCTL_NUM, "CSR2") },
    { BRDATAD (BICSR,   mctl_bcsr,  16, 32, MCTL_NUM, "BIIC CSR") },
    { BRDATAD (BER,     mctl_ber,   16, 32, MCTL_NUM, "BIIC bus error register") },
    { BRDATAD (EICR,    mctl_eicr,  16, 32, MCTL_NUM, "BIIC error interrupt control") },
    { BRDATAD (INTRDES, mctl_idest, 16, 16, MCTL_NUM, "BIIC interrupt destination") },
    { NULL }
    };

MTAB mctl_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "MEMORY", NULL,
      NULL, &mctl_show_sum, NULL, "Display the memory size and the boards" },
    { MTAB_XTD|MTAB_VUN, 0, "BOARD", NULL,
      NULL, &mctl_show_board, NULL, "Display the board" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "TYPE", "TYPE={MS820-AA|MS820-BA|MS820-CA|SIMH-128MB|NONE}",
      &mctl_set_type, &mctl_show_type, NULL, "Set/Display the board type (NONE: no board)" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO|MTAB_VALR, 0, "NODE", "NODE=n",
      &mctl_set_node, &mctl_show_node, NULL, "Set/Display the board's VAXBI node (0-15)" },
    { MTAB_XTD|MTAB_VUN, 1, NULL, "ENABLED",
      &mctl_set_enb, NULL, NULL, "Insert the board" },
    { MTAB_XTD|MTAB_VUN, 0, NULL, "DISABLED",
      &mctl_set_enb, NULL, NULL, "Remove the board" },
    { 0 }
    };

DEVICE mctl_dev = {
    "MCTL", mctl_unit, mctl_reg, mctl_mod,
    MCTL_NUM, 16, 16, 1, 16, 8,
    NULL, NULL, &mctl_reset,
    NULL, NULL, NULL,
    NULL, 0, 0,
    NULL, NULL, NULL, &mctl_help, NULL, NULL,
    &mctl_description
    };

/* Board helpers */

static uint32 mctl_size (int32 i)
{
return ms_types[mctl_type[i] % MST_NUM].size;
}

static uint32 mctl_start (int32 i)                      /* contiguous, board order */
{
uint32 a = 0;
int32 j;

for (j = 0; j < i; j++) {
    if (mctl_pres[j])
        a += mctl_size (j);
    }
return a;
}

static uint32 mctl_total (void)
{
return mctl_start (MCTL_NUM);
}

static int32 mctl_count (void)
{
int32 i, n = 0;

for (i = 0; i < MCTL_NUM; i++) {
    if (mctl_pres[i])
        n++;
    }
return n;
}

static uint32 mctl_def_node (int32 i)                   /* MCTL0-7: nodes 15-8 */
{
return (i < 8) ? (uint32) (15 - i) : MCTL_NONODE;
}

t_bool mctl_board_node (int32 i, int32 *node)           /* a present board's node */
{
if ((i < 0) || (i >= MCTL_NUM) || !mctl_pres[i] || (mctl_nodes[i] >= MCTL_NONODE))
    return FALSE;
*node = (int32) mctl_nodes[i];
return TRUE;
}

static int32 mctl_board_at (int32 node)                 /* present board on a node */
{
int32 i, n;

for (i = 0; i < MCTL_NUM; i++) {
    if (mctl_board_node (i, &n) && (n == node))
        return i;
    }
return -1;
}

void mctl_board_text (int32 i, char *buf, size_t lnt)   /* for SHOW BI */
{
snprintf (buf, lnt, "%s %uMB, %08X-%08X", ms_types[mctl_type[i] % MST_NUM].name,
          mctl_size (i) / MCTL_MB, mctl_start (i), mctl_start (i) + mctl_size (i) - 1);
}

static void mctl_init_board (int32 i)                   /* registers after a reset */
{
mctl_bcsr[i] = (1u << BICSR_V_IF) | BICSR_STS | (mctl_nodes[i] & BICSR_NODE);
mctl_ber[i] = 0;
mctl_eicr[i] = 0;
mctl_idest[i] = 0;
mcsr_1[i] = 0;
mcsr_2[i] = 0;
}

/* The memory size follows the boards: called after a board change (SET
   MCTLn TYPE, ENABLED, DISABLED); the caller restores the boards if it
   fails (no board, too much memory, "Really truncate memory" answered no) */

static t_stat mctl_resize (void)
{
uint32 total = mctl_total ();
t_stat r;

if (total == 0)
    return sim_messagef (SCPE_ARG, "MCTL: at least one memory board is needed\n");
if (total > MAXMEMSIZE_X)
    return sim_messagef (SCPE_ARG, "MCTL: at most %uMB of memory (%uMB configured)\n",
                         (uint32) (MAXMEMSIZE_X / MCTL_MB), total / MCTL_MB);
if (total != (uint32) MEMSIZE) {
    r = cpu_set_size (&cpu_unit, (int32) total, NULL, MCTL_RESIZE);
    if (r != SCPE_OK)
        return r;
    if ((uint32) MEMSIZE != total)                      /* not truncated */
        return SCPE_ARG | SCPE_NOMESSAGE;
    }
return SCPE_OK;
}

/* Saved board state, for undoing a refused change */

typedef struct {
    uint32      type[MCTL_NUM], pres[MCTL_NUM], nodes[MCTL_NUM], manual;
    } MCTL_SNAP;

static void mctl_snap (MCTL_SNAP *s)
{
memcpy (s->type, mctl_type, sizeof (mctl_type));
memcpy (s->pres, mctl_pres, sizeof (mctl_pres));
memcpy (s->nodes, mctl_nodes, sizeof (mctl_nodes));
s->manual = mctl_manual;
}

static void mctl_unsnap (const MCTL_SNAP *s)
{
memcpy (mctl_type, s->type, sizeof (mctl_type));
memcpy (mctl_pres, s->pres, sizeof (mctl_pres));
memcpy (mctl_nodes, s->nodes, sizeof (mctl_nodes));
mctl_manual = s->manual;
}

/* A board that is put in needs a free node */

static t_stat mctl_check_insert (int32 i)
{
DEVICE *owner;
int32 ou;

if (mctl_nodes[i] >= MCTL_NONODE)
    return sim_messagef (SCPE_ARG, "MCTL%d has no VAXBI node: SET MCTL%d NODE=n first\n", i, i);
owner = bi_node_owner ((int32) mctl_nodes[i], &mctl_dev, i, &ou);
if (owner != NULL) {
    if (ou >= 0)
        return sim_messagef (SCPE_ARG, "MCTL%d: VAXBI node %u is in use by MCTL%d\n", i, mctl_nodes[i], ou);
    return sim_messagef (SCPE_ARG, "MCTL%d: VAXBI node %u is in use by %s\n", i, mctl_nodes[i], owner->name);
    }
return SCPE_OK;
}

/* SET CPU nM (cpu_set_size, VAX 8200): refused above 128M in the realism
   mode; then the table's boards on their default nodes */

t_stat mctl_cpu_size_check (uint32 size)
{
if (sim_sound_enabled () && (size > 128 * MCTL_MB))
    return sim_messagef (SCPE_ARG, "SET CPU: in the realism mode SET CPU goes up to 128M (8 MS820-CA);\n"
                         "more memory with SET MCTLn TYPE=MS820-CA (up to 12 boards, 192MB)\n");
return SCPE_OK;
}

void mctl_cpu_size (uint32 size)
{
int32 i, n = 0, type = MST_CA, ou;
DEVICE *owner;

for (i = 0; mctl_tab[i].size != 0; i++) {
    if (mctl_tab[i].size == size) {
        n = mctl_tab[i].n;
        type = mctl_tab[i].type;
        break;
        }
    }
if (n == 0) {                                           /* not in the table: */
    static const int32 pref[] = { MST_CA, MST_BA, MST_AA, MST_X128 };
    for (i = 0; i < 4; i++) {                           /* boards of one type */
        uint32 s = ms_types[pref[i]].size;
        if (((size % s) == 0) && ((size / s) <= MCTL_NUM)) {
            n = (int32) (size / s);
            type = pref[i];
            break;
            }
        }
    }
for (i = 0; i < MCTL_NUM; i++) {
    mctl_type[i] = (uint32) type;
    mctl_pres[i] = (i < n);
    mctl_nodes[i] = mctl_def_node (i);
    mctl_init_board (i);
    }
mctl_manual = 0;
if (n == 0)
    sim_printf ("MCTL: %uKB cannot be built from memory boards; SET MCTLn TYPE=... the boards\n",
                size >> 10);
for (i = 0; i < n; i++) {                               /* default node taken? */
    owner = bi_node_owner ((int32) mctl_nodes[i], &mctl_dev, i, &ou);
    if ((owner != NULL) && (owner != &mctl_dev))
        sim_printf ("MCTL%d: VAXBI node %u is in use by %s: SET MCTL%d NODE=n (or SET %s NODE=n)\n",
                    i, mctl_nodes[i], owner->name, i, owner->name);
    }
}

/* SET MCTLn TYPE=x */

t_stat mctl_set_type (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
int32 i = (int32) (uptr - mctl_unit);
int32 t;
MCTL_SNAP s;
t_stat r;

if ((cptr == NULL) || (*cptr == 0))
    return sim_messagef (SCPE_ARG, "MCTL%d: TYPE=MS820-AA, MS820-BA, MS820-CA, SIMH-128MB or NONE\n", i);
mctl_snap (&s);
if (strcasecmp (cptr, "NONE") == 0)
    mctl_pres[i] = 0;
else {
    for (t = 0; t < MST_NUM; t++) {
        if (strcasecmp (cptr, ms_types[t].name) == 0)
            break;
        }
    if (t >= MST_NUM)
        return sim_messagef (SCPE_ARG, "MCTL%d: unknown board type %s "
                             "(MS820-AA, MS820-BA, MS820-CA, SIMH-128MB, NONE)\n", i, cptr);
    if (!ms_types[t].real && sim_sound_enabled ())
        return sim_messagef (SCPE_ARG, "MCTL%d: %s is a SIMH-only board, not in the realism mode\n",
                             i, ms_types[t].name);
    mctl_type[i] = (uint32) t;
    if (!mctl_pres[i]) {
        if ((r = mctl_check_insert (i)) != SCPE_OK) {
            mctl_unsnap (&s);
            return r;
            }
        mctl_pres[i] = 1;
        }
    }
mctl_manual = 1;
if ((r = mctl_resize ()) != SCPE_OK) {
    mctl_unsnap (&s);
    return r;
    }
mctl_init_board (i);
return SCPE_OK;
}

t_stat mctl_show_type (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
int32 i = (int32) (uptr - mctl_unit);

if (mctl_pres[i])
    fprintf (st, "type=%s\n", ms_types[mctl_type[i] % MST_NUM].name);
else
    fprintf (st, "type=NONE\n");
return SCPE_OK;
}

/* SET MCTLn NODE=n (also after SET CPU nM; the boards stay as they are) */

t_stat mctl_set_node (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
int32 i = (int32) (uptr - mctl_unit);
int32 node;
t_stat r;

if ((r = bi_get_node (cptr, &mctl_dev, i, &node)) != SCPE_OK)
    return r;
mctl_nodes[i] = (uint32) node;
mctl_bcsr[i] = (mctl_bcsr[i] & ~BICSR_NODE) | ((uint32) node & BICSR_NODE);
return SCPE_OK;
}

t_stat mctl_show_node (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
int32 i = (int32) (uptr - mctl_unit);

if (mctl_nodes[i] >= MCTL_NONODE)
    fprintf (st, "node=none\n");
else
    fprintf (st, "node=%u\n", mctl_nodes[i]);
return SCPE_OK;
}

/* SET MCTLn ENABLED / DISABLED: put the board in (with its type, default
   MS820-CA) or take it out */

t_stat mctl_set_enb (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
int32 i = (int32) (uptr - mctl_unit);
MCTL_SNAP s;
t_stat r;

if (cptr != NULL)
    return SCPE_ARG;
if ((val != 0) == (mctl_pres[i] != 0))                  /* no change */
    return SCPE_OK;
mctl_snap (&s);
if (val) {
    if (!ms_types[mctl_type[i] % MST_NUM].real && sim_sound_enabled ())
        mctl_type[i] = MST_CA;
    if ((r = mctl_check_insert (i)) != SCPE_OK)
        return r;
    mctl_pres[i] = 1;
    }
else
    mctl_pres[i] = 0;
mctl_manual = 1;
if ((r = mctl_resize ()) != SCPE_OK) {
    mctl_unsnap (&s);
    return r;
    }
mctl_init_board (i);
return SCPE_OK;
}

/* SHOW MCTL: the device line and one line per board */

t_stat mctl_show_sum (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
fprintf (st, "%uMB on %d board%s (%s)", mctl_total () / MCTL_MB, mctl_count (),
         (mctl_count () == 1) ? "" : "s", mctl_manual ? "set by SET MCTLn" : "set by SET CPU");
return SCPE_OK;
}

t_stat mctl_show_board (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
int32 i = (int32) (uptr - mctl_unit);
const MS_TYPE *t = &ms_types[mctl_type[i] % MST_NUM];

if (!mctl_pres[i]) {
    fprintf (st, "no board");
    if (mctl_nodes[i] < MCTL_NONODE)
        fprintf (st, " (node %u reserved)", mctl_nodes[i]);
    return SCPE_OK;
    }
fprintf (st, "%s, %uMB, node=%u, %08X-%08X%s", t->name, t->size / MCTL_MB, mctl_nodes[i],
         mctl_start (i), mctl_start (i) + t->size - 1, t->real ? "" : " (SIMH only)");
return SCPE_OK;
}

/* SHOW CPU MEMORY */

t_stat cpu_show_memory (FILE* st, UNIT* uptr, int32 val, CONST void* desc)
{
int32 i;
char buf[96];

fprintf (st, "Memory: %uMB on %d MS820 board%s\n", mctl_total () / MCTL_MB, mctl_count (),
         (mctl_count () == 1) ? "" : "s");
for (i = 0; i < MCTL_NUM; i++) {
    if (!mctl_pres[i])
        continue;
    mctl_board_text (i, buf, sizeof (buf));
    fprintf (st, "  MCTL%-2d node %2u  %s\n", i, mctl_nodes[i], buf);
    }
return SCPE_OK;
}

/* The boards on the VAXBI: one nexus each (build_dib_tab) */

t_stat mctl_build_nexus (void)
{
int32 i;
t_stat r;

for (i = 0; i < MCTL_NUM; i++) {
    if (!mctl_pres[i])
        continue;
    if (mctl_nodes[i] >= MCTL_NONODE) {
        sim_printf ("MCTL%d has no VAXBI node: SET MCTL%d NODE=n\n", i, i);
        return SCPE_STOP;
        }
    mctl_dib[i].ba = mctl_nodes[i];
    mctl_dib[i].lnt = 0;
    mctl_dib[i].rd = &mctl_rdreg;
    mctl_dib[i].wr = &mctl_wrreg;
    if ((r = build_nexus_tab (&mctl_dev, &mctl_dib[i])) != SCPE_OK) {
        sim_printf ("MCTL%d: VAXBI node %u is in use (SHOW BI)\n", i, mctl_nodes[i]);
        return r;
        }
    }
return SCPE_OK;
}

/* Memory board register read */

t_stat mctl_rdreg (int32 *val, int32 pa, int32 lnt)
{
int32 i, ofs;

i = mctl_board_at (NEXUS_GETNEX (pa));                  /* the board on the node */
if (i < 0)
    return SCPE_NXM;
ofs = NEXUS_GETOFS (pa);                                /* get offset */
switch (ofs) {

    case BI_DTYPE:
        *val = DTYPE_MS820;
        break;

    case BI_CSR:
        *val = (int32) (mctl_bcsr[i] & BICSR_RD);
        break;

    case BI_BER:
        *val = (int32) (mctl_ber[i] & BIBER_RD);
        break;

    case BI_EICR:
        *val = (int32) (mctl_eicr[i] & BIECR_RD);
        break;

    case BI_IDEST:
        *val = (int32) (mctl_idest[i] & BIID_RD);
        break;

    case BI_SA:                                         /* start address */
        *val = (int32) mctl_start (i);
        break;

    case BI_EA:                                         /* end address */
        *val = (int32) (mctl_start (i) + mctl_size (i));
        break;

    case MCSR1_OF:                                      /* CSR 1 */
        *val = (int32) ((mcsr_1[i] & (MS1_RW | MS1_W1C)) |
               (((mctl_size (i) >> MS1_V_SIZE) & MS1_M_SIZE) << MS1_V_SIZE) |
               ms_types[mctl_type[i] % MST_NUM].ramty |
               ((mcsr_1[i] & (MS1_W1C)) || (mcsr_2[i] & MS2_W1C) ? MS1_ERRSUM : 0));
        break;

    case MCSR2_OF:                                      /* CSR 2 */
        *val = (int32) mcsr_2[i];
        break;

    default:
        return SCPE_NXM;
        }

return SCPE_OK;
}

/* Memory board register write */

t_stat mctl_wrreg (int32 val, int32 pa, int32 lnt)
{
int32 i, ofs;

i = mctl_board_at (NEXUS_GETNEX (pa));
if (i < 0)
    return SCPE_NXM;
ofs = NEXUS_GETOFS (pa);                                /* get offset */
switch (ofs) {

    case BI_CSR:
        mctl_bcsr[i] = (mctl_bcsr[i] & ~BICSR_RW) | ((uint32) val & BICSR_RW);
        break;

    case BI_BER:
        mctl_ber[i] = mctl_ber[i] & ~((uint32) val & BIBER_W1C);
        break;

    case BI_EICR:
        mctl_eicr[i] = (mctl_eicr[i] & ~BIECR_RW) | ((uint32) val & BIECR_RW);
        mctl_eicr[i] = mctl_eicr[i] & ~((uint32) val & BIECR_W1C);
        break;

    case BI_IDEST:
        mctl_idest[i] = (uint32) val & BIID_RW;
        break;

    case BI_SA:                                         /* the console's address */
    case BI_EA:                                         /* assignment: kept */
        break;

    case MCSR1_OF:                                      /* CSR 1 */
        mcsr_1[i] = (mcsr_1[i] & ~MS1_RW) | ((uint32) val & MS1_RW);
        mcsr_1[i] = mcsr_1[i] & ~((uint32) val & MS1_W1C);
        break;

    case MCSR2_OF:                                      /* CSR 2 */
        mcsr_2[i] = mcsr_2[i] & ~((uint32) val & MS2_W1C);
        break;

    default:
        return SCPE_NXM;
        }

return SCPE_OK;
}

/* Used by CPU and loader */

void rom_wr_B (int32 pa, int32 val)
{
return;
}

/* Realism mode: no SIMH-only boards */

static t_stat mctl_rlz_enable_check (t_bool apply)
{
int32 i;

for (i = 0; i < MCTL_NUM; i++) {
    if (mctl_pres[i] && !ms_types[mctl_type[i] % MST_NUM].real)
        return sim_messagef (SCPE_ARG, "SET REALISM ENABLED: MCTL%d is a %s board (SIMH only): "
                             "SET CPU 128M or SET MCTL%d TYPE=MS820-CA\n", i,
                             ms_types[mctl_type[i] % MST_NUM].name, i);
    }
return SCPE_OK;
}

/* MCTL reset: at the first reset the boards for the initial memory size */

t_stat mctl_reset (DEVICE *dptr)
{
int32 i;

if (!mctl_init_done) {
    mctl_cpu_size ((uint32) MEMSIZE);
    mctl_init_done = TRUE;
    }
for (i = 0; i < MCTL_NUM; i++)
    mctl_init_board (i);
sim_sound_register_enable_check (&mctl_rlz_enable_check);
return SCPE_OK;
}

const char *mctl_description (DEVICE *dptr)
{
return "MS820 memory boards";
}

t_stat mctl_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
fprintf (st, "MS820 memory boards (MCTL0-MCTL11)\n\n");
fprintf (st, "The VAX 8200/8300 memory is up to twelve MS820 boards, each a VAXBI node\n");
fprintf (st, "of its own (the 24-slot backplane of the 8220/8320):\n\n");
fprintf (st, "   MS820-AA   2MB  (module T1008)\n");
fprintf (st, "   MS820-BA   4MB  (module T1019)\n");
fprintf (st, "   MS820-CA  16MB  (module T1040)\n\n");
fprintf (st, "SET CPU nM puts in the boards for the size, on nodes 15, 14, ... (MCTL0,\n");
fprintf (st, "MCTL1, ...):\n\n");
fprintf (st, "   4M  2 x MS820-AA      32M  2 x MS820-CA      128M  8 x MS820-CA\n");
fprintf (st, "   8M  2 x MS820-BA      48M  3 x MS820-CA\n");
fprintf (st, "  16M  4 x MS820-BA      64M  4 x MS820-CA\n\n");
fprintf (st, "Outside the realism mode SET CPU 256M and 512M use SIMH-only boards of\n");
fprintf (st, "128MB (TYPE=SIMH-128MB); in the realism mode SET CPU goes up to 128M, and\n");
fprintf (st, "more memory (up to 12 x MS820-CA, 192MB) is set board by board.\n\n");
fprintf (st, "Each board can be set by hand; the memory size is then the sum of the\n");
fprintf (st, "boards, until the next SET CPU nM:\n\n");
fprintf (st, "   sim> SET MCTL2 TYPE=MS820-CA      (put in or change a board)\n");
fprintf (st, "   sim> SET MCTL2 NODE=13            (its VAXBI node, 0-15 or A-F)\n");
fprintf (st, "   sim> SET MCTL3 DISABLED           (take a board out; ENABLED: back in)\n");
fprintf (st, "   sim> SET MCTL3 TYPE=NONE          (the same)\n");
fprintf (st, "   sim> SHOW MCTL                    (all twelve boards)\n");
fprintf (st, "   sim> SHOW BI                      (all VAXBI nodes)\n\n");
fprintf (st, "MCTL8-MCTL11 have no default node.  The boards get contiguous addresses in\n");
fprintf (st, "board order (MCTL0 from 0), as the KA820 console assigns them.  A board\n");
fprintf (st, "answers with BI device type 0001, its start and end address, and CSR1 with\n");
fprintf (st, "its size and RAM type (bits after NetBSD; the MS820 technical manual is not\n");
fprintf (st, "available).  VMS V4 uses at most 8 memory boards (128MB).\n\n");
fprint_set_help (st, dptr);
fprint_show_help (st, dptr);
fprint_reg_help (st, dptr);
return SCPE_OK;
}
