/* vax_bi_dma.c: DMA with VAX address translation for VAXBI devices

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

   VAXBI devices move data to and from memory themselves, and several of
   them translate VAX addresses on their own: the DMB32 (TD 2.2.2) walks the
   system and global page tables, the KDB50 follows a page table given by
   physical address in the MSCP buffer descriptor.  This module has that
   translation and the memory access, shared by those devices (VAX 8200).

   A buffer is described by a BI_DMA descriptor:

     mode 0                  addr = physical address of the buffer
     BIDMA_PTE|BIDMA_PHYS    addr = physical address of the first PTE,
                             off = byte offset of the buffer from the start
                             of the page that PTE maps
     BIDMA_PTE alone         addr = system virtual address of the first
                             process PTE, off as above (global PTEs are
                             followed through the global page table)
     neither (virtual)       addr = system virtual address of the buffer

   The virtual forms need the page tables (BI_PT) of the device.  Errors
   are returned as 0x10 + reason (bits 3:2) + table (bits 1:0), the DMB32
   error code format (UG 3.4.8.2); other devices map them to their own.

   Modification history:

   15-Nov-26    Ulli/Claude  First version (moved from vax_dmb.c)
*/

#include "vax_bi_dma.h"

/* VAX page table entry fields (TD 2.2.2.3) */
#define BPTE_V          0x80000000              /* valid */
#define BPTE_V_PROT     27                      /* protection */
#define BPTE_TYP0       0x04000000              /* bit 26 */
#define BPTE_TYP1       0x00400000              /* bit 22 */
#define BPTE_PFN        0x001FFFFF              /* page frame number */
#define BPTE_GPTX       0x003FFFFF              /* global page table index */

/* Set up a descriptor. */
void bi_dma_setup (BI_DMA *d, uint32 addr, uint32 off, uint32 count, uint32 mode,
                   BI_PT *pt, DEVICE *dptr, uint32 dbg)
{
d->addr = addr;
d->off = off;
d->count = count;
d->mode = mode & (BIDMA_PTE|BIDMA_PHYS);
d->pos = 0;
d->page = 0;
d->pbase = 0;
d->err_addr = 0;
d->pt = pt;
d->dptr = dptr;
d->dbg = dbg;
}

/* One longword of physical memory; FALSE if no memory answers there. */
t_bool bi_mem_rl (uint32 pa, uint32 *val)
{
if (!ADDR_IS_MEM (pa))
    return FALSE;
*val = M[pa >> 2];
return TRUE;
}

/* Protection check of the first PTE that describes a page (kernel mode). */
static t_bool bi_prot_ok (uint32 pte, t_bool wr)
{
uint32 prot = (pte >> BPTE_V_PROT) & 0xF;

if ((prot == 0) || (prot == 1))
    return FALSE;
if (wr && ((prot == 3) || (prot == 7) || (prot == 11) || (prot == 15)))
    return FALSE;
return TRUE;
}

/* System PTE of system virtual address va: physical address of the page,
   or an error (table BIDMA_T_SYS or the table of the caller). */
static int32 bi_sys_xlate (BI_DMA *d, uint32 va, t_bool wr, t_bool chk, uint32 *pa, uint32 *eaddr)
{
uint32 vpn = (va >> 9) & 0x1FFFFF;
uint32 pteaddr, pte;

if (((va & 0xC0000000) != 0x80000000) || (vpn >= d->pt->spts)) {
    *eaddr = va;
    return BIDMA_ERR | BIDMA_R_INV | BIDMA_T_SYS;        /* not a system address, too long */
    }
pteaddr = d->pt->spte + (vpn << 2);
if (!bi_mem_rl (pteaddr, &pte)) {
    *eaddr = pteaddr;
    return BIDMA_ERR | BIDMA_R_NXM | BIDMA_T_SYS;
    }
if (!(pte & BPTE_V) && ((pte & (BPTE_TYP0|BPTE_TYP1)) != 0)) {  /* valid or transition */
    *eaddr = pteaddr;
    return BIDMA_ERR | BIDMA_R_INV | BIDMA_T_SYS;
    }
if (chk && !bi_prot_ok (pte, wr)) {
    *eaddr = pteaddr;
    return BIDMA_ERR | BIDMA_R_ACC | BIDMA_T_SYS;
    }
*pa = ((pte & BPTE_PFN) << 9) | (va & 0x1FF);
return 0;
}

/* Interpret a process PTE (or a physical-mode PTE): physical page base.
   A global page is looked up once in the global page table. */
static int32 bi_prc_pte (BI_DMA *d, uint32 pte, uint32 pteaddr, t_bool wr, uint32 *pbase, uint32 *eaddr)
{
int32 r;
uint32 gva, gpa, gpte;

if (!(pte & BPTE_V) && (pte & BPTE_TYP0) && !(pte & BPTE_TYP1)) {      /* global page */
    uint32 gptx = pte & BPTE_GPTX;

    if (gptx >= d->pt->gpts) {
        *eaddr = pteaddr;
        return BIDMA_ERR | BIDMA_R_INV | BIDMA_T_GBL;
        }
    gva = d->pt->gpte + (gptx << 2);
    if ((r = bi_sys_xlate (d, gva, FALSE, FALSE, &gpa, eaddr)) != 0)
        return r;
    if (!bi_mem_rl (gpa, &gpte)) {
        *eaddr = gpa;
        return BIDMA_ERR | BIDMA_R_NXM | BIDMA_T_GBL;
        }
    if (!(gpte & BPTE_V) && ((gpte & (BPTE_TYP0|BPTE_TYP1)) != 0)) {
        *eaddr = gva;
        return BIDMA_ERR | BIDMA_R_INV | BIDMA_T_GBL;
        }
    if (!bi_prot_ok (pte, wr)) {               /* protection of the first PTE */
        *eaddr = pteaddr;
        return BIDMA_ERR | BIDMA_R_ACC | BIDMA_T_PRC;
        }
    *pbase = (gpte & BPTE_PFN) << 9;
    return 0;
    }
if (!(pte & BPTE_V) && ((pte & (BPTE_TYP0|BPTE_TYP1)) != 0)) {
    *eaddr = pteaddr;
    return BIDMA_ERR | BIDMA_R_INV | BIDMA_T_PRC;
    }
if (!bi_prot_ok (pte, wr)) {
    *eaddr = pteaddr;
    return BIDMA_ERR | BIDMA_R_ACC | BIDMA_T_PRC;
    }
*pbase = (pte & BPTE_PFN) << 9;
return 0;
}

/* Physical address of character d->pos; 0 = ok, else an error code. */
int32 bi_dma_xlate (BI_DMA *d, t_bool wr, uint32 *pa)
{
uint32 byte, page, pte, pteaddr, ptepa;
int32 r;

if ((d->mode & (BIDMA_PHYS|BIDMA_PTE)) == BIDMA_PHYS) {  /* physical buffer */
    *pa = d->addr + d->pos;
    return 0;
    }
if (!(d->mode & BIDMA_PHYS) && !(d->pt && d->pt->valid)) {
    d->err_addr = d->addr;                      /* virtual without page tables */
    return BIDMA_ERR | BIDMA_R_INV | BIDMA_T_SYS;
    }
if (!(d->mode & BIDMA_PTE)) {                      /* system virtual buffer */
    uint32 va = d->addr + d->pos;

    page = (va >> 9) + 1;
    if (d->page != page) {
        if ((r = bi_sys_xlate (d, va, wr, TRUE, &d->pbase, &d->err_addr)) != 0)
            return r;
        d->pbase &= ~0x1FF;
        d->page = page;
        }
    *pa = d->pbase | (va & 0x1FF);
    return 0;
    }
byte = d->off + d->pos;              /* PTE modes */
page = (byte >> 9) + 1;
if (d->page != page) {
    pteaddr = d->addr + ((byte >> 9) << 2);
    if (d->mode & BIDMA_PHYS) {                    /* physical page table */
        if (!bi_mem_rl (pteaddr, &pte)) {
            d->err_addr = pteaddr;
            return BIDMA_ERR | BIDMA_R_NXM | BIDMA_T_PRC;
            }
        d->pbase = (pte & BPTE_PFN) << 9;        /* other bits ignored (2.2.2.2) */
        }
    else {                                      /* virtual process page table */
        if ((r = bi_sys_xlate (d, pteaddr, FALSE, FALSE, &ptepa, &d->err_addr)) != 0)
            return r;
        if (!bi_mem_rl (ptepa, &pte)) {
            d->err_addr = ptepa;
            return BIDMA_ERR | BIDMA_R_NXM | BIDMA_T_PRC;
            }
        if ((r = bi_prc_pte (d, pte, pteaddr, wr, &d->pbase, &d->err_addr)) != 0)
            return r;
        }
    d->page = page;
    }
*pa = d->pbase | (byte & 0x1FF);
return 0;
}

/* Read or write up to n characters at the descriptor's position.  Returns
   the number moved; *err is the error code that stopped it (0 = none). */
int32 bi_dma_xfer (BI_DMA *d, uint8 *buf, int32 n, t_bool wr, int32 *err)
{
int32 i;
uint32 pa;

*err = 0;
for (i = 0; i < n; i++) {
    if ((*err = bi_dma_xlate (d, wr, &pa)) != 0)
        break;
    if (!ADDR_IS_MEM (pa)) {
        d->err_addr = pa & ~0xF;                /* rounded to the octaword */
        *err = BIDMA_ERR | BIDMA_R_NXM | BIDMA_T_BUF;
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
    if (d->dptr)
    sim_debug (d->dbg, d->dptr, "DMA error %02X at %08X (addr %08X, pos %d)\n",
               *err, d->err_addr, d->addr, d->pos);
return i;
}

