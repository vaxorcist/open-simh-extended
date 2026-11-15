/* vax_combo_lib.h: shared parts of the DMF32 and DMB32 combination boards

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

   The DMF32 (Unibus) and the DMB32 (VAXBI) run the same microcode
   functions behind different host interfaces.  This library has the parts
   that do not depend on the host interface:

   - the line printer formatter (tabs, truncation, line wrap, automatic
     carriage return, form feed conversion, upper case, non printing
     characters, DAVFU codes, prefix and suffix characters).

   Each board keeps a COMBO_LP for its printer, sets the option flags from
   its own registers before a print cycle and supplies the output routine.

   Modification history:

   15-Nov-26    Ulli/Claude  First version: printer formatter (from vax_dmf.c)
*/

#ifndef VAX_COMBO_LIB_H_
#define VAX_COMBO_LIB_H_    0

#include "sim_defs.h"

/* Printer formatter options (COMBO_LP.flags) */

#define CLP_TAB         0x0001                  /* expand tabs (every 8 columns) */
#define CLP_TRUNC       0x0002                  /* truncate long lines */
#define CLP_AUTOCR      0x0004                  /* CR before LF and FF, strip own CRs */
#define CLP_FFCONV      0x0008                  /* form feed to line feeds */
#define CLP_NONPRINT    0x0010                  /* pass non printing characters */
#define CLP_DAVFU       0x0020                  /* pass DAVFU codes 200-237 */
#define CLP_WRAP        0x0040                  /* new line at the line end */
#define CLP_UPPER       0x0080                  /* lower to upper case */
#define CLP_NLCR        0x0100                  /* "new line" is always CR LF
                                                   (DMF32); else CR LF only with
                                                   CLP_AUTOCR, LF alone without
                                                   (DMB32, UG 3.3.22) */

typedef struct combo_lp COMBO_LP;

struct combo_lp {
    uint32  flags;                              /* options, CLP_xxx */
    int32   width;                              /* line width, 0 = no limit */
    int32   lpp;                                /* lines per page, 0 = unknown */
    int32   col;                                /* printer column */
    int32   row;                                /* line on the page */
    t_bool  crpend;                             /* data CR held (auto CR) */
    uint32  chars;                              /* characters sent to the printer */
    uint32  lines;                              /* line feeds (paper movement) */
    /* the board's output routine: returns TRUE if the character went to the
       printer (its column and line are then tracked), FALSE if it went
       elsewhere (DMF32 maintenance mode loops it into a register) */
    t_bool  (*put)(COMBO_LP *lp, int32 c);
    void    *ctx;                               /* for the board */
    };

void combo_lp_reset (COMBO_LP *lp);             /* top of form, column 0 */
void combo_lp_out (COMBO_LP *lp, int32 c);      /* one character, unformatted */
void combo_lp_newline (COMBO_LP *lp);           /* "new line" (prefix 0, wrap) */
void combo_lp_format (COMBO_LP *lp, int32 c);   /* one data character, formatted */
void combo_lp_fix (COMBO_LP *lp, int32 count, int32 c);    /* prefix/suffix */

#endif
