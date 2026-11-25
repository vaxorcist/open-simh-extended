<!-- open-simh-extended/docs/DUP11_boot_design.md  v1, 2026-11-25 (v1: design only, Ulli: "Design only now"; the build follows when the weekly limit allows) -->
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

## 2. Plan: BOOT DUPn as a bootstrap written for SIMH

The same approach as BOOT DMCn (patch 24), because the board has no boot
logic of its own: `BOOT DUPn` copies a PDP-11 program into memory and starts
it. The program is the PDP-11 side of a primary loader:

1. **Line set-up:** master reset (TXCSR DRESET), PARCSR = DEC mode with sync
   character 226, RXCSR receiver enable, DTR and request to send as the
   board needs (SIMH: DTR at attach; the bootstrap raises DTR as a host
   would).
2. **DDCMP maintenance framing in software** (what the DMC11's microcode
   did for BOOT DMCn): build header (DLE, count | 0xC000, 0, 0, 1), send it
   with TSOM ... TEOM (the board adds the header CRC), then the MOP message
   with TEOM (data CRC). Receive: collect 8 header bytes, check DLE and
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

## 4. Open points (for the build)

| Point | To be settled |
|---|---|
| MOP device type the bootstrap sends for a DUP11 | Not in the manuals. Look at what SECDUP.SYS sends when it asks for the tertiary (disassembly, as for SECUNA), or what VMS NCP calls the device (SERVICE DEVICE DUP); VMS seems not to check the device type strictly (the DELQA's 37 was accepted, known issue 122) |
| Which program type to ask for first | As BOOT DMCn: SECONDARY by default (VMS then sends SECDUP.SYS), SET DUPn PROGRAM to change it |
| DPV11 (Qbus) too | Ulli asked for the Unibus; the DPV11's registers differ slightly (TBEMPTY, protocol select); possible later |
| Sharing the MOP part with pdp11_dmc_boot.mac | One source with two link layers (conditional assembly) or two files; decide when building |
| DUP11 modem control in SIMH | Whether the bootstrap must raise DTR/RTS for SIMH to connect (W3/W5/W6 options); check at the first test |
