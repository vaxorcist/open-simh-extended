/* pdp11_dmc.c: DMC11/DMR11/DMP11/DMV11 Emulation
  ------------------------------------------------------------------------------

   Copyright (c) 2011, Robert M. A. Jarratt, Mark Pizzolato

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

  ------------------------------------------------------------------------------

  Written by Mark Pizzolato based on an original version by Robert Jarratt

  Modification history:

  25-Jan-13  RJ   Error checking for already attached in dmc_settype() fixed.
  25-Jan-13  RJ   If attach when already attached then detach first.
  23-Jan-13  RJ   Don't do anything if not attached. See https://github.com/simh/simh/issues/28
  23-Jan-13  RJ   Clock co-scheduling move to generic framework (from Mark Pizzolato)
  21-Jan-13  RJ   Added help.
  15-Jan-13  RJ   Contribution from Paul Koning:
                  Support for RSTS using the ROM INPUT (ROM I) command to get
                  the DMC11 to report DSR status.
                  Don't accept any data from the peer until a buffer has been
                  made available.
  04-Aug-13  MP   Massive Rewrite/Restructure to implement the DDCMP wire 
                  protocol for interoperation with other sync devices (DUP11, 
                  KDP, etc.)

  ------------------------------------------------------------------------------


I/O is done through sockets so that the remote system can be on the same 
host machine. The device starts polling for incoming connections when it 
receives its first read buffer. The device opens the connection for writing 
when it receives the first write buffer.

Transmit and receive buffers are added to their respective queues and the 
polling method in dmc_poll_svc() checks for input and sends any output.

Tested with two diagnostics. To run the diagnostics set the default 
directory to SYS$MAINTENANCE, run ESSAA and then configure it for the 
DMC-11 with the following commands:

The above commands can be put into a COM file in SYS$MAINTENANCE (works 
on VMS 3.0 but not 4.6, not sure why).

ATT DW780 SBI DW0 3 4
ATT DMC11 DW0 XMA0 760070 300 5
SELECT XMA0
(if putting these into a COM file to be executed by ESSAA add a "DS> " prefix)


The first is EVDCA which takes no parameters. Invoke it with the command 
R EVDCA. This diagnostic uses the DMC-11 loopback functionality and the 
transmit port is not used when LU LOOP is enabled. Seems to work only under 
later versions of VMS such as 4.6, does not work on 3.0.  It does not work
under VMS 3.x since in that environment, no receive buffers are ever made
available to the device while testing.

The second is EVDMC, invoke this with the command R EVDMC. For this I used 
the following commands inside the diagnostic:

RUN MODE=TRAN on one machine
RUN MODE=REC on the other

or using loopback mode:

SET TRANSMIT=CCITT/SIZE=25/COPY=5
SET EXPECT=CCITT/SIZE=25/COPY=5
RUN MODE=ACTIVE/LOOP=INT/STATUS/PASS=3

You can add /PASS=n to the above commands to get the diagnostic to send and 
receive more buffers.

The other test was to configure DECnet on VMS 4.6 and do SET HOST.
*/

// TODO: Test MOP.

#if defined (VM_PDP10)                                  /* PDP10 version */
#include "pdp10_defs.h"

#if !defined(DMP_NUMDEVICE)
#define DMP_NUMDEVICE 1         /* Minimum number for array size DMP-11/DMV-11 devices */
#endif

#elif defined (VM_VAX)                                  /* VAX version */
#include "vax_defs.h"

#else                                                   /* PDP-11 version */
#include "pdp11_defs.h"
#endif

#include "sim_tmxr.h"
#include "pdp11_ddcmp.h"

#define DMC_CONNECT_POLL    2   /* Seconds */

extern int32 IREQ (HLVL);
extern int32 tmxr_poll;                                 /* calibrated delay */
extern int32 clk_tps;                                   /* clock ticks per second */
extern int32 tmr_poll;                                  /* instructions per tick */

#if !defined(DMC_NUMDEVICE)
#define DMC_NUMDEVICE 8         /* default MAX # DMC-11 devices */
#endif

#if !defined(DMP_NUMDEVICE)
#define DMP_NUMDEVICE 8         /* default MAX # DMP-11/DMV-11 devices */
#endif

#define DMC_RDX                     8


/* DMC/DMR register SEL0 */
#define DMC_SEL0_V_ITYPE     0
#define DMC_SEL0_S_ITYPE     4
#define DMC_SEL0_M_ITYPE    (((1<<DMC_SEL0_S_ITYPE)-1)<<DMC_SEL0_V_ITYPE)
#define DMC_C_TYPE_XBACC     0      /* Transmit Buffer Address/Character Count Command and Response */
#define DMC_C_TYPE_CNTL      1      /* Control In Command and Control Out Response */
#define DMC_C_TYPE_HALT      2      /* Halt In Command */
#define DMC_C_TYPE_BASEIN    3      /* Base In Command */
#define DMC_C_TYPE_RBACC     4      /* Receive Buffer Address/Character Count Command and Response */
#define DMC_SEL0_V_RQI       5
#define DMC_SEL0_M_RQI      0x0020
#define DMC_SEL0_V_IEI       6
#define DMC_SEL0_M_IEI      0x0040
#define DMC_SEL0_V_RDI       7
#define DMC_SEL0_M_RDI      0x0080
#define DMC_SEL0_V_STEPUP    8
#define DMC_SEL0_M_STEPUP   0x0100
#define DMC_SEL0_V_ROMI      9
#define DMC_SEL0_M_ROMI     0x0200
#define DMC_SEL0_V_ROMO     10
#define DMC_SEL0_M_ROMO     0x0400
#define DMC_SEL0_V_LU_LOOP  11
#define DMC_SEL0_M_LU_LOOP  0x0800
#define DMC_SEL0_V_STEPLU   12
#define DMC_SEL0_M_STEPLU   0x1000
#define DMC_SEL0_V_UDIAG    13
#define DMC_SEL0_M_UDIAG    0x2000
#define DMC_SEL0_V_MCLEAR   14
#define DMC_SEL0_M_MCLEAR   0x4000
#define DMC_SEL0_V_RUN      15
#define DMC_SEL0_M_RUN      0x8000

/* DMP/DMV register SEL0 */
#define DMP_SEL0_V_IEI       0
#define DMP_SEL0_M_IEI      0x0001
#define DMP_SEL0_V_IEO       4
#define DMP_SEL0_M_IEO      0x0010
#define DMP_SEL0_V_RQI       7
#define DMP_SEL0_M_RQI      0x0080

/* DMC/DMR register SEL2 */
#define DMC_SEL2_V_CODE      0
#define DMC_SEL2_S_CODE      3
#define DMC_SEL2_M_CODE     (((1<<DMC_SEL2_S_CODE)-1)<<DMC_SEL2_V_CODE)
#define DMC_SEL2_V_IEO       6
#define DMC_SEL2_M_IEO      0x0040
#define DMC_SEL2_V_RDO       7
#define DMC_SEL2_M_RDO      0x0080

/* DMP/DMV register SEL2 */
#define DMP_SEL2_V_CODE      0
#define DMP_SEL2_S_CODE      3
#define DMP_SEL2_M_CODE     (((1<<DMP_SEL2_S_CODE)-1)<<DMP_SEL2_V_CODE)
#define DMP_C_TYPE_RBACC     0      /* Receive Buffer Address/Character Count Command and Response */
#define DMP_C_TYPE_CNTL      1      /* Control Command or Control Response */
#define DMP_C_TYPE_MODE      2      /* Mode Definition Command or Information Response */
#define DMP_C_TYPE_UNUSED    3      /* Receive Buffer disposition (unused) Response */
#define DMP_C_TYPE_XBACC     4      /* Transmit Buffer Address/Character Count Command and Response */
#define DMP_C_TYPE_RESERVED  5      /* Reserved/Unused code */
#define DMP_C_TYPE_SENT      6      /* Buffer disposition (sent but not acknowledged) Response */
#define DMP_C_TYPE_UNSENT    7      /* Buffer disposition (not sent) Response */
#define DMP_SEL2_V_22BIT     3
#define DMP_SEL2_M_22BIT    0x0008
#define DMP_SEL2_V_RDI       4
#define DMP_SEL2_M_RDI      0x0010
#define DMP_SEL2_V_RDO       7
#define DMP_SEL2_M_RDO      0x0080
#define DMP_SEL2_V_STATION   8      /* BSEL3: tributary address */

/* DMP/DMV control command: request key in BSEL6 bits 0-4.  When one of the
   status access bits (BSEL6 bits 5-7) is set instead, bits 0-4 are a status
   memory location.  BSEL7 carries independent tributary controls (common
   buffer pool, polling state) that may accompany keys 000-004.  Values from
   the DMV11 Technical Manual EK-DMV11-TM-001 and user's guide EK-DMVQM-UG-003. */
#define DMP_KEY_MASK        037
#define DMP_KEY_NOP         000     /* no request (BSEL7 controls only) */
#define DMP_KEY_ESTABLISH   001     /* establish tributary status slot */
#define DMP_KEY_DELETE      002     /* delete (halted) tributary */
#define DMP_KEY_START       003     /* start DDCMP */
#define DMP_KEY_MAINT       004     /* start maintenance (MOP) mode */
#define DMP_KEY_HALT        005     /* halt tributary, or all with address 0 */
#define DMP_KEY_READ_MODEM  020     /* read modem signals */
#define DMP_KEY_WRITE_MODEM 021     /* write modem outputs (DTR, RTS) */
#define DMP_STATUS_READ     0040    /* status access: read */
#define DMP_STATUS_CLEAR    0100    /* status access: read and clear (counters) */
#define DMP_STATUS_WRITE    0200    /* status access: write (parameters) */
#define DMP_STATUS_MASK     0340

/* Information response return keys (BSEL6) */
#define DMP_RETKEY_MODEM    010     /* answer to read modem */
#define DMP_RETKEY_HALT     020     /* halt complete */

/* Control response output codes (BSEL6, or BSEL10 in the Q22 format), from
   Table 3-7 "Output Codes" of the DMV11 Technical Manual EK-DMV11-TM-001.
   002-030 are network errors and protocol/system events for one tributary,
   0100-0276 procedural errors, 0300-0310 buffer errors and global events.
   Not generated here: 002 receive threshold (no line errors on TCP), 006
   select threshold, 022 polling state dead, 026 babbling and 030 streaming
   tributary (multipoint/half duplex only), 016 start received in
   maintenance state (the DDCMP engine ignores it). */
#define DMP_RSP_RCV_THRESH  0002    /* receive threshold error (7 consecutive) */
#define DMP_RSP_XMT_THRESH  0004    /* transmit threshold error (7 consecutive) */
#define DMP_RSP_SEL_THRESH  0006    /* select threshold error */
#define DMP_RSP_STRT_RUN    0010    /* start received while running: tributary halted */
#define DMP_RSP_MAINT_RUN   0012    /* maintenance message received while running
                                       (or ISTRT/ASTRT): now in maintenance state */
#define DMP_RSP_MAINT_HALT  0014    /* maintenance message received while halted */
#define DMP_RSP_STRT_MAINT  0016    /* start received in maintenance state */
#define DMP_RSP_RUNNING     0024    /* DDCMP running (start-up complete) */
#define DMP_RSP_BUF_SMALL   0300    /* receive buffer too small */
#define DMP_RSP_NXM         0302    /* non-existent memory on a buffer */
#define DMP_RSP_DISCONNECT  0304    /* modem disconnected (DSR lost) */
#define DMP_RSP_OVERFLOW    0306    /* queue overflow */
#define DMP_RSP_CARRIER     0310    /* carrier lost during receive */
/* Procedural errors */
#define DMP_ERR_NO_MODE     0100    /* command before mode definition */
#define DMP_ERR_BAD_TYPE    0102    /* invalid command type */
#define DMP_ERR_BAD_MODE    0104    /* second, different mode definition */
#define DMP_ERR_NOT_ESTAB   0106    /* tributary not established */
#define DMP_ERR_NEED_TRIB   0110    /* request needs a tributary address */
#define DMP_ERR_NOT_HALTED  0112    /* tributary must be halted first */
#define DMP_ERR_TOO_MANY    0114    /* no free tributary status slot */
#define DMP_ERR_ESTAB       0116    /* tributary already established */
#define DMP_ERR_BAD_KEY     0120    /* invalid request key */
#define DMP_ERR_BUF_UNESTAB 0122    /* buffer for an unestablished tributary */
#define DMP_ERR_BUF_HALTED  0124    /* buffer for a halted tributary */
#define DMP_ERR_ZERO_COUNT  0126    /* buffer with zero byte count */
#define DMP_ERR_XMT_TRIB0   0130    /* transmit buffer with tributary address 0 */
#define DMP_ERR_BAD_ACCESS  0132    /* status location not accessible that way */
#define DMP_ERR_BSEL7       0134    /* reserved BSEL7 bit used in a control command */
#define DMP_ERR_POOL_INUSE  0136    /* global halt while the common pool is in use */
#define DMP_ERR_QUOTA       0140    /* common pool quota raised to 0377 or more */

/* Buffer disposition response types (BSEL2 bits 0-2, Figure 3-8) for
   buffers handed back when a tributary halts */
#define DMP_DISP_RCV_UNUSED 03      /* receive buffer unused */
#define DMP_DISP_XMT_NOACK  06      /* transmit buffer sent but not acknowledged */
#define DMP_DISP_XMT_UNSENT 07      /* transmit buffer not sent */

/* Control command BSEL7 (SEL6 bits 8-15, Figure 3-6 and Table 3-5) */
#define DMP_CTL_POOL_ENABLE 0x0100  /* enable common pool, quota += BSEL4 (0377 = unlimited) */
#define DMP_CTL_POOL_DISABLE 0x0200 /* disable common pool, quota = 0 */
#define DMP_CTL_UNLATCH     0x1000  /* unlatch polling state (multipoint only) */
#define DMP_CTL_LATCH       0x2000  /* latch polling state from BSEL4 bits 0-1 */
#define DMP_CTL_RESERVED    0xCC00  /* bits 10, 11, 14 and 15 */
#define DMP_POOL_UNLIMITED  0377

/* Tributary status slot words (Figure 5-11) maintained by this emulation */
#define DMP_TSS_QUOTA       005     /* low byte: common pool quota */
#define DMP_TSS_THRESH      006     /* low byte: transmit threshold error count */
#define DMP_TSS_XMT_MSGS    007     /* data messages transmitted (16 bit) */
#define DMP_TSS_RCV_MSGS    010     /* data messages received (16 bit) */
#define DMP_TSS_REPLY_TO    017     /* low byte: local reply timeouts (REPs sent) */
#define DMP_XMT_THRESHOLD   7       /* consecutive transmit errors for 004 */
#define DDCMP_NAK_MSG_TOO_LONG 16   /* DDCMP NAK reason: message too long for the receive buffer */

/* Self-test signature left in BSEL4/BSEL6 after master clear */
#define DMV_SIGNATURE_BSEL4 033
#define DMP_SIGNATURE_BSEL4 077
#define DMP_SIGNATURE_BSEL6 0305

/* DMC/DMR register SEL4 */
#define DMC_SEL4_V_CAR       0
#define DMC_SEL4_M_CAR      0x0001
#define DMC_SEL4_V_STN       1
#define DMC_SEL4_M_STN      0x0002
#define DMC_SEL4_V_CTS       2
#define DMC_SEL4_M_CTS      0x0004
#define DMC_SEL4_V_DSR       3
#define DMC_SEL4_M_DSR      0x0008
#define DMC_SEL4_V_HDX       4
#define DMC_SEL4_M_HDX      0x0010
#define DMC_SEL4_V_RTS       5
#define DMC_SEL4_M_RTS      0x0020
#define DMC_SEL4_V_DTR       6
#define DMC_SEL4_M_DTR      0x0040
#define DMC_SEL4_V_RI        7
#define DMC_SEL4_M_RI       0x0080

/* SEL2 */
#define DMP_TYPE_INPUT_MASK 0x0007

/* DMC/DMR register SEL6 */
/* Bit Flags used in a Control Out command */
#define DMC_SEL6_V_NAKTRSH   0      /* NAK Threshold */
#define DMC_SEL6_M_NAKTRSH  0x0001
#define DMC_SEL6_V_TIMEOUT   1
#define DMC_SEL6_M_TIMEOUT  0x0002
#define DMC_SEL6_V_NOBUF     2
#define DMC_SEL6_M_NOBUF    0x0004
#define DMC_SEL6_V_MAINTRCV  3
#define DMC_SEL6_M_MAINTRCV 0x0008
#define DMC_SEL6_V_LOSTDATA  4
#define DMC_SEL6_M_LOSTDATA 0x0010
#define DMC_SEL6_V_DISCONN   6
#define DMC_SEL6_M_DISCONN  0x0040
#define DMC_SEL6_V_STRTRCVD  7
#define DMC_SEL6_M_STRTRCVD 0x0080
#define DMC_SEL6_V_NXM       8
#define DMC_SEL6_M_NXM      0x0100
#define DMC_SEL6_V_HALTCOMP  9
#define DMC_SEL6_M_HALTCOMP 0x0200
/* Bit Flags used in a Control In command */
#define DMC_SEL6_V_MAINT     8      /* NAK Threshold */
#define DMC_SEL6_M_MAINT    0x0100
#define DMC_SEL6_V_HDX      10
#define DMC_SEL6_M_HDX      0x0400
#define DMC_SEL6_V_LONGSTRT 11
#define DMC_SEL6_M_LONGSTRT 0x0800
/* Bit flags used in DMR modem status after RDI */
#define DMC_SEL6_V_LMODEM 11
#define DMC_SEL6_M_LMODEM 0x0800

#define DSPDSR     0x22b3    /* KMC opcode to move line unit status to SEL2 */
#define DROPDTR    0xa40b    /* KMC opcode to drop DTR */
#define UINST_CNF  0x2296    /* KMC opcode to get config switches */
#define UINST_CNF3 0x2293    /* KMC opcode ditto but to BSEL3 */
#define UINST_RROM 0x8100    /* KMC opcode to read DMC ROM, address in low bits */

#define SEL2_TYPEO_BIT 0
#define SEL2_RDO_BIT 7
#define SEL2_IEO_BIT 6
#define SEL2_OUT_IO_BIT 2
#define SEL2_LINE_BIT 8
#define SEL2_LINE_BIT_LENGTH 6
#define SEL2_PRIO_BIT 14
#define SEL2_PRIO_BIT_LENGTH 2

#define DMC_QUEUE_SIZE  7
#define DMR_QUEUE_SIZE 64
#define DMP_QUEUE_SIZE 64

struct csrs {
    uint16 *sel0;
    uint16 *sel2;
    uint16 *sel4;
    uint16 *sel6;
    uint16 *sel10;
    };

typedef struct csrs CSRS;

typedef enum {
    Uninitialised,  /* before MASTER CLEAR */
    Initialised,    /* after MASTER CLEAR */
    Running,        /* after any transmit or receive buffer has been supplied */
    Halted          /* after reciept of explicit halt input command */
    } ControllerState;

typedef enum {
    Idle,
    InputTransfer,
    OutputTransfer,
    OutputControl
    } TransferState;

/* 129 words taken from RSTS/E ECO checker listing; the rest is not
   used so we can leave it as zeroes.  */
static const uint16 hi_speed_ucode[256] = {
    063220,
    063223,
    063237,
    063232,
    061200,
    061202,
    03370,
    063130,
    076423,
    063060,
    0101414,
    0100407,
    03401,
    063231,
    010162,
    0626,
    062234,
    016403,
    016402,
    016406,
    016407,
    016401,
    010210,
    016455,
    016472,
    010067,
    016471,
    02471,
    043236,
    010022,
    016424,
    02424,
    043235,
    010240,
    016642,
    02642,
    0600,
    061221,
    0110642,
    020620,
    0173202,
    020640,
    0167203,
    010210,
    0140620,
    020620,
    0103060,
    0100463,
    0433,
    060342,
    0101047,
    0123620,
    0113246,
    023240,
    060520,
    0103451,
    010211,
    0140620,
    060601,
    0103100,
    0123400,
    01620,
    0117034,
    0100447,
    02611,
    0100447,
    0123400,
    0103505,
    0114434,
    060520,
    0103603,
    0120400,
    01620,
    0103047,
    02514,
    0100447,
    0123400,
    0102527,
    02654,
    0102124,
    01620,
    02722,
    0102524,
    02701,
    060601,
    0102130,
    0100447,
    0102140,
    02472,
    0500,
    061260,
    010177,
    016402,
    02400,
    042233,
    0114474,
    060721,
    0102130,
    02472,
    010017,
    0136500,
    0136520,
    0122560,
    0123000,
    0500,
    061260,
    02133,
    040620,
    0103164,
    010151,
    016406,
    02700,
    063161,
    0641,
    03374,
    0110727,
    03004,
    063070,
    010017,
    0731,
    0110463,
    010154,
    057310,
    057231,
    057235,
    043237,
    043232,
    063170,
    063161
};
    
/* Queue elements
 * A queue is a double-linked list of element headers.
 * The head of the queue is an element without a body.
 * A queue is empty when the only element in the queue is
 * the head.
 * Each queue has an asociated count of the elements in the
 * queue; this simplifies knowing its state.
 * Each queue also has a maximum length.
 *
 * Queues are manipulated with initqueue, insqueue, and remqueue.
 */
struct queuehdr {
    struct queuehdr     *next;
    struct queuehdr     *prev;
    struct buffer_queue *queue;
    };
typedef struct queuehdr QH;

typedef struct buffer_queue {
    QH hdr;                             /* Forward/Back Buffer pointers */
    const char * name;                  /* Queue name */
    size_t size;                        /* Maximum number of entries (0 means no limit) */
    size_t count;                       /* Current Used Count */
    struct dmc_controller *controller;  /* back pointer to the containing controller */
    } BUFFER_QUEUE;

/* Queue management */

/* Insert entry on queue after pred, if count < max.
 * Increment count.
 *   To insert at head of queue, specify &head for predecessor.
 *   To insert at tail, specify head.prev
 *
 * returns FALSE if queue is full.
 */

static t_bool insqueue (QH *entry, QH *pred)
{
if ((pred->queue->size > 0) && (pred->queue->count >= pred->queue->size))
    return FALSE;
ASSURE (entry->queue == NULL);
entry->next = pred->next;
entry->prev = pred;
entry->queue = pred->queue;
pred->next->prev = entry;
pred->next = entry;
++pred->queue->count;
return TRUE;
}

/* Remove entry from queue.
 * Decrement count.
 *  To remove from head of queue, specify head.next.
 *  To remove form tail of queue, specify head.prev.
 *
 * returns NULL if queue is empty.
 */

static void *remqueue (QH *entry)
{
if (entry->queue->count == 0)
    return NULL;
entry->prev->next = entry->next;
entry->next->prev = entry->prev;
--entry->queue->count;
entry->next = entry->prev = NULL;
entry->queue = NULL;
return (void *)entry;
}

/* Initialize a queue to empty.
 *
 * Optionally, adds a list of elements to the queue.
 * max, list and size are only used if list is non-NULL.
 *
 * Convenience macros:
 *   MAX_LIST_SIZE(q) specifies max, list and size for an array of elements q
 *   INIT_HDR_ONLY provides placeholders for these arguments when only the
 * header and count are to be initialized.
 */

#define DIM(x) (sizeof(x)/sizeof((x)[0]))
/* Convenience for initqueue() calls */
#    define MAX_LIST_SIZE(q)                    DIM(q),    (q),       sizeof(q[0])
#    define INIT_HDR_ONLY                       0,         NULL,      0

static void initqueue (QH *head, struct buffer_queue *queue, size_t max, void *list, size_t size)
{
head->next = head->prev = head;
head->queue = queue;
head->queue->count = 0;
head->queue->size = max;
if (list == NULL)
    return;
while (insqueue ((QH *)list, head->prev))
    list = (QH *)(((char *)list)+size);
return;
}

typedef enum {
    Receive,                /* Receive Buffer */
    TransmitData,           /* Transmit Buffer */
    TransmitControl         /* Transmit Control Packet */
    } BufferType;

typedef struct buffer {
    QH hdr;                   /* queue linkage */
    BufferType type;          /* 0 = Receive Buffer, 1 = Transmit Buffer */
    uint32 address;           /* unibus address of the buffer (or 0 for DDCMP control messages) */
    uint16 count;             /* size of the buffer passed to the device by the driver */
    uint8 station;            /* DMP/DMV: tributary address (BSEL3) the buffer was given
                                 for, reported back in BSEL3 when it is returned */
    t_bool q22;               /* DMP/DMV: buffer was given in the Q22 (22-bit) address and
                                 count format; its return uses the same format */
    uint8 *transfer_buffer;   /* the buffer into which data is received or from which it is transmitted*/
    int actual_bytes_transferred;/* the number of bytes from the actual block that have been read or written */
    uint32 buffer_return_time;/* time to return buffer to host */
    } BUFFER;

typedef enum {
    Halt,       /* initially */
    IStart,     /* Initiating Start */
    AStart,     /* Acknowledging Start */
    Run,        /* Start ACK received */
    Maintenance,/* off-line maintenance messages */
    All         /* Not settable, but match for any state in state table */
    } DDCMP_LinkState;

typedef struct {
    uint8 R;                /* number of the highest sequential data message received . 
                               Sent in the RESP field of data messages, ACK messages,
                               and NAK messages as acknowledgment to the other station. */
    uint8 N;                /* Number of the highest sequential data message 
                               transmitted.  Sent in the NUM field of REP 
                               messages. N is the number assigned to the last 
                               user transmit request which has been transmitted 
                               (sent in the NUM field of that data message).*/
    uint8 A;                /* Number of the highest sequential data message acknowledged.
                               Received in the RESP field of data messages, ACK messages, 
                               and NAK messages.*/
    uint8 T;                /* Number of the next data message to be transmitted.
                               When sending new data messages T will have the 
                               value N+l. When retransmitting T will be set back
                               to A+l and will advance to N+l.*/
    uint8 X;                /* Number of the last data message that has been 
                               transmitted.  When a new data message has been 
                               completely transmitted X will have the value N. 
                               When retransmitting and receiving acknowledgments 
                               asynchronously with respect to transmission, X will 
                               have some value less than or equal to N. */
    uint8 NAKed;            /* The value of R sent in the most recent NAK message.  This
                               is used to avoid sending additional NAKs when one has already
                               been sent while the remaining packets in the transmit pipeline
                               are still arriving. */
    t_bool SACK;            /* Send ACK flag.  This flag is set when either R 
                               is incremented, meaning a new sequential data 
                               message has been received which requires an ACK 
                               reply, or a REP message is received which requires 
                               an ACK reply. The SACK flag is cleared when 
                               sending either a DATA message with the latest RESP 
                               field information, an ACK with the latest RESP 
                               field information, or when the SNAK flag is set. */
                 /* 
                    The SACK and SNAK flags are mutually exclusive. At most one 
                    will be set at a given time. The events that set the SACK 
                    flag, R is increment~d or a REP is received requiring an ACK, 
                    also clear the SNAK flag. Similarly, the events that set the 
                    SNAK flag, reasons for send ing a NAK (see 5.3.7), also clear 
                    the SACK flag. Setting or clearing a flag that is already set 
                    or cleared respectively has no effect. For the SNAK flag, a 
                    reason variable (or field) is also maintained which is 
                    overwritten with the latest NAK error reason.
                    Whenever the SNAK flag is set the NAK reason variable is set 
                    to the reason for the NAK. */
    t_bool SNAK;            /* Send NAK flag.  This flag is set when a receive 
                               error occurs that requires a NAK reply. It is 
                               cleared when a NAK message is sent with the 
                               latest RESP information, or when the SACK flag
                               is set.*/
    t_bool SREP;            /* Send REP flag.  This flag is set when a reply 
                               timer expires in the running state and a REP 
                               should be sent. It is independent of the SACK 
                               and SNAK flags.*/
    uint8 *rcv_pkt;         /* Current Rcvd Packet buffer */
    uint16 rcv_pkt_size;    /* Current Rcvd Packet size */
    BUFFER *xmt_buffer;     /* Current Transmit Buffer */
    BUFFER *xmt_done_buffer;/* Just Completed Transmit Buffer */
    uint8 nak_reason;       /*  */
    uint8 nak_crc_reason;   /* CRC status for current received packet */
    DDCMP_LinkState state;  /* Current State */
    t_bool TimerRunning;    /* Timer Running Flag */
    t_bool TimeRemaining;   /* Seconds remaining before timeout (when timer running) */
    t_bool Scanning;        /* Event Scanning in progress */
    uint32 ScanningEvents;  /* Event Mask while scanning */
    t_bool RecurseScan;     /* Scan was attempted while scanning */
    uint32 RecurseEventMask;/* Mask for recursive Scan */
#define DDCMP_EVENT_XMIT_DONE 0x001
#define DDCMP_EVENT_PKT_RCVD  0x002
#define DDCMP_EVENT_TIMER     0x004
#define DDCMP_EVENT_MAINTMODE 0x008
    } DDCMP;

typedef struct control {
    struct control *next;       /* Link */
    uint16  sel6;               /* Control Out Status Flags (DMC/DMR) */
    t_bool  dmp;                /* DMP/DMV response: present the register images below */
#define DMP_IMG_SEL2    0       /* type code, Q22 flag, tributary address (BSEL3) */
#define DMP_IMG_SEL4    1       /* data, or buffer address bits 0-15 */
#define DMP_IMG_SEL6    2       /* code, count, or address bits 16-21 */
#define DMP_IMG_SEL10   3       /* Q22 format: code or count */
    uint16  image[4];           /* DMP/DMV: register contents of the response */
    } CONTROL_OUT;

/* DMP/DMV status memory: one global status slot (GSS) and one tributary
   status slot (TSS) per established tributary, each 32 words, addressed by
   the 5-bit location field of a control command. */
#define DMP_STATUS_WORDS    32
#define DMP_MAX_TRIBS       32          /* tributary status slots: DMP11 32, DMV11 12 */
#define DMV_MAX_TRIBS       12          /* (see dmp_trib_limit) */
#define DMP_SELECT_TIMER    10000       /* msec a tributary may keep the line.  Longer
                                           than the 3 s default of Table 4-2: VMS V3.5
                                           with a DMF32 tributary answers a poll with
                                           an ACK that keeps the line and returns it
                                           only about 3 s later; selecting it again
                                           before that makes it restart the line. */
#define DMP_POLL_INTERVAL   500         /* msec: an idle tributary that has returned the
                                           line is polled (selected with an ACK) after this */

typedef struct dmp_trib {
    t_bool  in_use;                     /* established (request key 001) */
    uint8   address;                    /* tributary address, 1-255 */
    uint8   run;                        /* 0 halted, 1 DDCMP started, 2 maintenance */
    uint16  tss[DMP_STATUS_WORDS];      /* tributary status slot */
    t_bool  pool_enabled;               /* may use the common receive buffer pool */
    uint8   pool_quota;                 /* common pool buffers it may still use (0377 = unlimited) */
    /* Multipoint modes (4-7) only: this tributary's own DDCMP session.  It
       is swapped into the controller (link, queue pointers, state) while
       the DDCMP engine works on it; see dmp_session_load(). */
    struct {
        DDCMP           link;           /* protocol variables and state */
        BUFFER_QUEUE    xmt;            /* messages waiting to be sent */
        BUFFER_QUEUE    ackw;           /* sent, waiting for acknowledgement */
        BUFFER_QUEUE    rcv;            /* private receive buffers */
        ControllerState state;          /* Running after START, Halted after HALT */
        uint8           xmt_errors;     /* consecutive transmit errors (code 004) */
        TMLN            *drop;          /* control station hub: the drop line this
                                           tributary was last heard on (NULL: unknown) */
        t_bool          holds_select;   /* control station: the tributary owns the line -
                                           it was selected and has not yet sent a message
                                           with the select flag - so it must not be
                                           selected again */
        uint32          select_time;    /* when it got or returned the line (sim_os_msec) */
        } ses;
    } DMP_TRIB;

typedef enum {
    DMC,
    DMR,
    DMP
    } DEVTYPE;

typedef struct dmc_controller {
    CSRS *csrs;
    DEVICE *device;
    UNIT *unit;
    int index;                      /* Index in controller array */
    ControllerState state;
    TransferState transfer_state;   /* current transfer state (type of transfer) */
    DDCMP link;
    int transfer_type;              /* Input Command setting at start of input transfer as host changes it during transfer! */
    TMLN *line;
    BUFFER_QUEUE *rcv_queue;        /* Receive Buffer Queue */
    BUFFER_QUEUE *completion_queue; /* Transmit and Recieve Buffers waiting to pass to driver */
    BUFFER_QUEUE *xmt_queue;        /* Control and/or Data packets pending transmission */
    BUFFER_QUEUE *ack_wait_queue;   /* Data packets awaiting acknowledgement */
    BUFFER_QUEUE *free_queue;       /* Unused Buffer Queue */
    BUFFER **buffers;               /* Buffers */
    CONTROL_OUT *control_out;
    DEVTYPE dev_type;
    /* DMP/DMV host interface state (reset by master clear) */
    t_bool dmp_station_known;       /* a mode definition command was accepted */
    uint16 dmp_station_type;        /* its mode field (Table 3-4): bit 2 multipoint,
                                       bit 1 tributary, bit 0 full duplex */
    uint16 dmp_gss[DMP_STATUS_WORDS];/* global status slot */
    DMP_TRIB dmp_trib[DMP_MAX_TRIBS];/* established tributaries */
    uint8 dmp_xmt_errors;           /* consecutive transmit errors (output code 004) */
    /* Multipoint (modes 4-7): one DDCMP session per tributary */
    int dmp_loaded;                 /* tributary slot whose session is in the
                                       controller fields below, -1 = none */
    struct {                        /* the controller's own values while a
                                       tributary session is loaded */
        DDCMP link;
        BUFFER_QUEUE *xmt, *ackw, *rcv;
        ControllerState state;
        uint8 xmt_errors;
        } dmp_own;
    BUFFER_QUEUE *dmp_pool;         /* common receive pool (receive buffers given
                                       for tributary 0) */
    TMXR *dmp_hub;                  /* DMP/DMV attached without a PEER: the
                                       multiplexer it listens on, else NULL */
    t_bool dmp_down_reported;       /* 304 already sent for this line outage */
    uint32 in_int;
    uint32 out_int;
    uint32 dmc_wr_delay;
    uint32 *baseaddr;
    uint16 *basesize;
    uint8 *modem;
    int32 *corruption_factor;
    uint32 buffers_received_from_net;
    uint32 buffers_transmitted_to_net;
    uint32 receive_buffer_output_transfers_completed;
    uint32 transmit_buffer_output_transfers_completed;
    uint32 receive_buffer_input_transfers_completed;
    uint32 transmit_buffer_input_transfers_completed;
    uint32 control_out_operations_completed;
    uint32 ddcmp_control_packets_received;
    uint32 ddcmp_control_packets_sent;
    uint32 byte_wait;                           /* rcv/xmt byte delay */
    } CTLR;

/* 
    DDCMP implementation follows the DDCMP protocol specification documented in the 
    "DECNET DIGITAL NETWORK ARCHITECTURE Digital Data Communications Message Protocol"
    Version 4.0, March 1, 1978.
    
    available on http://bitsavers.org/pdf/dec/decnet/AA-D599A-TC_DDCMP4.0_Mar78.pdf

 */

typedef void (*DDCMP_LinkAction_Routine)(CTLR *controller);

typedef t_bool (*DDCMP_Condition_Routine)(CTLR *controller);

t_bool ddcmp_UserHalt             (CTLR *controller);
t_bool ddcmp_UserStartup          (CTLR *controller);
t_bool ddcmp_UserMaintenanceMode  (CTLR *controller);
t_bool ddcmp_ReceiveStack         (CTLR *controller);
t_bool ddcmp_ReceiveStrt          (CTLR *controller);
t_bool ddcmp_TimerRunning         (CTLR *controller);
t_bool ddcmp_TimerNotRunning      (CTLR *controller);
t_bool ddcmp_TimerExpired         (CTLR *controller);
t_bool ddcmp_ReceiveMaintMessage  (CTLR *controller);
t_bool ddcmp_ReceiveAck           (CTLR *controller);
t_bool ddcmp_ReceiveNak           (CTLR *controller);
t_bool ddcmp_ReceiveRep           (CTLR *controller);
t_bool ddcmp_NUMEqRplus1          (CTLR *controller);   /* (NUM == R+1) */
t_bool ddcmp_NUMGtRplus1          (CTLR *controller);   /* (NUM > R+1) */
t_bool ddcmp_ReceiveDataMsg       (CTLR *controller);   /* Receive Data Message */
t_bool ddcmp_ReceiveMaintMsg      (CTLR *controller);   /* Receive Maintenance Message */
t_bool ddcmp_ALtRESPleN           (CTLR *controller);   /* (A < RESP <= N) */
t_bool ddcmp_ALeRESPleN           (CTLR *controller);   /* (A <= RESP <= N) */
t_bool ddcmp_RESPleAOrRESPgtN     (CTLR *controller);   /* (RESP <= A) OR (RESP > N) */
t_bool ddcmp_TltNplus1            (CTLR *controller);   /* T < N + 1 */
t_bool ddcmp_TeqNplus1            (CTLR *controller);   /* T == N + 1 */
t_bool ddcmp_ReceiveMessageError  (CTLR *controller);   
t_bool ddcmp_NumEqR               (CTLR *controller);   /* (NUM == R) */
t_bool ddcmp_NumNeR               (CTLR *controller);   /* (NUM != R) */
t_bool ddcmp_TransmitterIdle      (CTLR *controller);
t_bool ddcmp_TramsmitterBusy      (CTLR *controller);
t_bool ddcmp_SACKisSet            (CTLR *controller);
t_bool ddcmp_SACKisClear          (CTLR *controller);
t_bool ddcmp_SNAKisSet            (CTLR *controller);
t_bool ddcmp_SNAKisClear          (CTLR *controller);
t_bool ddcmp_SREPisSet            (CTLR *controller);
t_bool ddcmp_SREPisClear          (CTLR *controller);
t_bool ddcmp_UserSendMessage      (CTLR *controller);
t_bool ddcmp_LineConnected        (CTLR *controller);
t_bool ddcmp_LineDisconnected     (CTLR *controller);
t_bool ddcmp_DataMessageSent      (CTLR *controller);
t_bool ddcmp_REPMessageSent       (CTLR *controller);

void ddcmp_StartTimer             (CTLR *controller);
void ddcmp_StopTimer              (CTLR *controller);
void ddcmp_ResetVariables         (CTLR *controller);
void ddcmp_SendStrt               (CTLR *controller);
void ddcmp_SendStack              (CTLR *controller);
void ddcmp_SendAck                (CTLR *controller);
void ddcmp_SendNak                (CTLR *controller);
void ddcmp_SendRep                (CTLR *controller);
void ddcmp_SetSACK                (CTLR *controller);
void ddcmp_ClearSACK              (CTLR *controller);
void ddcmp_SetSNAK                (CTLR *controller);
void ddcmp_ClearSNAK              (CTLR *controller);
void ddcmp_SetSREP                (CTLR *controller);
void ddcmp_CountTransmitError     (CTLR *controller);
void ddcmp_ClearSREP              (CTLR *controller);
void ddcmp_IncrementR             (CTLR *controller);
void ddcmp_SetAeqRESP             (CTLR *controller);
void ddcmp_SetTequalAplus1        (CTLR *controller);
void ddcmp_IncrementT             (CTLR *controller);
void ddcmp_SetNAKreason3          (CTLR *controller);
void ddcmp_SetNAKReasonCRCError   (CTLR *controller);
void ddcmp_NAKMissingPacket       (CTLR *controller);
void ddcmp_IfTleAthenSetTeqAplus1 (CTLR *controller);
void ddcmp_IfAltXthenStartTimer   (CTLR *controller);
void ddcmp_IfAgeXthenStopTimer    (CTLR *controller);
void ddcmp_Ignore                 (CTLR *controller);
void ddcmp_GiveBufferToUser       (CTLR *controller);
void ddcmp_CompleteAckedTransmits (CTLR *controller);
void ddcmp_ReTransmitMessageT     (CTLR *controller);
void ddcmp_NotifyDisconnect       (CTLR *controller);
void ddcmp_NotifyStartRcvd        (CTLR *controller);
void ddcmp_NotifyMaintRcvd        (CTLR *controller);
void ddcmp_SendDataMessage        (CTLR *controller);
void ddcmp_SendMaintMessage       (CTLR *controller);
void ddcmp_SetXSetNUM             (CTLR *controller);

typedef struct _ddcmp_state_table {
    int RuleNumber;
    DDCMP_LinkState State;
    DDCMP_Condition_Routine Conditions[10];
    DDCMP_LinkState NewState;
    DDCMP_LinkAction_Routine Actions[10];
    } DDCMP_STATETABLE;

DDCMP_STATETABLE DDCMP_TABLE[] = {
    { 0, All,         {ddcmp_UserHalt},            Halt,           {ddcmp_StopTimer}},
    { 1, Halt,        {ddcmp_UserStartup,
                       ddcmp_LineConnected},       IStart,         {ddcmp_ResetVariables,
                                                                    ddcmp_SendStrt,
                                                                    ddcmp_StopTimer}},
    { 2, Halt,        {ddcmp_UserMaintenanceMode}, Maintenance,    {ddcmp_ResetVariables}},
    { 3, Halt,        {ddcmp_ReceiveMaintMsg},     Maintenance,    {ddcmp_ResetVariables, 
                                                                    ddcmp_NotifyMaintRcvd}},
    { 4, IStart,      {ddcmp_TimerNotRunning},     IStart,         {ddcmp_StartTimer}},
    { 5, IStart,      {ddcmp_ReceiveStack},        Run,            {ddcmp_SetSACK,
                                                                    ddcmp_StopTimer}},
    { 6, IStart,      {ddcmp_ReceiveStrt},         AStart,         {ddcmp_SendStack, 
                                                                    ddcmp_StartTimer}},
    { 7, IStart,      {ddcmp_TimerExpired},        IStart,         {ddcmp_ResetVariables, 
                                                                    ddcmp_SendStrt, 
                                                                    ddcmp_StartTimer,
                                                                    ddcmp_CountTransmitError}},
    { 8, IStart,      {ddcmp_ReceiveMaintMsg},     Maintenance,    {ddcmp_ResetVariables, 
                                                                    ddcmp_NotifyMaintRcvd}},
    { 9, IStart,      {ddcmp_ReceiveMessageError}, IStart,         {ddcmp_Ignore}},
    {10, IStart,      {ddcmp_UserSendMessage,
                       ddcmp_TimerNotRunning},     IStart,         {ddcmp_StartTimer}},
    {11, IStart,      {ddcmp_LineDisconnected},    Halt,           {ddcmp_StopTimer}},
    {12, AStart,      {ddcmp_LineDisconnected},    Halt,           {ddcmp_StopTimer}},
    {13, AStart,      {ddcmp_ReceiveAck},          Run,            {ddcmp_StopTimer}},
    {14, AStart,      {ddcmp_ReceiveDataMsg},      Run,            {ddcmp_StopTimer}},
    {15, AStart,      {ddcmp_ReceiveStack},        Run,            {ddcmp_SetSACK, 
                                                                    ddcmp_StopTimer}},
    {16, AStart,      {ddcmp_TimerExpired},        AStart,         {ddcmp_SendStack, 
                                                                    ddcmp_StartTimer,
                                                                    ddcmp_CountTransmitError}},
    {17, AStart,      {ddcmp_ReceiveMaintMsg},     Maintenance,    {ddcmp_ResetVariables, 
                                                                    ddcmp_NotifyMaintRcvd}},
    {18, AStart,      {ddcmp_ReceiveMessageError}, AStart,         {ddcmp_Ignore}},
    {19, Run,         {ddcmp_LineDisconnected},    Halt,           {ddcmp_StopTimer,
                                                                    ddcmp_NotifyDisconnect,
                                                                    ddcmp_NotifyStartRcvd}},
    {20, Run,         {ddcmp_ReceiveStrt},         Halt,           {ddcmp_NotifyStartRcvd}},
    {21, Run,         {ddcmp_ReceiveMaintMsg},     Maintenance,    {ddcmp_ResetVariables, 
                                                                    ddcmp_NotifyMaintRcvd}},
    {22, Run,         {ddcmp_ReceiveStack},        Run,            {ddcmp_SetSACK}},
    {23, Run,         {ddcmp_ReceiveDataMsg,
                       ddcmp_NUMGtRplus1},         Run,            {ddcmp_NAKMissingPacket}},
    {24, Run,         {ddcmp_ReceiveDataMsg,
                       ddcmp_NUMEqRplus1},         Run,            {ddcmp_GiveBufferToUser}},
    {25, Run,         {ddcmp_ReceiveMessageError}, Run,            {ddcmp_SetSNAK,
                                                                    ddcmp_SetNAKReasonCRCError}},
    {26, Run,         {ddcmp_ReceiveRep,
                       ddcmp_NumEqR},              Run,            {ddcmp_SetSACK}},
    {27, Run,         {ddcmp_ReceiveRep,
                       ddcmp_NumNeR},              Run,            {ddcmp_SetSNAK, 
                                                                    ddcmp_SetNAKreason3}},
    {28, Run,         {ddcmp_ReceiveDataMsg,
                       ddcmp_ALtRESPleN},          Run,            {ddcmp_CompleteAckedTransmits, 
                                                                    ddcmp_SetAeqRESP, 
                                                                    ddcmp_IfTleAthenSetTeqAplus1, 
                                                                    ddcmp_IfAgeXthenStopTimer}},
    {29, Run,         {ddcmp_ReceiveAck,
                       ddcmp_ALtRESPleN},          Run,            {ddcmp_CompleteAckedTransmits, 
                                                                    ddcmp_SetAeqRESP, 
                                                                    ddcmp_IfTleAthenSetTeqAplus1, 
                                                                    ddcmp_IfAgeXthenStopTimer}},
    {30, Run,         {ddcmp_ReceiveAck,
                       ddcmp_RESPleAOrRESPgtN},    Run,            {ddcmp_Ignore}},
    {31, Run,         {ddcmp_ReceiveDataMsg,
                       ddcmp_RESPleAOrRESPgtN},    Run,            {ddcmp_Ignore}},
    {32, Run,         {ddcmp_ReceiveNak,
                       ddcmp_ALeRESPleN},          Run,            {ddcmp_CompleteAckedTransmits, 
                                                                    ddcmp_SetAeqRESP, 
                                                                    ddcmp_SetTequalAplus1, 
                                                                    ddcmp_StopTimer}},
    {33, Run,         {ddcmp_ReceiveNak,
                       ddcmp_RESPleAOrRESPgtN},    Run,            {ddcmp_Ignore}},
    {34, Run,         {ddcmp_TimerExpired},        Run,            {ddcmp_SetSREP,
                                                                    ddcmp_CountTransmitError}},
    {35, Run,         {ddcmp_TransmitterIdle,
                       ddcmp_SNAKisSet},           Run,            {ddcmp_SendNak, 
                                                                    ddcmp_ClearSNAK}},
    {36, Run,         {ddcmp_TransmitterIdle,
                       ddcmp_SNAKisClear,
                       ddcmp_SREPisSet},           Run,            {ddcmp_SendRep, 
                                                                    ddcmp_ClearSREP}},
    {37, Run,         {ddcmp_TransmitterIdle,
                       ddcmp_SNAKisClear,
                       ddcmp_SREPisClear,
                       ddcmp_TltNplus1},           Run,            {ddcmp_ReTransmitMessageT, 
                                                                    ddcmp_IncrementT, 
                                                                    ddcmp_ClearSACK}},
    {38, Run,         {ddcmp_UserSendMessage,
                       ddcmp_TeqNplus1,
                       ddcmp_TransmitterIdle,
                       ddcmp_SNAKisClear,
                       ddcmp_SREPisClear},         Run,            {ddcmp_SendDataMessage, 
                                                                    ddcmp_ClearSACK}},
    {39, Run,         {ddcmp_TransmitterIdle,
                       ddcmp_SNAKisClear,
                       ddcmp_SREPisClear,
                       ddcmp_SACKisSet,
                       ddcmp_TeqNplus1},           Run,            {ddcmp_SendAck, 
                                                                    ddcmp_ClearSACK}},
    {40, Run,         {ddcmp_DataMessageSent},     Run,            {ddcmp_SetXSetNUM, 
                                                                    ddcmp_IfAltXthenStartTimer, 
                                                                    ddcmp_IfAgeXthenStopTimer}},
    {41, Run,         {ddcmp_REPMessageSent},      Run,            {ddcmp_StartTimer}},
    {42, Maintenance, {ddcmp_ReceiveMaintMsg},     Maintenance,    {ddcmp_GiveBufferToUser}},
    {43, Maintenance, {ddcmp_UserSendMessage,
                       ddcmp_LineConnected,
                       ddcmp_TransmitterIdle},     Maintenance,    {ddcmp_SendMaintMessage}},
    {44, All}           /* End of Table */
    };

typedef struct _ddcmp_condition_name {
    DDCMP_Condition_Routine Condition;
    const char *Name;
    } DDCMP_CONDITION_NAME;

#define NAME(name) {ddcmp_##name,#name}
DDCMP_CONDITION_NAME ddcmp_Condition_Names[] = {
    NAME(UserHalt),
    NAME(UserStartup),
    NAME(UserMaintenanceMode),
    NAME(ReceiveStack),
    NAME(ReceiveStrt),
    NAME(TimerRunning),
    NAME(TimerNotRunning),
    NAME(TimerExpired),
    NAME(ReceiveMaintMessage),
    NAME(ReceiveAck),
    NAME(ReceiveNak),
    NAME(ReceiveRep),
    NAME(NUMEqRplus1),
    NAME(NUMGtRplus1),
    NAME(ReceiveDataMsg),
    NAME(ReceiveMaintMsg),
    NAME(ALtRESPleN),
    NAME(ALeRESPleN),
    NAME(RESPleAOrRESPgtN),
    NAME(TltNplus1),
    NAME(TeqNplus1),
    NAME(ReceiveMessageError),
    NAME(NumEqR),
    NAME(NumNeR),
    NAME(TransmitterIdle),
    NAME(TramsmitterBusy),
    NAME(SACKisSet),
    NAME(SACKisClear),
    NAME(SNAKisSet),
    NAME(SNAKisClear),
    NAME(SREPisSet),
    NAME(SREPisClear),
    NAME(UserSendMessage),
    NAME(LineConnected),
    NAME(LineDisconnected),
    NAME(DataMessageSent),
    NAME(REPMessageSent),
    {NULL, NULL}
    };

char *ddcmp_conditions(DDCMP_Condition_Routine *Conditions)
{
static char buf[512];
DDCMP_CONDITION_NAME *Name;

buf[0] = '\0';
while (*Conditions)
    {
    for (Name = ddcmp_Condition_Names; Name->Condition != NULL; ++Name) {
        if (Name->Condition == *Conditions)
            break;
        }
    ++Conditions;
    if (Name->Name)
        sprintf (&buf[strlen(buf)], "%s%s", Name->Name, *Conditions ? " && " : "");
    }
return buf;
}

typedef struct _ddcmp_action_name {
    DDCMP_LinkAction_Routine Action;
    const char *Name;
    } DDCMP_ACTION_NAME;

DDCMP_ACTION_NAME ddcmp_Actions[] = {
    NAME(StartTimer),
    NAME(StopTimer),
    NAME(ResetVariables),
    NAME(SendStrt),
    NAME(SendStack),
    NAME(SendAck),
    NAME(SendNak),
    NAME(SendRep),
    NAME(SetSACK),
    NAME(ClearSACK),
    NAME(SetSNAK),
    NAME(ClearSNAK),
    NAME(SetSREP),
    NAME(CountTransmitError),
    NAME(ClearSREP),
    NAME(IncrementR),
    NAME(SetAeqRESP),
    NAME(SetTequalAplus1),
    NAME(IncrementT),
    NAME(SetNAKreason3),
    NAME(SetNAKReasonCRCError),
    NAME(NAKMissingPacket),
    NAME(IfTleAthenSetTeqAplus1),
    NAME(IfAltXthenStartTimer),
    NAME(IfAgeXthenStopTimer),
    NAME(Ignore),
    NAME(GiveBufferToUser),
    NAME(CompleteAckedTransmits),
    NAME(ReTransmitMessageT),
    NAME(NotifyDisconnect),
    NAME(NotifyStartRcvd),
    NAME(NotifyMaintRcvd),
    NAME(SendDataMessage),
    NAME(SendMaintMessage),
    NAME(SetXSetNUM),
    {NULL, NULL}
    };

char *ddcmp_actions(DDCMP_LinkAction_Routine *Actions)
{
static char buf[512];
DDCMP_ACTION_NAME *Name;

buf[0] = '\0';
while (*Actions)
    {
    for (Name = ddcmp_Actions; Name->Action != NULL; ++Name) {
        if (Name->Action == *Actions)
            break;
        }
    ++Actions;
    if (Name->Name)
        sprintf (&buf[strlen(buf)], "%s%s", Name->Name, *Actions ? " + " : "");
    }
return buf;
}

char *ddcmp_link_state(DDCMP *link)
{
static char buf[512];

sprintf (buf, "(R:%d,N:%d,A:%d,T:%d,X:%d,SACK:%d,SNAK:%d,SREP:%d,NAKed:%d)", link->R, link->N, link->A, link->T, link->X, link->SACK, link->SNAK, link->SREP, link->NAKed);
return buf;
}

char *controller_queue_state(CTLR *controller)
{
static char buf[512];

sprintf (buf, "(ACKW:%d,XMT:%d,RCV:%d,CMPL:%d,FREE:%d) TOT:%d", 
                    (int)controller->ack_wait_queue->count, 
                    (int)controller->xmt_queue->count, 
                    (int)controller->rcv_queue->count, 
                    (int)controller->completion_queue->count, 
                    (int)controller->free_queue->count,
                    (int)controller->ack_wait_queue->count+
                    (int)controller->xmt_queue->count+
                    (int)controller->rcv_queue->count+ 
                    (int)controller->completion_queue->count+
                    (int)controller->free_queue->count);
return buf;
}

DDCMP_LinkState NewState;
DDCMP_LinkAction_Routine Actions[10];

#define ctlr up7                        /* Unit back pointer to controller */

void ddcmp_dispatch               (CTLR *controller, uint32 EventMask);


t_stat dmc_rd (int32* data, int32 PA, int32 access);
t_stat dmc_wr (int32  data, int32 PA, int32 access);
t_stat dmc_svc (UNIT * uptr);
t_stat dmc_poll_svc (UNIT * uptr);
t_stat dmc_timer_svc (UNIT * uptr);
t_stat dmc_reset (DEVICE * dptr);
t_stat dmc_attach (UNIT * uptr, CONST char * cptr);
t_stat dmc_detach (UNIT * uptr);
int32 dmc_ininta   (void);
int32 dmc_outinta (void);
t_stat dmc_setnumdevices (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat dmc_shownumdevices (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat dmc_setpeer (UNIT* uptr, int32 val, CONST char* cptr, void* desc);
t_stat dmc_showpeer (FILE* st, UNIT* uptr, int32 val, CONST void* desc);
t_stat dmc_setspeed (UNIT* uptr, int32 val, CONST char* cptr, void* desc);
t_stat dmc_showspeed (FILE* st, UNIT* uptr, int32 val, CONST void* desc);
t_stat dmc_setcorrupt (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat dmc_showcorrupt (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat dmc_set_microdiag (UNIT* uptr, int32 val, CONST char* cptr, void* desc);
t_stat dmc_show_microdiag (FILE* st, UNIT* uptr, int32 val, CONST void* desc);
t_stat dmc_settype (UNIT* uptr, int32 val, CONST char* cptr, void* desc);
t_stat dmc_showtype (FILE* st, UNIT* uptr, int32 val, CONST void* desc);
t_stat dmc_setstats (UNIT* uptr, int32 val, CONST char* cptr, void* desc);
t_stat dmc_showstats (FILE* st, UNIT* uptr, int32 val, CONST void* desc);
t_stat dmc_showqueues (FILE* st, UNIT* uptr, int32 val, CONST void* desc);
t_stat dmc_setconnectpoll (UNIT* uptr, int32 val, CONST char* cptr, void* desc);
t_stat dmc_showconnectpoll (FILE* st, UNIT* uptr, int32 val, CONST void* desc);
t_stat dmc_showddcmp (FILE* st, UNIT* uptr, int32 val, CONST void* desc);
t_stat dmc_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
t_stat dmc_help_attach (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *dmc_description (DEVICE *dptr);
const char *dmp_description (DEVICE *dptr);
int dmc_is_attached (UNIT* uptr);
int dmc_is_dmc (CTLR *controller);
int dmc_is_rqi_set (CTLR *controller);
int dmc_is_rdyi_set (CTLR *controller);
int dmc_is_iei_set (CTLR *controller);
int dmc_is_ieo_set (CTLR *controller);
uint32 dmc_get_addr (CTLR *controller);
void dmc_set_addr (CTLR *controller, uint32 addr);
uint16 dmc_get_count (CTLR *controller);
void dmc_set_count (CTLR *controller, uint16 count);
uint8 dmc_get_modem (CTLR *controller);
void dmc_set_modem_dtr (CTLR *controller);
void dmc_clr_modem_dtr (CTLR *controller);
void dmc_process_immediate(CTLR *controller);
void dmc_process_command (CTLR *controller);
t_bool dmc_buffer_fill_receive_buffers (CTLR *controller);
void dmc_start_transfer_buffer (CTLR *controller);
void dmc_buffer_queue_init (CTLR *controller, BUFFER_QUEUE *q, const char *name, size_t size, BUFFER *buffers);
void dmc_buffer_queue_init_all (CTLR *controller);
BUFFER *dmc_buffer_queue_head (BUFFER_QUEUE *q);
BUFFER *dmc_buffer_allocate (CTLR *controller);
t_bool dmc_transmit_queue_empty  (CTLR *controller);
void dmc_ddcmp_start_transmitter (CTLR *controller);
void dmc_queue_control_out (CTLR *controller, uint16 sel6);
/* DMP/DMV host interface */
void dmp_reset_host_state (CTLR *controller);
void dmp_process_command (CTLR *controller);
void dmp_queue_response (CTLR *controller, uint16 sel2, uint16 sel4, uint16 sel6, uint16 sel10);
void dmp_ddcmp_event (CTLR *controller, uint16 dmc_sel6);
void dmp_link_running (CTLR *controller);
uint8 dmp_buffer_station (CTLR *controller, BUFFER *buffer);
void dmp_count_event (CTLR *controller, int word, t_bool low_byte);
t_bool dmp_multipoint (CTLR *controller);
int dmp_trib_limit (CTLR *controller);
t_bool dmp_hub_mp (CTLR *controller);
TMLN *dmp_own_line (CTLR *controller);
void dmp_session_load (CTLR *controller, int slot);
void dmp_session_store (CTLR *controller);
void dmp_dispatch (CTLR *controller, uint32 events);
t_bool dmp_mp_receive (CTLR *controller);
void dmp_mp_transmit (CTLR *controller);
void dmp_mp_timers (CTLR *controller, int32 *active);
int32 dmp_hub_poll (CTLR *controller);

/* debugging bitmaps */
#define DBG_TRC  0x0001                                 /* trace routine calls */
#define DBG_REG  0x0002                                 /* trace programatic read/write registers */
#define DBG_RGC  0x0004                                 /* internal read/write registers changes */
#define DBG_WRN  0x0008                                 /* display warnings */
#define DBG_INF  0x0010                                 /* display informational messages (high level trace) */
#define DBG_DTS  (DDCMP_DBG_PXMT|DDCMP_DBG_PRCV)        /* display data summary */
#define DBG_DAT  (DBG_DTS|DDCMP_DBG_PDAT)               /* display data buffer contents */
#define DBG_MDM  0x0040                                 /* modem related transitions */
#define DBG_CON  TMXR_DBG_CON                           /* display socket open/close, connection establishment */
#define DBG_INT  0x0080                                 /* display interrupt activites */


DEBTAB dmc_debug[] = {
    {"TRACE",   DBG_TRC, "trace routine calls"},
    {"WARN",    DBG_WRN, "warnings"},
    {"REG",     DBG_REG, "read/write registers"},
    {"INTREG",  DBG_RGC, "internal read/write registers changes"},
    {"INFO",    DBG_INF, "informational messages (high level trace)"},
    {"DATA",    DBG_DAT, "data buffer contents"},
    {"DATASUM", DBG_DTS, "data summary"},
    {"MODEM",   DBG_MDM, "modem related transitions"},
    {"CONNECT", DBG_CON, "connection establishment"},
    {"INT",     DBG_INT, "interrupt activites"},
    {0}
    };

UNIT dmc_units[DMC_NUMDEVICE+2];            /* One per device plus an I/O polling unit and a timing unit */

UNIT dmc_unit_template = { UDATA (&dmc_svc, UNIT_ATTABLE|UNIT_IDLE, 0) };
UNIT dmc_poll_unit_template = { UDATA (&dmc_poll_svc, UNIT_DIS|UNIT_IDLE, 0) };
UNIT dmc_timer_unit_template = { UDATA (&dmc_timer_svc, UNIT_DIS|UNIT_IDLE, 0) };

UNIT dmp_units[DMP_NUMDEVICE+2];            /* One per device plus an I/O polling unit and a timing unit */

CSRS dmc_csrs[DMC_NUMDEVICE];
uint16 dmc_sel0[DMC_NUMDEVICE];
uint16 dmc_sel2[DMC_NUMDEVICE];
uint16 dmc_sel4[DMC_NUMDEVICE];
uint16 dmc_sel6[DMC_NUMDEVICE];

#define PEERSIZE 512
uint32 dmc_speed[DMC_NUMDEVICE];
char dmc_peer[DMC_NUMDEVICE][PEERSIZE];
char dmc_port[DMC_NUMDEVICE][PEERSIZE];
uint32 dmc_baseaddr[DMC_NUMDEVICE];
uint16 dmc_basesize[DMC_NUMDEVICE];
uint8 dmc_modem[DMC_NUMDEVICE];
t_bool dmc_microdiag[DMC_NUMDEVICE];
int32 dmc_corruption[DMC_NUMDEVICE];

CSRS dmp_csrs[DMP_NUMDEVICE];
uint16 dmp_sel0[DMC_NUMDEVICE];
uint16 dmp_sel2[DMC_NUMDEVICE];
uint16 dmp_sel4[DMC_NUMDEVICE];
uint16 dmp_sel6[DMC_NUMDEVICE];
uint16 dmp_sel10[DMC_NUMDEVICE];

uint32 dmp_speed[DMP_NUMDEVICE];
char dmp_peer[DMP_NUMDEVICE][PEERSIZE];
char dmp_port[DMP_NUMDEVICE][PEERSIZE];
uint32 dmp_baseaddr[DMP_NUMDEVICE];
uint16 dmp_basesize[DMP_NUMDEVICE];
uint8 dmp_modem[DMP_NUMDEVICE];
int32 dmp_corruption[DMC_NUMDEVICE];

TMLN dmc_ldsc[DMC_NUMDEVICE];               /* line descriptors */
TMXR dmc_desc = { 1, NULL, 0, dmc_ldsc };   /* mux descriptor */
uint32 dmc_connect_poll = DMC_CONNECT_POLL; /* seconds between polls when no connection */

uint32 dmc_ini_summary = 0;         /* In Command Interrupt Summary for all controllers */
uint32 dmc_outi_summary = 0;        /* Out Command Interrupt Summary for all controllers */

BUFFER_QUEUE dmc_rcv_queues[DMC_NUMDEVICE];
BUFFER_QUEUE dmc_completion_queues[DMC_NUMDEVICE];
BUFFER_QUEUE dmc_xmt_queues[DMC_NUMDEVICE];
BUFFER_QUEUE dmc_ack_wait_queues[DMC_NUMDEVICE];
BUFFER_QUEUE dmc_free_queues[DMC_NUMDEVICE];
BUFFER *dmc_buffers[DMC_NUMDEVICE];

TMLN dmp_ldsc[DMC_NUMDEVICE];               /* line descriptors */
TMXR dmp_desc = { 1, NULL, 0, dmp_ldsc };   /* mux descriptor */
uint32 dmp_connect_poll = DMC_CONNECT_POLL;/* seconds between polls when no connection */

/* Listening hub: a DMP11/DMV11 line attached without a PEER listens on its
   port for as many stations as the controller has tributary status slots
   (DMP11 32, DMV11 12), each on its own line ("drop line") of a private
   multiplexer.  A multipoint control station uses every drop line; any
   other station uses the one connection it gets.  The multiplexer has its
   own (device-less) unit so that its polling cannot interfere with the
   device's poll unit.  Indexed by the DMP/DMV line number. */
TMLN dmp_hub_ldsc[DMP_NUMDEVICE][DMP_MAX_TRIBS];
TMXR dmp_hub_desc[DMP_NUMDEVICE];
UNIT dmp_hub_unit[DMP_NUMDEVICE];

BUFFER_QUEUE dmp_rcv_queues[DMP_NUMDEVICE];
BUFFER_QUEUE dmp_completion_queues[DMP_NUMDEVICE];
BUFFER_QUEUE dmp_xmt_queues[DMP_NUMDEVICE];
BUFFER_QUEUE dmp_ack_wait_queues[DMP_NUMDEVICE];
BUFFER_QUEUE dmp_free_queues[DMP_NUMDEVICE];
BUFFER *dmp_buffers[DMC_NUMDEVICE];

REG dmc_reg[] = {
    { GRDATAD (RXINT,      dmc_ini_summary, DEV_RDX, 32, 0,                     "input interrupt summary") },
    { GRDATAD (TXINT,     dmc_outi_summary, DEV_RDX, 32, 0,                     "output interrupt summary") },
    { GRDATAD (POLL,      dmc_connect_poll, DEV_RDX, 32, 0,                     "connect poll interval") },
    { BRDATAD (SEL0,              dmc_sel0, DEV_RDX, 16, DMC_NUMDEVICE,         "Select 0 CSR") },
    { BRDATAD (SEL2,              dmc_sel2, DEV_RDX, 16, DMC_NUMDEVICE,         "Select 2 CSR") },
    { BRDATAD (SEL4,              dmc_sel4, DEV_RDX, 16, DMC_NUMDEVICE,         "Select 4 CSR") },
    { BRDATAD (SEL6,              dmc_sel6, DEV_RDX, 16, DMC_NUMDEVICE,         "Select 6 CSR") },
    { BRDATAD (SPEED,            dmc_speed, DEV_RDX, 32, DMC_NUMDEVICE,         "line speed") },
    { BRDATAD (CORRUPT,     dmc_corruption, DEV_RDX, 32, DMC_NUMDEVICE,         "data corruption factor (0.1%)") },
    { BRDATAD (DIAG,         dmc_microdiag, DEV_RDX,  1, DMC_NUMDEVICE,         "Microdiagnostic Enabled") },
    { CRDATAD (PEER,             dmc_peer,  DEV_RDX,  8, DMC_NUMDEVICE*PEERSIZE, "peer address:port") },
    { CRDATAD (PORT,             dmc_port,  DEV_RDX,  8, DMC_NUMDEVICE*PEERSIZE, "listen port") },
    { BRDATAD (BASEADDR,      dmc_baseaddr, DEV_RDX, 32, DMC_NUMDEVICE,         "program set base address") },
    { BRDATAD (BASESIZE,      dmc_basesize, DEV_RDX, 16, DMC_NUMDEVICE,         "program set base size") },
    { BRDATAD (MODEM,            dmc_modem, DEV_RDX,  8, DMC_NUMDEVICE,         "modem control bits") },
    { NULL }  };

REG dmp_reg[] = {
    { GRDATAD (RXINT,      dmc_ini_summary, DEV_RDX, 32, 0,                     "input interrupt summary") },
    { GRDATAD (TXINT,     dmc_outi_summary, DEV_RDX, 32, 0,                     "output interrupt summary") },
    { GRDATAD (POLL,      dmp_connect_poll, DEV_RDX, 32, 0,                     "connect poll interval") },
    { BRDATAD (SEL0,              dmp_sel0, DEV_RDX, 16, DMC_NUMDEVICE,         "Select 0 CSR") },
    { BRDATAD (SEL2,              dmp_sel2, DEV_RDX, 16, DMC_NUMDEVICE,         "Select 2 CSR") },
    { BRDATAD (SEL4,              dmp_sel4, DEV_RDX, 16, DMC_NUMDEVICE,         "Select 4 CSR") },
    { BRDATAD (SEL6,              dmp_sel6, DEV_RDX, 16, DMC_NUMDEVICE,         "Select 6 CSR") },
    { BRDATAD (SEL10,            dmp_sel10, DEV_RDX, 16, DMP_NUMDEVICE,         "Select 10 CSR") },
    { BRDATAD (SPEED,            dmp_speed, DEV_RDX, 32, DMC_NUMDEVICE,         "line speed") },
    { BRDATAD (CORRUPT,     dmp_corruption, DEV_RDX, 32, DMC_NUMDEVICE,         "data corruption factor (0.1%)") },
    { CRDATAD (PEER,              dmp_peer, DEV_RDX,  8, DMC_NUMDEVICE*PEERSIZE, "peer address:port") },
    { CRDATAD (PORT,              dmp_port, DEV_RDX,  8, DMC_NUMDEVICE*PEERSIZE, "listen port") },
    { BRDATAD (BASEADDR,      dmp_baseaddr, DEV_RDX, 32, DMC_NUMDEVICE,         "program set base address") },
    { BRDATAD (BASESIZE,      dmp_basesize, DEV_RDX, 16, DMC_NUMDEVICE,         "program set base size") },
    { BRDATAD (MODEM,            dmp_modem, DEV_RDX,  8, DMP_NUMDEVICE,         "modem control bits") },
    { NULL }  };

extern DEVICE dmc_dev;

MTAB dmc_mod[] = {
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "LINES", "LINES=n",
        &dmc_setnumdevices, &dmc_shownumdevices, (void *) &dmc_dev, "Display number of devices" },
    { MTAB_XTD|MTAB_VUN,          0, "PEER", "PEER=address:port",
        &dmc_setpeer, &dmc_showpeer, NULL, "Display destination/source" },
    { MTAB_XTD|MTAB_VUN,          0, "SPEED", "SPEED=bits/sec (0=unrestricted)" ,
        &dmc_setspeed, &dmc_showspeed, NULL, "Display rate limit" },
    { MTAB_XTD|MTAB_VUN,          0, "MICRODIAG", "MICRODIAG={ENABLE,DISABLE}" ,
        &dmc_set_microdiag, &dmc_show_microdiag, NULL, "MicroDiagnostic Enable" },
#if !defined (VM_PDP10)
    { MTAB_XTD|MTAB_VUN|MTAB_VALR,0, "TYPE", "TYPE={DMR,DMC}" ,&dmc_settype, &dmc_showtype, NULL, "Set/Display device type"  },
#endif
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "STATS", "STATS",
        &dmc_setstats, &dmc_showstats, NULL, "Display/Clear statistics" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "QUEUES", "QUEUES",
        NULL, &dmc_showqueues, NULL, "Display Queue state" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "DDCMP", "DDCMP",
        NULL, &dmc_showddcmp, NULL, "Display DDCMP state information" },
    { MTAB_XTD|MTAB_VDV,          0, "CONNECTPOLL", "CONNECTPOLL=seconds",
        &dmc_setconnectpoll, &dmc_showconnectpoll, (void *) &dmc_connect_poll, "Display connection poll interval" },
    { MTAB_XTD|MTAB_VUN|MTAB_VALR|MTAB_NMO, 0, "CORRUPT", "CORRUPTION=factor (0=uncorrupted)" ,
      &dmc_setcorrupt, &dmc_showcorrupt, NULL, "Display corruption factor (0.1% of packets)" },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR,        020, "ADDRESS", "ADDRESS",
        &set_addr, &show_addr, NULL, "Bus address" },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR,          1, "VECTOR", "VECTOR",
        &set_vec,  &show_vec,  NULL, "Interrupt vector" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "SYNC", NULL,
      NULL, &tmxr_show_sync, NULL, "Display attachable DDCMP synchronous links" },
    { 0 } };

extern DEVICE dmp_dev;

MTAB dmp_mod[] = {
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "LINES", "LINES=n",
        &dmc_setnumdevices, &dmc_shownumdevices, (void *) &dmp_dev, "Display number of devices" },
    { MTAB_XTD|MTAB_VUN,          0, "PEER", "PEER=address:port",
        &dmc_setpeer, &dmc_showpeer, NULL, "Display destination/source" },
    { MTAB_XTD|MTAB_VUN,          0, "SPEED", "SPEED=bits/sec (0=unrestricted)" ,
        &dmc_setspeed, &dmc_showspeed, NULL, "Display rate limit" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "STATS", "STATS",
        &dmc_setstats, &dmc_showstats, NULL, "Display/Clear statistics" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "QUEUES", "QUEUES",
        NULL, &dmc_showqueues, NULL, "Display Queue state" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "DDCMP", "DDCMP",
        NULL, &dmc_showddcmp, NULL, "Display DDCMP state information" },
    { MTAB_XTD|MTAB_VDV,          0, "CONNECTPOLL", "CONNECTPOLL=seconds",
        &dmc_setconnectpoll, &dmc_showconnectpoll, (void *) &dmp_connect_poll, "Display connection poll interval" },
    { MTAB_XTD|MTAB_VUN|MTAB_VALR|MTAB_NMO, 0, "CORRUPTION", "CORRUPTION=factor (0=uncorrupted)" ,
      &dmc_setcorrupt, &dmc_showcorrupt, NULL, "Display corruption factor (0.1% of packets)" },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR,        020, "ADDRESS", "ADDRESS",
        &set_addr, &show_addr, NULL, "Bus address" },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR,          1, "VECTOR", "VECTOR",
        &set_vec,  &show_vec,  NULL, "Interrupt vector" },
    { 0 } };

extern DEVICE dmv_dev;

MTAB dmv_mod[] = {
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "LINES", "LINES=n",
        &dmc_setnumdevices, &dmc_shownumdevices, (void *) &dmv_dev, "Display number of devices" },
    { MTAB_XTD|MTAB_VUN,          0, "PEER", "PEER=address:port",
        &dmc_setpeer, &dmc_showpeer, NULL, "Display destination/source" },
    { MTAB_XTD|MTAB_VUN,          0, "SPEED", "SPEED=bits/sec (0=unrestricted)" ,
        &dmc_setspeed, &dmc_showspeed, NULL, "Display rate limit" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "STATS", "STATS",
        &dmc_setstats, &dmc_showstats, NULL, "Display/Clear statistics" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "QUEUES", "QUEUES",
        NULL, &dmc_showqueues, NULL, "Display Queue state" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "DDCMP", "DDCMP",
        NULL, &dmc_showddcmp, NULL, "Display DDCMP state information" },
    { MTAB_XTD|MTAB_VDV,          0, "CONNECTPOLL", "CONNECTPOLL=seconds",
        &dmc_setconnectpoll, &dmc_showconnectpoll, (void *) &dmp_connect_poll, "Display connection poll interval" },
    /* as on the DMP: the corruption troll for this line (shares dmp_corruption,
       since the DMP and DMV devices share their lines) */
    { MTAB_XTD|MTAB_VUN|MTAB_VALR|MTAB_NMO, 0, "CORRUPTION", "CORRUPTION=factor (0=uncorrupted)" ,
      &dmc_setcorrupt, &dmc_showcorrupt, NULL, "Display corruption factor (0.1% of packets)" },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR,        020, "ADDRESS", "ADDRESS",
        &set_addr, &show_addr, NULL, "Bus address" },
    { MTAB_XTD|MTAB_VDV|MTAB_VALR,          1, "VECTOR", "VECTOR",
        &set_vec,  &show_vec,  NULL, "Interrupt vector" },
    { 0 } };

#define IOLN_DMC        010

DIB dmc_dib = { IOBA_AUTO, IOLN_DMC, &dmc_rd, &dmc_wr, 2, IVCL (DMCRX), VEC_AUTO, {&dmc_ininta, &dmc_outinta}, IOLN_DMC };

#define IOLN_DMP        010

DIB dmp_dib = { IOBA_AUTO, IOLN_DMP, &dmc_rd, &dmc_wr, 2, IVCL (DMCRX), VEC_AUTO, {&dmc_ininta, &dmc_outinta }, IOLN_DMP};

#define IOLN_DMV        020             /* DMV11: BSEL0-BSEL17, shares dmp_dib with the DMP */

DEVICE dmc_dev =
    { 
#if defined (VM_PDP10)
    "DMR",
#else
    "DMC", 
#endif
    dmc_units, dmc_reg, dmc_mod, 3, DMC_RDX, 8, 1, DMC_RDX, 8,
    NULL, NULL, &dmc_reset, NULL, &dmc_attach, &dmc_detach,
    &dmc_dib, DEV_DISABLE | DEV_DIS | DEV_UBUS | DEV_DEBUG | DEV_DONTAUTO, 0, dmc_debug,
    NULL, NULL, &dmc_help, &dmc_help_attach, NULL, &dmc_description };

/*
   We have two devices defined here (dmp_dev and dmv_dev) which have the 
   same units.  This would normally never be allowed since two devices can't
   actually share units.  This problem is avoided in this case since both 
   devices start out as disabled and the logic in dmc_reset allows only 
   one of these devices to be enabled at a time.  The DMP device is allowed 
   on Unibus systems and the DMV device Qbus systems.
   This monkey business is necessary due to the fact that although both
   the DMP and DMV devices have almost the same functionality and almost
   the same register programming interface, they are different enough that
   they fall in different priorities in the autoconfigure address and vector
   rules.
 */
DEVICE dmp_dev =
    { "DMP", dmp_units, dmp_reg, dmp_mod, 3, DMC_RDX, 8, 1, DMC_RDX, 8,
    NULL, NULL, &dmc_reset, NULL, &dmc_attach, &dmc_detach,
    &dmp_dib, DEV_DISABLE | DEV_DIS | DEV_UBUS | DEV_DEBUG | DEV_DONTAUTO, 0, dmc_debug,
    NULL, NULL, &dmc_help, &dmc_help_attach, NULL, &dmp_description };

DEVICE dmv_dev =
    { "DMV", dmp_units, dmp_reg, dmv_mod, 3, DMC_RDX, 8, 1, DMC_RDX, 8,
    NULL, NULL, &dmc_reset, NULL, &dmc_attach, &dmc_detach,
    &dmp_dib, DEV_DISABLE | DEV_DIS | DEV_QBUS | DEV_DEBUG | DEV_DONTAUTO, 0, dmc_debug,
    NULL, NULL, &dmc_help, &dmc_help_attach, NULL, &dmp_description };

CTLR dmc_ctrls[DMC_NUMDEVICE + DMP_NUMDEVICE];

int dmc_is_attached(UNIT* uptr)
{
return uptr->flags & UNIT_ATT;
}

int dmc_is_dmc(CTLR *controller)
{
return controller->dev_type != DMP;
}

CTLR *dmc_get_controller_from_unit(UNIT *unit)
{
return (CTLR *)unit->ctlr;
}

CTLR* dmc_get_controller_from_address(uint32 address)
{
int i;

for (i=0; i<DMC_NUMDEVICE + DMP_NUMDEVICE; i++) {
    DIB *dib = (DIB *)dmc_ctrls[i].device->ctxt;
    /* i is the first controller of the matching device (DMC controllers
       come first, then DMP/DMV); each line has 8 (Unibus) or 16 (Qbus)
       bytes of registers. */
    if ((address >= dib->ba) && (address < (dib->ba + dib->lnt)))
        return &dmc_ctrls[i + ((address - dib->ba) >> ((UNIBUS) ? 3 : 4))];
    }
/* not found */
return 0;
}

t_stat dmc_showpeer (FILE* st, UNIT* uptr, int32 val, CONST void* desc)
{
DEVICE *dptr = (UNIBUS) ? ((&dmc_dev == find_dev_from_unit(uptr)) ? &dmc_dev : &dmp_dev) : &dmv_dev;
int32 dmc = (int32)(uptr-dptr->units);
char *peer = ((dptr == &dmc_dev)? &dmc_peer[dmc][0] : &dmp_peer[dmc][0]);

if (peer[0])
    fprintf(st, "peer=%s", peer);
else
    fprintf(st, "peer=unspecified");
return SCPE_OK;
}

t_stat dmc_setpeer (UNIT* uptr, int32 val, CONST char* cptr, void* desc)
{
DEVICE *dptr = (UNIBUS) ? ((&dmc_dev == find_dev_from_unit(uptr)) ? &dmc_dev : &dmp_dev) : &dmv_dev;
int32 dmc = (int32)(uptr-dptr->units);
char *peer = ((dptr == &dmc_dev)? &dmc_peer[dmc][0] : &dmp_peer[dmc][0]);
char host[PEERSIZE], port[PEERSIZE];

if ((!cptr) || (!*cptr))
    return SCPE_ARG;
if (dmc_is_attached(uptr))
    return SCPE_ALATT;
if (sim_parse_addr (cptr, host, sizeof(host), NULL, port, sizeof(port), NULL, NULL))
    return SCPE_ARG;
if (host[0] == '\0')
    return SCPE_ARG;
strncpy(peer, cptr, PEERSIZE-1);
return SCPE_OK;
}

t_stat dmc_showspeed (FILE* st, UNIT* uptr, int32 val, CONST void* desc)
{
DEVICE *dptr = (UNIBUS) ? ((&dmc_dev == find_dev_from_unit(uptr)) ? &dmc_dev : &dmp_dev) : &dmv_dev;
int32 dmc = (int32)(uptr-dptr->units);
uint32 *speeds = ((dptr == &dmc_dev)? dmc_speed : dmp_speed);

if (speeds[dmc] > 0)
    fprintf(st, "speed=%d bits/sec", speeds[dmc]);
else
    fprintf(st, "speed=0 (unrestricted)");
return SCPE_OK;
}


t_stat dmc_setspeed (UNIT* uptr, int32 val, CONST char* cptr, void* desc)
{
DEVICE *dptr = (UNIBUS) ? ((&dmc_dev == find_dev_from_unit(uptr)) ? &dmc_dev : &dmp_dev) : &dmv_dev;
int32 dmc = (int32)(uptr-dptr->units);
uint32 *speeds = ((dptr == &dmc_dev)? dmc_speed : dmp_speed);
t_stat r;
int32 newspeed;

if (cptr == NULL)
    return SCPE_ARG;
newspeed = (int32) get_uint (cptr, 10, 100000000, &r);
if (r != SCPE_OK)
    return r;
speeds[dmc] = newspeed;
return SCPE_OK;
}

t_stat dmc_show_microdiag (FILE* st, UNIT* uptr, int32 val, CONST void* desc)
{
int32 dmc = (int32)(uptr-dmc_dev.units);

fprintf(st, "MicroDiag=%s", dmc_microdiag[dmc] ? "enabled" : "disabled");
return SCPE_OK;
}

/* Manage the corruption troll's appetite, in units of milli-gulps.
 *
 * See ddcmp_feedCorruptionTroll for usage.
 */
t_stat dmc_setcorrupt (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
DEVICE *dptr = (UNIBUS) ? ((&dmc_dev == find_dev_from_unit(uptr)) ? &dmc_dev : &dmp_dev) : &dmv_dev;
int32 dmc = (int32)(uptr-dptr->units);
int32 *hunger = (dptr == &dmc_dev) ? &dmc_corruption[dmc] : &dmp_corruption[dmc];
t_stat r;
int32 appetite;

if ((cptr == NULL) || (*cptr == '\0'))
    return SCPE_ARG;

appetite = (int32) get_uint (cptr, 10, 999, &r);
if (r != SCPE_OK)
    return r;

*hunger = appetite;

return SCPE_OK;
}

/* Display the corruption troll's appetite */

t_stat dmc_showcorrupt (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr = (UNIBUS) ? ((&dmc_dev == find_dev_from_unit(uptr)) ? &dmc_dev : &dmp_dev) : &dmv_dev;
int32 dmc = (int32)(uptr-dptr->units);
int32 *hunger = (dptr == &dmc_dev) ? &dmc_corruption[dmc] : &dmp_corruption[dmc];

if (*hunger)
    fprintf(st, "Corruption=%d milligulps (%.1f%% of messages processed)", 
             *hunger, ((double)*hunger)/10.0);
else
    fprintf(st, "No Corruption");

return SCPE_OK;
}

t_stat dmc_set_microdiag(UNIT* uptr, int32 val, CONST char* cptr, void* desc)
{
int32 dmc = (int32)(uptr-dmc_dev.units);
char gbuf[CBUFSIZE];

if ((cptr == NULL) || (*cptr == '\0'))
    return SCPE_ARG;
get_glyph (cptr, gbuf, 0);
if (MATCH_CMD (gbuf, "ENABLE") == 0)
    dmc_microdiag[dmc] = TRUE;
else
    if (MATCH_CMD (gbuf, "DISABLE") == 0)
        dmc_microdiag[dmc] = FALSE;
    else
        return SCPE_ARG;
return SCPE_OK;
}

t_stat dmc_showtype (FILE* st, UNIT* uptr, int32 val, CONST void* desc)
{
CTLR *controller = dmc_get_controller_from_unit(uptr);

switch (controller->dev_type) {
    case DMC:
        fprintf(st, "type=DMC");
        break;
    case DMR:
        fprintf(st, "type=DMR");
        break;
    case DMP:
        fprintf(st, "type=%s", (UNIBUS) ? "DMP" : "DMV");
        break;
    default:
        fprintf(st, "type=???");
        break;
    }
return SCPE_OK;
}

t_stat dmc_settype (UNIT* uptr, int32 val, CONST char* cptr, void* desc)
{
char gbuf[80];
t_stat status = SCPE_OK;
CTLR *controller = dmc_get_controller_from_unit(uptr);

if ((!cptr) || (!*cptr))
    return SCPE_2FARG;
if (dmc_is_attached(uptr))
    return SCPE_ALATT;
cptr = get_glyph (cptr, gbuf, 0);
if (strcmp (gbuf,"DMC") == 0)
    controller->dev_type = DMC;
else 
    if (strcmp (gbuf,"DMR") == 0)
        controller->dev_type = DMR;
    else
        status = SCPE_ARG;

return status;
}

t_stat dmc_showstats (FILE* st, UNIT* uptr, int32 val, CONST void* desc)
{
CTLR *controller = dmc_get_controller_from_unit(uptr);
    
fprintf (st, "%s%d\n", controller->device->name, (int)(uptr-controller->device->units));

tmxr_fstats (st, controller->line, -1);

fprintf(st, "Buffers received from the network=%d\n", controller->buffers_received_from_net);
fprintf(st, "Buffers sent to the network=%d\n", controller->buffers_transmitted_to_net);
fprintf(st, "Output transfers completed for receive buffers=%d\n", controller->receive_buffer_output_transfers_completed);
fprintf(st, "Output transfers completed for transmit buffers=%d\n", controller->transmit_buffer_output_transfers_completed);
fprintf(st, "Input transfers completed for receive buffers=%d\n", controller->receive_buffer_input_transfers_completed);
fprintf(st, "Input transfers completed for transmit buffers=%d\n", controller->transmit_buffer_input_transfers_completed);
fprintf(st, "Control Out operations processed=%d\n", controller->control_out_operations_completed);
fprintf(st, "DDCMP control packets received=%d\n", controller->ddcmp_control_packets_received);
fprintf(st, "DDCMP control packets sent=%d\n", controller->ddcmp_control_packets_sent);

return SCPE_OK;
}

void dmc_showqueue (FILE* st, BUFFER_QUEUE *queue, t_bool detail)
{
size_t i;

fprintf (st, "%s Queue:\n", queue->name);
fprintf (st, "    Size: %d\n", (int)queue->size);
fprintf (st, "   Count: %d\n", (int)queue->count);
if (detail) {
    BUFFER *buffer = (BUFFER *)queue->hdr.next;
    size_t same;

    for (i=same=0; i<queue->count; ++i) {
        if ((i == 0) || 
            (0 != memcmp(((char *)buffer)+sizeof(QH), ((char *)(buffer->hdr.prev))+sizeof(QH), sizeof(*buffer)-sizeof(QH)))) {
            if (same > 0)
                fprintf (st, "%s.queue[%d] thru %s.queue[%d] same as above\n", queue->name, (int)(i-same), queue->name, (int)(i-1));
            fprintf (st, "%s.queue[%d] at %p\n", queue->name, (int)i, buffer);
            fprintf (st, "   address:                  0x%08X\n", buffer->address);
            fprintf (st, "   count:                    0x%04X\n", buffer->count);
            fprintf (st, "   actual_bytes_transferred: 0x%04X\n", buffer->actual_bytes_transferred);
            if (buffer->buffer_return_time)
                fprintf (st, "   buffer_return_time:       0x%08X\n", buffer->buffer_return_time);
            if (strcmp(queue->name, "transmit") == 0) {
                uint8 *msg = buffer->transfer_buffer;
                static const char *const flags [4] = { "..", ".Q", "S.", "SQ" };
                static const char *const nak[18] = { "", " (HCRC)", " (DCRC)", " (REPREPLY)", /* 0-3 */
                                                     "", "", "", "",                          /* 4-7 */
                                                     " (NOBUF)", " (RXOVR)", "", "",          /* 8-11 */
                                                     "", "", "", "",                          /* 12-15 */
                                                     " (TOOLONG)", " (HDRFMT)" };             /* 16-17 */
                const char *flag = flags[msg[2]>>6];
                int msg2 = msg[2] & 0x3F;

                switch (msg[0]) {
                    case DDCMP_SOH:   /* Data Message */
                        fprintf (st, "Data Message, Count: %d, Num: %d, Flags: %s, Resp: %d, HDRCRC: %s, DATACRC: %s\n", (msg2 << 8)|msg[1], msg[4], flag, msg[3], 
                                                    (0 == ddcmp_crc16 (0, msg, 8)) ? "OK" : "BAD", (0 == ddcmp_crc16 (0, msg+8, 2+((msg2 << 8)|msg[1]))) ? "OK" : "BAD");
                        break;
                    case DDCMP_ENQ:   /* Control Message */
                        fprintf (st, "Control: Type: %d ", msg[1]);
                        switch (msg[1]) {
                            case DDCMP_CTL_ACK: /* ACK */
                                fprintf (st, "(ACK) ACKSUB: %d, Flags: %s, Resp: %d\n", msg2, flag, msg[3]);
                                break;
                            case DDCMP_CTL_NAK: /* NAK */
                                fprintf (st, "(NAK) Reason: %d%s, Flags: %s, Resp: %d\n", msg2, ((msg2 > 17)? "": nak[msg2]), flag, msg[3]);
                                break;
                            case DDCMP_CTL_REP: /* REP */
                                fprintf (st, "(REP) REPSUB: %d, Num: %d, Flags: %s\n", msg2, msg[4], flag);
                                break;
                            case DDCMP_CTL_STRT: /* STRT */
                                fprintf (st, "(STRT) STRTSUB: %d, Flags: %s\n", msg2, flag);
                                break;
                            case DDCMP_CTL_STACK: /* STACK */
                                fprintf (st, "(STACK) STCKSUB: %d, Flags: %s\n", msg2, flag);
                                break;
                            default: /* Unknown */
                                fprintf (st, "(Unknown=0%o)\n", msg[1]);
                                break;
                            }
                        if (buffer->count != DDCMP_HEADER_SIZE)
                            fprintf (st, "Unexpected Control Message Length: %d expected %d\n", buffer->count, DDCMP_HEADER_SIZE);
                    }
                }
            same = 0;
            }
        else
            ++same;
        buffer = (BUFFER *)buffer->hdr.next;
        }
    if (same > 0)
        fprintf (st, "%s.queue[%d] thru %s.queue[%d] same as above\n", queue->name, (int)(i-same), queue->name, (int)(i-1));
    }
}

t_stat dmc_showqueues (FILE* st, UNIT* uptr, int32 val, CONST void* desc)
{
CTLR *controller     = dmc_get_controller_from_unit(uptr);
static const char *states[] = {"Uninitialised", "Initialised", "Running", "Halted"};
static const char *tstates[] = {"Idle", "InputTransfer", "OutputTransfer", "OutputControl"};

dmc_showstats (st, uptr, val, desc);
fprintf (st, "State: %s\n", states[controller->state]);
fprintf (st, "TransferState: %s\n", tstates[controller->transfer_state]);
dmc_showqueue (st, controller->completion_queue, TRUE);
dmc_showqueue (st, controller->xmt_queue, TRUE);
dmc_showqueue (st, controller->ack_wait_queue, TRUE);
dmc_showqueue (st, controller->rcv_queue, TRUE);
dmc_showqueue (st, controller->free_queue, TRUE);
if (controller->control_out) {
    CONTROL_OUT *control;

    fprintf (st, "Control Out Queue:\n");
    for (control=controller->control_out; control; control=control->next)
        fprintf (st, "   SEL6:  0x%04X\n", control->sel6);
    }
return SCPE_OK;
}

t_stat dmc_setstats (UNIT* uptr, int32 val, CONST char* cptr, void* desc)
{
CTLR *controller = dmc_get_controller_from_unit(uptr);

controller->receive_buffer_output_transfers_completed = 0;
controller->transmit_buffer_output_transfers_completed = 0;
controller->receive_buffer_input_transfers_completed = 0;
controller->transmit_buffer_input_transfers_completed = 0;
controller->control_out_operations_completed = 0;
controller->ddcmp_control_packets_received = 0;
controller->ddcmp_control_packets_sent = 0;

sim_printf("Statistics reset\n");

return SCPE_OK;
}

t_stat dmc_showconnectpoll (FILE* st, UNIT* uptr, int32 val, CONST void* desc)
{
uint32 poll_interval = *((const uint32 *)desc);

fprintf(st, "connectpoll=%u", poll_interval);
return SCPE_OK;
}

t_stat dmc_setconnectpoll (UNIT* uptr, int32 val, CONST char* cptr, void* desc)
{
t_stat status = SCPE_OK;
uint32 *poll_interval = ((uint32 *)desc);
uint32 newpoll;

if (!cptr) 
    return SCPE_IERR;
newpoll = (int32) get_uint (cptr, 10, 1800, &status);
if ((status != SCPE_OK) || (newpoll == (*poll_interval)))
    return status;
*poll_interval = newpoll;
return tmxr_connection_poll_interval ((poll_interval == &dmc_connect_poll) ? &dmc_desc : &dmp_desc, newpoll);
}

/* SET LINES processor */

t_stat dmc_setnumdevices (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
int32 newln;
uint32 i, j;
t_stat r;
DEVICE *dptr = (DEVICE *)desc;
TMXR *mp = (dptr == &dmc_dev) ? &dmc_desc : &dmp_desc;
int maxunits = (&dmc_dev == dptr) ? DMC_NUMDEVICE : DMP_NUMDEVICE;
DIB *dibptr = (DIB *)dptr->ctxt;
int addrlnt = (UNIBUS) ? IOLN_DMC : IOLN_DMV;

for (j=0; j<2; j++) {
    dptr = (j == 0) ? &dmc_dev : &dmp_dev;
    for (i=0; i<dptr->numunits; i++)
        if (dptr->units[i].flags&UNIT_ATT)
            return SCPE_ALATT;
    }
dptr = (DEVICE *)desc;
for (i=0; i<dptr->numunits; i++)
    if (dptr->units[i].flags&UNIT_ATT)
        return SCPE_ALATT;
if (cptr == NULL)
    return SCPE_ARG;
newln = (int32) get_uint (cptr, 10, maxunits, &r);
if ((r != SCPE_OK) || (newln == (int32)(dptr->numunits - 2)))
    return r;
if (newln == 0)
    return SCPE_ARG;
sim_cancel (dptr->units + dptr->numunits - 2);
sim_cancel (dptr->units + dptr->numunits - 1);
for (i=dptr->numunits-2; i<(uint32)newln; ++i) {
    dptr->units[i] = dmc_unit_template;
    dptr->units[i].ctlr = &dmc_ctrls[(dptr == &dmc_dev) ? i : i+DMC_NUMDEVICE];
    }
dibptr->lnt = newln * addrlnt;                      /* set length */
dptr->numunits = newln + 2;
dptr->units[newln] = dmc_poll_unit_template;
dptr->units[newln].ctlr = &dmc_ctrls[(dptr == &dmc_dev) ? 0 : DMC_NUMDEVICE];
dptr->units[newln+1] = dmc_timer_unit_template;
dptr->units[newln+1].ctlr = &dmc_ctrls[(dptr == &dmc_dev) ? 0 : DMC_NUMDEVICE];
mp->lines = newln;
mp->uptr = dptr->units + newln;                     /* Identify polling unit */
return dmc_reset ((DEVICE *)desc);                  /* setup devices and auto config */
}

t_stat dmc_shownumdevices (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
DEVICE *dptr = (UNIBUS) ? find_dev_from_unit (uptr) : &dmv_dev;

fprintf (st, "lines=%d", dptr->numunits-2);
return SCPE_OK;
}

t_stat dmc_showddcmp (FILE* st, UNIT* uptr, int32 val, CONST void* desc)
{
CTLR *controller = dmc_get_controller_from_unit(uptr);
static const char *states[] = {"Halt", "IStart", "AStart", "Run", "Maintenance"};

fprintf (st, "%s%d\n", controller->device->name, (int)(uptr-controller->device->units));

if (dmp_multipoint(controller)) {           /* DMP/DMV: one session per tributary */
    int slot;

    for (slot = 0; slot < DMP_MAX_TRIBS; slot++) {
        DMP_TRIB *trib = &controller->dmp_trib[slot];
        DDCMP *l = (controller->dmp_loaded == slot) ? &controller->link : &trib->ses.link;

        if (!trib->in_use)
            continue;
        fprintf(st, "Tributary %3d: %-11s R=%d N=%d A=%d T=%d X=%d%s%s%s timer=%s xmt=%d ackwait=%d rcv=%d%s\n",
                trib->address, states[l->state], l->R, l->N, l->A, l->T, l->X,
                l->SACK ? " SACK" : "", l->SNAK ? " SNAK" : "", l->SREP ? " SREP" : "",
                l->TimerRunning ? "on" : "off", (int)trib->ses.xmt.count, (int)trib->ses.ackw.count,
                (int)trib->ses.rcv.count,
                controller->dmp_hub ? (trib->ses.drop ? "" : " (drop line unknown)") : "");
        }
    fprintf(st, "Common receive pool: %d buffers\n", (int)controller->dmp_pool->count);
    return SCPE_OK;
    }
fprintf(st, "DDCMP Link State: %s\n", states[controller->link.state]);
fprintf(st, "               R: %d\n", controller->link.R);
fprintf(st, "               N: %d\n", controller->link.N);
fprintf(st, "               A: %d\n", controller->link.A);
fprintf(st, "               T: %d\n", controller->link.T);
fprintf(st, "               X: %d\n", controller->link.X);
fprintf(st, "            SACK: %s\n", controller->link.SACK ? "true" : "false");
fprintf(st, "            SNAK: %s\n", controller->link.SNAK ? "true" : "false");
fprintf(st, "            SREP: %s\n", controller->link.SREP ? "true" : "false");
fprintf(st, "         rcv_pkt: %p\n", controller->link.rcv_pkt);
fprintf(st, "    rcv_pkt_size: %d\n", controller->link.rcv_pkt_size);
fprintf(st, "      xmt_buffer: %p\n", controller->link.xmt_buffer);
fprintf(st, "      nak_reason: %d\n", controller->link.nak_reason);
fprintf(st, "    TimerRunning: %s\n", controller->link.TimerRunning ? "true" : "false");
fprintf(st, "   TimeRemaining: %d\n", controller->link.TimeRemaining);
return SCPE_OK;
}

t_stat dmc_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
const char helpString[] =
 /* The '*'s in the next line represent the standard text width of a help line */
     /****************************************************************************/
    " The %1s is a communication subsystem which consists of a microprocessor\n"
    " based, intelligent synchronous communications controller which employs\n"
    " the DIGITAL Data Communications Message Protocol (DDCMP).\n"
    "1 Hardware Description\n"
    " The %1s consists of a microprocessor module and a synchronous line unit\n"
    " module.\n"
#if !defined (VM_PDP10)
    "2 Models\n"
    " There were a number of microprocessor DDCMP devices produced.\n"
    "3 DMC11\n"
    " The original kmc11 microprocessor board with DMC microcode and a sync\n"
    " line unit.\n"
    "3 DMR11\n"
    " The more advanced kmc11 microprocessor board with DMR microcode and a sync\n"
    " line unit.\n"
    "3 DMP11\n"
    " A newly designed Unibus board with a more complete programming interface\n"
    " and a sync line unit.\n"
    "3 DMV11\n"
    " A Qbus version of the DMP11 with some more advanced refinements and a sync\n"
    " line unit.\n"
#endif
    "2 $Registers\n"
    "\n"
    " These registers contain the emulated state of the device.  These values\n"
    " don't necessarily relate to any detail of the original device being\n"
    " emulated but are merely internal details of the emulation.\n"
    "1 Configuration\n"
    " A %D device is configured with various simh SET and ATTACH commands\n"
    "2 $Set commands\n"
    "3 Lines\n"
    " A maximum of %2s %1s devices can be emulated concurrently in the %S\n"
    " simulator. The number of simulated %1s devices or lines can be\n"
    " specified with command:\n"
    "\n"
    "+sim> SET %D LINES=n\n"
    "3 Peer\n"
    " To set the host and port to which data is to be transmitted use the\n"
    " following command:\n"
    "\n"
    "+sim> SET %U PEER=host:port\n"
    "3 Connectpoll\n"
    " The minimum interval between attempts to connect to the other side is set\n"
    " using the following command:\n"
    "\n"
    "+sim> SET %D CONNECTPOLL=n\n"
    "\n"
    " Where n is the number of seconds. The default is %3s seconds.\n"
    "3 Speed\n"
    " If you want to experience the actual data rates of the physical hardware\n"
    " you can set the bit rate of the simulated line can be set using the\n"
    " following command:\n"
    "\n"
    "+sim> SET %U SPEED=n\n"
    "\n"
    " Where n is the number of data bits per second that the simulated line\n"
    " runs at.  In practice this is implemented as a delay while transmitting\n"
    " bytes to the socket.  Use a value of zero to run at full speed with no\n"
    " artificial throttling.\n"
    ;
/* SET TYPE: DMC device only (the DMR11 is a variant of the DMC device; the
   DMP11 and DMV11 are devices of their own) */
const char helpType[] =
#if !defined (VM_PDP10)
    "3 Type\n"
    " Each line of the DMC device emulates a DMC11 or a DMR11; the type can be\n"
    " changed with the following command:\n"
    "\n"
    "+sim> SET %U TYPE={DMR|DMC}\n"
    "\n"
    " A SET TYPE command should be entered before the device is attached to a\n"
    " listening port.  The DMP11 and DMV11 are separate devices (DMP, DMV).\n"
#endif
    "";
const char helpMid[] =
    "3 Corruption\n"
    " Corruption Troll - the DDCMP emulation includes a process that will\n"
    " intentionally drop or corrupt some messages.  This emulates the\n"
    " less-than-perfect communications lines encountered in the real world,\n"
    " and enables network monitoring software to see non-zero error counters.\n"
    "\n"
    " The troll selects messages with a probablility selected by the SET %U\n"
    " CORRUPTION command.  The units are 0.1%%; that is, a value of 1 means that\n"
    " every message has a 1/1000 chance of being selected to be corrupted\n"
    " or discarded.\n"
     /****************************************************************************/
#define DMC_HLP_ATTACH "Configuration Attach"
    "2 Attach\n"
    " The device must be attached to a receive port, use the ATTACH command\n"
    " specifying the receive port number.\n"
    "\n"
    "+sim> ATTACH %U port\n"
    "\n"
    " A DMC11 or DMR11 needs its PEER (host:port) set before the ATTACH command.\n"
    " A DMP11 or DMV11 attached without a PEER listens for the other station(s)\n"
    " (see Multipoint).\n"
    " The default connection uses TCP transport between the local system and\n"
    " the peer.  Alternatively, UDP can be used by specifying UDP on the\n"
    " ATTACH command:\n"
    "\n"
    "+sim> ATTACH %U port,UDP\n"
    "\n"
    " Communication may alternately use the DDCMP synchronous framer device.\n"
    " The DDCMP synchronous device is a USB device that can send and\n"
    " receive DDCMP frames over either RS-232 or coax synchronous lines.\n"
    " Refer to https://github.com/pkoning2/ddcmp for documentation.\n"
    "\n"
    "+sim> ATTACH %U SYNC=ifname:mode:speed\n"
    "\n"
    " Communicate via the synchronous DDCMP framer interface \"ifname\", \n"
    " and framer mode \"mode\" -- one of INTEGRAL, RS232_DTE, or\n"
    " RS232_DCE.  The \"speed\" argument is the bit rate for the line.\n"
    " In SYNC mode, the \"PEER\" parameter is not used and need not be set.\n"
    " You can use \"SHOW SYNC\" to see the list of synchronous DDCMP devices.\n"
    "2 Examples\n"
    " To configure two simulators to talk to each other use the following\n"
    " example:\n"
    " \n"
    " Machine 1\n"
    "+sim> SET %D ENABLE\n"
    "+sim> SET %U PEER=LOCALHOST:2222\n"
    "+sim> ATTACH %U 1111\n"
    " \n"
    " Machine 2\n"
    "+sim> SET %D ENABLE\n"
    "+sim> SET %U PEER=LOCALHOST:1111\n"
    "+sim> ATTACH %U 2222\n"
    "\n"
    " To communicate with an \"integral modem\" DMC or similar, at 56 kbps:\n"
    "+sim> ATTACH %U SYNC=sync0:INTEGRAL:56000\n"
    ;
/* multipoint: DMP/DMV devices only */
const char helpMpExample[] =
    "\n"
    " A %1s multipoint control station with two tributary stations:\n"
    " \n"
    " Control station (no PEER: it listens for the tributary stations)\n"
    "+sim> ATTACH %U 1111\n"
    " \n"
    " Each tributary station\n"
    "+sim> SET %U PEER=controlhost:1111\n"
    "+sim> ATTACH %U 2222\n"
    ;
const char helpMonitor[] =
    "1 Monitoring\n"
    " The %D device and %U line configuration and state can be displayed with\n"
    " one of the available show commands.\n"
    "2 $Show commands\n"
    "1 Diagnostics\n"
    " Corruption Troll - the DDCMP emulation includes a process that will\n"
    " intentionally drop or corrupt some messages.  This emulates the\n"
    " less-than-perfect communications lines encountered in the real world,\n"
    " and enables network monitoring software to see non-zero error counters.\n"
    "\n"
    " The troll selects messages with a probablility selected by the SET %U\n"
    " CORRUPTION command.  The units are 0.1%%; that is, a value of 1 means that\n"
    " every message has a 1/1000 chance of being selected to be corrupted\n"
    " or discarded.\n"
    ;
const char helpMp[] =
    "1 Multipoint\n"
    " Real hardware synchronous connections could operate in Multi-Point mode,\n"
    " sharing a single line between a control station and several tributary\n"
    " stations.  The DMP11 and DMV11 emulate it: the operating system selects\n"
    " control station or tributary mode, and each tributary address gets its\n"
    " own DDCMP session.  A DMP11 can establish up to 32 tributaries, a\n"
    " DMV11 up to 12.\n"
    "\n"
    " Each tributary station connects to the control station with its normal\n"
    " PEER and ATTACH settings.  The control station is attached without a\n"
    " PEER:\n"
    "\n"
    "+sim> ATTACH %U 1111\n"
    "\n"
    " It then listens on its port for as many stations as it has tributaries\n"
    " and learns which tributary address answers on which connection (SHOW\n"
    " %U DDCMP).  A control station with a single tributary station can also\n"
    " use a normal PEER connection.  A DMP11 or DMV11 that is not a control\n"
    " station uses the first connection it gets, so it can also be attached\n"
    " without a PEER for a point-to-point line.\n"
    "\n"
    " Give a listening station's ATTACH only a port number, so that it listens\n"
    " on all local addresses; an address such as localhost:1111 limits it to\n"
    " one address (for example IPv4 only), and stations whose PEER name\n"
    " resolves to another one (such as IPv6 ::1) cannot connect.\n"
    "\n"
    " Polling is not emulated: the TCP connections are full duplex.\n"
    ;
const char helpTail[] =
    "1 Restrictions\n"
    " In real hardware, the DMC11 spoke a version of DDCMP which peer devices\n"
    " needed to be aware of.  The DMR11, DMP11, and DMV11 boards have\n"
    " configuration switches or programatic methods to indicate that the peer\n"
    " device was a DMC11.  The emulated devices all speak the same level of\n"
    " DDCMP so no special remote device awareness need be considered.\n"
    "\n"
    " The DMC11 and DMR11 are point-to-point devices; multipoint operation is\n"
    " available with the DMP11 and DMV11 (devices DMP and DMV).\n"
    "1 Implementation\n"
    " A real %1s transports data using DDCMP via a synchronous connection, the\n"
    " emulated device makes a TCP/IP connection to another emulated device which\n"
    " either speaks DDCMP over the TCP connection directly, or interfaces to a\n"
    " simulated computer where the operating system speaks the DDCMP protocol on\n"
    " the wire.\n"
    "\n"
    " The %1s can be used for DDCMP connections carrying DECnet and other\n"
    " types of networking, e.g. from ULTRIX or DSM.\n"
    "1 Debugging\n"
    " The simulator has a number of debug options, these are:\n"
    "\n"
    "++REG     Shows whenever a CSR is programatically read or written\n"
    "++++and the current value.\n"
    "++INTREG  Shows internal register value changes.\n"
    "++INFO    Shows higher-level tracing only.\n"
    "++WARN    Shows any warnings.\n"
    "++TRACE   Shows more detailed trace information.\n"
    "++DATASUM Brief summary of each received and transmitted buffer.\n"
    "++++Ignored if DATA is set.\n"
    "++DATA    Shows the actual data sent and received.\n"
    "++MODEM   Shows modem signal transitions details.\n"
    "++CONNECT Shows sockets connection activity.\n"
    "++INT     Shows Interrupt activity.\n"
    "\n"
    " To get a full trace use\n"
    "\n"
    "+sim> SET %D DEBUG\n"
    "\n"
    " However it is recommended to use the following when sending traces:\n"
    "\n"
    "+sim> SET %D DEBUG=REG;INFO;WARN\n"
    "\n"
    "1 Related Devices\n"
    " The %D can facilitate communication with other simh simulators which\n"
    " have emulated synchronous network devices available.  These include\n"
    " the following:\n"
    "\n"
    "++DUP11*       Unibus PDP11 simulators\n"
    "++DPV11*       Qbus PDP11 simulators\n"
    "++KDP11*       Unibus PDP11 simulators and PDP10 simulators\n"
    "++DMR11        Unibus PDP11 simulators and Unibus VAX simulators\n"
    "++DMC11        Unibus PDP11 simulators and Unibus VAX simulators\n"
    "++DMP11        Unibus PDP11 simulators and Unibus VAX simulators\n"
    "++DMV11        Qbus VAX simulators and Qbus PDP11 simulators\n"
    "\n"
    "++* Indicates systems which have OS provided DDCMP implementations.\n"
    ;
char devname[16];
char devcount[16];
char connectpoll[16];
t_bool dmc = (dptr == &dmc_dev);
char *help;
t_stat r;

sprintf (devname, "%s11" , dptr->name);
sprintf (devcount, "%d", dmc ? DMC_NUMDEVICE : DMP_NUMDEVICE);
sprintf (connectpoll, "%d", DMC_CONNECT_POLL);
/* The help is put together from the parts that apply to this device: the
   DMC device has SET TYPE, the DMP and DMV devices multipoint. */
help = (char *)malloc (strlen (helpString) + strlen (helpType) + strlen (helpMid) +
                       strlen (helpMpExample) + strlen (helpMonitor) + strlen (helpMp) +
                       strlen (helpTail) + 1);
if (help == NULL)
    return SCPE_MEM;
strcpy (help, helpString);
if (dmc)
    strcat (help, helpType);
strcat (help, helpMid);
if (!dmc)
    strcat (help, helpMpExample);
strcat (help, helpMonitor);
if (!dmc)
    strcat (help, helpMp);
strcat (help, helpTail);
r = scp_help (st, dptr, uptr, flag, help, cptr, devname, devcount, connectpoll);
free (help);
return r;
}

t_stat dmc_help_attach (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
return dmc_help (st, dptr, uptr, flag, DMC_HLP_ATTACH);
}

void dmc_setinint(CTLR *controller)
{
if (!dmc_is_iei_set(controller))
    return;
if (!controller->in_int) {
    sim_debug(DBG_INT, controller->device, "SET_INT(RX:%s%d) summary=0x%x\n", controller->device->name, controller->index, dmc_ini_summary);
    }
controller->in_int = 1;
dmc_ini_summary |= (1u << controller->index);
SET_INT(DMCRX);
}

void dmc_clrinint(CTLR *controller)
{
controller->in_int = 0;
if (dmc_ini_summary & (1u << controller->index)) {
    sim_debug(DBG_INT, controller->device, "CLR_INT(RX:%s%d) summary=0x%x\n", controller->device->name, controller->index, dmc_ini_summary);
    }
dmc_ini_summary &= ~(1u << controller->index);
if (!dmc_ini_summary)
    CLR_INT(DMCRX);
else
    SET_INT(DMCRX);
}

void dmc_setoutint(CTLR *controller)
{
if (!dmc_is_ieo_set(controller))
    return;
if (!controller->out_int) {
    sim_debug(DBG_INT, controller->device, "SET_INT(TX:%s%d) summary=0x%x\n", controller->device->name, controller->index, dmc_outi_summary );
    }
controller->out_int = 1;
dmc_outi_summary |= (1u << controller->index);
SET_INT(DMCTX);
}

void dmc_clroutint(CTLR *controller)
{
controller->out_int = 0;
if (dmc_outi_summary & (1u << controller->index)) {
    sim_debug(DBG_INT, controller->device, "CLR_INT(TX:%s%d) summary=0x%x\n", controller->device->name, controller->index, dmc_outi_summary);
    }
dmc_outi_summary &= ~(1u << controller->index);
if (!dmc_outi_summary)
    CLR_INT(DMCTX);
else
    SET_INT(DMCTX);
}

int dmc_getsel(int addr)
{
return (addr >> 1) & ((UNIBUS) ? 03 : 07);
}

uint16 dmc_bitfld(int data, int start_bit, int length)
{
uint16 ans = (uint16)(data >> start_bit);
uint32 mask = (1 << (length))-1;
ans &= mask;
return ans;
}

void dmc_dumpregsel0(CTLR *controller, int trace_level, const char * prefix, uint16 data)
{
const char *type_str = "";
uint16 type = dmc_bitfld(data, DMC_SEL0_V_ITYPE, DMC_SEL0_S_ITYPE);
static const char *dmc_types[] = {"XMT BA/CC", "CNTL IN", "HALT", "BASE IN", "RCV BA/CC", "?????",  "?????", "?????"};

if (dmc_is_dmc(controller)) {
    if (dmc_is_rqi_set(controller)) {
        type_str = dmc_types[type];
        }

    sim_debug(
        trace_level,
        controller->device,
        "%s%d: %s SEL0 (0x%04x) %s%s%s%s%s%s%s%s%s\n",
        controller->device->name, controller->index,
        prefix,
        data,
        dmc_bitfld(data, DMC_SEL0_V_RUN, 1) ? "RUN " : "",
        dmc_bitfld(data, DMC_SEL0_V_MCLEAR, 1) ? "MCLR " : "",
        dmc_bitfld(data, DMC_SEL0_V_LU_LOOP, 1) ? "LU LOOP " : "",
        dmc_bitfld(data, DMC_SEL0_V_ROMI, 1) ? "ROMI " : "",
        dmc_bitfld(data, DMC_SEL0_V_STEPUP, 1) ? "STEPUP " : "",
        dmc_bitfld(data, DMC_SEL0_V_RDI, 1) ? "RDI " : "",
        dmc_bitfld(data, DMC_SEL0_V_IEI, 1) ? "IEI " : "",
        dmc_bitfld(data, DMC_SEL0_V_RQI, 1) ? "RQI " : "",
        type_str
        );
    }
else {
    sim_debug(
        trace_level,
        controller->device,
        "%s%d: %s SEL0 (0x%04x) %s%s%s%s%s%s\n",
        controller->device->name, controller->index,
        prefix,
        data,
        dmc_bitfld(data, DMC_SEL0_V_RUN, 1) ? "RUN " : "",
        dmc_bitfld(data, DMC_SEL0_V_MCLEAR, 1) ? "MCLR " : "",
        dmc_bitfld(data, DMC_SEL0_V_LU_LOOP, 1) ? "LU LOOP " : "",
        dmc_bitfld(data, DMP_SEL0_V_RQI, 1) ? "RQI " : "",
        dmc_bitfld(data, DMP_SEL0_V_IEO, 1) ? "IEO " : "",
        dmc_bitfld(data, DMP_SEL0_V_IEI, 1) ? "IEI " : ""
        );
    }
}

void dmc_dumpregsel2(CTLR *controller, int trace_level, const char *prefix, uint16 data)
{
const char *type_str = "";
uint16 type = dmc_bitfld(data, DMC_SEL2_V_CODE, DMC_SEL2_S_CODE);
static const char *dmc_types[] = {"XMT BA/CC OUT", "CNTL OUT", "?????", "?????", "RCV BA/CC OUT", "?????",  "?????", "?????"};

type_str = dmc_types[type];

sim_debug(
    trace_level,
    controller->device,
    "%s%d: %s SEL2 (0x%04x) PRIO=%d LINE=%d %s%s%s%s\n",
    controller->device->name, controller->index,
    prefix,
    data,
    dmc_bitfld(data, SEL2_PRIO_BIT, SEL2_PRIO_BIT_LENGTH),
    dmc_bitfld(data, SEL2_LINE_BIT, SEL2_LINE_BIT_LENGTH),
    dmc_bitfld(data, SEL2_RDO_BIT, 1) ? "RDO " : "",
    dmc_bitfld(data, SEL2_IEO_BIT, 1) ? "IEO " : "",
    dmc_bitfld(data, SEL2_OUT_IO_BIT, 1) ? "OUT I/O " : "",
    type_str
    );
}

void dmc_dumpregsel4(CTLR *controller, int trace_level, const char *prefix, uint16 data)
{
if (dmc_is_rdyi_set(controller)) {
    sim_debug(
        trace_level,
        controller->device,
        "%s%d: %s SEL4 (0x%04x) %s%s%s%s%s%s%s%s\n",
        controller->device->name, controller->index,
        prefix,
        data,
        dmc_bitfld(data, DMC_SEL4_V_RI, 1) ? "RI " : "",
        dmc_bitfld(data, DMC_SEL4_V_DTR, 1) ? "DTR " : "",
        dmc_bitfld(data, DMC_SEL4_V_RTS, 1) ? "RTS " : "",
        dmc_bitfld(data, DMC_SEL4_V_HDX, 1) ? "HDX " : "",
        dmc_bitfld(data, DMC_SEL4_V_DSR, 1) ? "DSR " : "",
        dmc_bitfld(data, DMC_SEL4_V_CTS, 1) ? "CTS " : "",
        dmc_bitfld(data, DMC_SEL4_V_STN, 1) ? "STN " : "",
        dmc_bitfld(data, DMC_SEL4_V_CAR, 1) ? "CAR " : "");
    }
else {
    sim_debug(
        trace_level,
        controller->device,
        "%s%d: %s SEL4 (0x%04x)\n",
        controller->device->name, controller->index,
        prefix,
        data);
    }
}

void dmc_dumpregsel6(CTLR *controller, int trace_level, const char *prefix, uint16 data)
{
if (dmc_is_rdyi_set(controller)) {
    sim_debug(
        trace_level,
        controller->device,
        "%s%d: %s SEL6 (0x%04x) %s%s\n",
        controller->device->name, controller->index,
        prefix,
        data,
        dmc_bitfld(data, DMC_SEL6_M_LOSTDATA, 1) ? "LOST_DATA " : "",
        dmc_bitfld(data, DMC_SEL6_M_DISCONN, 1) ? "DISCONNECT " : "");
    }
else {
    sim_debug(
        trace_level,
        controller->device,
        "%s%d: %s SEL6 (0x%04x)\n",
        controller->device->name, controller->index,
        prefix,
        data);
    }
}

void dmc_dumpregsel10(CTLR *controller, int trace_level, const char *prefix, uint16 data)
{
sim_debug(
    trace_level,
    controller->device,
    "%s%d: %s SEL10 (0x%04x) %s\n",
    controller->device->name, controller->index,
    prefix,
    data,
    dmc_bitfld(data, DMC_SEL6_M_LOSTDATA, 1) ? "LOST_DATA " : "");
}

uint16 dmc_getreg(CTLR *controller, int reg, int ctx)
{
uint16 ans = 0;

switch (dmc_getsel(reg)) {
    case 00:
        ans = *controller->csrs->sel0;
        dmc_dumpregsel0(controller, ctx, "Getting", ans);
        break;
    case 01:
        ans = *controller->csrs->sel2;
        dmc_dumpregsel2(controller, ctx, "Getting", ans);
        break;
    case 02:
        ans = *controller->csrs->sel4;
        dmc_dumpregsel4(controller, ctx, "Getting", ans);
        break;
    case 03:
        ans = *controller->csrs->sel6;
        dmc_dumpregsel6(controller, ctx, "Getting", ans);
        break;
    case 04:
        ans = *controller->csrs->sel10;
        dmc_dumpregsel10(controller, ctx, "Getting", ans);
        break;
    default:
        sim_debug(DBG_WRN, controller->device, "%s%d: dmc_getreg(). Invalid register %d", controller->device->name, controller->index, reg);
        break;
    }

return ans;
}

void dmc_setreg(CTLR *controller, int reg, uint16 data, int ctx)
{
const char *trace = "Setting";

switch (dmc_getsel(reg)) {
    case 00:
        dmc_dumpregsel0(controller, ctx, trace, data);
        *controller->csrs->sel0 = data;
        break;
    case 01:
        dmc_dumpregsel2(controller, ctx, trace, data);
        *controller->csrs->sel2 = data;
        break;
    case 02:
        dmc_dumpregsel4(controller, ctx, trace, data);
        *controller->csrs->sel4 = data;
        break;
    case 03:
        dmc_dumpregsel6(controller, ctx, trace, data);
        *controller->csrs->sel6 = data;
        break;
    case 04:
        dmc_dumpregsel10(controller, ctx, trace, data);
        *controller->csrs->sel10 = data;
        break;
    default:
        sim_debug(DBG_WRN, controller->device, "%s%d: dmc_setreg(). Invalid register %d", controller->device->name, controller->index, reg);
    }
}

int dmc_is_master_clear_set(CTLR *controller)
{
return *controller->csrs->sel0 & DMC_SEL0_M_MCLEAR;
}

int dmc_is_lu_loop_set(CTLR *controller)
{
if (dmc_is_dmc(controller))
    return ((*controller->csrs->sel0 & DMC_SEL0_M_LU_LOOP) != 0);
else
    return FALSE;
}

int dmc_is_run_set(CTLR *controller)
{
return *controller->csrs->sel0 & DMC_SEL0_M_RUN;
}

int dmc_is_rqi_set(CTLR *controller)
{
int ans = 0;

if (dmc_is_dmc(controller))
    ans = *controller->csrs->sel0 & DMC_SEL0_M_RQI;
else
    ans = *controller->csrs->sel0 & DMP_SEL0_M_RQI;
return ans;
}

int dmc_is_rdyi_set(CTLR *controller)
{
int ans = 0;

if (dmc_is_dmc(controller))
    ans = *controller->csrs->sel0 & DMC_SEL0_M_RDI;
else
    ans = *controller->csrs->sel2 & DMP_SEL2_M_RDI;
return ans;
}

int dmc_is_iei_set(CTLR *controller)
{
int ans = 0;

if (dmc_is_dmc(controller))
    ans = *controller->csrs->sel0 & DMC_SEL0_M_IEI;
else
    ans = *controller->csrs->sel0 & DMP_SEL0_M_IEI;
return ans;
}

int dmc_is_ieo_set(CTLR *controller)
{
int ans = 0;

if (dmc_is_dmc(controller))
    ans = *controller->csrs->sel2 & DMC_SEL2_M_IEO;
else
    ans = *controller->csrs->sel0 & DMP_SEL0_M_IEO;
return ans;
}

int dmc_is_in_io(CTLR *controller)
{
int ans = 0;

if (dmc_is_dmc(controller))
    ans = ((*controller->csrs->sel0 & DMC_SEL0_M_ITYPE) == DMC_C_TYPE_RBACC);
else
    ans = ((*controller->csrs->sel2 & DMP_SEL2_M_CODE) == DMP_C_TYPE_RBACC);
return ans;
}

int dmc_is_rdyo_set(CTLR *controller)
{
return *controller->csrs->sel2 & DMC_SEL2_M_RDO;
}

void dmc_set_rdyi(CTLR *controller)
{
if (dmc_is_dmc(controller)) {
    dmc_setreg(controller, 0, *controller->csrs->sel0 | DMC_SEL0_M_RDI, DBG_RGC);
    if (controller->dev_type == DMR) {
        dmc_setreg(controller, 4, *controller->modem | 0x0800, DBG_RGC);
        dmc_setreg(controller, 6,
                   (*controller->modem & DMC_SEL4_M_DTR) | DMC_SEL6_M_LMODEM,
                   DBG_RGC);
        }
    }
else
    dmc_setreg(controller, 2, *controller->csrs->sel2 | DMP_SEL2_M_RDI, DBG_RGC);
dmc_setinint(controller);
}

void dmc_clear_rdyi(CTLR *controller)
{
if (dmc_is_dmc(controller))
    dmc_setreg(controller, 0, *controller->csrs->sel0 & ~(DMC_SEL0_M_RDI|DMC_SEL0_M_ITYPE), DBG_RGC);
else
    dmc_setreg(controller, 2, *controller->csrs->sel2 & ~DMP_SEL2_M_RDI, DBG_RGC);
}

void dmc_set_rdyo(CTLR *controller)
{
dmc_setreg(controller, 2, *controller->csrs->sel2 | DMC_SEL2_M_RDO, DBG_RGC);

dmc_setoutint(controller);
}

uint32 dmc_get_addr(CTLR *controller)
{
if (dmc_is_dmc(controller) || (!(*controller->csrs->sel2 & DMP_SEL2_M_22BIT)))
    return ((uint32)dmc_getreg(controller, 4, DBG_RGC))  | ((((uint32)dmc_getreg(controller, 6, DBG_RGC) & 0xc000)) << 2);
else
    return ((uint32)dmc_getreg(controller, 4, DBG_RGC))  | (((uint32)dmc_getreg(controller, 6, DBG_RGC) & 0x3F) << 16);   /* Q22: BSEL6 bits 0-5 = address bits 16-21 */
}

void dmc_set_addr(CTLR *controller, uint32 addr)
{
if (dmc_is_dmc(controller) || (!(*controller->csrs->sel2 & DMP_SEL2_M_22BIT))) {
    dmc_setreg(controller, 4, addr & 0xFFFF, DBG_RGC);
    dmc_setreg(controller, 6, (uint16)(((addr >> 16) << 14) | (*controller->csrs->sel6 & 0x3FFF)) , DBG_RGC);
    }
else {
    dmc_setreg(controller, 4, addr & 0xFFFF, DBG_RGC);
    dmc_setreg(controller, 6, ((addr >> 16) & 0x3F), DBG_RGC);
    }
}

uint16 dmc_get_count(CTLR *controller)
{
if (dmc_is_dmc(controller) || (!(*controller->csrs->sel2 & DMP_SEL2_M_22BIT)))
    return dmc_getreg(controller, 6, DBG_RGC) & 0x3FFF;
else
    return dmc_getreg(controller, 010, DBG_RGC) & 0x3FFF;
}

void dmc_set_count(CTLR *controller, uint16 count)
{
if (dmc_is_dmc(controller) || (!(*controller->csrs->sel2 & DMP_SEL2_M_22BIT)))
    dmc_setreg(controller, 6, (*controller->csrs->sel6 & 0xc000) | (0x3FFF & count), DBG_RGC);
else
    dmc_setreg(controller, 010, (*controller->csrs->sel10 & 0xc000) | (0x3FFF & count), DBG_RGC);
}

uint8 dmc_get_modem(CTLR *controller)
{
int32 modem_bits;

tmxr_set_get_modem_bits (controller->line, 0, 0, &modem_bits);
*controller->modem &= ~(DMC_SEL4_M_CTS|DMC_SEL4_M_CAR|DMC_SEL4_M_RI|DMC_SEL4_M_DSR|DMC_SEL4_M_DTR|DMC_SEL4_M_RTS);
*controller->modem |= (modem_bits&TMXR_MDM_DCD) ? DMC_SEL4_M_CAR : 0;
*controller->modem |= (modem_bits&TMXR_MDM_CTS) ? DMC_SEL4_M_CTS : 0;
*controller->modem |= (modem_bits&TMXR_MDM_DSR) ? DMC_SEL4_M_DSR : 0;
*controller->modem |= (modem_bits&TMXR_MDM_RNG) ? DMC_SEL4_M_RI : 0;
*controller->modem |= (modem_bits&TMXR_MDM_DTR) ? DMC_SEL4_M_DTR : 0;
*controller->modem |= (modem_bits&TMXR_MDM_RTS) ? DMC_SEL4_M_RTS : 0;
return (*controller->modem);
}

void dmc_set_modem_dtr(CTLR *controller)
{
if (dmc_is_attached(controller->unit) && (!(DMC_SEL4_M_DTR & *controller->modem))) {
    sim_debug(DBG_MDM, controller->device, "%s%d: DTR State Change to UP(ON)\n", controller->device->name, controller->index);
    tmxr_set_get_modem_bits (controller->line, TMXR_MDM_DTR|TMXR_MDM_RTS, 0, NULL);
    dmc_get_modem(controller);
    controller->line->rcve = 1;
    }
}

void dmc_clr_modem_dtr(CTLR *controller)
{
if (*controller->modem & DMC_SEL4_M_DTR) {
    sim_debug(DBG_MDM, controller->device, "%s%d: DTR State Change to DOWN(OFF)\n", controller->device->name, controller->index);
    }
tmxr_set_get_modem_bits (controller->line, 0, TMXR_MDM_DTR|TMXR_MDM_RTS, NULL);
dmc_get_modem(controller);
}

void dmc_set_lost_data(CTLR *controller)
{
dmc_setreg(controller, 6, *controller->csrs->sel6 | DMC_SEL6_M_LOSTDATA, DBG_RGC);
}

void dmc_clear_master_clear(CTLR *controller)
{
dmc_setreg(controller, 0,
           *controller->csrs->sel0 & ~(DMC_SEL0_M_MCLEAR|DMC_SEL0_M_STEPUP|DMC_SEL0_M_ROMI|DMC_SEL0_M_ROMO|DMC_SEL0_M_LU_LOOP|DMC_SEL0_M_STEPLU), DBG_RGC);
}

void dmc_set_run(CTLR *controller)
{
dmc_setreg(controller, 0, *controller->csrs->sel0 | DMC_SEL0_M_RUN, DBG_RGC);
}

int dmc_get_input_transfer_type(CTLR *controller)
{
int ans = 0;

if (dmc_is_dmc(controller))
    ans = *controller->csrs->sel0 & DMC_SEL0_M_ITYPE;
else
    ans = *controller->csrs->sel2 & DMP_SEL2_M_CODE;
return ans;
}

void dmc_set_type_output(CTLR *controller, int type)
{
dmc_setreg(controller, 2, (*controller->csrs->sel2 & ~DMC_SEL2_M_CODE) | (type & DMC_SEL2_M_CODE), DBG_RGC);
}

void dmc_process_master_clear(CTLR *controller)
{
CONTROL_OUT *control;

sim_debug(DBG_INF, controller->device, "%s%d: Master clear\n", controller->device->name, controller->index);
dmc_clear_master_clear(controller);
dmc_clr_modem_dtr(controller);
controller->link.state = Halt;
controller->state = Initialised;
while ((control = controller->control_out)) {
    controller->control_out = control->next;
    free (control);
    }
controller->control_out = NULL;
if (controller->dev_type == DMR) {
    if (dmc_is_attached(controller->unit)) {
        /* Indicates microdiagnostics complete */
        if (((*controller->csrs->sel0 & DMC_SEL0_M_UDIAG) != 0) ^
            (dmc_microdiag[controller->index]))
            dmc_setreg(controller, 2, 0x8000, DBG_RGC);/* Microdiagnostics Complete */
        else
            dmc_setreg(controller, 2, 0x4000, DBG_RGC); /* Microdiagnostics Inhibited */
        }
    else {
        /* Indicate M8203 (Line Unit) test failed */
        dmc_setreg(controller, 2, 0x0200, DBG_RGC);
        }
    }
if (controller->dev_type == DMP) {
    /* Self-test passed signature; VMS treats anything else on a DMV as a
       failed controller.  Master clear also forgets the mode and every
       tributary. */
    dmc_setreg(controller, 4, (controller->device == &dmv_dev) ? DMV_SIGNATURE_BSEL4 : DMP_SIGNATURE_BSEL4, DBG_RGC);
    dmc_setreg(controller, 6, DMP_SIGNATURE_BSEL6, DBG_RGC);
    dmp_reset_host_state(controller);
    }
/* The buffer pool is rebuilt below; a transmission still marked as in
   progress (possible with SET SPEED) would otherwise point into it. */
controller->link.xmt_buffer = NULL;
controller->link.xmt_done_buffer = NULL;
dmc_buffer_queue_init_all(controller);

controller->transfer_state = Idle;
if (dmc_is_attached(controller->unit))
    dmc_set_run(controller);
}

void dmc_start_input_transfer(CTLR *controller)
{
sim_debug(DBG_INF, controller->device, "%s%d: Starting input transfer\n", controller->device->name, controller->index);
controller->transfer_state = InputTransfer;
dmc_set_rdyi(controller);
}

void dmc_start_control_output_transfer(CTLR *controller)
{
if ((!controller->control_out) || 
    (controller->transfer_state != Idle) || 
    (dmc_is_rdyo_set(controller)))
    return;
controller->transfer_state = OutputControl;
if (controller->control_out->dmp) {
    /* DMP/DMV response: type code, Q22 flag and tributary in SEL2, then the
       data/address, code and (Q22) code registers.  RDI stays clear here
       because an output is only started while the input side is idle. */
    const uint16 *img = controller->control_out->image;

    sim_debug(DBG_INF, controller->device, "%s%d: Starting response output: SEL2 = 0x%04X SEL4 = 0x%04X SEL6 = 0x%04X SEL10 = 0x%04X\n",
              controller->device->name, controller->index,
              img[DMP_IMG_SEL2], img[DMP_IMG_SEL4], img[DMP_IMG_SEL6], img[DMP_IMG_SEL10]);
    dmc_setreg (controller, 2, (*controller->csrs->sel2 & ~(0xFF00|DMP_SEL2_M_22BIT|DMP_SEL2_M_CODE)) | img[DMP_IMG_SEL2], DBG_RGC);
    dmc_setreg (controller, 4, img[DMP_IMG_SEL4], DBG_RGC);
    dmc_setreg (controller, 6, img[DMP_IMG_SEL6], DBG_RGC);
    if (img[DMP_IMG_SEL2] & DMP_SEL2_M_22BIT)
        dmc_setreg (controller, 010, img[DMP_IMG_SEL10], DBG_RGC);
    }
else {
    sim_debug(DBG_INF, controller->device, "%s%d: Starting control output transfer: SEL6 = 0x%04X\n", controller->device->name, controller->index, controller->control_out->sel6);
    dmc_setreg (controller, 6, controller->control_out->sel6, DBG_RGC);
    dmc_set_type_output(controller, DMC_C_TYPE_CNTL);
    }
dmc_set_rdyo(controller);
}

void dmc_complete_transmit(CTLR *controller)
{
BUFFER *buffer = controller->link.xmt_buffer;
if (!buffer)
    return;
controller->link.xmt_done_buffer = buffer;
ddcmp_dispatch (controller, DDCMP_EVENT_XMIT_DONE);
controller->link.xmt_done_buffer = NULL;
buffer = (BUFFER *)remqueue (&buffer->hdr);
buffer->actual_bytes_transferred = buffer->count;
controller->buffers_transmitted_to_net++;
if (buffer->type == TransmitData) {
    buffer->actual_bytes_transferred = buffer->count - (DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE);
    ASSURE (insqueue (&buffer->hdr, controller->ack_wait_queue->hdr.prev));
    }
else {
    ASSURE (insqueue (&buffer->hdr, &controller->free_queue->hdr));
    }
controller->link.xmt_buffer = NULL;
}

t_stat dmc_svc(UNIT* uptr)
{
CTLR *controller = dmc_get_controller_from_unit(uptr);
DEVICE *dptr = controller->device;

sim_debug(DBG_TRC, dptr, "dmc_svc(%s%d)\n", controller->device->name, controller->index);

/* DMP/DMV multipoint: ddcmp_dispatch() calls this routine directly at the
   end of a recursive scan.  If that happens while a tributary session is
   loaded, come back later instead of nesting another session load. */
if (!dmc_is_dmc(controller) && (controller->dmp_loaded >= 0)) {
    sim_clock_coschedule (uptr, tmxr_poll);
    return SCPE_OK;
    }

/* Perform delayed register actions */
if (controller->dmc_wr_delay) {
    controller->dmc_wr_delay = 0;
    dmc_process_command(controller);
    if (controller->link.xmt_buffer) {
        sim_activate_notbefore (uptr, controller->link.xmt_buffer->buffer_return_time);
        return SCPE_OK;
        }
    }
if (dmc_is_attached(controller->unit)) {
    if (dmp_multipoint(controller)) {
        /* Every tributary session: send what is queued and let the
           protocol machine act on pending work (no SPEED limiting). */
        dmp_mp_transmit (controller);
        dmc_start_control_output_transfer(controller);
        dmp_dispatch (controller, 0);
        }
    else {
    /* Speed limited transmissions are completed here after the appropriate service delay */
    if (controller->link.xmt_buffer) {
        dmc_complete_transmit(controller);
        dmc_ddcmp_start_transmitter (controller);
        }

    dmc_start_control_output_transfer(controller);

    if (controller->xmt_queue->count)
        ddcmp_dispatch (controller, 0);
        }
    if (controller->transfer_state == Idle)
        dmc_start_transfer_buffer(controller);

    dmc_buffer_fill_receive_buffers(controller);
    if (controller->transfer_state == Idle)
        dmc_start_transfer_buffer(controller);
    /* Execution of this routine normally is triggered by programatic access by the
       simulated system's device driver to the device registers or the arrival of 
       traffic from the network.  Network traffic is handled by the dmc_poll_svc 
       which may call this routine as needed.  Direct polling here is extra overhead 
       and is only scheduled when needed.
     */
    if ((controller->completion_queue->count) ||    /* if completion queue not empty? */
        (controller->control_out)             ||    /*    or pending control outs? */
        (controller->transfer_state != Idle))       /*    or registers are busy */
        sim_clock_coschedule (uptr, tmxr_poll);     /* wake up periodically until these don't exist */
    }

return SCPE_OK;
}

t_stat dmc_poll_svc (UNIT *uptr)
{
DEVICE *dptr = ((CTLR *)uptr->ctlr)->device;
CTLR *controller;
TMXR *mp = (dptr == &dmc_dev) ? &dmc_desc : &dmp_desc;
int32 dmc, active, attached;

sim_debug(DBG_TRC, dptr, "dmc_poll_svc()\n");

dmc = tmxr_poll_conn(mp);
if (dmc >= 0) {                                 /* new connection? */
    controller = (CTLR *)dptr->units[dmc].ctlr;
    dmc_get_modem (controller);
    sim_debug(DBG_MDM, dptr, "dmc_poll_svc(dmc=%d) - Connection State Change to UP(ON)\n", dmc);
    controller->dmp_down_reported = FALSE;      /* DMP/DMV: a new outage gets a new 304 */
    dmp_dispatch (controller, 0);
    sim_clock_coschedule (controller->unit, tmxr_poll); /* be sure to wake up soon to continue processing */
    }
tmxr_poll_rx (mp);
tmxr_poll_tx (mp);
for (dmc=active=attached=0; dmc < mp->lines; dmc++) {
    CTLR *controller = (CTLR *)dptr->units[dmc].ctlr;
    TMLN *lp = controller->line;
    uint8 old_modem, new_modem;

    controller = (CTLR *)dptr->units[dmc].ctlr;
    if (controller->unit->flags & UNIT_ATT)
        ++attached;
    if (lp->conn)
        ++active;
    if (controller->dmp_hub)                    /* DMP/DMV multipoint control station */
        active += dmp_hub_poll (controller);
    old_modem = *controller->modem;
    new_modem = dmc_get_modem (controller);
    if ((old_modem & DMC_SEL4_M_CAR) && 
        (!(new_modem & DMC_SEL4_M_CAR))) {
        sim_debug(DBG_MDM, controller->device, "dmc_poll_svc(dmc=%d) - Connection State Change to %s\n", dmc, (new_modem & DMC_SEL4_M_CAR) ? "UP(ON)" : "DOWN(OFF)");
        dmp_dispatch (controller, 0);
        sim_clock_coschedule (controller->unit, tmxr_poll); /* wake up soon to finish processing */
        }
    if (dmp_multipoint(controller)) {
        if (dmc_is_attached(controller->unit))  /* sessions: transmit, timers, host */
            dmc_svc (&controller->device->units[dmc]);
        }
    else
    if ((lp->xmte && tmxr_tpbusyln(lp)) ||
        (lp->xmte && controller->link.xmt_buffer) ||
        ddcmp_UserSendMessage(controller)) {
        if (tmxr_tpbusyln(lp)) {
            sim_debug(DBG_DAT, controller->device, "dmc_poll_svc(dmc=%d) - Packet Transmission of remaining %d bytes restarting...\n", dmc, tmxr_tpqln (lp));
            }
        dmc_svc (&controller->device->units[dmc]);  /* Flush pending output */
        }
    if (dmc_buffer_fill_receive_buffers(controller))
        dmc_svc (&controller->device->units[dmc]);  /* return ACKed transmit packets */
    }
if (active)
    sim_clock_coschedule (uptr, tmxr_poll);         /* reactivate */
else {
    for (dmc=0; dmc < mp->lines; dmc++) {
        uint32 *speeds = (dptr == &dmc_dev) ? dmc_speed : dmp_speed;
        CTLR *controller = (CTLR *)dptr->units[dmc].ctlr;

        if (speeds[dmc]/8)
            controller->byte_wait = (tmr_poll*clk_tps)/(speeds[dmc]/8);
        else
            controller->byte_wait = 0;
        }
    if (attached)
        sim_activate_after (uptr, DMC_CONNECT_POLL*1000000);/* periodic check for connections */
    }
return SCPE_OK;
}

t_stat dmc_timer_svc (UNIT *uptr)
{
int32 dmc, active;
DEVICE *dptr = (UNIBUS) ? ((&dmc_dev == find_dev_from_unit(uptr)) ? &dmc_dev : &dmp_dev) : &dmv_dev;
TMXR *mp = (dptr == &dmc_dev) ? &dmc_desc : &dmp_desc;
CTLR *controller;

sim_debug(DBG_TRC, dptr, "dmc_timer_svc()\n");

for (dmc=active=0; dmc < mp->lines; dmc++) {
    controller = (CTLR *)dptr->units[dmc].ctlr;
    if (dmp_multipoint(controller)) {           /* one reply timer per tributary */
        dmp_mp_timers (controller, &active);
        continue;
        }
    if (controller->link.TimerRunning) {
        if (controller->link.TimeRemaining > 0) {
            --controller->link.TimeRemaining;
            if (controller->link.TimeRemaining > 0)
                ++active;
            else
                ddcmp_dispatch (controller, DDCMP_EVENT_TIMER);  /* Timer Expiring Event */
            }
        }
    }
if (active)
    sim_activate_after (uptr, 1000000);         /* Per Second Event trigger */
return SCPE_OK;
}

void dmc_buffer_queue_init(CTLR *controller, BUFFER_QUEUE *q, const char *name, size_t size, BUFFER *buffers)
{
q->name = name;
initqueue (&q->hdr, q, size, buffers, sizeof(*buffers));
q->controller = controller;
}

t_bool dmc_transmit_queue_empty(CTLR *controller)
{
return (controller->xmt_queue->count == 0);
}

void dmc_buffer_queue_init_all(CTLR *controller)
{
size_t queue_size = (controller->dev_type == DMC) ? DMC_QUEUE_SIZE : 
                                                    ((controller->dev_type == DMR) ? DMR_QUEUE_SIZE : 
                                                                                        DMP_QUEUE_SIZE);
/* DMP/DMV: the pool also feeds the multipoint tributary sessions, up to
   12 on a DMV11 and 32 on a DMP11. */
queue_size *= (controller->dev_type == DMP) ? (2 + (2 * dmp_trib_limit(controller)) / 3) : 2;
*controller->buffers = (BUFFER *)realloc(*controller->buffers, queue_size*sizeof(**controller->buffers));
memset (*controller->buffers, 0, queue_size*sizeof(**controller->buffers));
dmc_buffer_queue_init(controller, controller->rcv_queue,        "receive",    0, NULL);
dmc_buffer_queue_init(controller, controller->completion_queue, "completion", 0, NULL);
dmc_buffer_queue_init(controller, controller->xmt_queue,        "transmit",   0, NULL);
dmc_buffer_queue_init(controller, controller->ack_wait_queue,   "ack wait",   0, NULL);
dmc_buffer_queue_init(controller, controller->free_queue,       "free",       queue_size, *controller->buffers);
}

BUFFER *dmc_buffer_allocate(CTLR *controller)
{
BUFFER *buffer = (BUFFER *)remqueue (controller->free_queue->hdr.next);

if (!buffer) {
    sim_printf ("DDCMP Buffer allocation failure.\n");
    sim_printf ("This is a fatal error which should never happen.\n");
    sim_printf ("Please submit this output when you report this bug.\n");
    dmc_showqueues (stdout, controller->unit, 0, NULL);
    dmc_showstats (stdout, controller->unit, 0, NULL);
    dmc_showddcmp (stdout, controller->unit, 0, NULL);
    if (sim_log) {
        dmc_showqueues (sim_log, controller->unit, 0, NULL);
        dmc_showstats (sim_log, controller->unit, 0, NULL);
        dmc_showddcmp (sim_log, controller->unit, 0, NULL);
        fflush (sim_log);
        }
    if (sim_deb) {
        dmc_showqueues (sim_deb, controller->unit, 0, NULL);
        dmc_showstats (sim_deb, controller->unit, 0, NULL);
        dmc_showddcmp (sim_deb, controller->unit, 0, NULL);
        fflush (sim_deb);
        }
    return buffer;
    }
buffer->address = 0;
buffer->count = 0;
buffer->station = 0;
buffer->q22 = FALSE;
free (buffer->transfer_buffer);
buffer->transfer_buffer = NULL;
buffer->actual_bytes_transferred = 0;
buffer->type = TransmitControl;         /* Default */
buffer->buffer_return_time = 0;
return buffer;
}

BUFFER *dmc_buffer_queue_add(BUFFER_QUEUE *q, uint32 address, uint16 count, BufferType type)
{
BUFFER *buffer = dmc_buffer_allocate(q->controller);

if (buffer) {
    buffer->type = type;
    buffer->address = address;
    buffer->count = count;
    if (!dmc_is_dmc(q->controller)) {   /* DMP/DMV: remember tributary and format */
        buffer->station = (uint8)(*q->controller->csrs->sel2 >> DMP_SEL2_V_STATION);
        buffer->q22 = ((*q->controller->csrs->sel2 & DMP_SEL2_M_22BIT) != 0);
        }
    ASSURE (insqueue (&buffer->hdr, q->hdr.prev)); /* Insert at tail */
    sim_debug(DBG_INF, q->controller->device, "%s%d: Queued %s buffer address=0x%08x count=%d %s\n", q->controller->device->name, q->controller->index, q->name, address, count, controller_queue_state(q->controller));
    }
else {
    sim_debug(DBG_WRN, q->controller->device, "%s%d: Failed to queue %s buffer address=0x%08x, queue full %s\n", q->controller->device->name, q->controller->index, q->name, address, controller_queue_state(q->controller));
    // TODO: Report error here.
    }
return buffer;
}


BUFFER *dmc_buffer_queue_head(BUFFER_QUEUE *q)
{
return ((q->hdr.next == &q->hdr) ? NULL : (BUFFER *)q->hdr.next);
}

/* Append a response to the controller's output queue (FIFO). */
static void dmc_append_control_out(CTLR *controller, CONTROL_OUT *control)
{
CONTROL_OUT *last;

if (controller->control_out) {
    last = controller->control_out;
    while (last->next)
        last = last->next;
    last->next = control;
    }
else
    controller->control_out = control;
}

/* Queue a DMP/DMV response with all of its register values.  sel2 holds the
   response type code (bits 0-2), the Q22 flag (bit 3) and the tributary
   address (bits 8-15); the RDO handshake bit is added when it is presented. */
void dmp_queue_response(CTLR *controller, uint16 sel2, uint16 sel4, uint16 sel6, uint16 sel10)
{
CONTROL_OUT *control = (CONTROL_OUT *)calloc(1, sizeof(*control));

if (!control)
    return;
control->dmp = TRUE;
control->image[DMP_IMG_SEL2] = sel2;
control->image[DMP_IMG_SEL4] = sel4;
control->image[DMP_IMG_SEL6] = sel6;
control->image[DMP_IMG_SEL10] = sel10;
dmc_append_control_out(controller, control);
}

void dmc_queue_control_out(CTLR *controller, uint16 sel6)
{
CONTROL_OUT *control;
CONTROL_OUT *last = NULL;

if (!dmc_is_dmc(controller)) {
    /* The DDCMP engine reports link events with DMC control-out bits.  A
       DMP/DMV has its own response codes; translate (or drop) them. */
    dmp_ddcmp_event(controller, sel6);
    return;
    }
control = (CONTROL_OUT *)calloc(1, sizeof(*control));
control->sel6 = sel6;
if (controller->control_out) {
    last = controller->control_out;
    while (last->next)
        last = last->next;
    last->next = control;
    }
else
    controller->control_out = control;
}

/* returns true if some data was received */
t_bool dmc_buffer_fill_receive_buffers(CTLR *controller)
{
int ans = FALSE;

if (dmp_multipoint(controller))             /* frames are routed by tributary address */
    return dmp_mp_receive(controller);

if (controller->state == Running) {
    BUFFER *buffer = dmc_buffer_queue_head(controller->rcv_queue);

    while (buffer) {
        ddcmp_tmxr_get_packet_ln (controller->line, (const uint8 **)&controller->link.rcv_pkt, &controller->link.rcv_pkt_size, *controller->corruption_factor);
        if (!controller->link.rcv_pkt)
            break;
        ans = TRUE;
        controller->buffers_received_from_net++;
        controller->ddcmp_control_packets_received += (controller->link.rcv_pkt[0] == DDCMP_ENQ) ? 1 : 0;
        ddcmp_dispatch(controller, DDCMP_EVENT_PKT_RCVD);
        buffer = dmc_buffer_queue_head(controller->rcv_queue);
        }
    }
return ans;
}

void dmc_start_transfer_buffer(CTLR *controller)
{
BUFFER *head = dmc_buffer_queue_head(controller->completion_queue);
uint16 count;

if ((!head) ||
    (controller->transfer_state != Idle) ||
    (dmc_is_rdyo_set(controller)))
    return;
count = (uint16)head->actual_bytes_transferred;
if (!dmc_is_dmc(controller)) {
    /* DMP/DMV: report the buffer for the tributary and in the address/count
       format it was given with, not whatever the host last wrote to BSEL3
       and the Q22 bit.  (VMS discards a buffer returned for tributary 0, and
       its receive buffers then run out.)  Set SEL2 before the address and
       count, whose layout depends on the Q22 bit. */
    dmc_setreg(controller, 2, (*controller->csrs->sel2 & ~(0xFF00|DMP_SEL2_M_22BIT)) |
                              ((uint16)dmp_buffer_station(controller, head) << DMP_SEL2_V_STATION) |
                              (head->q22 ? DMP_SEL2_M_22BIT : 0), DBG_RGC);
    }
switch (head->type) {
    case Receive:
        sim_debug(DBG_INF, controller->device, "%s%d: Starting data output transfer for receive, address=0x%08x, count=%d %s\n", controller->device->name, controller->index, head->address, count, controller_queue_state(controller));
        dmc_set_type_output(controller, dmc_is_dmc(controller) ? DMC_C_TYPE_RBACC : DMP_C_TYPE_RBACC);
        break;
    case TransmitData:
        sim_debug(DBG_INF, controller->device, "%s%d: Starting data output transfer for transmit, address=0x%08x, count=%d %s\n", controller->device->name, controller->index, head->address, count, controller_queue_state(controller));
        dmc_set_type_output(controller, dmc_is_dmc(controller) ? DMC_C_TYPE_XBACC : DMP_C_TYPE_XBACC);
        break;
    default:
        break;
    }
dmc_set_addr(controller, head->address);
dmc_set_count(controller, count);
controller->transfer_state = OutputTransfer;
dmc_set_rdyo(controller);
}

void dmc_check_for_output_transfer_completion(CTLR *controller)
{
BUFFER *buffer;

if ((dmc_is_rdyo_set(controller)) ||
    (controller->transfer_state != OutputTransfer))
    return;
sim_debug(DBG_INF, controller->device, "%s%d: Output transfer completed %s\n", controller->device->name, controller->index, controller_queue_state(controller));
buffer = (BUFFER *)remqueue (controller->completion_queue->hdr.next);
ASSURE (insqueue (&buffer->hdr, controller->free_queue->hdr.prev));
controller->transmit_buffer_output_transfers_completed++;
controller->transfer_state = Idle;
dmc_process_command(controller); // check for any other transfers
}

void dmc_check_for_output_control_completion(CTLR *controller)
{
CONTROL_OUT *control = controller->control_out;

if ((dmc_is_rdyo_set(controller)) ||
    (controller->transfer_state != OutputControl))
    return;

dmc_dumpregsel6(controller, DBG_INF, "Output command completed:", control->sel6);

controller->transfer_state = Idle;
controller->control_out = control->next;
free(control);
controller->control_out_operations_completed++;
dmc_process_command(controller); // check for any other transfers
}

/* ---------------------------------------------------------------------------
   DMP11/DMV11 host interface

   Command and response protocol of the DMP11 (Unibus) and DMV11 (Qbus), as
   described in the DMV11 Technical Manual (EK-DMV11-TM-001) and the DMV11
   user's guide (EK-DMVQM-UG-003):

   - The host requests a command with RQI; the controller answers with RDI.
     The host then loads the command type (BSEL2 bits 0-2), the Q22 format
     flag (BSEL2 bit 3), the tributary address (BSEL3) and the command
     fields (BSEL4-BSEL7, and BSEL10-BSEL11 in Q22 format), and releases the
     command by clearing RDI.
   - Commands: mode definition (type 2), control (type 1: request keys and
     status memory access), receive buffer (type 0), transmit buffer
     (type 4).
   - Responses use RDO: receive/transmit buffer complete (types 0/4),
     control response (type 1: status and error codes) and information
     response (type 2: status memory, modem signals, halt completion).

   This implementation runs one DDCMP session per controller, which covers
   point-to-point operation and a multipoint station with a single partner.
   Multipoint with several tributaries needs one session per tributary.
   --------------------------------------------------------------------------- */

/* Forget the mode and every tributary (master clear). */
void dmp_reset_host_state(CTLR *controller)
{
controller->dmp_station_known = FALSE;
controller->dmp_station_type = 0;
memset(controller->dmp_gss, 0, sizeof(controller->dmp_gss));
memset(controller->dmp_trib, 0, sizeof(controller->dmp_trib));
controller->dmp_xmt_errors = 0;
controller->dmp_loaded = -1;                /* the sessions went with the tributaries */
controller->dmp_down_reported = FALSE;
/* Default global parameters (Table 4-3): NUMSYNC, streaming tributary
   timer, DELTA T, DEAD T, poll delay */
controller->dmp_gss[033] = 010;
controller->dmp_gss[034] = 013560;
controller->dmp_gss[035] = 0310;
controller->dmp_gss[036] = 017500;
controller->dmp_gss[037] = 0;
}

/* Default tributary parameters (Table 4-2), set when a tributary is
   established: transmit delay, Q/R for the three polling states, NDM-INACT
   and TO-UNRSP, TO-DEAD and MXMC, selection timer (3 s), babbling timer (6 s).
   Low byte first, as the words are read by the host. */
static void dmp_tss_defaults(DMP_TRIB *trib)
{
trib->tss[030] = 0;
trib->tss[031] = 0377;                  /* Q 0377, R 0 */
trib->tss[032] = 0100 << 8;             /* Q 0, R 0100 */
trib->tss[033] = 020 << 8;              /* Q 0, R 020 */
trib->tss[034] = 010 | (2 << 8);        /* NDM-INACT 010, TO-UNRSP 2 */
trib->tss[035] = 020 | (4 << 8);        /* TO-DEAD 020, MXMC 4 */
trib->tss[036] = 05670;
trib->tss[037] = 013560;
}

/* Counters in a status slot latch at their maximum (Section 5.3). */
static void dmp_count16(uint16 *word)
{
if (*word != 0xFFFF)
    ++*word;
}
static void dmp_count_lo(uint16 *word)
{
if ((*word & 0xFF) != 0xFF)
    ++*word;
}

/* Slot of the established tributary with this address, or -1. */
static int dmp_find_trib(CTLR *controller, uint16 address)
{
int i;

for (i = 0; i < DMP_MAX_TRIBS; i++)
    if (controller->dmp_trib[i].in_use && (controller->dmp_trib[i].address == (address & 0xFF)))
        return i;
return -1;
}

/* Address of the tributary the DDCMP session belongs to.  Multipoint: the
   tributary whose session is loaded.  Point-to-point: the started one if
   any, else the first established one, else 0. */
static uint8 dmp_link_station(CTLR *controller)
{
int i, first = -1;

if (controller->dmp_loaded >= 0)
    return controller->dmp_trib[controller->dmp_loaded].address;
for (i = 0; i < DMP_MAX_TRIBS; i++)
    if (controller->dmp_trib[i].in_use) {
        if (controller->dmp_trib[i].run)
            return controller->dmp_trib[i].address;
        if (first < 0)
            first = i;
        }
return (first < 0) ? 0 : controller->dmp_trib[first].address;
}

/* Status slot of the tributary the DDCMP session belongs to, or NULL. */
static uint16 *dmp_session_tss(CTLR *controller)
{
int slot;

if (dmc_is_dmc(controller) || ((slot = dmp_find_trib(controller, dmp_link_station(controller))) < 0))
    return NULL;
return controller->dmp_trib[slot].tss;
}

/* Count an event in the session tributary's status slot (16-bit word, or
   the 8-bit counter in its low byte).  No-op for the DMC/DMR. */
void dmp_count_event(CTLR *controller, int word, t_bool low_byte)
{
uint16 *tss = dmp_session_tss(controller);

if (!tss)
    return;
if (low_byte)
    dmp_count_lo(&tss[word]);
else
    dmp_count16(&tss[word]);
}

/* Tributary a completed buffer is reported for.  A buffer given with
   tributary address 0 belongs to the common pool and is reported for the
   tributary that used it. */
uint8 dmp_buffer_station(CTLR *controller, BUFFER *buffer)
{
return buffer->station ? buffer->station : dmp_link_station(controller);
}

/* Control response (type 1) carrying a status or error code.  Responses
   that are not about a buffer use the 18-bit format (code in BSEL6). */
static void dmp_control_response(CTLR *controller, uint8 station, uint16 code)
{
sim_debug(DBG_INF, controller->device, "%s%d: Control response %03o for tributary %d\n",
          controller->device->name, controller->index, code, station);
dmp_queue_response(controller, ((uint16)station << DMP_SEL2_V_STATION) | DMP_C_TYPE_CNTL, 0, code, 0);
}

/* Control response about a buffer (non-existent memory, buffer too small):
   BSEL4/BSEL5 = buffer address bits 0-15.  18-bit format (Figure 3-9 of
   EK-DMV11-TM-001): output code in BSEL6, address bits 16-21 in BSEL7 bits
   0-5.  Q22 format: BSEL6 = address bits 16-21 and the code is in BSEL10
   (Figure 3-8, not yet checked against the manual). */
static void dmp_buffer_response(CTLR *controller, uint8 station, uint32 address, t_bool q22, uint16 code)
{
uint16 sel2 = ((uint16)station << DMP_SEL2_V_STATION) | DMP_C_TYPE_CNTL;

sim_debug(DBG_WRN, controller->device, "%s%d: Buffer response %03o for tributary %d, address 0x%06X\n",
          controller->device->name, controller->index, code, station, address);
if (q22)
    dmp_queue_response(controller, sel2 | DMP_SEL2_M_22BIT, (uint16)(address & 0xFFFF),
                       (uint16)((address >> 16) & 0x3F), code);
else
    dmp_queue_response(controller, sel2, (uint16)(address & 0xFFFF),
                       (uint16)(((address >> 16) & 077) << 8) | (code & 0377), 0);
}

/* Information response (type 2): BSEL4/BSEL5 = data, BSEL6 = status memory
   location with the access flag, or a return key. */
static void dmp_info_response(CTLR *controller, uint8 station, uint16 data, uint16 sel6)
{
sim_debug(DBG_INF, controller->device, "%s%d: Information response %03o for tributary %d, data 0x%04X\n",
          controller->device->name, controller->index, sel6, station, data);
dmp_queue_response(controller, ((uint16)station << DMP_SEL2_V_STATION) | DMP_C_TYPE_MODE, data, sel6, 0);
}

/* Procedural error (codes 0100-0140).  Per the note under output code 302
   in Table 3-7 only the type code in BSEL2 and the output code in BSEL6 are
   set; every other field (tributary in BSEL3, 22-bit flag, SEL4, BSEL7)
   stays as the user program wrote it in the rejected command, which is
   still in the registers when this is called. */
static void dmp_procedural_error(CTLR *controller, uint8 station, uint16 code)
{
uint16 sel2 = *controller->csrs->sel2;

sim_debug(DBG_WRN, controller->device, "%s%d: Procedural error %03o (tributary %d, SEL2 0x%04X SEL4 0x%04X SEL6 0x%04X)\n",
          controller->device->name, controller->index, code, station,
          sel2, *controller->csrs->sel4, *controller->csrs->sel6);
dmp_queue_response(controller,
                   (sel2 & (0xFF00 | DMP_SEL2_M_22BIT)) | DMP_C_TYPE_CNTL,
                   *controller->csrs->sel4,
                   (*controller->csrs->sel6 & 0xFF00) | code,
                   *controller->csrs->sel10);
}

/* The DDCMP session has completed start-up. */
void dmp_link_running(CTLR *controller)
{
controller->dmp_xmt_errors = 0;
dmp_control_response(controller, dmp_link_station(controller), DMP_RSP_RUNNING);
}

static void dmp_return_buffers(CTLR *controller, uint8 station);
static void dmp_session_release(CTLR *controller, int slot);

/* Mark the tributary of the DDCMP session as halted (run = 0) after the
   protocol stopped on its own, so that the user program can start it again
   without getting procedural error 112 ("not halted"). */
static void dmp_session_halted(CTLR *controller, int new_run)
{
uint8 station = dmp_link_station(controller);
int slot = dmp_find_trib(controller, station);

if (slot >= 0)
    controller->dmp_trib[slot].run = (uint8)new_run;
}

/* Link events the shared DDCMP engine reports with DMC control-out bits,
   translated to the DMV11 output codes of Table 3-7.  The engine calls this
   from the rule's actions, before the link state changes, so
   controller->link.state is still the state the event happened in.
     DISCONN   line/DSR dropped (rule 19)        -> 304, global (BSEL3 = 0)
     STRTRCVD  STRT received in Run (rule 20)    -> 010, tributary halted
               (rule 19 also reports STRTRCVD after a disconnect: the 304
               already says it all, so no 010 then)
     MAINTRCV  maintenance message in Run, ISTRT -> 012
               or ASTRT (rules 8, 17, 21)
               ... while halted (rule 3)         -> 014
               both leave the tributary in the maintenance state.
   On 010 and 012 the tributary's buffers are handed back, followed by
   information response 020 (Table 3-8).
   Other bits (DMC halt complete) have no DMV meaning and are dropped. */
void dmp_ddcmp_event(CTLR *controller, uint16 dmc_sel6)
{
uint8 station = dmp_link_station(controller);

if (dmc_sel6 & DMC_SEL6_M_DISCONN) {        /* the line (DSR) went away */
    if (!controller->dmp_down_reported)     /* global: once, not once per session */
        dmp_control_response(controller, 0, DMP_RSP_DISCONNECT);
    controller->dmp_down_reported = TRUE;
    dmp_session_halted(controller, 0);
    }
if ((dmc_sel6 & DMC_SEL6_M_STRTRCVD) && !ddcmp_LineDisconnected(controller)) {
    dmp_control_response(controller, station, DMP_RSP_STRT_RUN);
    dmp_session_halted(controller, 0);
    dmp_return_buffers(controller, station);    /* Table 3-8 case 1 */
    }
if (dmc_sel6 & DMC_SEL6_M_MAINTRCV) {
    t_bool was_halted = (controller->link.state == Halt);

    dmp_control_response(controller, station, was_halted ? DMP_RSP_MAINT_HALT : DMP_RSP_MAINT_RUN);
    dmp_session_halted(controller, 2);
    if (!was_halted)
        dmp_return_buffers(controller, station);/* Table 3-8 case 2 */
    }
if (dmc_sel6 & ~(DMC_SEL6_M_DISCONN|DMC_SEL6_M_STRTRCVD|DMC_SEL6_M_MAINTRCV))
    sim_debug(DBG_INF, controller->device, "%s%d: DDCMP event 0x%04X not reported to the host\n",
              controller->device->name, controller->index,
              dmc_sel6 & ~(DMC_SEL6_M_DISCONN|DMC_SEL6_M_STRTRCVD|DMC_SEL6_M_MAINTRCV));
}

/* Status memory access: read, read-and-clear or write one word of the GSS
   (tributary address 0) or of the addressed tributary's TSS.  Table 3-5:
   any word can be read; only the error counters can be read and cleared
   (TSS 07-17, GSS 15-17); only the parameters can be written (TSS 030-037,
   GSS 033-037).  Anything else is procedural error 132. */
static void dmp_status_access(CTLR *controller, uint8 station, uint16 sel6)
{
uint16 location = sel6 & DMP_KEY_MASK;
uint16 access = sel6 & DMP_STATUS_MASK;
uint16 *word;
int slot;

if (station) {
    if ((slot = dmp_find_trib(controller, station)) < 0) {
        dmp_procedural_error(controller, station, DMP_ERR_NOT_ESTAB);
        return;
        }
    word = &controller->dmp_trib[slot].tss[location];
    }
else
    word = &controller->dmp_gss[location];
switch (access) {
    case DMP_STATUS_WRITE:
        if (location < (station ? 030 : 033)) {
            dmp_procedural_error(controller, station, DMP_ERR_BAD_ACCESS);
            return;
            }
        *word = *controller->csrs->sel4;
        return;                             /* writes are not answered */
    case DMP_STATUS_CLEAR:
        if ((location < (station ? 07 : 015)) || (location > 017)) {
            dmp_procedural_error(controller, station, DMP_ERR_BAD_ACCESS);
            return;
            }
        dmp_info_response(controller, station, *word, location | DMP_STATUS_CLEAR);
        *word = 0;
        return;
    case DMP_STATUS_READ:
        dmp_info_response(controller, station, *word, location | DMP_STATUS_READ);
        return;
    default:                                /* more than one access bit */
        dmp_procedural_error(controller, station, DMP_ERR_BAD_ACCESS);
        return;
    }
}

/* Hand one buffer back to the host with a buffer disposition response
   (Figure 3-8), queued in order with the other responses: BSEL3 =
   tributary, SEL4 = address bits 0-15; 18-bit format SEL6 bits 14-15 =
   address bits 16-17 and bits 0-13 = count; Q22 format BSEL6 bits 0-5 =
   address bits 16-21 and SEL10 = count.  The count is the buffer's size for
   a receive buffer and the message length for a transmit buffer. */
static void dmp_return_buffer(CTLR *controller, BUFFER *buffer, uint8 station, uint16 type)
{
uint16 count = (uint16)((buffer->type == Receive) ? buffer->count
                                                  : buffer->count - (DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE));
uint16 sel2 = ((uint16)station << DMP_SEL2_V_STATION) | type | (buffer->q22 ? DMP_SEL2_M_22BIT : 0);

sim_debug(DBG_INF, controller->device, "%s%d: Returning buffer (type %o) for tributary %d, address=0x%08x, count=%d\n",
          controller->device->name, controller->index, type, station, buffer->address, count);
if (buffer->q22)
    dmp_queue_response(controller, sel2, (uint16)(buffer->address & 0xFFFF),
                       (uint16)((buffer->address >> 16) & 077), count & 0x3FFF);
else
    dmp_queue_response(controller, sel2, (uint16)(buffer->address & 0xFFFF),
                       (uint16)((((buffer->address >> 16) & 03) << 14) | (count & 0x3FFF)), 0);
ASSURE (insqueue (&buffer->hdr, controller->free_queue->hdr.prev));
}

/* Return the buffers of a halted tributary (Section 3.4.1, Table 3-8):
   its private receive buffers as "receive buffer unused", transmit
   buffers waiting for an acknowledgement as "sent but not acknowledged",
   and queued transmit buffers as "not sent"; then information response 020
   "buffer return complete".  With station 0 (global halt) only the unused
   common pool receive buffers are returned.  A buffer that is on the wire
   right now (speed-limited transmission) stays where it is. */
static void dmp_return_buffers(CTLR *controller, uint8 station)
{
BUFFER_QUEUE *q;
BUFFER *buffer, *next;

for (buffer = dmc_buffer_queue_head(controller->rcv_queue); buffer; buffer = next) {
    next = (buffer->hdr.next == &controller->rcv_queue->hdr) ? NULL : (BUFFER *)buffer->hdr.next;
    if (buffer->station == station)
        dmp_return_buffer(controller, (BUFFER *)remqueue(&buffer->hdr), station, DMP_DISP_RCV_UNUSED);
    }
if (station) {
    q = controller->ack_wait_queue;
    for (buffer = dmc_buffer_queue_head(q); buffer; buffer = next) {
        next = (buffer->hdr.next == &q->hdr) ? NULL : (BUFFER *)buffer->hdr.next;
        if ((buffer->type == TransmitData) && (buffer != controller->link.xmt_buffer))
            dmp_return_buffer(controller, (BUFFER *)remqueue(&buffer->hdr), station, DMP_DISP_XMT_NOACK);
        }
    q = controller->xmt_queue;
    for (buffer = dmc_buffer_queue_head(q); buffer; buffer = next) {
        next = (buffer->hdr.next == &q->hdr) ? NULL : (BUFFER *)buffer->hdr.next;
        if ((buffer->type == TransmitData) && (buffer != controller->link.xmt_buffer))
            dmp_return_buffer(controller, (BUFFER *)remqueue(&buffer->hdr), station, DMP_DISP_XMT_UNSENT);
        }
    }
dmp_info_response(controller, station, 0, DMP_RETKEY_HALT);
}

/* Halt one tributary's protocol. */
static void dmp_halt_trib(CTLR *controller, int slot)
{
uint8 station = controller->dmp_trib[slot].address;

controller->dmp_trib[slot].run = 0;
if (dmp_multipoint(controller)) {
    /* Halt just this tributary's session (the line stays up for the
       others) and return its buffers while its queues are loaded. */
    dmp_session_load(controller, slot);
    controller->state = Halted;
    ddcmp_dispatch(controller, 0);
    dmp_return_buffers(controller, station);
    dmp_session_store(controller);
    return;
    }
controller->state = Halted;
/* Drop DTR before running the protocol machine so that the halted link
   cannot count as starting up again in the same scan. */
dmc_clr_modem_dtr(controller);
ddcmp_dispatch(controller, 0);
dmp_return_buffers(controller, station);
}

/* Common receive pool controls of a control command (BSEL7 bits 0-1,
   Table 3-5), applied after its request key so that they can accompany
   ESTABLISH (VMS sends key 001 with "enable common pool") and START.
   Enable adds BSEL4 to the quota (0377 = unlimited; a sum of 0377 or more
   is error 140 and leaves 0376); disable clears it. */
static void dmp_pool_controls(CTLR *controller, uint8 station, uint16 sel6)
{
int slot;

if (sel6 & (DMP_CTL_POOL_ENABLE|DMP_CTL_POOL_DISABLE)) {
    DMP_TRIB *trib;

    if (!station) {
        dmp_procedural_error(controller, station, DMP_ERR_NEED_TRIB);
        return;
        }
    if ((slot = dmp_find_trib(controller, station)) < 0) {
        dmp_procedural_error(controller, station, DMP_ERR_NOT_ESTAB);
        return;
        }
    trib = &controller->dmp_trib[slot];
    if (sel6 & DMP_CTL_POOL_DISABLE) {
        trib->pool_enabled = FALSE;
        trib->pool_quota = 0;
        }
    if (sel6 & DMP_CTL_POOL_ENABLE) {
        uint16 add = *controller->csrs->sel4 & 0377;

        trib->pool_enabled = TRUE;
        if ((add == DMP_POOL_UNLIMITED) || (trib->pool_quota == DMP_POOL_UNLIMITED))
            trib->pool_quota = DMP_POOL_UNLIMITED;
        else if (trib->pool_quota + add >= DMP_POOL_UNLIMITED) {
            trib->pool_quota = DMP_POOL_UNLIMITED - 1;
            dmp_procedural_error(controller, station, DMP_ERR_QUOTA);
            }
        else
            trib->pool_quota = (uint8)(trib->pool_quota + add);
        }
    trib->tss[DMP_TSS_QUOTA] = (trib->tss[DMP_TSS_QUOTA] & 0xFF00) | trib->pool_quota;
    sim_debug(DBG_INF, controller->device, "%s%d: Tributary %d common pool %s, quota %o\n",
              controller->device->name, controller->index, station,
              trib->pool_enabled ? "enabled" : "disabled", trib->pool_quota);
    }
}

/* Control command (type 1). */
static void dmp_control_command(CTLR *controller, uint8 station)
{
uint16 sel6 = *controller->csrs->sel6;
uint16 key = sel6 & DMP_KEY_MASK;
int slot;

if (!controller->dmp_station_known && (key != DMP_KEY_NOP)) {
    dmp_procedural_error(controller, station, DMP_ERR_NO_MODE);
    return;
    }
if (sel6 & DMP_STATUS_MASK) {
    dmp_status_access(controller, station, sel6);
    return;
    }
/* BSEL7 tributary controls (Table 3-5), allowed with keys 000-004:
   enable the common receive pool with a quota increment in BSEL4 (0377 =
   unlimited, a sum of 0377 or more is error 140 and leaves 0376), disable
   it, or latch/unlatch the polling state (multipoint polling is not
   emulated, so those are accepted and ignored).  Reserved bits: error 134,
   and the command is ignored. */
if (sel6 & DMP_CTL_RESERVED) {
    dmp_procedural_error(controller, station, DMP_ERR_BSEL7);
    return;
    }
if (sel6 & (DMP_CTL_LATCH|DMP_CTL_UNLATCH))
    sim_debug(DBG_INF, controller->device, "%s%d: Tributary %d polling state control 0%o ignored\n",
              controller->device->name, controller->index, station, sel6 >> 8);
switch (key) {
    case DMP_KEY_NOP:
        dmp_pool_controls(controller, station, sel6);
        return;
    case DMP_KEY_ESTABLISH:
        if (!station)
            dmp_procedural_error(controller, station, DMP_ERR_NEED_TRIB);
        else if (dmp_find_trib(controller, station) >= 0)
            dmp_procedural_error(controller, station, DMP_ERR_ESTAB);
        else {
            for (slot = 0; slot < dmp_trib_limit(controller); slot++)
                if (!controller->dmp_trib[slot].in_use)
                    break;
            if (slot == dmp_trib_limit(controller))     /* DMV11 12, DMP11 32 */
                dmp_procedural_error(controller, station, DMP_ERR_TOO_MANY);
            else {
                memset(&controller->dmp_trib[slot], 0, sizeof(controller->dmp_trib[slot]));
                controller->dmp_trib[slot].in_use = TRUE;
                controller->dmp_trib[slot].address = station;
                dmp_tss_defaults(&controller->dmp_trib[slot]);
                if (dmp_multipoint(controller)) {   /* its own DDCMP session */
                    DMP_TRIB *trib = &controller->dmp_trib[slot];

                    dmc_buffer_queue_init(controller, &trib->ses.xmt, "transmit", 0, NULL);
                    dmc_buffer_queue_init(controller, &trib->ses.ackw, "ack wait", 0, NULL);
                    dmc_buffer_queue_init(controller, &trib->ses.rcv, "private receive", 0, NULL);
                    trib->ses.link.state = Halt;
                    trib->ses.state = Initialised;
                    }
                dmp_pool_controls(controller, station, sel6);
                sim_debug(DBG_INF, controller->device, "%s%d: Established tributary %d\n",
                          controller->device->name, controller->index, station);
                }
            }
        return;
    case DMP_KEY_DELETE:
        if (!station)
            dmp_procedural_error(controller, station, DMP_ERR_NEED_TRIB);
        else if ((slot = dmp_find_trib(controller, station)) < 0)
            dmp_procedural_error(controller, station, DMP_ERR_NOT_ESTAB);
        else if (controller->dmp_trib[slot].run)
            dmp_procedural_error(controller, station, DMP_ERR_NOT_HALTED);
        else {
            dmp_session_release(controller, slot);
            controller->dmp_trib[slot].in_use = FALSE;
            }
        return;
    case DMP_KEY_START:
    case DMP_KEY_MAINT:
        if (!station)
            dmp_procedural_error(controller, station, DMP_ERR_NEED_TRIB);
        else if ((slot = dmp_find_trib(controller, station)) < 0)
            dmp_procedural_error(controller, station, DMP_ERR_NOT_ESTAB);
        else if (controller->dmp_trib[slot].run)
            dmp_procedural_error(controller, station, DMP_ERR_NOT_HALTED);
        else {
            t_bool mp = dmp_multipoint(controller);

            controller->dmp_trib[slot].run = (key == DMP_KEY_START) ? 1 : 2;
            if (mp)
                dmp_session_load(controller, slot);
            controller->state = Running;
            if (!dmp_hub_mp(controller))        /* the line itself, shared by all sessions */
                dmc_set_modem_dtr(controller);
            ddcmp_dispatch(controller, (key == DMP_KEY_MAINT) ? DDCMP_EVENT_MAINTMODE : 0);
            if (mp)
                dmp_session_store(controller);
            dmp_pool_controls(controller, station, sel6);
            }
        return;
    case DMP_KEY_HALT:
        /* Table 3-6: halting a tributary returns all of its outstanding
           buffers; a "global" halt (tributary 0) does not halt anything but
           returns the unused common pool buffers, and is error 136 while a
           running tributary may still use the pool.  Either way the returns
           end with information response 020 (Table 3-8). */
        if (!station) {
            for (slot = 0; slot < DMP_MAX_TRIBS; slot++)
                if (controller->dmp_trib[slot].in_use && controller->dmp_trib[slot].run &&
                    controller->dmp_trib[slot].pool_enabled && controller->dmp_trib[slot].pool_quota) {
                    dmp_procedural_error(controller, station, DMP_ERR_POOL_INUSE);
                    return;
                    }
            }
        else if ((slot = dmp_find_trib(controller, station)) < 0) {
            dmp_procedural_error(controller, station, DMP_ERR_NOT_ESTAB);
            return;
            }
        else {
            dmp_halt_trib(controller, slot);    /* also returns its buffers */
            return;
            }
        dmp_return_buffers(controller, station);/* global: the common pool */
        return;
    case DMP_KEY_READ_MODEM:
        dmp_info_response(controller, 0, dmc_get_modem(controller) |
                          ((dmp_hub_mp(controller) && dmc_is_attached(controller->unit)) ? (DMC_SEL4_M_DSR|DMC_SEL4_M_CAR) : 0),
                          DMP_RETKEY_MODEM);
        return;
    case DMP_KEY_WRITE_MODEM:
        {
        uint16 data = *controller->csrs->sel4;  /* only DTR and RTS are outputs */
        int32 set_bits = 0, clear_bits = 0;

        if (data & DMC_SEL4_M_DTR) set_bits |= TMXR_MDM_DTR; else clear_bits |= TMXR_MDM_DTR;
        if (data & DMC_SEL4_M_RTS) set_bits |= TMXR_MDM_RTS; else clear_bits |= TMXR_MDM_RTS;
        tmxr_set_get_modem_bits(controller->line, set_bits, clear_bits, NULL);
        if (set_bits & TMXR_MDM_DTR)
            controller->line->rcve = 1;
        dmc_get_modem(controller);
        }
        return;
    default:
        dmp_procedural_error(controller, station, DMP_ERR_BAD_KEY);
        return;
    }
}

/* Transmit buffer command (type 4): read the message from host memory and
   queue it for the DDCMP session. */
static void dmp_queue_transmit(CTLR *controller, uint8 station, uint32 addr, uint16 count);

static void dmp_transmit_command(CTLR *controller, uint8 station, uint32 addr, uint16 count)
{
int slot;

if (!station) {                             /* transmit buffers need a tributary */
    dmp_procedural_error(controller, station, DMP_ERR_XMT_TRIB0);
    return;
    }
if ((slot = dmp_find_trib(controller, station)) < 0) {
    dmp_procedural_error(controller, station, DMP_ERR_BUF_UNESTAB);
    return;
    }
if (!controller->dmp_trib[slot].run) {
    dmp_procedural_error(controller, station, DMP_ERR_BUF_HALTED);
    return;
    }
if (dmp_multipoint(controller)) {           /* queue it in the tributary's session */
    dmp_session_load(controller, slot);
    dmp_queue_transmit(controller, station, addr, count);
    dmp_session_store(controller);
    }
else
    dmp_queue_transmit(controller, station, addr, count);
}

/* Read a transmit buffer from host memory and queue it for the (loaded)
   DDCMP session, with room for the header in front and the CRC behind. */
static void dmp_queue_transmit(CTLR *controller, uint8 station, uint32 addr, uint16 count)
{
BUFFER *buffer;
int n;

buffer = dmc_buffer_queue_add(controller->xmt_queue, addr, count, TransmitData);
if (!buffer) {
    dmp_control_response(controller, 0, DMP_RSP_OVERFLOW);   /* global: BSEL3 = 0 */
    return;
    }
/* Room for the DDCMP header in front and the data CRC behind. */
buffer->transfer_buffer = (uint8 *)realloc(buffer->transfer_buffer, count + DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE);
memset(buffer->transfer_buffer, 0, DDCMP_HEADER_SIZE);
n = Map_ReadB(addr, count, buffer->transfer_buffer + DDCMP_HEADER_SIZE);
if (n > 0) {                                /* ran into non-existent memory */
    buffer = (BUFFER *)remqueue(&buffer->hdr);
    dmp_buffer_response(controller, station, addr + count - n, buffer->q22, DMP_RSP_NXM);
    free(buffer->transfer_buffer);
    buffer->transfer_buffer = NULL;
    ASSURE(insqueue(&buffer->hdr, controller->free_queue->hdr.prev));
    return;
    }
buffer->count += DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE;
controller->transmit_buffer_input_transfers_completed++;
ddcmp_dispatch(controller, 0);
}

/* A complete DMP/DMV command is in the registers (RDI released). */
void dmp_process_command(CTLR *controller)
{
uint16 sel2 = *controller->csrs->sel2;
uint8 station = (uint8)(sel2 >> DMP_SEL2_V_STATION);
uint32 addr = dmc_get_addr(controller);
uint16 count = dmc_get_count(controller);

switch (controller->transfer_type) {
    case DMP_C_TYPE_MODE:
        {
        static const char *modes[] = {"half-duplex point-to-point", "full-duplex point-to-point",
                                      "half-duplex point-to-point", "full-duplex point-to-point",
                                      "half-duplex control station", "full-duplex control station",
                                      "half-duplex tributary", "full-duplex tributary"};
        uint16 mode = *controller->csrs->sel6 & DMP_TYPE_INPUT_MASK;

        /* Bit 2 = multipoint, bit 1 = tributary, bit 0 = full duplex.  The
           station type can only be changed by a master clear. */
        if (controller->dmp_station_known && (controller->dmp_station_type != mode))
            dmp_procedural_error(controller, station, DMP_ERR_BAD_MODE);
        else {
            controller->dmp_station_type = mode;
            controller->dmp_station_known = TRUE;
            sim_debug(DBG_INF, controller->device, "%s%d: Mode: %s\n",
                      controller->device->name, controller->index, modes[mode]);
            }
        }
        break;
    case DMP_C_TYPE_CNTL:
        dmp_control_command(controller, station);
        break;
    case DMP_C_TYPE_RBACC:
        if (!controller->dmp_station_known)
            dmp_procedural_error(controller, station, DMP_ERR_NO_MODE);
        else if (count == 0)
            dmp_procedural_error(controller, station, DMP_ERR_ZERO_COUNT);
        /* tributary 0 = common buffer pool, always accepted; a buffer for a
           specific tributary needs it established (122) and not halted (124) */
        else if (station && (dmp_find_trib(controller, station) < 0))
            dmp_procedural_error(controller, station, DMP_ERR_BUF_UNESTAB);
        else if (station && !controller->dmp_trib[dmp_find_trib(controller, station)].run)
            dmp_procedural_error(controller, station, DMP_ERR_BUF_HALTED);
        else if (!dmc_buffer_queue_add((dmp_multipoint(controller) && station) ?
                                       &controller->dmp_trib[dmp_find_trib(controller, station)].ses.rcv :
                                       controller->rcv_queue,  /* tributary 0: common pool */
                                       addr, count, Receive))
            dmp_control_response(controller, 0, DMP_RSP_OVERFLOW);   /* global: BSEL3 = 0 */
        else {
            controller->receive_buffer_input_transfers_completed++;
            dmc_buffer_fill_receive_buffers(controller);
            dmp_dispatch(controller, 0);
            }
        break;
    case DMP_C_TYPE_XBACC:
        if (!controller->dmp_station_known)
            dmp_procedural_error(controller, station, DMP_ERR_NO_MODE);
        else if (count == 0)
            dmp_procedural_error(controller, station, DMP_ERR_ZERO_COUNT);
        else
            dmp_transmit_command(controller, station, addr, count);
        break;
    default:
        dmp_procedural_error(controller, station, DMP_ERR_BAD_TYPE);
        break;
    }
}

/* ---------------------------------------------------------------------------
   DMP/DMV multipoint (mode definition bit 2: control station or tributary)

   DDCMP multipoint runs one logical link per tributary address over the
   shared line; every message carries the tributary address in its header
   (byte 5).  The emulation keeps one complete DDCMP session per established
   tributary (protocol variables, transmit and acknowledge queues, private
   receive buffers, run state) in the tributary's slot and swaps it into the
   controller while the unchanged DDCMP engine works on it:

     dmp_session_load()   copy the session into controller->link and point
                          xmt_queue/ack_wait_queue/rcv_queue at its queues
     dmp_session_store()  copy it back and restore the controller's own values

   Sessions are only loaded from the entry points below (service, receive,
   timer and the host's START/HALT/transmit commands), never nested.

   Transport.  A tributary station uses its normal line (PEER/ATTACH) for
   all of its tributary addresses.  A control station either does the same
   (one partner station) or, attached without a PEER, listens on its ATTACH
   port for up to one tributary simulator per tributary status slot ("drop
   lines").  It learns which drop
   line each tributary address answers on; until then (and for MOP
   messages) it sends to every drop line not claimed by another tributary,
   and stations ignore messages that carry an address they do not have.
   A drop line going away is not a modem disconnect at the control station
   (VMS would master clear the whole DMV on 304); that tributary simply
   stops answering, which ends in transmit threshold errors (004).

   Receive buffers given for tributary 0 form the common pool; a message is
   put into a pool buffer when the tributary may use the pool (BSEL7 enable
   and quota), else into one of its private buffers (manual 4.6.2.2).
   --------------------------------------------------------------------------- */

t_bool dmp_multipoint(CTLR *controller)
{
return (!dmc_is_dmc(controller) && controller->dmp_station_known &&
        (controller->dmp_station_type & 4));
}

/* A multipoint control station (mode field: multipoint, not tributary). */
static t_bool dmp_control_station(CTLR *controller)
{
return dmp_multipoint(controller) && !(controller->dmp_station_type & 2);
}

/* Tributaries the controller can establish (tributary status slots): 12 on
   the DMV11 (EK-DMV11-TM-001), 32 on the DMP11.  A listening station accepts
   as many connections. */
int dmp_trib_limit(CTLR *controller)
{
return (controller->device == &dmv_dev) ? DMV_MAX_TRIBS : DMP_MAX_TRIBS;
}

/* A multipoint control station listening for its tributary simulators:
   every drop line of the hub is used, and the line itself is up as long as
   the unit is attached.  Any other listening station (point-to-point, or a
   station before the operating system has set its mode) uses one drop line
   as its line, see dmp_hub_poll(). */
t_bool dmp_hub_mp(CTLR *controller)
{
return (controller->dmp_hub != NULL) && dmp_multipoint(controller);
}

/* The controller's own line descriptor (in the DMP/DMV multiplexer). */
TMLN *dmp_own_line(CTLR *controller)
{
return &dmp_desc.ldsc[controller->unit - controller->device->units];
}

void dmp_session_load(CTLR *controller, int slot)
{
DMP_TRIB *trib = &controller->dmp_trib[slot];

if (controller->dmp_loaded >= 0) {          /* must not happen: keep the loaded one */
    sim_debug(DBG_WRN, controller->device, "%s%d: Session %d still loaded, %d not loaded\n",
              controller->device->name, controller->index, controller->dmp_loaded, slot);
    return;
    }
controller->dmp_own.link = controller->link;
controller->dmp_own.xmt = controller->xmt_queue;
controller->dmp_own.ackw = controller->ack_wait_queue;
controller->dmp_own.rcv = controller->rcv_queue;
controller->dmp_own.state = controller->state;
controller->dmp_own.xmt_errors = controller->dmp_xmt_errors;
controller->link = trib->ses.link;
controller->xmt_queue = &trib->ses.xmt;
controller->ack_wait_queue = &trib->ses.ackw;
controller->rcv_queue = &trib->ses.rcv;
controller->state = trib->ses.state;
controller->dmp_xmt_errors = trib->ses.xmt_errors;
controller->dmp_loaded = slot;
}

void dmp_session_store(CTLR *controller)
{
DMP_TRIB *trib;

if (controller->dmp_loaded < 0)
    return;
trib = &controller->dmp_trib[controller->dmp_loaded];
trib->ses.link = controller->link;
trib->ses.link.rcv_pkt = NULL;              /* points into a line buffer: stale later */
trib->ses.state = controller->state;
trib->ses.xmt_errors = controller->dmp_xmt_errors;
controller->link = controller->dmp_own.link;
controller->xmt_queue = controller->dmp_own.xmt;
controller->ack_wait_queue = controller->dmp_own.ackw;
controller->rcv_queue = controller->dmp_own.rcv;
controller->state = controller->dmp_own.state;
controller->dmp_xmt_errors = controller->dmp_own.xmt_errors;
controller->dmp_loaded = -1;
}

/* Move every buffer of a tributary's session back to the free queue
   (tributary deleted, or the controller is reset). */
static void dmp_session_release(CTLR *controller, int slot)
{
DMP_TRIB *trib = &controller->dmp_trib[slot];
BUFFER_QUEUE *queues[3];
int i;

if (!trib->in_use || !trib->ses.xmt.hdr.next)   /* no session queues set up */
    return;
queues[0] = &trib->ses.xmt; queues[1] = &trib->ses.ackw; queues[2] = &trib->ses.rcv;
for (i = 0; i < 3; i++)
    while (queues[i]->count) {
        BUFFER *buffer = (BUFFER *)remqueue(queues[i]->hdr.next);

        ASSURE(insqueue(&buffer->hdr, controller->free_queue->hdr.prev));
        }
}

/* Run the protocol machine: for every tributary session in multipoint
   (unless one is loaded already), else for the controller's own link. */
void dmp_dispatch(CTLR *controller, uint32 events)
{
int slot;

if (!dmp_multipoint(controller) || (controller->dmp_loaded >= 0)) {
    ddcmp_dispatch(controller, events);
    return;
    }
for (slot = 0; slot < DMP_MAX_TRIBS; slot++)
    if (controller->dmp_trib[slot].in_use) {
        dmp_session_load(controller, slot);
        ddcmp_dispatch(controller, events);
        dmp_session_store(controller);
        }
}

/* Can tributary 'trib' take a data message now (a pool or private buffer)? */
static t_bool dmp_has_receive_buffer(CTLR *controller, DMP_TRIB *trib)
{
return ((trib->ses.rcv.count > 0) ||
        (trib->pool_enabled && trib->pool_quota && (controller->dmp_pool->count > 0)));
}

/* Read from a line only while a tributary heard on it has a receive buffer,
   as the point-to-point code does: otherwise the peer is held off by TCP
   instead of by a stream of "no buffer" NAKs.  A line no tributary has been
   heard on yet is always read (to learn who is there). */
static t_bool dmp_line_may_receive(CTLR *controller, TMLN *lp)
{
int slot;
t_bool bound = FALSE;

for (slot = 0; slot < DMP_MAX_TRIBS; slot++) {
    DMP_TRIB *trib = &controller->dmp_trib[slot];

    if (!trib->in_use || (controller->dmp_hub && (trib->ses.drop != lp)))
        continue;
    bound = TRUE;
    if (dmp_has_receive_buffer(controller, trib))
        return TRUE;
    }
return !bound;
}

/* Pick the receive queue for a message to the loaded tributary (manual
   4.6.2.2): the common pool when the tributary may use it and its next
   buffer is big enough, else the private queue (whose next buffer may be
   too small or missing: the engine answers that with NAK 16 or NAK 8),
   else a too-small pool buffer. */
static void dmp_choose_receive_queue(CTLR *controller, DMP_TRIB *trib, uint16 size)
{
BUFFER *pool = dmc_buffer_queue_head(controller->dmp_pool);
int payload = size - (DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE);
t_bool pool_ok = trib->pool_enabled && trib->pool_quota && pool;

if (pool_ok && (pool->count >= payload))
    controller->rcv_queue = controller->dmp_pool;
else if (trib->ses.rcv.count || !pool_ok)
    controller->rcv_queue = &trib->ses.rcv;
else
    controller->rcv_queue = controller->dmp_pool;
}

/* Receive: read messages from the line(s) and hand each one to the session
   of the tributary address in its header. */
t_bool dmp_mp_receive(CTLR *controller)
{
t_bool ans = FALSE;
int nlines = controller->dmp_hub ? controller->dmp_hub->lines : 1;
int i;

if ((controller->dmp_loaded >= 0) || !dmc_is_attached(controller->unit))
    return FALSE;
for (i = 0; i < nlines; i++) {
    TMLN *lp = controller->dmp_hub ? &controller->dmp_hub->ldsc[i] : controller->line;

    while (lp->conn && dmp_line_may_receive(controller, lp)) {
        const uint8 *pkt = NULL;
        uint16 size = 0;
        int slot;
        DMP_TRIB *trib;
        size_t pool_before;

        ddcmp_tmxr_get_packet_ln (lp, &pkt, &size, *controller->corruption_factor);
        if (!pkt)
            break;
        ans = TRUE;
        controller->buffers_received_from_net++;
        controller->ddcmp_control_packets_received += (pkt[0] == DDCMP_ENQ) ? 1 : 0;
        if ((slot = dmp_find_trib(controller, pkt[5])) < 0) {
            sim_debug(DBG_INF, controller->device, "%s%d: Message for tributary %d ignored (not established here)\n",
                      controller->device->name, controller->index, pkt[5]);
            continue;
            }
        trib = &controller->dmp_trib[slot];
        if (controller->dmp_hub && (trib->ses.drop != lp)) {
            sim_debug(DBG_INF, controller->device, "%s%d: Tributary %d answers on drop line %d\n",
                      controller->device->name, controller->index, trib->address, i);
            trib->ses.drop = lp;
            }
        /* Multipoint line discipline (DDCMP 4.1, 5.3): a selected tributary
           owns the line until it sends a message with the select flag set.
           While it does, the control station's messages to it (e.g. the
           ACKs it waits for) must not select it again.  A tributary that
           runs DDCMP in software (the DMF32 under VMS V3.5) answers extra
           selects with REPs and finally restarts the line ("Line
           synchronization lost"); the emulated DMP11/DMV11 tributaries
           ignore the flag.  See also dmp_mp_send_session(). */
        if ((pkt[0] != DDCMP_DLE) && dmp_control_station(controller)) {
            t_bool holds = !(pkt[2] & (DDCMP_FLAG_SELECT << 6));

            if (holds != trib->ses.holds_select)   /* took or returned the line: */
                trib->ses.select_time = sim_os_msec();  /* selection timer or poll delay */
            trib->ses.holds_select = holds;     /* select flag set: line returned */
            }
        dmp_session_load(controller, slot);
        dmp_choose_receive_queue(controller, trib, size);
        pool_before = controller->dmp_pool->count;
        controller->link.rcv_pkt = (uint8 *)pkt;
        controller->link.rcv_pkt_size = size;
        ddcmp_dispatch(controller, DDCMP_EVENT_PKT_RCVD);
        controller->link.rcv_pkt = NULL;
        if ((controller->dmp_pool->count < pool_before) &&
            (trib->pool_quota != DMP_POOL_UNLIMITED) && trib->pool_quota) {
            --trib->pool_quota;                 /* a pool buffer was used */
            trib->tss[DMP_TSS_QUOTA] = (trib->tss[DMP_TSS_QUOTA] & 0xFF00) | trib->pool_quota;
            }
        dmp_session_store(controller);
        }
    }
return ans;
}

/* Polling (control station): a real DMP11/DMV11 selects its tributaries all
   the time, so that a tributary which may only transmit when selected (VMS
   running DDCMP in software on a DMF32) can send its routing hellos and
   data.  A tributary that has returned the line (or whose selection timer
   expired) and has not been selected for DMP_POLL_INTERVAL gets an ACK with
   the select flag (the DDCMP way to select a station without sending data);
   it owns the line from then on, until it returns it.  Without polling such
   a tributary could only answer right after a message from the control
   station; every couple of hours its hello missed the window and the
   control station's listen timer expired ("adjacency down, listener receive
   timeout").  The emulated DMP11/DMV11 tributaries ignore the select flag. */
static void dmp_mp_poll_tributary(CTLR *controller)
{
DMP_TRIB *trib = &controller->dmp_trib[controller->dmp_loaded];
TMLN *lp = controller->dmp_hub ? trib->ses.drop : controller->line;
uint8 poll[DDCMP_HEADER_SIZE];
uint32 now = sim_os_msec();

if (!dmp_control_station(controller) ||
    (controller->link.state != Run) ||          /* DDCMP running with it */
    controller->link.xmt_buffer ||              /* something is going out anyway */
    dmc_buffer_queue_head(controller->xmt_queue) ||
    trib->ses.holds_select ||                   /* it owns the line */
    ((now - trib->ses.select_time) < DMP_POLL_INTERVAL) ||
    !lp || !lp->conn || tmxr_tpbusyln(lp))
    return;
ddcmp_build_ack_packet (poll, controller->link.R, DDCMP_FLAG_SELECT);
poll[5] = trib->address;
if (ddcmp_tmxr_put_packet_crc_ln (lp, poll, DDCMP_HEADER_SIZE, 0) == SCPE_OK) {
    trib->ses.holds_select = TRUE;              /* selected: the line is its own */
    trib->ses.select_time = now;
    }
}

/* Send what the loaded session has queued, with its tributary address in
   each DDCMP/DMC header (MOP messages keep address 1). */
static void dmp_mp_send_session(CTLR *controller)
{
DMP_TRIB *trib = &controller->dmp_trib[controller->dmp_loaded];
BUFFER *buffer;

if (controller->link.xmt_buffer)
    return;
if (trib->ses.holds_select &&                   /* the tributary owns the line */
    ((sim_os_msec() - trib->ses.select_time) >= DMP_SELECT_TIMER)) {
    sim_debug(DBG_INF, controller->device, "%s%d: Selection timer expired for tributary %d\n",
              controller->device->name, controller->index, trib->address);
    trib->ses.holds_select = FALSE;             /* selection timer: take the line back */
    }
while ((buffer = dmc_buffer_queue_head(controller->xmt_queue)) != NULL) {
    uint8 *msg = buffer->transfer_buffer;
    t_stat r = SCPE_OK;

    if (msg[0] == 0)                            /* not built yet */
        return;
    if ((controller->link.state != Maintenance) && (msg[DDCMP_RESP_OFFSET] != controller->link.R))
        msg[DDCMP_RESP_OFFSET] = controller->link.R;    /* latest acknowledgement */
    if (msg[0] != DDCMP_DLE) {
        msg[5] = trib->address;
        if (dmp_control_station(controller)) {
            if (trib->ses.holds_select)         /* do not select it again */
                msg[2] &= ~(DDCMP_FLAG_SELECT << 6);
            else {                              /* select it: the line is its own */
                msg[2] |= (DDCMP_FLAG_SELECT << 6);
                trib->ses.holds_select = TRUE;
                trib->ses.select_time = sim_os_msec();
                }
            }
        }
    if (!controller->dmp_hub) {                 /* the station's own line */
        if (!controller->line->conn)
            return;
        r = ddcmp_tmxr_put_packet_crc_ln (controller->line, msg, buffer->count, *controller->corruption_factor);
        }
    else if (trib->ses.drop && trib->ses.drop->conn)    /* its drop line */
        r = ddcmp_tmxr_put_packet_crc_ln (trib->ses.drop, msg, buffer->count, *controller->corruption_factor);
    else {                                      /* not heard from yet: every free drop */
        int i, slot;

        for (i = 0; i < controller->dmp_hub->lines; i++) {
            TMLN *lp = &controller->dmp_hub->ldsc[i];
            t_bool claimed = FALSE;

            for (slot = 0; slot < DMP_MAX_TRIBS; slot++)
                if (controller->dmp_trib[slot].in_use && (controller->dmp_trib[slot].ses.drop == lp) &&
                    (&controller->dmp_trib[slot] != trib))
                    claimed = TRUE;
            if (lp->conn && !claimed)
                ddcmp_tmxr_put_packet_crc_ln (lp, msg, buffer->count, *controller->corruption_factor);
            }
        }
    if (r != SCPE_OK)                           /* line busy: try again later */
        return;
    controller->link.xmt_buffer = buffer;
    controller->ddcmp_control_packets_sent += (msg[0] == DDCMP_ENQ) ? 1 : 0;
    dmc_complete_transmit (controller);
    }
dmp_mp_poll_tributary(controller);
}

void dmp_mp_transmit(CTLR *controller)
{
int slot;

if (controller->dmp_loaded >= 0) {
    dmp_mp_send_session(controller);
    return;
    }
for (slot = 0; slot < DMP_MAX_TRIBS; slot++)
    if (controller->dmp_trib[slot].in_use) {
        dmp_session_load(controller, slot);
        dmp_mp_send_session(controller);
        dmp_session_store(controller);
        }
}

/* One-second tick of the reply timers of all sessions. */
void dmp_mp_timers(CTLR *controller, int32 *active)
{
int slot;

for (slot = 0; slot < DMP_MAX_TRIBS; slot++) {
    DMP_TRIB *trib = &controller->dmp_trib[slot];

    if (!trib->in_use || !trib->ses.link.TimerRunning ||
        (trib->ses.link.TimeRemaining <= 0))    /* expired already: wait for a restart */
        continue;
    if (--trib->ses.link.TimeRemaining > 0) {
        ++*active;
        continue;
        }
    dmp_session_load(controller, slot);
    ddcmp_dispatch(controller, DDCMP_EVENT_TIMER);
    dmp_session_store(controller);
    if (trib->ses.link.TimerRunning)
        ++*active;
    }
}

/* Poll the drop multiplexer of a control station hub; returns the number
   of connected drop lines. */
int32 dmp_hub_poll(CTLR *controller)
{
TMXR *hub = controller->dmp_hub;
int32 ln, connected = 0;
int slot;

ln = tmxr_poll_conn(hub);
if (ln >= 0) {
    hub->ldsc[ln].rcve = 1;
    sim_debug(DBG_MDM, controller->device, "%s%d: Drop line %d connected\n",
              controller->device->name, controller->index, ln);
    }
tmxr_poll_rx(hub);
tmxr_poll_tx(hub);
for (ln = 0; ln < hub->lines; ln++)
    if (hub->ldsc[ln].conn)
        ++connected;
for (slot = 0; slot < DMP_MAX_TRIBS; slot++) {
    DMP_TRIB *trib = &controller->dmp_trib[slot];

    if (trib->in_use && trib->ses.drop && !trib->ses.drop->conn) {
        sim_debug(DBG_MDM, controller->device, "%s%d: Drop line of tributary %d disconnected\n",
                  controller->device->name, controller->index, trib->address);
        trib->ses.drop = NULL;
        }
    }
/* A station that is not a multipoint control station uses one drop line
   as its line (controller->line), so that the point-to-point code works on
   it unchanged: the first connected one, and the next one after it went
   away.  Further connections are left unused.  A multipoint control station
   uses all drop lines and keeps its own (unconnected) line. */
if (dmp_multipoint(controller)) {
    if (controller->line != dmp_own_line(controller))
        controller->line = dmp_own_line(controller);
    }
else if ((controller->line == dmp_own_line(controller)) || !controller->line->conn) {
    for (ln = 0; ln < hub->lines; ln++) {
        TMLN *lp = &hub->ldsc[ln];

        if (lp->conn && (lp != controller->line)) {
            int32 dtr = (controller->line->modembits & TMXR_MDM_DTR);

            sim_debug(DBG_MDM, controller->device, "%s%d: Using drop line %d as the line\n",
                      controller->device->name, controller->index, ln);
            controller->line = lp;
            if (dtr)                            /* carry the host's DTR over */
                tmxr_set_get_modem_bits (lp, TMXR_MDM_DTR|TMXR_MDM_RTS, 0, NULL);
            lp->rcve = 1;
            controller->dmp_down_reported = FALSE;
            dmc_get_modem (controller);
            dmp_dispatch (controller, 0);
            break;
            }
        }
    }
return connected;
}

void dmc_process_input_transfer_completion(CTLR *controller)
{
if (dmc_is_dmc(controller)) {
    if (dmc_is_run_set(controller) && 
        (!dmc_is_rqi_set(controller))) {
        /* Time to decode input command arguments */
        uint16 sel6 = *controller->csrs->sel6;
        uint32 addr = dmc_get_addr(controller);
        uint16 count = dmc_get_count(controller);

        controller->transfer_type = dmc_get_input_transfer_type(controller);
        dmc_clear_rdyi(controller);
        switch (controller->transfer_type) {
            case DMC_C_TYPE_BASEIN:
                *controller->baseaddr = dmc_get_addr(controller);
                *controller->basesize = dmc_get_count(controller);
                sim_debug(DBG_INF, controller->device, "%s%d: Completing Base In input transfer, base address=0x%08x count=%d\n", controller->device->name, controller->index, *controller->baseaddr, *controller->basesize);
                break;
            case DMC_C_TYPE_XBACC:
                if (1) {
                    BUFFER *buffer = dmc_buffer_queue_add(controller->xmt_queue, addr, count, TransmitData);
                    int n;

                    /* construct buffer - leaving room for DDCMP header and CRCs */
                    buffer->transfer_buffer = (uint8 *)realloc (buffer->transfer_buffer, buffer->count + DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE);
                    memset (buffer->transfer_buffer, 0, DDCMP_HEADER_SIZE);
                    n = Map_ReadB (buffer->address, buffer->count, buffer->transfer_buffer + DDCMP_HEADER_SIZE);
                    if (n > 0) {
                        sim_debug(DBG_WRN, controller->device, "%s%d: DMA error\n", controller->device->name, controller->index);
                        }
                    buffer->count += DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE;
                    controller->transmit_buffer_input_transfers_completed++;
                    }
                if (controller->state != Running) {
                    controller->state = Running;
                    dmc_set_modem_dtr (controller);
                    }
                ddcmp_dispatch (controller, 0);
                break;
            case DMC_C_TYPE_RBACC:
                dmc_buffer_queue_add(controller->rcv_queue, addr, count, Receive);
                dmc_buffer_fill_receive_buffers(controller);
                controller->receive_buffer_input_transfers_completed++;
                ddcmp_dispatch (controller, 0);
                break;
            case DMC_C_TYPE_CNTL:
                sim_debug(DBG_INF, controller->device, "%s%d: Control In Command Received(%s MODE%s%s)\n", 
                                                       controller->device->name, controller->index,
                                                       (sel6&DMC_SEL6_M_MAINT) ? "MAINTENANCE" : "DDCMP", 
                                                       (sel6&DMC_SEL6_M_HDX) ? ", Half Duples" : "", 
                                                       (sel6&DMC_SEL6_M_LONGSTRT) ? ", Long Start Timer" : "");
                dmc_set_modem_dtr (controller);
                controller->transfer_state = Idle;
                ddcmp_dispatch (controller, (sel6&DMC_SEL6_M_MAINT) ? DDCMP_EVENT_MAINTMODE : 0);
                return;
            case DMC_C_TYPE_HALT:
                sim_debug(DBG_INF, controller->device, "%s%d: Halt Command Received\n", controller->device->name, controller->index);
                controller->state = Halted;
                ddcmp_dispatch(controller, 0);
                dmc_clr_modem_dtr(controller);
                dmc_queue_control_out(controller, DMC_SEL6_M_HALTCOMP);
                return;
            }
        controller->transfer_state = Idle;
        dmc_process_command (controller);
        }
    }
else {  /* DMP/DMV */
    /* The host loads BSEL2-BSEL11 while RDI is set and releases the command
       by clearing RDI; only then are the registers complete. */
    if (!dmc_is_rdyi_set(controller)) {
        controller->transfer_type = dmc_get_input_transfer_type(controller);
        dmp_process_command(controller);
        controller->transfer_state = Idle;
        dmc_process_command(controller);            /* start any pending output */
        }
    }
}

typedef enum {
    EQ,
    NE,
    LT,
    LE,
    GT,
    GE
} CompareOP;

/* DDCMP Modulo 256 compare - from Rob Jarratt */
static int Mod256Cmp(uint8 a, uint8 b, size_t queue_size)
{
int ans;
int abdiff = (int)b - (int)a;
int badiff = (int)a - (int)b;
 
if (abdiff == 0)
    ans = 0;
else 
    if (abdiff < 0)
        ans = -1;
    else
        ans = 1;
 
if (abs(badiff) <= (int)queue_size)
    ans = -1 * ans;
 
return ans;
}

t_bool ddcmp_compare (uint8 a, CompareOP Op, uint8 b, CTLR *controller)
{
/* The window for modulo 256 sequence number comparison is twice the
   device's queue size: the size the buffer pool always had.  It is
   computed here because the DMP/DMV pool is larger (one set of queues per
   multipoint tributary), and a window of 256 or more would make every
   comparison ignore the wrap-around of the message numbers. */
size_t window = 2 * ((controller->dev_type == DMC) ? DMC_QUEUE_SIZE :
                     ((controller->dev_type == DMR) ? DMR_QUEUE_SIZE : DMP_QUEUE_SIZE));
int cmp = Mod256Cmp(a & 0xFF, b & 0xFF, window);

switch (Op) {
    case EQ:
        return (cmp == 0);
    case NE:
        return (cmp != 0);
    case LE:
        if (cmp == 0)
            return TRUE;
    case LT:
        return (cmp < 0);
    case GE:
        if (cmp == 0)
            return TRUE;
    case GT:
        return (cmp > 0);
    default:    /* Never happens */
        return FALSE;
    }
}

void ddcmp_StartTimer             (CTLR *controller)
{
controller->link.TimerRunning = TRUE;
controller->link.TimeRemaining = DDCMP_PACKET_TIMEOUT;
sim_activate_after (controller->device->units+(controller->device->numunits-1), controller->link.TimeRemaining*1000000);
}
void ddcmp_StopTimer              (CTLR *controller)
{
controller->link.TimerRunning = FALSE;
}
void ddcmp_ResetVariables         (CTLR *controller)
{
size_t data_packets = 0;
BUFFER *buffer;

controller->link.R = 0;
controller->link.N = 0;
controller->link.A = 0;
controller->link.T = 1;
controller->link.X = 0;
controller->link.SACK = FALSE;
controller->link.SNAK = FALSE;
controller->link.SREP = FALSE;
controller->link.state = Halt;
controller->link.nak_reason = 0;
/* Move any ack wait packets back to the transmit queue so they get 
   resent when the link is restored */
while (controller->ack_wait_queue->count) {
    buffer = (BUFFER *)remqueue (controller->ack_wait_queue->hdr.prev);
    memset (buffer->transfer_buffer, 0, DDCMP_HEADER_SIZE);
    ASSURE (insqueue (&buffer->hdr, &controller->xmt_queue->hdr));
    }
/* Also make sure that the transmit queue has no control packets in it 
   and that any non transmit buffer(s) have zeroed headers so they will
   be properly constructed when the link comes up */
buffer = (BUFFER *)controller->xmt_queue->hdr.next;
while (controller->xmt_queue->count - data_packets) {
    if (((BUFFER *)controller->xmt_queue->hdr.next)->transfer_buffer[0] == DDCMP_ENQ) {
        BUFFER *buffer_next = (BUFFER *)buffer->hdr.next;

        buffer = (BUFFER *)remqueue (&buffer->hdr);
        ASSURE (insqueue (&buffer->hdr, &controller->free_queue->hdr));
        buffer = buffer_next;
        continue;
        }
    ++data_packets;
    memset (buffer->transfer_buffer, 0, DDCMP_HEADER_SIZE);
    buffer = (BUFFER *)buffer->hdr.next;
    }
}
void ddcmp_SendStrt               (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_allocate(controller);

buffer->transfer_buffer = (uint8 *)malloc (DDCMP_HEADER_SIZE);
buffer->count = DDCMP_HEADER_SIZE;
ddcmp_build_start_packet (buffer->transfer_buffer);
ASSURE (insqueue (&buffer->hdr, &controller->xmt_queue->hdr));
dmc_ddcmp_start_transmitter (controller);
}
void ddcmp_SendStack              (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_allocate(controller);

buffer->transfer_buffer = (uint8 *)malloc (DDCMP_HEADER_SIZE);
buffer->count = DDCMP_HEADER_SIZE;
ddcmp_build_start_ack_packet (buffer->transfer_buffer);
ASSURE (insqueue (&buffer->hdr, &controller->xmt_queue->hdr));
dmc_ddcmp_start_transmitter (controller);
}
void ddcmp_SendAck                (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_allocate(controller);

buffer->transfer_buffer = (uint8 *)malloc (DDCMP_HEADER_SIZE);
buffer->count = DDCMP_HEADER_SIZE;
ddcmp_build_ack_packet (buffer->transfer_buffer, controller->link.R, DDCMP_FLAG_SELECT);
ASSURE (insqueue (&buffer->hdr, &controller->xmt_queue->hdr));
dmc_ddcmp_start_transmitter (controller);
}
void ddcmp_SendNak                (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_allocate(controller);

buffer->transfer_buffer = (uint8 *)malloc (DDCMP_HEADER_SIZE);
buffer->count = DDCMP_HEADER_SIZE;
ddcmp_build_nak_packet (buffer->transfer_buffer, controller->link.nak_reason, controller->link.R, DDCMP_FLAG_SELECT);
ASSURE (insqueue (&buffer->hdr, &controller->xmt_queue->hdr));
dmc_ddcmp_start_transmitter (controller);
}
void ddcmp_SendRep                (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_allocate(controller);

buffer->transfer_buffer = (uint8 *)malloc (DDCMP_HEADER_SIZE);
buffer->count = DDCMP_HEADER_SIZE;
ddcmp_build_rep_packet (buffer->transfer_buffer, controller->link.N, DDCMP_FLAG_SELECT);
dmp_count_event(controller, DMP_TSS_REPLY_TO, TRUE);      /* DMP/DMV: TSS 017 low byte */
ASSURE (insqueue (&buffer->hdr, &controller->xmt_queue->hdr));
dmc_ddcmp_start_transmitter (controller);
}
void ddcmp_SetSACK                (CTLR *controller)
{
controller->link.SACK = TRUE;
}
void ddcmp_ClearSACK              (CTLR *controller)
{
controller->link.SACK = FALSE;
}
void ddcmp_SetSNAK                (CTLR *controller)
{
controller->link.SNAK = TRUE;
}
void ddcmp_ClearSNAK              (CTLR *controller)
{
controller->link.SNAK = FALSE;
}
/* A transmit error for output code 004: the STRT, STACK or data message
   timer expired (rules 7, 16, 34).  Seven in a row are reported to a DMP or
   DMV host as a transmit threshold error.  The count restarts after each
   report, except while the link stays in ISTRT or ASTRT (Table 3-7), and
   whenever a message is acknowledged or the link reaches Run.  DMC/DMR
   hosts have no such response, so this does nothing for them. */
void ddcmp_CountTransmitError     (CTLR *controller)
{
if (dmc_is_dmc(controller))
    return;
if (controller->dmp_xmt_errors < DMP_XMT_THRESHOLD)
    ++controller->dmp_xmt_errors;
{
uint16 *tss = dmp_session_tss(controller);

if (tss)                                    /* TSS 06 low byte: threshold count */
    tss[DMP_TSS_THRESH] = (tss[DMP_TSS_THRESH] & 0xFF00) | controller->dmp_xmt_errors;
}
if (controller->dmp_xmt_errors >= DMP_XMT_THRESHOLD) {
    dmp_control_response(controller, dmp_link_station(controller), DMP_RSP_XMT_THRESH);
    if (controller->link.state == Run)
        controller->dmp_xmt_errors = 0;
    }
}
void ddcmp_SetSREP                (CTLR *controller)
{
controller->link.SREP = TRUE;
}
void ddcmp_ClearSREP              (CTLR *controller)
{
controller->link.SREP = FALSE;
}
void ddcmp_IncrementR             (CTLR *controller)
{
controller->link.R++;
}
void ddcmp_SetAeqRESP             (CTLR *controller)
{
controller->link.A = controller->link.rcv_pkt[DDCMP_RESP_OFFSET];
}
void ddcmp_SetTequalAplus1        (CTLR *controller)
{
controller->link.T = controller->link.A + 1;
}
void ddcmp_IncrementT             (CTLR *controller)
{
controller->link.T++;
}
void ddcmp_SetNAKreason3          (CTLR *controller)
{
controller->link.nak_reason = 3;
}
void ddcmp_SetNAKReasonCRCError   (CTLR *controller)
{
controller->link.nak_reason = controller->link.nak_crc_reason;
}
void ddcmp_NAKMissingPacket       (CTLR *controller)
{
uint8 R = controller->link.R;
QH *qh = &controller->xmt_queue->hdr;

while (ddcmp_compare (controller->link.rcv_pkt[DDCMP_NUM_OFFSET], GE, R, controller)) {
    BUFFER *buffer = dmc_buffer_allocate(controller);

    if (NULL == buffer) {
        sim_debug(DBG_INF, controller->device, "%s%d: No Buffers cause NAKMissingPackets to stop %s\n", controller->device->name, controller->index, controller_queue_state(controller));
        break;
        }
    if (ddcmp_compare (controller->link.rcv_pkt[DDCMP_NUM_OFFSET], GE, controller->link.NAKed, controller)) {
        sim_debug(DBG_INF, controller->device, "%s%d: NAK for prior missing packet %d already sent, still waiting %s\n", controller->device->name, controller->index, controller->link.NAKed, controller_queue_state(controller));
        ASSURE (insqueue (&buffer->hdr, &controller->free_queue->hdr)); /* Release buffer */
        break;
        }
    buffer->transfer_buffer = (uint8 *)malloc (DDCMP_HEADER_SIZE);
    buffer->count = DDCMP_HEADER_SIZE;
    ddcmp_build_nak_packet (buffer->transfer_buffer, 2, R, DDCMP_FLAG_SELECT);
    controller->link.NAKed = R;
    R = R + 1;
    ASSURE (insqueue (&buffer->hdr, qh));
    qh = &buffer->hdr;
    break; /* Only generate a single NAK */
    }
dmc_ddcmp_start_transmitter (controller);
}
void ddcmp_IfTleAthenSetTeqAplus1 (CTLR *controller)
{
if (ddcmp_compare (controller->link.T, LE, controller->link.A, controller))
    controller->link.T = controller->link.A + 1;
}
void ddcmp_IfAltXthenStartTimer   (CTLR *controller)
{
if (ddcmp_compare (controller->link.A, LT, controller->link.X, controller))
    ddcmp_StartTimer (controller);
}
void ddcmp_IfAgeXthenStopTimer    (CTLR *controller)
{
if (ddcmp_compare (controller->link.A, GE, controller->link.X, controller))
    ddcmp_StopTimer (controller);
}
void ddcmp_Ignore                 (CTLR *controller)
{
}
void ddcmp_GiveBufferToUser       (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_queue_head(controller->rcv_queue);
int payload = controller->link.rcv_pkt_size - (DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE);
int remaining;

if (!buffer) {
    ddcmp_SetSNAK (controller);
    controller->link.nak_reason = 8;    /* buffer temporarily unavailable */
    return;
    }
if (!dmc_is_dmc(controller) && (payload > buffer->count)) {
    /* DMP/DMV: the message does not fit.  Report "buffer too small", keep
       the buffer, and NAK (reason 16) so the peer sends it again. */
    dmp_buffer_response(controller, dmp_buffer_station(controller, buffer), buffer->address,
                        buffer->q22, DMP_RSP_BUF_SMALL);
    ddcmp_SetSNAK (controller);
    controller->link.nak_reason = DDCMP_NAK_MSG_TOO_LONG; /* message longer than the buffer */
    return;
    }
buffer = (BUFFER *)remqueue (controller->rcv_queue->hdr.next);
buffer->actual_bytes_transferred = payload;
if (!dmc_is_dmc(controller) && (buffer->station == 0))
    buffer->station = dmp_link_station(controller); /* common pool: report the receiving tributary */
remaining = Map_WriteB(buffer->address, buffer->actual_bytes_transferred, controller->link.rcv_pkt + DDCMP_HEADER_SIZE);
if ((remaining > 0) && !dmc_is_dmc(controller)) {
    /* DMP/DMV: the buffer runs into non-existent memory. */
    dmp_buffer_response(controller, dmp_buffer_station(controller, buffer),
                        buffer->address + buffer->actual_bytes_transferred - remaining,
                        buffer->q22, DMP_RSP_NXM);
    ASSURE (insqueue (&buffer->hdr, controller->free_queue->hdr.prev));
    ddcmp_SetSNAK (controller);
    controller->link.nak_reason = 8;    /* treat as no buffer: the peer resends */
    return;
    }
ASSURE (insqueue (&buffer->hdr, controller->completion_queue->hdr.prev));     /* Insert at tail */
dmp_count_event(controller, DMP_TSS_RCV_MSGS, FALSE);     /* DMP/DMV: TSS 010 */
ddcmp_IncrementR (controller);
ddcmp_SetSACK (controller);
}
void ddcmp_CompleteAckedTransmits (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_queue_head(controller->ack_wait_queue);

controller->dmp_xmt_errors = 0;             /* acknowledged: errors no longer consecutive */

while (buffer != NULL) {
    if ((!buffer->transfer_buffer) || 
        (!controller->link.rcv_pkt) ||
        ddcmp_compare (buffer->transfer_buffer[DDCMP_NUM_OFFSET], GT, controller->link.rcv_pkt[DDCMP_RESP_OFFSET], controller))
        break;
    buffer = (BUFFER *)remqueue (&buffer->hdr);
    ASSURE (insqueue (&buffer->hdr, controller->completion_queue->hdr.prev)); /* Insert at tail */
    buffer = dmc_buffer_queue_head(controller->ack_wait_queue);
    }
}
void ddcmp_ReTransmitMessageT (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_queue_head(controller->ack_wait_queue);
size_t i;
uint8 T = controller->link.T;

for (i=0; i < controller->ack_wait_queue->count; ++i) {
    if ((!buffer->transfer_buffer) || 
        ddcmp_compare (buffer->transfer_buffer[DDCMP_NUM_OFFSET], NE, T, controller)) {
        buffer = (BUFFER *)buffer->hdr.next;
        continue;
        }
    ddcmp_build_data_packet (buffer->transfer_buffer, buffer->count - (DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE), DDCMP_FLAG_SELECT|DDCMP_FLAG_QSYNC, T, controller->link.R);
    buffer = (BUFFER *)remqueue (&buffer->hdr);
    ASSURE (insqueue (&buffer->hdr, controller->xmt_queue->hdr.prev)); /* Insert at tail */
    break;
    }
}
void ddcmp_NotifyDisconnect       (CTLR *controller)
{
dmc_queue_control_out(controller, DMC_SEL6_M_DISCONN);
}
void ddcmp_NotifyStartRcvd        (CTLR *controller)
{
dmc_queue_control_out(controller, DMC_SEL6_M_STRTRCVD);
}
void ddcmp_NotifyMaintRcvd        (CTLR *controller)
{
dmc_queue_control_out(controller, DMC_SEL6_M_MAINTRCV);
}
void ddcmp_SendDataMessage        (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_queue_head(controller->xmt_queue);

ddcmp_build_data_packet (buffer->transfer_buffer, buffer->count - (DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE), DDCMP_FLAG_SELECT|DDCMP_FLAG_QSYNC, controller->link.N + 1, controller->link.R);
controller->link.N += 1;
controller->link.T = controller->link.N + 1;
controller->link.SACK = 0;
dmp_count_event(controller, DMP_TSS_XMT_MSGS, FALSE);     /* DMP/DMV: TSS 07 */
dmc_ddcmp_start_transmitter (controller);
}
void ddcmp_SendMaintMessage       (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_queue_head(controller->xmt_queue);

ddcmp_build_maintenance_packet (buffer->transfer_buffer, buffer->count - (DDCMP_HEADER_SIZE + DDCMP_CRC_SIZE));
dmc_ddcmp_start_transmitter (controller);
}
void ddcmp_SetXSetNUM             (CTLR *controller)
{
controller->link.X = controller->link.xmt_done_buffer->transfer_buffer[DDCMP_NUM_OFFSET];
}

/* Conditions/Events */

t_bool ddcmp_UserHalt             (CTLR *controller)
{
return (controller->state == Halted);
}
t_bool ddcmp_UserStartup          (CTLR *controller)
{
/* DMP/DMV: the protocol is started only by control request key 003 (START),
   which sets the controller Running.  DTR alone is not enough: the host can
   raise DTR with a modem write while the tributary is still halted, and a
   halted link that also counted as "starting" would be halted and restarted
   by the same scan over and over. */
if (!dmc_is_dmc(controller) && (controller->state != Running))
    return FALSE;
if (dmp_hub_mp(controller))                 /* hub: no modem on the drop multiplexer */
    return TRUE;
return (*controller->modem & DMC_SEL4_M_DTR);
}
t_bool ddcmp_UserMaintenanceMode  (CTLR *controller)
{
return (controller->link.ScanningEvents & DDCMP_EVENT_MAINTMODE);
}
t_bool ddcmp_ReceiveStack         (CTLR *controller)
{
return ((controller->link.ScanningEvents & DDCMP_EVENT_PKT_RCVD) &&
        controller->link.rcv_pkt &&
        (controller->link.rcv_pkt[0] == DDCMP_ENQ) &&
        (controller->link.rcv_pkt[1] == DDCMP_CTL_STACK) &&
        (!ddcmp_ReceiveMessageError(controller)));
}
t_bool ddcmp_ReceiveStrt          (CTLR *controller)
{
return ((controller->link.ScanningEvents & DDCMP_EVENT_PKT_RCVD) &&
        controller->link.rcv_pkt &&
        (controller->link.rcv_pkt[0] == DDCMP_ENQ) &&
        (controller->link.rcv_pkt[1] == DDCMP_CTL_STRT) &&
        (!ddcmp_ReceiveMessageError(controller)));
}
t_bool ddcmp_TimerRunning         (CTLR *controller)
{
return (controller->link.TimerRunning);
}
t_bool ddcmp_TimerNotRunning      (CTLR *controller)
{
return (!controller->link.TimerRunning);
}
t_bool ddcmp_TimerExpired         (CTLR *controller)
{
return (controller->link.ScanningEvents & DDCMP_EVENT_TIMER);
}
t_bool ddcmp_ReceiveMaintMessage  (CTLR *controller)
{
return ((controller->link.ScanningEvents & DDCMP_EVENT_PKT_RCVD) && 
        controller->link.rcv_pkt &&
        (controller->link.rcv_pkt[0] == DDCMP_DLE) && 
        (!ddcmp_ReceiveMessageError(controller)));
}
t_bool ddcmp_ReceiveAck           (CTLR *controller)
{
return ((controller->link.ScanningEvents & DDCMP_EVENT_PKT_RCVD) &&
        controller->link.rcv_pkt &&
        (controller->link.rcv_pkt[0] == DDCMP_ENQ) &&
        (controller->link.rcv_pkt[1] == DDCMP_CTL_ACK) &&
        (!ddcmp_ReceiveMessageError(controller)));
}
t_bool ddcmp_ReceiveNak           (CTLR *controller)
{
return ((controller->link.ScanningEvents & DDCMP_EVENT_PKT_RCVD) &&
        controller->link.rcv_pkt &&
        (controller->link.rcv_pkt[0] == DDCMP_ENQ) &&
        (controller->link.rcv_pkt[1] == DDCMP_CTL_NAK) &&
        (!ddcmp_ReceiveMessageError(controller)));
}
t_bool ddcmp_ReceiveRep           (CTLR *controller)
{
return ((controller->link.ScanningEvents & DDCMP_EVENT_PKT_RCVD) &&
        controller->link.rcv_pkt &&
        (controller->link.rcv_pkt[0] == DDCMP_ENQ) &&
        (controller->link.rcv_pkt[1] == DDCMP_CTL_REP) &&
        (!ddcmp_ReceiveMessageError(controller)));
}
t_bool ddcmp_NUMEqRplus1          (CTLR *controller)
{
t_bool breturn = (controller->link.rcv_pkt &&
                  ddcmp_compare (controller->link.rcv_pkt[DDCMP_NUM_OFFSET], EQ, controller->link.R + 1, controller));
return breturn;
}
t_bool ddcmp_NUMGtRplus1          (CTLR *controller)
{
t_bool breturn = (controller->link.rcv_pkt &&
                  ddcmp_compare (controller->link.rcv_pkt[DDCMP_NUM_OFFSET], GT, controller->link.R + 1, controller));
return breturn;
}
t_bool ddcmp_ReceiveDataMsg       (CTLR *controller)
{
t_bool breturn = ((controller->link.ScanningEvents & DDCMP_EVENT_PKT_RCVD) &&
                  controller->link.rcv_pkt &&
                  (controller->link.rcv_pkt[0] == DDCMP_SOH) &&
                  (!ddcmp_ReceiveMessageError(controller)));
if (breturn)
    return breturn;
return breturn;
}
t_bool ddcmp_ReceiveMaintMsg      (CTLR *controller)
{
t_bool breturn = ((controller->link.ScanningEvents & DDCMP_EVENT_PKT_RCVD) &&
                  controller->link.rcv_pkt &&
                  (controller->link.rcv_pkt[0] == DDCMP_DLE) &&
                  (!ddcmp_ReceiveMessageError(controller)));
if (breturn)
    return breturn;
return breturn;
}
t_bool ddcmp_ALtRESPleN           (CTLR *controller)
{
return (controller->link.rcv_pkt &&
        ddcmp_compare (controller->link.A, LT, controller->link.rcv_pkt[DDCMP_RESP_OFFSET], controller) &&
        ddcmp_compare (controller->link.rcv_pkt[DDCMP_RESP_OFFSET], LE, controller->link.N, controller));
}
t_bool ddcmp_ALeRESPleN           (CTLR *controller)
{
return (controller->link.rcv_pkt &&
        ddcmp_compare (controller->link.A, LE, controller->link.rcv_pkt[DDCMP_RESP_OFFSET], controller) &&
        ddcmp_compare (controller->link.rcv_pkt[DDCMP_RESP_OFFSET], LE, controller->link.N, controller));
}
t_bool ddcmp_RESPleAOrRESPgtN     (CTLR *controller)
{
return (controller->link.rcv_pkt &&
        (ddcmp_compare (controller->link.rcv_pkt[DDCMP_RESP_OFFSET], LE, controller->link.A, controller) ||
         ddcmp_compare (controller->link.rcv_pkt[DDCMP_RESP_OFFSET], GT, controller->link.N, controller)));
}
t_bool ddcmp_TltNplus1            (CTLR *controller)
{
return (ddcmp_compare (controller->link.T, LT, controller->link.N + 1, controller));
}
t_bool ddcmp_TeqNplus1            (CTLR *controller)
{
return (ddcmp_compare (controller->link.T, EQ, controller->link.N + 1, controller));
}
t_bool ddcmp_ReceiveMessageError  (CTLR *controller)
{
if ((controller->link.ScanningEvents & DDCMP_EVENT_PKT_RCVD) && controller->link.rcv_pkt) {
    if (0 != ddcmp_crc16 (0, controller->link.rcv_pkt, 8)) {
        controller->link.nak_crc_reason = 1;    /* Header CRC Error */
        return TRUE;
        }
    if ((controller->link.rcv_pkt[0] != DDCMP_ENQ) &&
        (0 != ddcmp_crc16 (0, controller->link.rcv_pkt+8, controller->link.rcv_pkt_size-8))) {
        controller->link.nak_crc_reason = 2;    /* Data CRC Error */
        return TRUE;
        }
    controller->link.nak_crc_reason = 0;        /* No CRC Error */
    }
return FALSE;
}
t_bool ddcmp_NumEqR               (CTLR *controller)
{
return (controller->link.rcv_pkt &&
        ddcmp_compare (controller->link.rcv_pkt[DDCMP_NUM_OFFSET], EQ, controller->link.R, controller));
}
t_bool ddcmp_NumNeR               (CTLR *controller)
{
return (controller->link.rcv_pkt &&
        ddcmp_compare (controller->link.rcv_pkt[DDCMP_NUM_OFFSET], NE, controller->link.R, controller));
}
t_bool ddcmp_TransmitterIdle      (CTLR *controller)
{
return (NULL == controller->link.xmt_buffer);
}
t_bool ddcmp_TramsmitterBusy      (CTLR *controller)
{
return !ddcmp_TransmitterIdle(controller);
}
t_bool ddcmp_SACKisSet            (CTLR *controller)
{
return (controller->link.SACK);
}
t_bool ddcmp_SACKisClear          (CTLR *controller)
{
return (!(controller->link.SACK));
}
t_bool ddcmp_SNAKisSet            (CTLR *controller)
{
return (controller->link.SNAK);
}
t_bool ddcmp_SNAKisClear          (CTLR *controller)
{
return (!(controller->link.SNAK));
}
t_bool ddcmp_SREPisSet            (CTLR *controller)
{
return (controller->link.SREP);
}
t_bool ddcmp_SREPisClear          (CTLR *controller)
{
return (!(controller->link.SREP));
}
t_bool ddcmp_UserSendMessage      (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_queue_head(controller->xmt_queue);

return (buffer && (buffer->transfer_buffer[0] == 0));
}
t_bool ddcmp_LineConnected        (CTLR *controller)
{
if (dmp_hub_mp(controller))                 /* DMP/DMV control station hub */
    return dmc_is_attached(controller->unit);
return (*controller->modem & DMC_SEL4_M_CAR);
}
t_bool ddcmp_LineDisconnected     (CTLR *controller)
{
if (dmp_hub_mp(controller))                 /* DMP/DMV control station hub */
    return !dmc_is_attached(controller->unit);
return (!(*controller->modem & DMC_SEL4_M_CAR));
}
t_bool ddcmp_DataMessageSent      (CTLR *controller)
{
t_bool breturn = ((controller->link.ScanningEvents & DDCMP_EVENT_XMIT_DONE) &&
                    controller->link.xmt_done_buffer && 
                    (controller->link.xmt_done_buffer->transfer_buffer[0] == DDCMP_SOH));
if (breturn)
    return breturn;
return breturn;
}
t_bool ddcmp_REPMessageSent       (CTLR *controller)
{
return ((controller->link.ScanningEvents & DDCMP_EVENT_XMIT_DONE) &&
        controller->link.xmt_done_buffer && 
        (controller->link.xmt_done_buffer->transfer_buffer[0] == DDCMP_ENQ) && 
        (controller->link.xmt_done_buffer->transfer_buffer[1] == DDCMP_CTL_REP));
}

void ddcmp_dispatch(CTLR *controller, uint32 EventMask)
{
DDCMP_STATETABLE *table;
int matched = 0;
static const char *states[] = {"Halt", "IStart", "AStart", "Run", "Maintenance", "All"};

if (controller->link.Scanning) {
    if (!controller->link.RecurseScan) {
        controller->link.RecurseScan = TRUE;
        controller->link.RecurseEventMask |= EventMask;
        }
    return;
    }
controller->link.Scanning = TRUE;
controller->link.ScanningEvents |= EventMask;
for (table=DDCMP_TABLE; table->Conditions[0] != NULL; ++table) {
    if ((table->State == controller->link.state) ||
        (table->State == All)) {
        t_bool match = TRUE;
        DDCMP_Condition_Routine *cond = table->Conditions;
        DDCMP_LinkAction_Routine *action = table->Actions;

        while (match && *cond != NULL) {
            match = (*cond)(controller);
            ++cond;
            }
        if (!match)
            continue;
        ++matched;
        sim_debug (DBG_INF, controller->device, "%s%d: ddcmp_dispatch(%X) - %s conditions matching for rule %2d(%s)%s,\n"
                                                "                                           initiating actions (%s)\n", controller->device->name, controller->index, EventMask, states[table->State], table->RuleNumber, ddcmp_conditions(table->Conditions), ddcmp_link_state(&controller->link), ddcmp_actions(table->Actions));
        while (*action != NULL) {
            (*action)(controller);
            ++action;
            }
        if (table->NewState != controller->link.state) {
            sim_debug (DBG_INF, controller->device, "%s%d: ddcmp_dispatch(%X) - state changing from %s to %s\n", controller->device->name, controller->index, EventMask, states[controller->link.state], states[table->NewState]);
            if ((table->NewState == Run) && !dmc_is_dmc(controller))
                dmp_link_running(controller);       /* DMP/DMV: "DDCMP running" response */
            if ((table->NewState == IStart) || (table->NewState == AStart))
                controller->dmp_xmt_errors = 0;     /* DMP/DMV: threshold count restarts (5.3.3.1) */
            controller->link.state = table->NewState;
            }
        }
    }
if (matched) {
    sim_debug (DBG_INF, controller->device, "%s%d: ddcmp_dispatch(%X) - queues: %s\n", controller->device->name, controller->index, EventMask, controller_queue_state(controller));
    }
controller->link.Scanning = FALSE;
controller->link.ScanningEvents &= ~EventMask;
if (controller->link.RecurseScan) {
    controller->link.RecurseScan = FALSE;
    EventMask = controller->link.RecurseEventMask;
    controller->link.RecurseEventMask = 0;
    ddcmp_dispatch (controller, EventMask);
    dmc_svc (controller->unit);
    }
}


void dmc_ddcmp_start_transmitter (CTLR *controller)
{
BUFFER *buffer = dmc_buffer_queue_head(controller->xmt_queue);
t_stat r;

if (dmp_multipoint(controller)) {           /* per tributary session and drop line */
    dmp_mp_transmit(controller);
    return;
    }

if ((controller->link.xmt_buffer) ||        /* if Already Transmitting */
    (!buffer) ||                            /*    or nothing pending */
    (!controller->line->conn))              /*    or not connected */
    return;                                 /* Do nothing */
while (buffer) {
    if (buffer->transfer_buffer[0] == 0)
        return;
    if ((controller->link.state != Maintenance) && (buffer->transfer_buffer[DDCMP_RESP_OFFSET] != controller->link.R)) {
        sim_debug(DBG_INF, controller->device, "%s%d: Packet RESP fixup from %d to %d %s\n", controller->device->name, controller->index, buffer->transfer_buffer[DDCMP_RESP_OFFSET], controller->link.R, controller_queue_state(controller));
        buffer->transfer_buffer[DDCMP_RESP_OFFSET] = controller->link.R; /* Make sure that ACK or implied ACK is always up to date */
        }
    /* Need to make sure we dynamically compute the packet CRCs since header details can change */
    r = ddcmp_tmxr_put_packet_crc_ln (controller->line, buffer->transfer_buffer, buffer->count, *controller->corruption_factor);
    if (r == SCPE_OK) {
        controller->link.xmt_buffer = buffer;
        controller->ddcmp_control_packets_sent += (buffer->transfer_buffer[0] == DDCMP_ENQ) ? 1 : 0;
        if (controller->byte_wait) {  /* Speed limited? */
            buffer->buffer_return_time = buffer->count*controller->byte_wait + sim_grtime();
            sim_activate (controller->unit, buffer->count*controller->byte_wait);
            break;
            }
        dmc_complete_transmit (controller);
        buffer = dmc_buffer_queue_head(controller->xmt_queue);
        continue;
        }
    break;
    }
}

void dmc_check_romi(CTLR *controller)
{
if (dmc_is_dmc (controller) &&
    (*controller->csrs->sel0 & DMC_SEL0_M_ROMI) &&
    (*controller->csrs->sel0 & (DMC_SEL0_M_STEPUP | DMC_SEL0_M_RUN))) {
    /* DMC-11 or DMR-11, with ROMI and either STEP or RUN bits set.  */
    switch (*controller->csrs->sel6) {
        case DSPDSR:    /* 0x22b3 (read line status instruction), set the DSR and high speed bits in SEL2.  */
            sim_debug(DBG_TRC, controller->device, "%s%d: dmc_check_romi(). report DSR\n", controller->device->name, controller->index);
            dmc_setreg (controller, 2, 0xa00, DBG_RGC);
            break;
        case DROPDTR:   /* 0xa40b (drop DTR instruction) - VMS Driver uses this  */
            sim_debug(DBG_TRC, controller->device, "%s%d: dmc_check_romi(). drop_DTR\n", controller->device->name, controller->index);
            dmc_clr_modem_dtr (controller);
            break;
        case UINST_CNF3: /* 0x2293 (config switches to BSEL3) RSTS uses this */
            sim_debug(DBG_TRC, controller->device, "%s%d: dmc_check_romi(). report config\n", controller->device->name, controller->index);
            dmc_setreg (controller, 2, 0x0600, DBG_RGC); /* 1 Mb, DMR */
            break;
        case UINST_CNF: /* 0x2296 (get configuration switches) - VMS Driver uses this  */
            sim_debug(DBG_TRC, controller->device, "%s%d: dmc_check_romi(). report config\n", controller->device->name, controller->index);
            dmc_setreg (controller, 6, 0x0006, DBG_RGC); /* 1 Mb, DMR */
            break;
        default:
            if ((*controller->csrs->sel6 & 0xff00) == UINST_RROM) {
                /* Read ROM: VMS and RSTS do this */
                sim_debug(DBG_TRC, controller->device, "%s%d: dmc_check_romi(). read rom[0x%02x]\n", controller->device->name, controller->index, *controller->csrs->sel6 & 0xff);
                dmc_setreg (controller, 6,
                            hi_speed_ucode[*controller->csrs->sel6 & 0xff],
                            DBG_RGC);
                }
            else
                sim_debug(DBG_WRN, controller->device, "%s%d: dmc_check_romi(). Unknown Microcode instruction 0x%04x\n", controller->device->name, controller->index, *controller->csrs->sel6);
            break;
        }
    /* If it was a STEP we do this only once. */
    *controller->csrs->sel0 &= ~DMC_SEL0_M_STEPUP;
    controller->transfer_state = Idle;
    }
}

void dmc_process_immediate(CTLR *controller)
{
if (dmc_is_master_clear_set(controller)) {
    dmc_process_master_clear(controller);
    return;
    }
    dmc_check_romi(controller);             /* Do any needed immediate actions */
}

void dmc_process_command(CTLR *controller)
{
if (controller->transfer_state == InputTransfer) {
    dmc_process_input_transfer_completion(controller);
    return;
    }
if (controller->transfer_state == OutputTransfer) {
    dmc_check_for_output_transfer_completion(controller);
    return;
    }
if (controller->transfer_state == OutputControl) {
    dmc_check_for_output_control_completion(controller);
    return;
    }
/* transfer_state Idle */
if (dmc_is_rqi_set(controller)) {
    dmc_start_input_transfer(controller);
    return;
    }
if (!dmc_is_dmc (controller) ||
    (*controller->csrs->sel0 & DMC_SEL0_M_ROMI) == 0) {
    if (dmc_is_run_set(controller)) {
        dmc_start_control_output_transfer(controller);
        dmc_start_transfer_buffer(controller);
        }
    }
if (tmxr_get_line_loopback (controller->line) ^ dmc_is_lu_loop_set (controller)) {
    sim_debug(DBG_INF, controller->device, "%s%d: %s loopback mode\n", controller->device->name, controller->index, dmc_is_lu_loop_set (controller) ? "Enabling" : "Disabling");
    tmxr_set_line_loopback (controller->line, dmc_is_lu_loop_set (controller));
    }
}

t_stat dmc_rd(int32 *data, int32 PA, int32 access)
{
CTLR *controller = dmc_get_controller_from_address(PA);
int reg = PA & ((UNIBUS) ? 07 : 017);

*data = dmc_getreg(controller, PA, DBG_REG);
if (access == READ) {
    sim_debug(DBG_REG, controller->device, "dmc_rd(%s%d), addr=0x%08x, SEL%d, data=0x%04x\n", controller->device->name, controller->index, PA, reg, *data);
    }
else {
    sim_debug(DBG_REG, controller->device, "dmc_rd(%s%d), addr=0x%08x, BSEL%d, data=%02x\n", controller->device->name, controller->index, PA, reg, *data & 0xFF);
    }

return SCPE_OK;
}

t_stat dmc_wr(int32 data, int32 PA, int32 access)
{
CTLR *controller = dmc_get_controller_from_address(PA);
int reg = PA & ((UNIBUS) ? 07 : 017);
uint16 oldValue = dmc_getreg(controller, PA, 0);

if (access == WRITE) {
    sim_debug(DBG_REG, controller->device, "dmc_wr(%s%d), addr=0x%08x, SEL%d, newdata=0x%04x, olddata=0x%04x\n", controller->device->name, controller->index, PA, reg, data, oldValue);
    }
else {
    sim_debug(DBG_REG, controller->device, "dmc_wr(%s%d), addr=0x%08x, BSEL%d, newdata=%02x, olddata=0x%02x\n", controller->device->name, controller->index, PA, reg, data & 0xFF, ((PA&1) ? oldValue>>8 : oldValue) & 0xFF);
    }

if (access == WRITE) {
    if (PA & 1) {
        sim_debug(DBG_WRN, controller->device, "dmc_wr(%s%d), Unexpected non-16-bit write access to SEL%d\n", controller->device->name, controller->index, reg);
        }
    dmc_setreg(controller, PA, (uint16)data, DBG_REG);
    }
else {
    uint16 mask;
    if (PA & 1) {
        mask = 0xFF00;
        data = data << 8;
        }
    else {
        mask = 0x00FF;
        }

    dmc_setreg(controller, PA, (oldValue & ~mask) | (data & mask), DBG_REG);
    }

if (dmc_getsel(reg) == 0)    /* writes to SEL0 may need immediate action */
    dmc_process_immediate(controller);      /* Do any needed immediate actions */
if ((dmc_getsel(reg) == 0) || (dmc_getsel(reg) == 1)) {/* writes to SEL0 and SEL2 are actionable */
    if (0 == controller->dmc_wr_delay) {    /* Not waiting? */
        controller->dmc_wr_delay = 10;      /* Wait a bit before acting on the changed register */
        sim_activate_abs (controller->unit, controller->dmc_wr_delay);
        }
    }

return SCPE_OK;
}

int32 dmc_ininta (void)
{
int i;
int32 ans = 0; /* no interrupt request active */

for (i=0; i<DMC_NUMDEVICE+DMP_NUMDEVICE; i++) {
    CTLR *controller = &dmc_ctrls[i];
    if (controller->in_int != 0) {
        DIB *dib = (DIB *)controller->device->ctxt;
        ans = dib->vec + (8 * (int)(controller->unit - controller->device->units));
        dmc_clrinint(controller);
        sim_debug(DBG_INT, controller->device, "RXINTA Device %d - Vector: 0x%x(0%3o)\n", (int)(controller->unit-controller->device->units), ans, ans);
        break;
        }
    }

return ans;
}

int32 dmc_outinta (void)
{
int i;
int32 ans = 0; /* no interrupt request active */

for (i=0; i<DMC_NUMDEVICE+DMP_NUMDEVICE; i++) {
    CTLR *controller = &dmc_ctrls[i];
    if (controller->out_int != 0) {
        DIB *dib = (DIB *)controller->device->ctxt;
        ans = dib->vec + 4 + (8 * (int)(controller->unit - controller->device->units));
        dmc_clroutint(controller);
        sim_debug(DBG_INT, controller->device, "TXINTA Device %d - Vector: 0x%x(0%3o)\n", (int)(controller->unit-controller->device->units), ans, ans);
        break;
        }
    }

return ans;
}

t_stat dmc_reset (DEVICE *dptr)
{
t_stat ans = SCPE_OK;
CTLR *controller;
uint32 i, j;

sim_debug(DBG_TRC, dptr, "dmc_reset(%s)\n", dptr->name);

dmc_desc.packet = TRUE;
dmc_desc.buffered = 16384;
dmp_desc.packet = TRUE;
dmp_desc.buffered = 16384;
/* Connect structures together */
for (i=0; i < DMC_NUMDEVICE; i++) {
    dmc_csrs[i].sel0 = &dmc_sel0[i];
    dmc_csrs[i].sel2 = &dmc_sel2[i];
    dmc_csrs[i].sel4 = &dmc_sel4[i];
    dmc_csrs[i].sel6 = &dmc_sel6[i];
    controller = &dmc_ctrls[i];
    controller->csrs = &dmc_csrs[i];
    controller->line = &dmc_desc.ldsc[i];
    controller->rcv_queue = &dmc_rcv_queues[i];
    controller->completion_queue = &dmc_completion_queues[i];
    controller->xmt_queue = &dmc_xmt_queues[i];
    controller->ack_wait_queue = &dmc_ack_wait_queues[i];
    controller->free_queue = &dmc_free_queues[i];
    controller->buffers = &dmc_buffers[i];
    controller->device = &dmc_dev;
    controller->baseaddr = &dmc_baseaddr[i];
    controller->basesize = &dmc_basesize[i];
    controller->modem = &dmc_modem[i];
    controller->corruption_factor = &dmc_corruption[i];
    controller->unit = &controller->device->units[i];
    controller->unit->ctlr = (void *)controller;
    controller->index = i;
    }
tmxr_connection_poll_interval (&dmc_desc, dmc_connect_poll);
for (i=0; i < DMP_NUMDEVICE; i++) {
    dmp_csrs[i].sel0 = &dmp_sel0[i];
    dmp_csrs[i].sel2 = &dmp_sel2[i];
    dmp_csrs[i].sel4 = &dmp_sel4[i];
    dmp_csrs[i].sel6 = &dmp_sel6[i];
    dmp_csrs[i].sel10 = &dmp_sel10[i];
    controller = &dmc_ctrls[i+DMC_NUMDEVICE];
    controller->csrs = &dmp_csrs[i];
    controller->line = &dmp_desc.ldsc[i];
    controller->rcv_queue = &dmp_rcv_queues[i];
    controller->completion_queue = &dmp_completion_queues[i];
    controller->xmt_queue = &dmp_xmt_queues[i];
    controller->ack_wait_queue = &dmp_ack_wait_queues[i];
    controller->free_queue = &dmp_free_queues[i];
    controller->buffers = &dmp_buffers[i];
    controller->device = (UNIBUS) ? &dmp_dev : &dmv_dev;
    controller->dev_type = DMP;
    controller->baseaddr = &dmp_baseaddr[i];
    controller->basesize = &dmp_basesize[i];
    controller->modem = &dmp_modem[i];
    controller->corruption_factor = &dmp_corruption[i];
    controller->unit = &controller->device->units[i];
    controller->unit->ctlr = (void *)controller;
    controller->index = i + DMC_NUMDEVICE;
    controller->dmp_pool = controller->rcv_queue;   /* receive buffers for tributary 0 */
    if (controller->dmp_hub == NULL)
        controller->dmp_loaded = -1;
    }
tmxr_connection_poll_interval (&dmp_desc, dmp_connect_poll);
if (0 == dmc_units[0].flags) {       /* First Time Initializations */
    for (i=0; i < DMC_NUMDEVICE; i++) {
        controller = &dmc_ctrls[i];
        controller->state = Uninitialised;
        controller->transfer_state = Idle;
        controller->control_out = NULL;
        *controller->modem = 0;
#if defined (VM_PDP10)
        controller->dev_type = DMR;
#else
        controller->dev_type = DMC;
#endif
        dmc_dev.units[i] = dmc_unit_template;
        controller->unit->ctlr = (void *)controller;
        dmc_microdiag[i] = TRUE;
        dmc_corruption[i] = 0;
        }
    tmxr_set_modem_control_passthru (&dmc_desc);   /* We always want Modem Control */
    dmc_units[dmc_dev.numunits-2] = dmc_poll_unit_template;
    dmc_units[dmc_dev.numunits-2].ctlr = dmc_units[0].ctlr;
    dmc_units[dmc_dev.numunits-1] = dmc_timer_unit_template;
    dmc_units[dmc_dev.numunits-1].ctlr = dmc_units[0].ctlr;
    dmc_desc.notelnet = TRUE;                      /* We always want raw tcp socket */
    dmc_desc.dptr = &dmc_dev;                      /* Connect appropriate device */
    dmc_desc.uptr = dmc_units+dmc_desc.lines;      /* Identify polling unit */
    for (i=0; i < DMP_NUMDEVICE; i++) {
        controller = &dmc_ctrls[i+DMC_NUMDEVICE];
        controller->state = Uninitialised;
        controller->transfer_state = Idle;
        controller->control_out = NULL;
        *controller->modem = 0;
        dmp_dev.units[i] = dmc_unit_template;
        controller->unit->ctlr = (void *)controller;
        dmp_corruption[i] = 0;
        }
    tmxr_set_modem_control_passthru (&dmp_desc);   /* We always want Modem Control */
    dmp_units[dmp_dev.numunits-2] = dmc_poll_unit_template;
    dmp_units[dmp_dev.numunits-2].ctlr = dmp_units[0].ctlr;
    dmp_units[dmp_dev.numunits-1] = dmc_timer_unit_template;
    dmp_units[dmp_dev.numunits-1].ctlr = dmp_units[0].ctlr;
    dmp_desc.notelnet = TRUE;                      /* We always want raw tcp socket */
    dmp_desc.dptr = &dmp_dev;                      /* Connect appropriate device */
    dmp_desc.uptr = dmp_units+dmp_desc.lines;      /* Identify polling unit */
    }

/* The DMP (Unibus) and DMV (Qbus) share their units and so one DIB.  Size
   it for the variant being reset: 8 bytes per DMP line, 16 per DMV line
   (the DMV adds BSEL10-BSEL17 for the Q22 address and count fields). */
if ((dptr == &dmp_dev) || (dptr == &dmv_dev)) {
    dmp_dib.ulnt = (dptr == &dmv_dev) ? IOLN_DMV : IOLN_DMP;
    dmp_dib.lnt = (dptr->numunits - 2) * dmp_dib.ulnt;
    }
ans = auto_config (dptr->name, (dptr->flags & DEV_DIS) ? 0 : dptr->numunits - 2);

if (!(dptr->flags & DEV_DIS)) {
    int32 attached = 0;

    for (i = 0; i < DMC_NUMDEVICE + DMP_NUMDEVICE; i++) {
        if (dmc_ctrls[i].device == dptr) {
            BUFFER *buffer;

            controller = &dmc_ctrls[i];
            if (!dmc_is_dmc(controller)) {      /* DMP/DMV: buffers held by tributary sessions */
                int slot;

                dmp_session_store(controller);
                for (slot = 0; slot < DMP_MAX_TRIBS; slot++)
                    dmp_session_release(controller, slot);
                }
            /* Avoid memory leaks by moving all buffers back to the free queue
               and then freeing any allocated transfer buffers for each buffer
               on the free queue */
            while (controller->ack_wait_queue->count) {
                buffer = (BUFFER *)remqueue (controller->ack_wait_queue->hdr.next);
                ASSURE (insqueue (&buffer->hdr, controller->free_queue->hdr.prev));
                }
            while (controller->completion_queue->count) {
                buffer = (BUFFER *)remqueue (controller->completion_queue->hdr.next);
                ASSURE (insqueue (&buffer->hdr, controller->free_queue->hdr.prev));
                }
            while (controller->rcv_queue->count) {
                buffer = (BUFFER *)remqueue (controller->rcv_queue->hdr.next);
                ASSURE (insqueue (&buffer->hdr, controller->free_queue->hdr.prev));
                }
            while (controller->xmt_queue->count) {
                buffer = (BUFFER *)remqueue (controller->xmt_queue->hdr.next);
                ASSURE (insqueue (&buffer->hdr, controller->free_queue->hdr.prev));
                }
            for (j = 0; j < controller->free_queue->size; j++) {
                buffer = (BUFFER *)remqueue (controller->free_queue->hdr.next);
                free (buffer->transfer_buffer);
                buffer->transfer_buffer = NULL;
                ASSURE (insqueue (&buffer->hdr, controller->free_queue->hdr.prev));
                }
            dmc_buffer_queue_init_all(controller);
            dmc_clrinint(controller);
            dmc_clroutint(controller);
            controller->dmc_wr_delay = 0;
            for (j=0; j<dptr->numunits-1; j++) {
                sim_cancel (&dptr->units[j]); /* stop poll */
                if (dptr->units[j].flags & UNIT_ATT)
                    ++attached;
                }
            dmc_process_master_clear(controller);
            }
        }
    if (attached)
        sim_activate_after (dptr->units+(dptr->numunits-2), DMC_CONNECT_POLL*1000000);/* start poll */
    }

return ans;
}

t_stat dmc_attach (UNIT *uptr, CONST char *cptr)
{
DEVICE *dptr = (UNIBUS) ? ((&dmc_dev == find_dev_from_unit(uptr)) ? &dmc_dev : &dmp_dev) : &dmv_dev;
int32 dmc = (int32)(uptr-dptr->units);
TMXR *mp = (dptr == &dmc_dev) ? &dmc_desc : &dmp_desc;
t_stat ans = SCPE_OK;
char *peer = ((dptr == &dmc_dev)? &dmc_peer[dmc][0] : &dmp_peer[dmc][0]);
char *port = ((dptr == &dmc_dev)? &dmc_port[dmc][0] : &dmp_port[dmc][0]);
char attach_string[1024];

if (!cptr || !*cptr)
    return SCPE_ARG;
if (!(uptr->flags & UNIT_ATTABLE))
    return SCPE_NOATT;
if ((((CTLR *)uptr->ctlr)->dev_type == DMP) && !peer[0] && strncasecmp (cptr, "SYNC", 4)) {
    /* DMP/DMV without a PEER: listen on the port for as many stations as
       the controller has tributaries (DMV11 12, DMP11 32), each on its own
       drop line.  A multipoint control station uses them all, any other
       station the first one (see dmp_hub_poll). */
    CTLR *controller = (CTLR *)uptr->ctlr;
    int ci = dmc;                           /* hub tables are per DMP/DMV line */
    TMXR *hub = &dmp_hub_desc[ci];

    memset (hub, 0, sizeof(*hub));
    memset (dmp_hub_ldsc[ci], 0, sizeof(dmp_hub_ldsc[ci]));
    memset (&dmp_hub_unit[ci], 0, sizeof(dmp_hub_unit[ci]));
    dmp_hub_unit[ci].dptr = dptr;
    dmp_hub_unit[ci].flags = UNIT_ATT;
    dmp_hub_unit[ci].dynflags = TMUF_NOASYNCH;
    hub->lines = dmp_trib_limit(controller);
    hub->ldsc = dmp_hub_ldsc[ci];
    hub->packet = TRUE;
    hub->buffered = 16384;
    hub->notelnet = TRUE;
    hub->dptr = dptr;
    hub->uptr = &dmp_hub_unit[ci];
    ans = tmxr_open_master (hub, cptr);
    if (ans != SCPE_OK)
        return ans;
    controller->dmp_hub = hub;
    strncpy (port, cptr, sizeof(dmc_port[0]));
    uptr->filename = (char *)malloc (strlen(port)+1);
    strcpy (uptr->filename, port);
    uptr->flags |= UNIT_ATT;
    sim_activate_after (dptr->units+(dptr->numunits-2), DMC_CONNECT_POLL*1000000);/* start poll */
    return SCPE_OK;
    }
if (0 == strncasecmp (cptr, "SYNC", 4)) {
    sprintf (attach_string, "Line=%d,%s", dmc, cptr);
    ans = tmxr_open_master (mp, attach_string);
}
else {
    if (!peer[0]) {
        sim_printf ("Peer must be specified before attach\n");
        return SCPE_ARG;
    }
    sprintf (attach_string, "Line=%d,Connect=%s,%s", dmc, peer, cptr);
    ans = tmxr_open_master (mp, attach_string);                 /* open master socket */
}
if (ans != SCPE_OK)
    return ans;
mp->dptr = dptr;
strncpy (port, cptr, sizeof(dmc_port[0]));
uptr->filename = (char *)malloc (strlen(port)+1);
strcpy (uptr->filename, port);
uptr->flags |= UNIT_ATT;
sim_activate_after (dptr->units+(dptr->numunits-2), DMC_CONNECT_POLL*1000000);/* start poll */
return ans;
}

t_stat dmc_detach (UNIT *uptr)
{
DEVICE *dptr = (UNIBUS) ? ((&dmc_dev == find_dev_from_unit(uptr)) ? &dmc_dev : &dmp_dev) : &dmv_dev;
int32 dmc = (int32)(uptr-dptr->units);
TMXR *mp = (dptr == &dmc_dev) ? &dmc_desc : &dmp_desc;
TMLN *lp = &mp->ldsc[dmc];
int32 i, attached;
t_stat r;

if (!(uptr->flags & UNIT_ATT))                          /* attached? */
    return SCPE_OK;
sim_cancel (uptr);
uptr->flags &= ~UNIT_ATT;
for (i=attached=0; i<mp->lines; i++)
    if (dptr->units[i].flags & UNIT_ATT)
        ++attached;
if (!attached) {
    sim_cancel (dptr->units+mp->lines);              /* stop poll on last detach */
    sim_cancel (dptr->units+(mp->lines+1));          /* stop timer on last detach */
    }
if (((CTLR *)uptr->ctlr)->dmp_hub) {     /* DMP/DMV multipoint hub */
    CTLR *controller = (CTLR *)uptr->ctlr;
    int slot;

    r = tmxr_close_master (controller->dmp_hub);
    controller->dmp_hub = NULL;
    controller->line = dmp_own_line(controller);
    for (slot = 0; slot < DMP_MAX_TRIBS; slot++)
        controller->dmp_trib[slot].ses.drop = NULL;
    free (uptr->filename);
    uptr->filename = NULL;
    return r;
    }
r = tmxr_detach_ln (lp);
free (uptr->filename);
uptr->filename = NULL;
return r;
}

const char *dmc_description (DEVICE *dptr)
{
#if defined (VM_PDP10)
return "DMR11 Synchronous network controller";
#else
return "DMC11 Synchronous network controller";
#endif
}

const char *dmp_description (DEVICE *dptr)
{
return (UNIBUS) ? "DMP11 Synchronous network controller"
                : "DMV11 Synchronous network controller";
}

