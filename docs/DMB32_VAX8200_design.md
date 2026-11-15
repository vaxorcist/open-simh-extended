<!-- open-simh-extended/docs/DMB32_VAX8200_design.md  v1, 2026-11-15 (v1: first implementation plan for Ulli's decision) -->
# DMB32 on the VAX 8200/8300 (implementation plan)

Prepared with Claude for Ulli (vaxorcist), 2026-11-15. Status: **plan, waiting for Ulli's decision** (section 9).

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

## 2. Simulator structure

| Item | Plan |
|---|---|
| New file | `VAX/vax_dmb.c` (about 2500 lines), compiled only into `vax8200` (makefile, CMakeLists, VAX8200.vcproj) |
| Device name | `DMB`. Units `DMB-ASYNC` (TMXR, 8 lines), `DMB-SYNC`, `DMB-PRINT`, and a hidden timer unit, named the same way as the DMF32 units |
| Default | Disabled (`SET DMB ENABLE`), node 5 (`SET DMB NODE=5..15`; it must not collide with KA0/KA1 = 0/1, the memory nodes 2/3 or the DWBUA 4) |
| Reused DMF32 logic | TMXR line handling and modem bits, line parameter → `tmxr_set_config_line`, async DMA fill and transmit loop, printer formatting (PFIX, PCAR, PSIZE, TAB, AUTO.RETURN, UPPER, DAVFU), DDCMP framing and CRC with the same peer protocol as DMF-SYNC and the DMC |
| Reuse method | **Copy and adapt** (proposal). A shared library with `vax_dmf.c` would put the tested DMF32 at risk, so it is left as a later clean-up (decision D1) |
| Sync protocols | DDCMP only, as on the DMF32. HDLC, SDLC, BISYNC and general byte are accepted in LPR1, but a buffer completes with an error code (decision D3) |

## 3. Changes to the 8200 bus code (`VAX/vax820_bi.c`, `vax820_defs.h`)

The DMB32 is the first native BI I/O node in the simulator: so far there are only the CPUs, memory and the DWBUA. Three changes are needed:

1. **Vector supplied by the device.** `get_vector` computes a fixed `SCB_NEXUS + (l<<6) + (i<<2)` for every nexus except the UBA. That formula collides with the DMB32 vectors: at BR4, type 1 gives 0x140, which is the formula's slot for level 15. The fix adds a per-nexus acknowledge function (`nexus_ack[TR]`) that returns the device's own vector. The DMB DIB fills it in, and all other nexus stay as they are.
2. **Node number set at run time.** `TR_DMB` becomes a variable taken from the DIB (`dmb_dib.ba` = node), checked by `build_nexus_tab` as usual. `SHOW BUS` lists the node.
3. **Interrupt destination (8300).** Patch 7 lets a secondary take only IPIR, timer and software interrupts. The DMB32 honours INTRDES: if KA0's bit is set, CPU0 takes the interrupt, as now. If only KA1's bit is set (which VMS V4.7 is not expected to do), the request is routed to KA1 through the existing per-CPU path. On a plain 8200 nothing changes.

DMA uses `ReadLP`/`WriteLP` on physical memory. The page-table walk is the board's own and does not use the CPU's MMU or TLB.

## 4. Register model in `vax_dmb.c`

- **BIIC block** (BASE+0 to 44, F0): a register set like `uba_biic`, plus UINTRCSR, IPINTR*, SADR/EADR and GPR0. A write to NRST starts the self-test.
- **Self-test:** a timed event (about 50 ms of simulated time, decision D4). STS=1, BROKE=0, GPR0 = pass code, and the initial line states from UG 3.4.1: 9600 baud, 8N1, RX off, TX on, DTR and RTS off.
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

Ulli's rule: tests are systematic, with the prompts supplied by Ulli and no long trial and error. The DMB32 is not bootable, so every phase includes a VMS V4.7 boot of the `vax8200` with `DMB` enabled as the regression test.

| Phase | Content | Test (VMS V4.7 on `vax8200`, Ulli's VMS047_1.R81) |
|---|---|---|
| P0 | Ulli confirms the expected SYSGEN output and device names (section 8) | none |
| P1 | BI plumbing (section 3), BIIC block, self-test, CONFIG, MAINT, empty FIFOs | The boot autoconfigures; `SHOW DEVICE` lists the DMB units; `SYSGEN SHOW/BUS` or `/CONFIG` shows DMB at node 5; no `BROKE`, no machine check |
| P2 | Async lines: LPR, LNCTRL, LSTAT, RBUF, TBUF, PREEMPT, DMA in all four modes, modem, XON/XOFF | Telnet to line 1 and log in; `TYPE` a large file (DMA, virtual modes); `SET TERM/SPEED`; `SET TERM/MODEM` with hang-up; XOFF during output; two lines at once |
| P3 | Printer port | `PRINT` to the LI device attached to a file; compare it with the same job on an LP11; form feed, tabs, lower case; offline when detached |
| P4 | Sync port with DDCMP, 2+2 buffers, SBUF | DECnet circuit `DMB-0` between two `vax8200` instances; then `vax8200` DMB32 ↔ `vax780` DMF32, and ↔ DMC/DMV (patches 1, 2, 4). `NCP SHOW CIRCUIT`, file copy both ways, loop tests |
| P5 | 8300: INTRDES routing with KA1 enabled | `START/CPU`, then telnet sessions and DECnet traffic while both CPUs are busy; `STOP/CPU`, `START/CPU` again |
| P6 | Maintenance and loopback (internal UART loopback via LPR, `SET DMB LOOPBACK`) | UETP DMB test (UETDMBS00); DCLT if available; EVDAJ/EVDAL/EVAAA only if Ulli has them |
| P7 | Help text, SHOW commands, `SHOW DMB-SYNC PEER`, debug flags (REG, INT, DMA, ASYNC, SYNC, LP, TRC) | `HELP DMB` |
| P8 | Patch 8 (`opensimh_8_dmb32_v1.patch`), branch `dmb32`, then into integration; docs: known issues, overview, sound device table (DMB32 = "without sound" row), handover | `git am` on 87eb7d5 with the full series 1–8; build vax8200 plus the representatives vax780 (DMF32 unchanged) and microvax3900 |

Estimated size: `vax_dmb.c` is about 2500 lines; `vax820_bi.c`, `vax820_defs.h` and the build files take about 80 lines. P1–P3 can be tested with your VMS image alone. P4 needs two simulator instances, so I would use the existing DMF/DMC test harness.

## 7. Sound (optional)

The DMB32 itself makes no noise. The printer port could use the line-printer sound, if Ulli wants it there and when the printer sound exists. Until then the DMB32 stays in the "without sound" table.

## 8. Questions for Ulli (to keep testing short)

1. What does `SYSGEN> SHOW/CONFIGURATION` (or the autoconfigure output) look like on a real 8200 with a DMB32? Which device names did VMS V4.7 give the async lines (TXA? TXB?), the printer (LIA0?) and the sync line (SIA0?)?
2. Do you have the VAX diagnostics EVDAJ/EVDAL/EVAAA, or should P6 be only UETP?
3. Which sync protocols do you want: DDCMP only (enough for DECnet), or HDLC too (for X.25/PSI)?
4. Is ULTRIX-32 on the 8200 (dmb driver) a test target as well?

## 9. Decisions

| # | Decision | Proposal |
|---|---|---|
| D1 | Copy and adapt from `vax_dmf.c`, or factor the shared parts into a library | Copy and adapt now; the library is a later clean-up |
| D2 | Default node | 5, settable with `SET DMB NODE=` |
| D3 | Sync protocols | DDCMP only, others end with an error code |
| D4 | Self-test time | About 50 ms simulated time (real board: several seconds; too long for scripts) |
| D5 | Only the 8200 simulator (and 8300 via KA1) | Yes; the 8800 and the BI MicroVAX/VAX 6000 are not simulated |
| D6 | Patch number and branch | Patch 8 after patch 7, branch `dmb32` |
