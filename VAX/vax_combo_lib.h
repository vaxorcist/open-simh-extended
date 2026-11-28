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
     characters, DAVFU codes, prefix and suffix characters);
   - the synchronous line's link to its peer and the DDCMP framing.

   Each board keeps a COMBO_LP for its printer, sets the option flags from
   its own registers before a print cycle and supplies the output routine.

   For the synchronous line each board keeps a COMBO_SYNC: the one-line
   TMXR connection to the peer (SET <unit> PEER=host:port, ATTACH <unit>
   port), as DMF-SYNC and the DMC11 use it: whole DDCMP frames, CRCs
   included.  In DDCMP mode the boards only frame messages (DMB32 TD 2.7,
   DMF32 UG 4.11): the host software runs the protocol; the board takes
   the 6-byte header and the data from a transmit buffer and inserts the
   header and data CRCs, and on receive it checks and strips them and
   stores header and data.  combo_ddcmp_encode/decode do that; the
   register handling, buffers and interrupts stay with the board.

   Modification history:

   17-Nov-26    Ulli/Claude  Synchronous line link and DDCMP framing (from
                             vax_dmf.c), for the DMB32 sync line
   15-Nov-26    Ulli/Claude  First version: printer formatter (from vax_dmf.c)
*/

#ifndef VAX_COMBO_LIB_H_
#define VAX_COMBO_LIB_H_    0

#include "sim_defs.h"
#include "sim_tmxr.h"

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

/* Synchronous line: link to the peer and DDCMP framing */

#define COMBO_SFRAME    (16384 + 16)            /* largest message + header/CRCs */

/* combo_ddcmp_decode results */
#define CDD_OK          0                       /* good message */
#define CDD_DROP        1                       /* bad header (CRC, start
                                                   character, short frame):
                                                   the receiver resynchronizes,
                                                   nothing is stored */
#define CDD_DCRC        2                       /* stored, data CRC bad */

/* combo_ddcmp_encode results */
#define CDE_OK          0
#define CDE_HDR         1                       /* not SOH, ENQ or DLE, or shorter
                                                   than a header */
#define CDE_LEN         2                       /* data count beyond the buffer */

typedef struct combo_sync COMBO_SYNC;

struct combo_sync {
    TMLN    ln[1];                              /* the line */
    TMXR    desc;                               /* its one-line multiplexer */
    char    peer[CBUFSIZE];                     /* host:port to connect to */
    uint8   frame[COMBO_SFRAME];                /* received frame waiting for a
                                                   buffer (CRCs included) */
    int32   frame_len;                          /* its length, 0 = none */
    DEVICE  *dptr;                              /* for debug output */
    uint32  dbg;                                /* debug flag of connection messages */
    };

void combo_sync_setup (COMBO_SYNC *cs, DEVICE *dptr, UNIT *uptr, uint32 dbg);
t_stat combo_sync_set_peer (COMBO_SYNC *cs, UNIT *uptr, CONST char *cptr);
t_stat combo_sync_clear_peer (COMBO_SYNC *cs, UNIT *uptr, CONST char *cptr);
t_stat combo_sync_show_peer (FILE *st, COMBO_SYNC *cs);
t_stat combo_sync_attach (COMBO_SYNC *cs, UNIT *uptr, CONST char *cptr);
t_stat combo_sync_detach (COMBO_SYNC *cs, UNIT *uptr);
t_bool combo_sync_poll_rx (COMBO_SYNC *cs, UNIT *uptr);    /* TRUE: new connection */
void combo_sync_poll_tx (COMBO_SYNC *cs, UNIT *uptr);
t_bool combo_sync_up (COMBO_SYNC *cs);          /* connected to the peer */
t_bool combo_sync_tx_busy (COMBO_SYNC *cs);     /* previous frame still going */
void combo_sync_send (COMBO_SYNC *cs, uint8 *frame, int32 len);
void combo_sync_hold (COMBO_SYNC *cs, const uint8 *frame, int32 len);
t_bool combo_ddcmp_fetch (COMBO_SYNC *cs);      /* a DDCMP frame waits in cs->frame */
int32 combo_ddcmp_decode (const uint8 *frame, int32 len, uint8 *out, int32 *n);
int32 combo_ddcmp_encode (const uint8 *msg, int32 count, uint8 *out, int32 *len);

#endif
