/* vax_combo_lib.c: shared parts of the DMF32 and DMB32 combination boards

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

   Shared parts of the DMF32 (vax_dmf.c) and DMB32 (vax_dmb.c); see
   vax_combo_lib.h.

   References:
     EK-DMF32-UG-003  DMF32 User's Guide (line printer, Table 4-34)
     EK-DMB32-UG-001  DMB32 User Guide (printer registers, 3.3.22-3.3.27)

   Modification history:

   15-Nov-26    Ulli/Claude  First version: printer formatter (from vax_dmf.c)
*/

#include "vax_combo_lib.h"

/* ---------------------------------------------------------------------------
   Line printer formatter
   --------------------------------------------------------------------------- */

/* Top of form, column 0 (printer port initialized). */
void combo_lp_reset (COMBO_LP *lp)
{
lp->col = 0;
lp->row = 0;
lp->crpend = FALSE;
}

/* One character to the printer, unformatted: count it and track the
   carriage (column after CR, line after LF and FF). */
void combo_lp_out (COMBO_LP *lp, int32 c)
{
lp->chars++;
if (!lp->put (lp, c))                           /* did not reach the paper */
    return;
if (c == 012) {                                 /* line feed: paper moves */
    lp->lines++;
    lp->row++;
    }
else if (c == 014)
    lp->row = 0;
if (c == 015)
    lp->col = 0;
}

/* "New line" (prefix/suffix character 0, line wrap).  The DMF32 always
   sends CR LF (CLP_NLCR); on the DMB32 the CR comes only with automatic
   carriage return insertion (UG 3.3.22). */
void combo_lp_newline (COMBO_LP *lp)
{
lp->crpend = FALSE;
if ((lp->flags & CLP_NLCR) || (lp->flags & CLP_AUTOCR))
    combo_lp_out (lp, 015);
lp->col = 0;
combo_lp_out (lp, 012);
}

/* One data character through the formatting functions. */
void combo_lp_format (COMBO_LP *lp, int32 c)
{
if (c & 0200) {                                 /* MSB set: non printable */
    t_bool vfu_code = (lp->flags & CLP_DAVFU) && (c <= 0237);

    if (!(lp->flags & CLP_NONPRINT) && !vfu_code)
        return;
    combo_lp_out (lp, c);
    return;
    }
if ((lp->flags & CLP_UPPER) && (c >= 'a') && (c <= 'z'))
    c = c - 'a' + 'A';                          /* lower to upper case */
/* Automatic carriage return insertion: a CR is inserted before every LF
   and FF, and a CR in the data that precedes an LF or FF is stripped, so
   the printer sees exactly one.  A data CR is therefore held until the
   next character shows whether it precedes an LF or FF. */
if (lp->crpend) {
    lp->crpend = FALSE;
    if (((c != 012) && (c != 014)) || !(lp->flags & CLP_AUTOCR))
        combo_lp_out (lp, 015);
    }
switch (c) {
    case 011:                                   /* tab */
        if (lp->flags & CLP_TAB) {
            do
                combo_lp_format (lp, ' ');
            while (lp->col & 7);
            return;
            }
        break;
    case 012:                                   /* line feed */
        if (lp->flags & CLP_AUTOCR)             /* automatic CR before LF */
            combo_lp_out (lp, 015);
        lp->col = 0;
        combo_lp_out (lp, 012);
        return;
    case 014:                                   /* form feed */
        if (lp->flags & CLP_AUTOCR)             /* automatic CR before FF */
            combo_lp_out (lp, 015);
        lp->col = 0;
        if ((lp->flags & CLP_FFCONV) && lp->lpp && !(lp->flags & CLP_DAVFU)) {
            do                                  /* line feeds to the next page */
                combo_lp_out (lp, 012);
            while (lp->row % lp->lpp);
            lp->row = 0;
            }
        else
            combo_lp_out (lp, 014);
        return;
    case 015:                                   /* carriage return: held until */
        lp->crpend = TRUE;                      /* the next character shows */
        lp->col = 0;                            /* whether it precedes LF/FF */
        return;
    default:
        break;
    }
if ((c >= 040) && lp->width && (lp->col >= lp->width)) {
    if (lp->flags & CLP_WRAP)                   /* wrap to the next line */
        combo_lp_newline (lp);
    else if (lp->flags & CLP_TRUNC)             /* truncate */
        return;
    }
combo_lp_out (lp, c);
if (c >= 040)
    lp->col++;
}

/* Prefix or suffix characters: a count and a character, 0 = new line. */
void combo_lp_fix (COMBO_LP *lp, int32 count, int32 c)
{
while (count-- > 0) {
    if (c == 0)
        combo_lp_newline (lp);
    else
        combo_lp_format (lp, c);
    }
}
