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
     EK-DMB32-UG-001  DMB32 User Guide (printer registers, 3.3.22-3.3.27;
                      sync line 3.3.28-3.3.43, 3.3.52, 3.4.8)
     EK-DMB32-TD-001  DMB32 Technical Description (DDCMP support, 2.7)

   Modification history:

   28-Nov-26    Ulli/Claude  combo_sync_clear_peer (SET ... NOPEER,
                             known issue 131)
   17-Nov-26    Ulli/Claude  Synchronous line link and DDCMP framing (from
                             vax_dmf.c), for the DMB32 sync line
   15-Nov-26    Ulli/Claude  First version: printer formatter (from vax_dmf.c)
*/

#include "vax_combo_lib.h"
#include "pdp11_ddcmp.h"                        /* DDCMP framing and CRC */

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

/* ---------------------------------------------------------------------------
   Synchronous line: link to the peer and DDCMP framing

   The link is a one-line TMXR connection; ATTACH gives the local port
   (and SET PEER the partner to connect to), as for DMF-SYNC and the DMC.
   It carries whole DDCMP frames with their CRCs (ddcmp_tmxr_put/get_
   packet_ln), so a board talks to a DMC11, DMR11, DMP11, DMV11, DMF32 or
   DMB32 on the other side.
   --------------------------------------------------------------------------- */

/* Set up the connection structures (device reset; the first one comes
   before any ATTACH): one line, message oriented raw TCP without Telnet
   (as the DMC11 and DMF-SYNC). */
void combo_sync_setup (COMBO_SYNC *cs, DEVICE *dptr, UNIT *uptr, uint32 dbg)
{
if (cs->desc.ldsc == NULL) {                    /* not yet set up */
    memset (&cs->desc, 0, sizeof (cs->desc));
    cs->desc.lines = 1;
    cs->desc.ldsc = cs->ln;
    }
cs->desc.packet = TRUE;                         /* message oriented, raw TCP */
cs->desc.notelnet = TRUE;
cs->desc.buffered = 16384;
cs->desc.uptr = uptr;
cs->desc.dptr = dptr;
cs->dptr = dptr;
cs->dbg = dbg;
}

/* SET <unit> PEER=host:port (only while not attached) */
t_stat combo_sync_set_peer (COMBO_SYNC *cs, UNIT *uptr, CONST char *cptr)
{
char host[CBUFSIZE], port[CBUFSIZE];

if ((cptr == NULL) || (*cptr == 0))
    return SCPE_ARG;
if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
if (sim_parse_addr (cptr, host, sizeof (host), NULL, port, sizeof (port), NULL, NULL))
    return SCPE_ARG;
strlcpy (cs->peer, cptr, sizeof (cs->peer));
return SCPE_OK;
}

/* SET <unit> NOPEER: forget the peer, so that the next ATTACH only
   listens (only while not attached, like PEER=) */
t_stat combo_sync_clear_peer (COMBO_SYNC *cs, UNIT *uptr, CONST char *cptr)
{
if ((cptr != NULL) && (*cptr != 0))     /* NOPEER takes no value */
    return SCPE_ARG;
if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
memset (cs->peer, 0, sizeof (cs->peer));
return SCPE_OK;
}

t_stat combo_sync_show_peer (FILE *st, COMBO_SYNC *cs)
{
fprintf (st, "peer=%s", cs->peer[0] ? cs->peer : "unspecified");
return SCPE_OK;
}

/* ATTACH <unit> port: listen on the port, connect to the peer if set */
t_stat combo_sync_attach (COMBO_SYNC *cs, UNIT *uptr, CONST char *cptr)
{
char attach[CBUFSIZE * 2 + 32];
t_stat r;

if (cs->peer[0])
    snprintf (attach, sizeof (attach), "Line=0,Connect=%s,%s", cs->peer, cptr);
else
    snprintf (attach, sizeof (attach), "Line=0,%s", cptr);
r = tmxr_open_master (&cs->desc, attach);
if (r != SCPE_OK)
    return r;
uptr->filename = (char *)malloc (strlen (cptr) + 1);
strcpy (uptr->filename, cptr);
uptr->flags |= UNIT_ATT;
cs->frame_len = 0;
return SCPE_OK;
}

t_stat combo_sync_detach (COMBO_SYNC *cs, UNIT *uptr)
{
t_stat r;

if (!(uptr->flags & UNIT_ATT))
    return SCPE_OK;
r = tmxr_close_master (&cs->desc);
uptr->flags &= ~UNIT_ATT;
free (uptr->filename);
uptr->filename = NULL;
cs->frame_len = 0;
return r;
}

/* Connections and input; TRUE if the peer has just connected */
t_bool combo_sync_poll_rx (COMBO_SYNC *cs, UNIT *uptr)
{
t_bool newconn = FALSE;

if (!(uptr->flags & UNIT_ATT))
    return FALSE;
if (tmxr_poll_conn (&cs->desc) >= 0) {
    sim_debug (cs->dbg, cs->dptr, "sync line connected\n");
    cs->ln[0].rcve = 1;
    newconn = TRUE;
    }
tmxr_poll_rx (&cs->desc);
return newconn;
}

void combo_sync_poll_tx (COMBO_SYNC *cs, UNIT *uptr)
{
if (uptr->flags & UNIT_ATT)
    tmxr_poll_tx (&cs->desc);
}

t_bool combo_sync_up (COMBO_SYNC *cs)
{
return cs->ln[0].conn ? TRUE : FALSE;
}

t_bool combo_sync_tx_busy (COMBO_SYNC *cs)
{
return (cs->ln[0].conn && tmxr_tpbusyln (&cs->ln[0])) ? TRUE : FALSE;
}

/* A whole frame (CRCs included) to the peer */
void combo_sync_send (COMBO_SYNC *cs, uint8 *frame, int32 len)
{
if (cs->ln[0].conn)
    ddcmp_tmxr_put_packet_ln (&cs->ln[0], frame, len, 0);
}

/* Keep a received frame (also a looped back one) until a buffer is free */
void combo_sync_hold (COMBO_SYNC *cs, const uint8 *frame, int32 len)
{
if (len > COMBO_SFRAME)
    len = COMBO_SFRAME;
if (len <= 0)
    return;
memcpy (cs->frame, frame, len);
cs->frame_len = len;
}

/* TRUE if a DDCMP frame waits in cs->frame (taken from the line if none
   was waiting). */
t_bool combo_ddcmp_fetch (COMBO_SYNC *cs)
{
const uint8 *pkt = NULL;
uint16 size = 0;

if (cs->frame_len)
    return TRUE;
if (!cs->ln[0].conn)
    return FALSE;
ddcmp_tmxr_get_packet_ln (&cs->ln[0], &pkt, &size, 0);
if (pkt == NULL)
    return FALSE;
combo_sync_hold (cs, pkt, size);
return cs->frame_len != 0;
}

/* A received frame as the host gets it: the 6 header characters and the
   data, without the two CRCs (TD 2.7.2).  *n is the number of characters
   for the buffer.  A bad header CRC, a bad start character or a short
   frame: CDD_DROP (the receiver resynchronizes, TD 2.7.2.6); a bad data
   CRC: CDD_DCRC (stored, with the message error). */
int32 combo_ddcmp_decode (const uint8 *frame, int32 len, uint8 *out, int32 *n)
{
int32 dlen;

*n = 0;
if (len < DDCMP_HEADER_SIZE)
    return CDD_DROP;
if ((frame[0] != DDCMP_SOH) && (frame[0] != DDCMP_ENQ) && (frame[0] != DDCMP_DLE))
    return CDD_DROP;
if (ddcmp_crc16 (0, frame, DDCMP_HEADER_SIZE) != 0)
    return CDD_DROP;
memcpy (out, frame, 6);
*n = 6;
if (frame[0] == DDCMP_ENQ)                      /* control message: header only */
    return CDD_OK;
dlen = ((frame[2] & 077) << 8) | frame[1];
if ((dlen + DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE) > len)
    return CDD_DROP;                            /* (short frame) */
memcpy (out + 6, frame + DDCMP_HEADER_SIZE, dlen);
*n += dlen;
if (ddcmp_crc16 (0, frame + DDCMP_HEADER_SIZE, dlen + DDCMP_CRC_SIZE) != 0)
    return CDD_DCRC;
return CDD_OK;
}

/* A message from a transmit buffer (header and data, count characters)
   as a frame with both CRCs; *len is the frame length.  A control message
   (ENQ) is the header alone; the data count of a data or maintenance
   message must fit in the buffer. */
int32 combo_ddcmp_encode (const uint8 *msg, int32 count, uint8 *out, int32 *len)
{
int32 dlen;
uint16 crc;

*len = 0;
if ((count < 6) || ((msg[0] != DDCMP_SOH) && (msg[0] != DDCMP_ENQ) && (msg[0] != DDCMP_DLE)))
    return CDE_HDR;
dlen = (msg[0] == DDCMP_ENQ) ? 0 : (((msg[2] & 077) << 8) | msg[1]);
if (count < 6 + dlen)
    return CDE_LEN;
memcpy (out, msg, 6);
crc = ddcmp_crc16 (0, out, 6);
out[6] = crc & 0377;
out[7] = (crc >> 8) & 0377;
*len = DDCMP_HEADER_SIZE;
if (dlen) {
    memcpy (out + DDCMP_HEADER_SIZE, msg + 6, dlen);
    crc = ddcmp_crc16 (0, out + DDCMP_HEADER_SIZE, dlen);
    out[DDCMP_HEADER_SIZE + dlen] = crc & 0377;
    out[DDCMP_HEADER_SIZE + dlen + 1] = (crc >> 8) & 0377;
    *len += dlen + DDCMP_CRC_SIZE;
    }
return CDE_OK;
}
