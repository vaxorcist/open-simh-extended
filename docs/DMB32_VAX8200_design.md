<!-- open-simh-extended/docs/DMB32_VAX8200_design.md  v8, 2026-11-19 (v8: section 12, the sync line under VMS V4.7 with the backported SIDRIVER; v7: sync line CTS follows RTS, DDCMP tributary under VMS V5.5 (patch 16, known issues 84); v6: sync line (P4) with VMS V5.5's SIDRIVER, DMF32 sync line on the shared code (P4a), patch 12; v5: patch 8, P5 corrected (the CPU loop really runs now), DMA engine moved to vax_bi_dma.c; v4: section 10, results of P1-P3, P4a (printer) and P5; SIDRIVER missing; v3: all decisions D1–D6 made, Ulli's answers, VMS device names from the User Guide, ULTRIX phase; v2: D1 decided: shared library for printer formatter and DDCMP sync engine; v1: first implementation plan for Ulli's decision) -->
# DMB32 on the VAX 8200/8300 (implementation plan)

Prepared with Claude for Ulli (vaxorcist), 2026-11-15. Status: **done for VMS** (patch 8 `opensimh_8_dmb32_v1.patch`, branch `dmb32`; the sync line in patch 12 `opensimh_12_dmb32_sync_v1.patch`, branch `dmb32-sync`): P1-P5 done, P4 with VMS V5.5's SIDRIVER (section 11); P5u (ULTRIX) and P6 (UETP) open (section 10).

Sources:
- EK-DMB32-TD-001 *DMB32 Technical Description* and EK-DMB32-UG-001 *DMB32 User Guide*, both supplied by Ulli.
- `$DMBDEF` in the VMS V4.7 macro library, taken from Ulli's disk image VMS047_1.R81 and used to cross-check the bits.
- Our DMF32 code in `VAX/vax_dmf.c` (patch 2).

## 1. What the DMB32 is

The DMB32 is the VAXBI version of the DMF32, called "BICOMBO" in VMS. One BI module provides:

| Function | DMB32 | DMF32 (patch 2) |
|---|---|---|
| Asynchronous lines | 8, DMA output, receive FIFO, PREEMPT single characters | 8, DMA output, receive silo |
| Synchronous line | 1: DDCMP, HDLC, SDLC, BISYNC, general byte; 2 DMA buffers per direction | 1: DDCMP and others |
| Line printer port | 1, DMA, formatting, DAVFU | 1, formatting |
| DR11-C parallel port | **none** | yes |

The functions are the same, but the host interface is new:

- **BI node.** The 8 KB node space is at `2000 0000 + node*2000`. It holds the BIIC registers DTYPE (= 0109), BICSR, BER, EINTRCSR, INTRDES, IPINTRMSK, FIPSDES, IPINTRSRC, SADR/EADR, UINTRCSR and GPR0 (the self-test summary). There is no CSR on the Unibus.
- **Longword registers** at BASE+100 to 1FC. MAINT, ACSR, SCSR, PCSR, CONFIG, ACSR2, SCSR2, PCSR2, SPTE/SPTS/GPTE/GPTS and the printer registers are direct. The async registers (1C0–1D8) and the sync registers (180–1BC) are indexed through ACSR<7:0> and SCSR<7:0>.
- **Non-destructive FIFOs.** TBUF (204), SBUF (208) and RBUF (20C) are read without being popped. The host pops an entry by writing the register (a BI rule). An entry is valid when bit 31 is set.
- **DMA with the board's own address translation.** There are four modes: physical; physical PTE address plus offset; system virtual; and virtual process-PTE address plus offset. The board walks the SPT and GPT itself, using SPTE, SPTS, GPTE and GPTS. MAINT<2> (PTE.VALID) says that the host has set them up. Transfers are octawords, with write-mask at the edges.
- **Interrupts.** The vector is supplied by the board (UINTRCSR EX.VECTOR is always 1) and goes out at BR4 (IPL 14):
  `vector = 0x100 | type<<6 | node<<2`, where the type is 0 for BIIC error, 1 for sync, 2 for async and 3 for printer.
  INTRDES chooses the CPU(s) that may take the interrupt.
- **Self-test and reset.** A BI reset, BICSR<10> (NRST) or MAINT<1> (PROG.RESET) starts the self-test. When it is done, STS (BICSR<11>) is set, BROKE (BICSR<12>) is cleared, INIT and PROG.RESET read 0, and GPR0 holds the result codes.
- **VMS support in V4.7** (checked on VMS047_1.R81). SYSGEN knows the node type `DMB`. The drivers are:
  - YIDRIVER: async lines;
  - SIDRIVER: sync line, which is the DECnet datalink "DMB-n";
  - LIDRIVER: printer.

  There is also a UETP test, UETDMBS00.EXE.
- **VMS device names** (examples in the User Guide, chapter 4): async lines `TXA0:`–`TXA7:` (`SHOW DEVICE/FULL TXA`), sync line `_SIA0:` (EVDAL example), printer `LIA` (EVAAA example). The letter is `B` instead of `A` if another TX controller (DMF32, DHU11) was configured first. VMS DECnet startup maps device type `SI` to the datalink name `DMB-n`. No example of `SYSGEN SHOW/CONFIGURATION` for a DMB32 was found in the manuals; P1 will show it.

## 2. Simulator structure

| Item | Plan |
|---|---|
| New files | `VAX/vax_dmb.c` (about 1900 lines), compiled only into `vax8200`; `VAX/vax_combo_lib.c` + `VAX/vax_combo_lib.h` (about 700 lines, name open), compiled into every simulator that has the DMF32 or the DMB32 (makefile, CMakeLists, .vcproj files) |
| Device name | `DMB`. Units `DMB-ASYNC` (TMXR, 8 lines), `DMB-SYNC`, `DMB-PRINT`, and a hidden timer unit, named the same way as the DMF32 units |
| Default | Disabled (`SET DMB ENABLE`), node 5 (`SET DMB NODE=5..15`; it must not collide with KA0/KA1 = 0/1, the memory nodes 2/3 or the DWBUA 4) |
| Shared library (D1) | `vax_combo_lib.c` holds (a) the printer formatter (PFIX, PCAR, PSIZE, TAB, AUTO.RETURN, UPPER, DAVFU, column/row state) and (b) the DDCMP sync engine (receive, deliver, hold, transmit, modem, the peer protocol shared with DMF-SYNC and the DMC). Both work on a per-device state struct (`COMBO_LP`, `COMBO_SYNC`) and call back into the device for DMA, buffer completion and interrupts. DDCMP framing and CRC stay in `PDP11/pdp11_ddcmp.h` |
| Kept per device | Register access, indexing, FIFOs, interrupts and DMA addressing; async line handling (too little in common: DMF silo vs DMB RBUF/TBUF/PREEMPT) |
| Sync protocols | DDCMP only, as on the DMF32 (D3; Ulli does not want HDLC). HDLC, SDLC, BISYNC and general byte are accepted in LPR1, but a buffer completes with an error code |
| Self-test time | `SET DMB SELFTEST=<ms>` (default 50), also register `STTIME` (D4) |

## 3. Changes to the 8200 bus code (`VAX/vax820_bi.c`, `vax820_defs.h`)

The DMB32 is the first native BI I/O node in the simulator: so far there are only the CPUs, memory and the DWBUA. Three changes are needed:

1. **Vector supplied by the device.** `get_vector` computes a fixed `SCB_NEXUS + (l<<6) + (i<<2)` for every nexus except the UBA. That formula collides with the DMB32 vectors: at BR4, type 1 gives 0x140, which is the formula's slot for level 15. The fix adds a per-nexus acknowledge function (`nexus_ack[TR]`) that returns the device's own vector. The DMB DIB fills it in, and all other nexus stay as they are.
2. **Node number set at run time.** `TR_DMB` becomes a variable taken from the DIB (`dmb_dib.ba` = node), checked by `build_nexus_tab` as usual. `SHOW BUS` lists the node.
3. **Interrupt destination (8300).** Patch 7 lets a secondary take only IPIR, timer and software interrupts. The DMB32 honours INTRDES: if KA0's bit is set, CPU0 takes the interrupt, as now. If only KA1's bit is set (which VMS V4.7 is not expected to do), the request is routed to KA1 through the existing per-CPU path. On a plain 8200 nothing changes.

DMA uses `ReadLP`/`WriteLP` on physical memory. The page-table walk is the board's own and does not use the CPU's MMU or TLB.

## 4. Register model in `vax_dmb.c`

- **BIIC block** (BASE+0 to 44, F0): a register set like `uba_biic`, plus UINTRCSR, IPINTR*, SADR/EADR and GPR0. A write to NRST starts the self-test.
- **Self-test:** a timed event, 50 ms of simulated time by default, configurable (D4). STS=1, BROKE=0, GPR0 = pass code, and the initial line states from UG 3.4.1: 9600 baud, 8N1, RX off, TX on, DTR and RTS off.
- **ACSR/ACSR2, SCSR/SCSR2, PCSR/PCSR2:** index, interrupt enables (RX.IE, TX.IE, SYNC.IE, PR.IE), and the per-function reset bit 10, ASYNC/SYNC/PRINTER.RESET. RX.TIMER (ACSR2<23:16>) delays the receive interrupt.
- **CONFIG** = 01 01 08 (1 printer, 1 sync, 8 async).
- **MAINT:**
  - FORCE.FAIL sets BROKE.
  - SKIP.SELF.TEST shortens the self-test.
  - MAINT.LEVEL 1 and 2 open the CASRAM window at 218–1FFC as plain RAM, so that the diagnostics can read and write it.
  - The cable bits SYNC/ASYNC/PRINT read as "loopback connector present" only when `SET DMB LOOPBACK` is in effect.
- **FIFOs.**
  - RBUF: 256 entries, each with the character, line number, error bits and modem-change records.
  - TBUF: one completion record per line (DMA done, preempt done, error code).
  - SBUF: one record per sync buffer completion or modem change.
  - A write pops the top entry, and a write to an empty FIFO is ignored.
- **Async lines:**
  - LPR: speed, character size, parity, stop bits, RX enable, auto-flow;
  - LNCTRL: DTR, RTS, BREAK, TX abort, auto-flow modes;
  - LSTAT: modem inputs;
  - FLOWC: XON/XOFF characters;
  - TBUFFAD/TBUFFCT: DMA;
  - PREEMPT: GO bit 15, sent ahead of DMA.
- **Sync line:** two transmit buffers (TBUFFADn/CTn/TLNCTRLn) and two receive buffers (RBUFFADn/CTn/RLNCTRLn), plus LPR1–3 and BUFCTRL. A completion goes to SBUF with the TX.ACT/SECOND bits and an error code.
- **Printer:** PBUFFAD/PBUFFCT DMA and PCTRL. Status bits for OFFLINE (unit not attached), CONNECT.VERIFY and DAVFU.READY.

## 5. DMA engine (shared by all three functions)

`dmb_dma (mode, addr, offset, count, dir, buf)` works page by page:

| Mode (DMA.PHYS / DMA.PTE) | Translation |
|---|---|
| PHYS=1, PTE=0 | `addr` is physical |
| PHYS=1, PTE=1 | `addr` is the physical address of the first PTE; the page offset is in CT<..> |
| PHYS=0, PTE=0 | `addr` is a system virtual address, translated through SPTE/SPTS |
| PHYS=0, PTE=1 | `addr` is the system virtual address of the first process PTE, plus the offset; the PTE is fetched through the SPT, and a global PTE (VMS format) goes through GPTE/GPTS |

An invalid PTE, a length beyond SPTS/GPTS or non-existent memory ends the transfer with the DMA error code (UG 3.4.8.2). The registers are then updated with the rest of the count, as the manual describes. The first version moves the whole buffer when it is started and reports completion after a delay for the character time (async: from the line speed). Real octaword masking is not needed, because the transfer goes to memory byte by byte.

## 6. Phases and tests

Order for D1: the library is built with the DMB32 (P3, P4) and used by the DMB32 from the start. The DMF32 is switched to it afterwards in its own commit and patch step (P4a), with its own regression test, so a DMB32 problem and a DMF32 regression can never be mixed up.

Ulli's rule: tests are systematic, with the prompts supplied by Ulli and no long trial and error. The DMB32 is not bootable, so every phase includes a VMS V4.7 boot of the `vax8200` with `DMB` enabled as the regression test.

| Phase | Content | Test (VMS V4.7 on `vax8200`, Ulli's VMS047_1.R81) |
|---|---|---|
| P0 | Ulli confirms the expected SYSGEN output and device names (section 8) | none |
| P1 | BI plumbing (section 3), BIIC block, self-test, CONFIG, MAINT, empty FIFOs | The boot autoconfigures; `SHOW DEVICE` lists the DMB units; `SYSGEN SHOW/BUS` or `/CONFIG` shows DMB at node 5; no `BROKE`, no machine check |
| P2 | Async lines: LPR, LNCTRL, LSTAT, RBUF, TBUF, PREEMPT, DMA in all four modes, modem, XON/XOFF | Telnet to line 1 and log in; `TYPE` a large file (DMA, virtual modes); `SET TERM/SPEED`; `SET TERM/MODEM` with hang-up; XOFF during output; two lines at once |
| P3 | Printer port, via the library's printer formatter | `PRINT` to the LI device attached to a file; compare it with the same job on an LP11; form feed, tabs, lower case; offline when detached |
| P4 | Sync port with DDCMP, 2+2 buffers, SBUF, via the library's sync engine | DECnet circuit `DMB-0` between two `vax8200` instances; then `vax8200` DMB32 ↔ `vax780` DMF32, and ↔ DMC/DMV (patches 1, 2, 4). `NCP SHOW CIRCUIT`, file copy both ways, loop tests |
| P4a | Switch `vax_dmf.c` to the library (printer formatter, sync engine); separate commit, patch 2 rebuilt as v7 on top of it or a separate patch step (decided when the patch series is rebuilt) | DMF32 regression on `vax780` with Ulli's prompts: async login and `TYPE`, `PRINT` to the DMF printer, DECnet over DMF-SYNC to DMC and DMV, the existing DMF/DMC test harness; byte-for-byte compare of printer output before/after |
| P5u | ULTRIX-32 on the `vax8200` with the DMB32 (dmb driver), after all VMS phases | Boot; DMB32 found at the BI node; login on a dmb line; `lpr` to the printer port; optionally DECnet-ULTRIX over the sync line. Needs an ULTRIX-32 image or kit from Ulli |
| P5 | 8300: INTRDES routing with KA1 enabled | `START/CPU`, then telnet sessions and DECnet traffic while both CPUs are busy; `STOP/CPU`, `START/CPU` again |
| P6 | Maintenance and loopback (internal UART loopback via LPR, `SET DMB LOOPBACK`) | UETP DMB test (UETDMBS00) only (Ulli's decision); the diagnostics EVDAJ/EVDAL/EVAAA are not used for now |
| P7 | Help text, SHOW commands, `SHOW DMB-SYNC PEER`, debug flags (REG, INT, DMA, ASYNC, SYNC, LP, TRC) | `HELP DMB` |
| P8 | Patch 8 (`opensimh_8_dmb32_v1.patch`), branch `dmb32`, then into integration; docs: known issues, overview, sound device table (DMB32 = "without sound" row), handover | `git am` on 87eb7d5 with the full series 1–8; build vax8200 plus all simulators with the DMF32 (vax780, vax750, vax730, vax8600; the library is linked there now) and microvax3900 |

Estimated size: `vax_dmb.c` about 1900 lines, the library about 700 lines (mostly moved out of `vax_dmf.c`, which shrinks by about the same amount); `vax820_bi.c`, `vax820_defs.h` and the build files take about 80 lines. P1–P3 can be tested with your VMS image alone. P4 needs two simulator instances, so I would use the existing DMF/DMC test harness.

## 7. Sound (optional)

The DMB32 itself makes no noise. The printer port could use the line-printer sound, if Ulli wants it there and when the printer sound exists. Until then the DMB32 stays in the "without sound" table.

## 8. Questions for Ulli (answered 2026-11-15)

1. VMS device names: Ulli has no real 8200. The User Guide gives `TXA0:`–`TXA7:`, `SIA0:` and `LIA` (section 1). `SYSGEN SHOW/CONFIGURATION` will be checked in P1.
2. Diagnostics: only UETP for now; EVDAJ/EVDAL/EVAAA are not used.
3. Sync protocols: DDCMP only; no HDLC.
4. ULTRIX-32: yes, as an extra step after VMS (P5u).

## 9. Decisions

| # | Decision | Proposal |
|---|---|---|
| D1 | Copy and adapt from `vax_dmf.c`, or factor the shared parts into a library | **Decided (Ulli, 2026-11-15): middle way.** Shared library for the printer formatter and the DDCMP sync engine, used by the DMB32 from the start; the DMF32 is switched in a separate commit with a DMF32 regression test (P4a); async register handling stays per device. Reasons: one copy of the risky DDCMP code, guaranteed identical behaviour of DMB32/DMF32 sync lines; cost about one extra day and one DMF32 regression run |
| D2 | Default node | **Decided:** 5, settable with `SET DMB NODE=` |
| D3 | Sync protocols | **Decided:** DDCMP only, others end with an error code |
| D4 | Self-test time | **Decided:** about 50 ms simulated time by default, configurable (`SET DMB SELFTEST=<ms>`, register `STTIME`) |
| D5 | Only the 8200 simulator (and 8300 via KA1) | **Decided:** yes; other VAX 8xxx models later, separately |
| D6 | Patch number and branch | **Decided:** patch 8 after patch 7, branch `dmb32` |

## 10. Results (2026-11-15)

Branch `dmb32` (on top of patch 7). Test scripts in `tests/dmb32/`.

| Phase | Result |
|---|---|
| P1 | **Pass.** VMS V4.7 configures the DMB32 at node 5: `TXA0`–`TXA7` and `LIA0`; `SYSGEN SHOW/ADAPTER`: "5 BI Combo Board (DMB32)". One fix: VMS resets the node (BICSR NRST) and then waits for **BROKE** (BICSR<12>) to clear, not for STS; the self-test now sets BROKE and clears it at the end (`p1.ini`) |
| P2 | **Pass.** Telnet login on `TXA0`, SHOW TERMINAL, DIRECTORY (237 files: 2927 DMA transfers, no errors), XOFF/XON, LOGOUT (`p2.ini` + `p2.py`). VMS uses only the system virtual DMA mode; the other three modes wait for ULTRIX (P5u) |
| P3 | **Pass.** COPY and DIRECTORY/OUTPUT to `LIA0:` and to `LPA0:` (LP11) give the same text (532 print transfers, no errors) (`p3.ini`) |
| P4 | **Pass (2026-11-17, section 11).** Was waiting for `SIDRIVER.EXE`, which is not on VMS047_1.R81; Ulli's VMS V5.5 image (VMS055.R82) has it |
| P4a (printer) | **Pass.** The DMF32 printer uses the shared formatter (`VAX/vax_combo_lib.c`); VMS print jobs with six printer settings on the vax780 give byte for byte the same `LCA0:` output before and after (`p4a_lp.ini`). The DDCMP engine part of P4a follows with P4 |
| P5 | **Pass.** VAX 8300: after START/CPU and with two CPU-bound loop processes (attached processor in the EXECUTE state), the P2 checks pass. VMS sets INTRDES to KA0 only (`p5.ini` v2). Correction: the first run (v1) assumed `LOOP.EXE` on the disk, where it was not, so the loop never ran; v2 builds it |
| P5u, P6 | Open: ULTRIX V2.2 image from Ulli; UETP prompts from Ulli |

Also found on the way (known issues): case 66, `BICSR_HES` had the value of `BICSR_SES` (fixed); case 67, BI nodes could not supply their own vector (fixed, section 3).

Patch 8 `opensimh_8_dmb32_v1.patch` (after patch 7) holds the DMB32 work; the sync line's DDCMP engine follows when a SIDRIVER is available. With the KDB50 (patch 9) the DMA engine moved to `VAX/vax_bi_dma.c` (KDB50 plan D3); the DMB32 tests P2 and P3 pass unchanged.

## 11. Synchronous line (2026-11-17, patch 12)

VMS V5.5 (Ulli's VMS055.R82) has SIDRIVER: it configures `SIA0` ("network device") and the DECnet line and circuit `DMB-0`. Implemented from the User Guide (3.3.28-3.3.43 sync registers, 3.3.52 SBUF, 3.4.6 DMA, 3.4.8 error codes, 3.4.11 sync modem control) and the Technical Description (2.7 DDCMP support), both supplied by Ulli again:

| Item | Implementation |
|---|---|
| Framing | The board frames DDCMP only; SIDRIVER runs the protocol. Transmit: header (6) and data from the buffer, the board adds both CRCs; TBUFFCT then holds the characters still to send (0). Receive: header and data without the CRCs; RBUFFCT then holds the number of characters received. Bad header CRC or start character: frame dropped (resynchronize); bad data CRC: stored, message error 22; message longer than the buffer: 23 (24 with a bad CRC). Buffer errors 41 (header) and 42 (length); host abort 61 |
| Buffers | Two per direction, DMA.START/ABORT in TLNCTRLn/RLNCTRLn, BUFCTRL TX/RX.BUFF.PRIO when both are ready; a completion is an SBUF entry (TX.ACT, SECOND.BUFFER, error) and the sync interrupt |
| Line | LPR1 RX.ENABLE, LOOP (internal loopback), MODEM.SUPPRESS, LINE.RESET; LPR2 modem outputs, modem inputs <15:8> (clocks, DCD, DSR on while connected; CTS follows RTS, which SIDRIVER as a DDCMP tributary needs: patch 16, known issues 84), PROTOCOL (only DDCMP; another protocol gives buffer error 41), MODEM.OVERRIDE (without it no transmission without DSR and CTS: modem error 52); a modem change is an SBUF entry; BUFCTRL reads a V.24 cable (SYNC.VALID) |
| Link | Unit `DMB-SYNC`: `SET DMB-SYNC PEER=host:port`, `ATTACH DMB-SYNC port`; whole DDCMP frames over a SIMH TCP connection, as the DMC11 and DMF-SYNC, so the partner can be a DMC11, DMR11, DMP11, DMV11, DMF32 or DMB32 |
| Shared code (D1) | The link and the DDCMP framing are in `vax_combo_lib.c` (taken from the DMF32); the DMF32 sync line uses them too (P4a, its own commit). Patch 12 holds both (Ulli, 2026-11-17: patch 9 had moved the DMB32 DMA engine to `vax_bi_dma.c`, so patch 8 could not take the sync line without changing patch 9) |

Tests (`tests/dmb32/p4.sh`; VMS V5.5 on the 8200 from the KDB50, DECnet partner node MV2 = 1.2 with VMS V4.7):

| Test | Result |
|---|---|
| P4, partner MicroVAX II with DMV11 (`p4_mv2.ini`, VMS line DMP-0) | **Pass.** Circuit DMB-0 up, HELPLIB.HLB (5675 blocks) there and back with 0 differences, remote DIRECTORY, NCP LOOP NODE, SET HOST; circuit counters about 3.1 MB each way, no errors |
| P4, partner VAX-11/780 with DMF32 (`p4_780.ini`, line DMF-0) | **Pass**, before and after the DMF32 switch to the shared code |
| P4a, VMS V4.7 XGDRIVER on the DMF32 of the 8200 (`p4a_8200.ini`, VMS047_1.R81) to the DMV11 | **Pass** (DMF32 on the shared code) |
| Builds | vax730, vax750, vax780, vax8600, vax8200 without warnings |

Found on the way (own code, fixed before the commit): the device reset did not keep an attached DMB-SYNC polling, and the link was set up as a Telnet line; it is a raw packet line, as for the DMF32. VMS V4.7 calls the DMV11 line `DMP-0` (XDDRIVER), not DMV-0.

## 12. Sync line under VMS V4.7 (2026-11-19)

VMS V4.7 has no SIDRIVER. Ulli asked for V5.5's driver to be backported; it was done by binary patching (`docs/SIDRIVER_V47_backport.md`). Under V4.7 SYSGEN configures SIA0, and DECnet runs over line DMB-0 as DDCMP POINT (to a MicroVAX II DMV11) and as DDCMP TRIBUTARY (to a VAX-11/780 DMP11 control station). Tests: `tests/dmb32/p4b_8200.ini`, `p4g_8200.ini`, `p4c_8200.ini` + `p4c_780.ini`. DDCMP CONTROL is not supported by SIDRIVER (V5.5 and the backport answer NCP with "Invalid parameter value, Protocol").
