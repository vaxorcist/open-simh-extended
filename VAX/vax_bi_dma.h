/* vax_bi_dma.h: DMA with VAX address translation for VAXBI devices

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

#ifndef VAX_BI_DMA_H_
#define VAX_BI_DMA_H_   0

#include "vax_defs.h"

#define BIDMA_PTE       0x00000002              /* addr is a PTE address */
#define BIDMA_PHYS      0x00000004              /* addr is physical */

#define BIDMA_ERR       0x10                    /* error: + reason + table */
#define BIDMA_R_NXM     (0 << 2)                /* memory did not respond */
#define BIDMA_R_INV     (2 << 2)                /* PTE not valid */
#define BIDMA_R_ACC     (3 << 2)                /* PTE does not allow the access */
#define BIDMA_T_SYS     0                       /* in a system PTE */
#define BIDMA_T_GBL     1                       /* in a global PTE */
#define BIDMA_T_PRC     2                       /* in a process PTE */
#define BIDMA_T_BUF     3                       /* in the buffer itself */

typedef struct {
    uint32  spte;                               /* system page table, physical */
    uint32  spts;                               /* its length in PTEs */
    uint32  gpte;                               /* global page table, virtual */
    uint32  gpts;                               /* its length in PTEs */
    t_bool  valid;                              /* the above are loaded */
    } BI_PT;

typedef struct {
    uint32  addr;                               /* buffer or PTE address */
    uint32  off;                                /* byte offset (PTE forms) */
    uint32  count;                              /* length of the buffer */
    uint32  mode;                               /* BIDMA_PTE, BIDMA_PHYS */
    uint32  pos;                                /* bytes done */
    uint32  page;                               /* translated page + 1, 0 = none */
    uint32  pbase;                              /* its physical base */
    uint32  err_addr;                           /* address reported with an error */
    BI_PT   *pt;                                /* page tables (virtual forms) */
    DEVICE  *dptr;                              /* for debug output */
    uint32  dbg;                                /* debug flag of that device */
    } BI_DMA;

t_bool bi_mem_rl (uint32 pa, uint32 *val);
void bi_dma_setup (BI_DMA *d, uint32 addr, uint32 off, uint32 count, uint32 mode,
                   BI_PT *pt, DEVICE *dptr, uint32 dbg);
int32 bi_dma_xlate (BI_DMA *d, t_bool wr, uint32 *pa);
int32 bi_dma_xfer (BI_DMA *d, uint8 *buf, int32 n, t_bool wr, int32 *err);

#endif
