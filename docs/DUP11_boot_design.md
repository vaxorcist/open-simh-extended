<!-- open-simh-extended/docs/DUP11_boot_design.md  v2, 2026-11-25 (v2: facts from DEC's SECDUP.SYS (MOP device type 10, software CRC with CRC inhibit), a likely SIMH gap for that mode (known issue 124), decisions for Ulli; v1: design only, Ulli: "Design only now"; the build follows when the weekly limit allows) -->
# DUP11: BOOT DUPn (design)

Ulli asked (2026-11-25) to make the DUP11 bootable on Unibus PDP-11s. With
15 % of the weekly limit left he chose to have the design written now and
the work done later. This document is the starting point for that work.

Sources: the DUP11 User's Manual EK-DUP11-OP-002 (Aug 82) and Maintenance
Manual EK-DUP11-MM-003 (Ulli's uploads, DEC copyright: facts only, in our own
words; they stay out of the fork and the project), SIMH's `PDP11/pdp11_dup.c`
and `pdp11_ddcmp.h`, and our own BOOT DMCn (patch 24,
`docs/DMV11_switches_boot_design.md` section 10).

## 1. Facts

### 1.1 What the DUP11 itself does (manuals)

- The DUP11 is a bit-synchronous line interface without a processor or
  microcode: the host sends and receives the protocol byte by byte (or bit
  stream for bit-oriented protocols) through four registers. It does not
  run DDCMP itself and has no boot function of its own: no remote load
  detect, no boot switches, no boot ROM on the board.
- For DDCMP (character mode, PARCSR "DEC mode" set, sync character 226
  octal) the board does the framing help a host needs: transmit start and
  end of message (TXDBUF TSOM, TEOM), CRC-16 generation on transmit and CRC
  checking on receive (RXDBUF RCRCER), sync character search.
- The User's Manual describes DDCMP's bootstrap message only at protocol
  level (1.3.4.7: a maintenance message begins with DLE and carries the
  reload program transparently). Booting over a DUP11 is therefore a matter
  of host software: a program in the PDP-11 that speaks DDCMP maintenance
  messages and MOP through the DUP11 registers.
- DEC's DECnet-11M V4.2 kit (Ulli's DECKIT tape, [136,54]) has secondary
  and tertiary loaders for the DUP11: SECDUP.SYS (1536 bytes) and
  TERDUP.SYS (2560 bytes). So DEC did down-line load systems over a DUP11;
  the first stage on the PDP-11 (the "primary loader") must have been a
  program in the host (a ROM or a toggled-in loader); which one is not in
  the documents we have.

### 1.2 What SIMH's DUP does (pdp11_dup.c)

- Device DUP (Unibus, DUP11) / DPV (Qbus, DPV11), auto-configured CSR and
  vector (floating), 8 bytes per line: RXCSR, RXDBUF/PARCSR, TXCSR, TXDBUF.
  Disabled by default.
- On the wire SIMH sends whole DDCMP messages without sync characters over
  TCP (tmxr packets, `ddcmp_tmxr_put_packet_ln`), the same framing as the
  DMC11/DMR11/DMP11/DMV11 (`pdp11_ddcmp.h`), so a DUP line can be connected
  to a SIMH DMC11 on a VAX (`SET DMC0 PEER=...` and `SET DUP0 PEER=...`).
- Transmit in DEC mode: the host writes the bytes into TXDBUF; a leading
  sync character is dropped; at TEOM SIMH appends the CRC-16
  (`ddcmp_crc16`, low byte first). A DDCMP message is sent when it is
  complete: the 6-byte header with its CRC, then for data and maintenance
  messages the data field with its CRC (SIMH sends once the buffer holds
  more than 8 bytes, or at once for a control message, ENQ).
- Receive in DEC mode: SIMH delivers the message byte by byte through
  RXDBUF with RXDONE: the 6 header bytes and their 2 CRC bytes, then the
  data and its 2 CRC bytes. RCRCER is set at byte 8 and at the end of the
  data when the CRC over everything so far is zero, i.e. it means "CRC good"
  at those two points (the name is the manual's; the meaning at that point
  is CRC = 0).
- SIMH's DUP has no BOOT routine.

### 1.3 DDCMP maintenance message (DDCMP V4.0 specification, our summary)

- Header: DLE (220 octal), count (14 bits, low byte first) with the
  QSYNC and SELECT flags in the top two bits (both set on a point-to-point
  maintenance message), two zero bytes (RESP, NUM), address 1, then the
  header CRC.
- Data field: the MOP message, then its CRC.
- A maintenance message needs no DDCMP start-up: a station in DDCMP
  maintenance mode just sends and receives them. A load host (VMS NCP LOAD
  or the autoservice of a circuit with SERVICE ENABLED) puts its own circuit
  into maintenance mode when it gets a MOP Request Program in a maintenance
  message.

### 1.4 What DEC's DUP11 loader does (SECDUP.SYS, disassembled)

SECDUP.SYS from the DECnet-11M V4.2 kit is the secondary loader for a
DUP11 host: it runs on the PDP-11 (loaded at 0, entry near 2000) and asks
for the tertiary. Facts from its code (our own reading, no DEC text):

- **MOP Request Program:** code 8, **device type 10**, format 1, program
  type 1 (tertiary). For comparison the other secondary loaders of the kit
  ask with DMC 12, DMP 18, DMV 34, DLV 16, DPV 36, DUV 2 (the DMC, DMP and
  DMV values are the ones our BOOT DMCn and the DMV/DMP boot use already).
  So a primary loader on a DUP11 asks with device type 10.
- **MOP messages handled:** Memory Load (2) and Parameter Load with Transfer
  Address (20); it answers with Request Memory Load (10) and the next load
  number, and starts the tertiary at the transfer address.
- **Line set-up:** RXCSR = DTR and request to send, waits for clear to send
  and data set ready; PARCSR = **DEC mode, CRC inhibit**, sync character
  226 (101226 octal); receiver: RXCSR receive enable, strip sync, DTR.
- **DDCMP framing in software, CRC too:** it builds 4 sync characters, the
  DDCMP maintenance header (DLE, count with QSYNC and SELECT set, 0, address
  1), computes the header CRC and the data CRC itself (CRC-16, polynomial
  120001 octal reflected), writes the whole message into TXDBUF with TSOM on
  the first sync character, a few sync characters behind it, then clears
  SEND. It does not use TEOM. On receive it reads the bytes and checks both
  CRCs itself.

### 1.5 Consequence for SIMH (known issue 124, not tested yet)

SIMH's DUP does not look at PARCSR CRC inhibit: in DEC mode it appends its
own CRC when the message ends (TEOM, or SEND cleared) and strips only the
leading sync characters. A message sent the SECDUP way would go out with
SECDUP's two CRCs, the trailing sync characters and an extra CRC from SIMH.
Whether the receiving DDCMP code (`pdp11_ddcmp.h`, e.g. a VAX DMC11) takes
the message by its header count and ignores the rest is not known. If not,
a load that goes through DEC's SECDUP (program type SECONDARY, the normal
VMS chain SECDUP, TERDUP, system) fails at the secondary stage, whatever our
bootstrap does.

## 2. Plan: BOOT DUPn as a bootstrap written for SIMH

The same approach as BOOT DMCn (patch 24), because the board has no boot
logic of its own: `BOOT DUPn` copies a PDP-11 program into memory and starts
it. The program is the PDP-11 side of a primary loader:

1. **Line set-up:** master reset (TXCSR DRESET), PARCSR = DEC mode with sync
   character 226, RXCSR receiver enable, DTR and request to send as the
   board needs (SIMH: DTR at attach; the bootstrap raises DTR as a host
   would).
2. **DDCMP maintenance framing in software** (what the DMC11's microcode
   did for BOOT DMCn): build header (DLE, count | 0xC000, 0, 0, 1). Two
   ways (decision D1 below): (a) the board's CRC: send the header with
   TSOM ... TEOM (the board adds the header CRC), then the MOP message with
   TEOM (data CRC), as SIMH's DUP supports today; (b) as DEC's SECDUP: CRC
   inhibit, CRCs computed by the bootstrap, SEND cleared at the end (needs
   known issue 124 fixed). Receive: collect 8 header bytes, check DLE and
   RCRCER, take the count, collect count + 2 bytes, check RCRCER, hand the
   MOP message to the MOP layer. Control messages (ENQ) and data messages
   (SOH) from a peer that is still in DDCMP run mode are skipped.
3. **MOP layer:** reused from `PDP11/pdp11_dmc_boot.mac` (Request Program
   with device type, format 1, program type from `SET DUPn PROGRAM`,
   software ID; Memory Load / Memory Load with Transfer Address; Request
   Memory Load with the next load number; retries every 5 s; start the
   loaded program with R1 = CSR, SP 1000, PSW 340), so both bootstraps share
   one MOP part. The DMC version's buffer handling (four receive buffers,
   one transmit buffer, BA/CC IN) is replaced by the byte-by-byte register
   loop above; polling only, no interrupts.
4. **SIMH side:** `dup_boot()` (PDP-11 only, Unibus DUP11 as Ulli asked; the
   DPV11 on the Qbus could get the same later), `DUP_BOOT_RTN` in the
   device, the bootstrap copied to the 8 KB below min(MEMSIZE, 160000) with
   its parameters (CSR, MOP device type, program type, timer ticks) as for
   BOOT DMCn; `SET DUPn PROGRAM={SECONDARY|TERTIARY|SYSTEM}` (as on the
   DMC); `HELP DUP` gets a Booting topic; `HELP BOOT` lists DUP;
   `tools/dmc_bootstrap/mkdmcboot.py` builds `pdp11_dup_boot.h` from the
   object file.

## 3. Test plan

- **T1 (no VMS):** `tests/dmv11_boot/mopctl.py` (our Python MOP station,
  already used for BOOT DMCn R8-R10) on the DUP's ATTACH port: Request
  Program arrives in a DDCMP maintenance message with good CRCs; a Memory
  Load with Transfer Address of a small test program (hello.mac) is loaded
  and started.
- **T2:** retries without an answer, restart after repeats, a lost Memory
  Load (the next Request Memory Load asks again).
- **T3 (VMS V4.7 as load host):** VAX-11/780 with a DMC11 (circuit DMC-0,
  SERVICE ENABLED) connected to the PDP-11's DUP11; node with SERVICE
  CIRCUIT DMC-0, SERVICE DEVICE DUP (or DMC, to be found), SECONDARY
  LOADER SECDUP.SYS, TERTIARY LOADER TERDUP.SYS, LOAD FILE BRUSYS.SYS;
  `BOOT DUP0` on an 11/70: the BRU banner = PASS (as B13 for the XQ).
  Based on `tests/dmc_boot/vmsdmc.sh`.
- Regression: the DUP's existing use (DDCMP with RSX/DECnet hosts is not
  available here; `pdp11` DUP with a DMC peer as a link test), BOOT DMCn
  R8-R10 (shared MOP part).

## 4. Settled and open points

| Point | State |
|---|---|
| MOP device type for a DUP11 | **10** (from DEC's SECDUP.SYS, section 1.4) |
| MOP messages a DUP11 loader handles | Memory Load (2), Parameter Load with Transfer Address (20), Request Memory Load (10); our MOP part (from BOOT DMCn) also takes Memory Load with Transfer Address (0) |
| D1: how the bootstrap frames messages | (a) the board's CRC (works with SIMH's DUP now) or (b) DEC's way with CRC inhibit (needs 124). Proposal: (a) for the bootstrap, and fix 124 so DEC's SECDUP works too |
| D2: default program type | SECONDARY (VMS sends SECDUP.SYS, then TERDUP.SYS, then the system: the real chain; needs 124) or SYSTEM (VMS sends the LOAD FILE at once; works without 124). Proposal: SECONDARY as default, as on the DMC, once 124 is fixed |
| Known issue 124 (SIMH DUP ignores CRC inhibit) | To be tested first (a SECDUP message over a DUP line to a VAX DMC11), then fixed if needed: with CRC inhibit SIMH sends the bytes as written, finds the message end from the DDCMP header count, and drops the trailing sync characters |
| DPV11 (Qbus) too | Ulli asked for the Unibus; the DPV11's registers differ slightly (TBEMPTY, protocol select); possible later (DEC's SECDPV asks with device type 36) |
| Sharing the MOP part with pdp11_dmc_boot.mac | One source with two link layers (conditional assembly) or two files; decide when building |
| DUP11 modem control in SIMH | SECDUP waits for CTS and DSR after raising DTR and RTS; check that SIMH's DUP gives them on a connected line (W3/W5/W6 options) |

## 5. Order of work (when building)

1. Test known issue 124: a hand-made SECDUP-style message from a PDP-11
   program over a DUP line to a VAX-11/780 DMC11 (VMS V4.7 with
   SERVICE ENABLED), DUP DEBUG; fix SIMH's DUP if needed.
2. The bootstrap (`PDP11/pdp11_dup_boot.mac`), `dup_boot()`, SET DUPn
   PROGRAM, HELP; tests T1, T2 with mopctl.py.
3. T3 with VMS V4.7: PROGRAM=SYSTEM (BRUSYS.SYS), then SECONDARY (SECDUP,
   TERDUP, BRUSYS.SYS).
4. Patch, docs, known issues, delivery.
