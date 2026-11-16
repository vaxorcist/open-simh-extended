<!-- open-simh-extended/docs/DEBNA_VAX8200_design.md  v1, 2026-11-16 (v1: first implementation plan for Ulli's decision: DEBNA or DEBNK, sources, phases, open points) -->
# DEBNA (and later DEBNK) on the VAX 8200/8300 (implementation plan)

Prepared with Claude for Ulli (vaxorcist), 2026-11-16. Status: **plan, waiting for Ulli's decisions** (section 8).

Sources:
- EK-DEBNX-IN-001 *DEBNA/DEBNK Installation Guide* (Aug 1987), supplied by Ulli. Appendix B has the registers, appendix C the network boot, appendix D the Ethernet address.
- NetBSD `sys/dev/bi/if_ni.c` and `if_nireg.h` (Ludd, 2000; BSD licence with advertising clause): the BVP NI port as a host driver uses it. These are used only for interface facts, and the code is not copied (the same rule as for the DMV11 and the KDB50).
- The VMS V4.7 disk: `ETDRIVER.EXE` (the DEBNA/DEBNK Ethernet port, VMS device `ETA0`, DECnet line `ET-0`), `PBDRIVER.EXE` (BVP port), the SDL names `AT$_BVP`, `AT$_BVP_NIP`, `ADP$L_BVP...`, and the message "BVP failure - Status/Error/Data". Traced on a running VMS where the NetBSD driver does not settle a point.
- In SIMH: the BI node and DMA groundwork of the DMB32 and KDB50 (`VAX/vax_bi_dma.c`, per-node vectors, node reset with BROKE), and SIMH's Ethernet layer `sim_ether.c` (as used by the DEUNA/DELUA `XU` and the DEQNA/DELQA `XQ`).

There is no DEBNA technical manual and no BVP specification at hand.

## 1. DEBNA or DEBNK: the recommendation

| | DEBNA | DEBNK |
|---|---|---|
| What it is | VAXBI Ethernet adapter (T1034 module), a MicroVAX chip with LANCE | The same Ethernet adapter **plus a TK50 controller** (80186, MPSC) on the same module (T1032) |
| DTYPE | `410F` | `410E` |
| Ethernet port | BVP NI port, registers in the BCI3 GPRs at `bb+204` (PC), `208` (PS), `20C` (PE), `210` (PD) | identical |
| Tape port | none | a second BVP port in the BIIC GPRs at `bb+F0`-`FC`; the protocol behind it (BVP storage port carrying TMSCP) is **not documented** anywhere I can reach; NetBSD does not support it |
| Self-test PUDR | `7FFB0FDF` | `7FFC1FDF` |
| Host side | VMS ETDRIVER; NetBSD `ni` | the same for Ethernet; VMS PBDRIVER (+ a tape class driver) for the TK50 |

**Recommendation: the DEBNA first.** It is the better documented one (guide + NetBSD driver + VMS driver), and everything it needs is also the Ethernet half of the DEBNK. The DEBNK then becomes "DEBNA + DTYPE 410E + PUDR tape bits + a second BVP port for the TK50", a separate later step whose protocol has to be traced from VMS PBDRIVER (and which could reuse SIMH's TMSCP server in `pdp11_tq.c`, as the KDB50 reused the RQ). Until then a `SET ET TYPE=DEBNK` could at most report the DEBNK identity with the tape port absent, which buys nothing; I would leave it out.

## 2. The BVP NI port (what the simulator must do)

From the guide (appendix B) and the NetBSD driver:

| Item | Content |
|---|---|
| Port registers | PC (port control, written by the host: instruction in bits 0-6, OWN bit 7, data above), PS (port status: OWN 31, RSQ, STD self-test done, ACC adapter can communicate, XSTP, ERL error lost, FQE free queue exhausted, PST port state undefined/initialized/enabled/stopped/maint, ETYPE, SUME), PE (error code), PD (error data) |
| After self-test | PE = 0, PD = 0, PS: STD + ACC + state undefined; PUDR (`bb+214`) `7FFB0FDF`; Ethernet address at `bb+218`-`21D` |
| Port instructions | INIT (PC data = physical address of the PQB), ENABLE, SHUTDOWN, "command queue 0 not empty", "free queue n not empty" |
| Port queue block (PQB) | host memory: command queues, response queue, interrupt vector and level (`(vector << 2) + 2`), host node, virtual addresses of PQB, free queue block and buffer descriptor table, system and global page table base and length (SBR/SLR) |
| Queues | VAX self-relative interlocked queues in host memory (INSQTI/REMQHI semantics, secondary interlock in bit 0 of the header): command queue 0, response queue, three free queues (messages, transmit datagrams, receive datagrams) in a free queue block with their entry lengths; entries 512 bytes |
| Entries | datagram (transmit / received: status, length, opcode, PTDB index, command reference, buffer list with offset, length, key, BDT index; chained fragments), message (opcode `MSG`, sub-opcode: write parameters (returns the station address), read and clear counters, set/clear protocol type data block (PTDB: protocol type, flags broadcast / promiscuous / all-multicast, multicast list)) |
| Buffers | buffer descriptor table (BDT) in host memory: valid bit + byte offset, length, **pointer to the buffer's PTE**, key; the port maps through the system page table, the same idea as the KDB50's mapped buffer descriptors, so `vax_bi_dma.c` covers it |
| Interrupt | the vector from the PQB, raised when a response is queued (PS RSQ); the host clears PS OWN/RSQ |
| Ethernet | SIMH `sim_ether` (`ATTACH ET eth0`, tap, NAT, UDP links between simulators), the same attach syntax as `XU`/`XQ`; a protocol-type filter from the PTDBs, broadcast and multicast filtering |
| MOP | a MOP Boot message from the network: passed to the driver if the MOP protocol is enabled, or BI RESET with the boot-enable jumper (guide C.1); not needed for VMS operation |

Open points (each settled by a short, targeted trace of VMS ETDRIVER, no long trial and error; if VMS needs a dialog I ask Ulli for the prompts):
- the exact field offsets of the PQB, free queue block, queue entries and the BDT (NetBSD's `if_nireg.h` gives them; VMS confirms);
- what VMS sends besides NI_WPARAM / RCCNTR / STPTDB / CLPTDB (e.g. read parameters, maintenance), and the counter block format that `NCP SHOW LINE ET-0 COUNTERS` shows;
- whether VMS uses the global page table and process-space buffers (the BVP header has room for it; `vax_bi_dma.c` already walks process PTEs for the DMB32);
- how VMB on the 8200 boots from the DEBNA (`B ET50`: device code, MOP load) — only for the optional boot phase.

## 3. Simulator structure (proposal)

| Item | Plan |
|---|---|
| File | new `VAX/vax_debna.c` (`vax8200` only), using `vax_bi_dma.c` and `sim_ether.c` |
| Device | **`ET`** (decision D2; VMS calls it ETA0, the console boot device is ET), one unit, default **BI node 7** (D3; 5 is the DMB32, 6 the KDB50), `SET ET NODE=`, `SET ET MAC=` (default a DEC 08-00-2B address as XU/XQ), `ATTACH ET <interface>` |
| BI node | BIIC block with DTYPE `410F`, BICSR, BER, EINTRCSR, INTRDES, UINTRCSR; node reset and self-test with BROKE while testing (DMB32 rule); the BCI3 GPRs with PC/PS/PE/PD, PUDR and the address bytes in node space |
| Port engine | a service routine that works the command queue (interlocked queue access with the secondary interlock honoured), transmits datagrams, fills receive datagrams from the receive free queue, answers messages, queues responses and interrupts |
| Default | **disabled** by default (D4), as Ethernet devices are elsewhere in SIMH; `SET ET ENABLE` |
| Help | `HELP ET` |

Estimated size: about 1,000-1,400 lines.

## 4. Sound

The DEBNA makes no sound. Nothing in the realism mode.

## 5. Phases and tests

Tests run VMS V4.7 on the `vax8200` (Ulli's VMS047_1.R81, system disk on the KDB50). The DECnet partner is a MicroVAX II with its DELQA (`XQ`) running VMS V4.7, connected by a SIMH UDP Ethernet link, so no host interface is needed. At least one boot test per bootable device: the network boot is phase P5.

| Phase | Content | Test |
|---|---|---|
| P0 | Short traces for section 2 (field offsets, extra messages) | none |
| P1 | BI node, self-test values, port states (INIT, ENABLE, SHUTDOWN), PQB and queues, messages (station address, counters, PTDBs), interrupts | VMS boots; `SYSGEN SHOW/ADAPTER` shows the DEBNA at node 7; `ETA0` exists; no BVP failure message, no machine check |
| P2 | Transmit and receive datagrams, protocol filtering, broadcast/multicast | `NCP SET LINE ET-0 STATE ON`; DECnet over Ethernet to the MicroVAX II: adjacency up, `SET HOST`, `COPY` both ways with `DIFFERENCES` 0, `NCP LOOP NODE`, `SHOW LINE ET-0 COUNTERS` plausible |
| P3 | 8300: both CPUs busy during network load | `START/CPU`, copy jobs on both CPUs |
| P4 | Regression: the other BI nodes and Ethernet devices | KDB50 and DMB32 tests P2/P3; MicroVAX II `XQ` unchanged (shared `sim_ether`, untouched) |
| P5 | Optional: network boot (`BOOT ET`, VMB `B ET70`) from a MOP load host; MOP boot message handling | only if Ulli wants it (D6) and a load host is available (VMS on the MicroVAX II as a MOP server, or a host tool) |
| P6 | Help, docs, the ODS device table, known issues, patch 10 | `HELP ET`; full patch series on 87eb7d5 |

## 6. Risks

- **No BVP specification:** the field layout comes from the NetBSD driver and VMS traces. If VMS uses BVP features NetBSD does not (process-space buffers, extra messages), they need short traces, which the KDB50 showed to be quick.
- **Interlocked queues shared with two CPUs (8300):** the port must honour the secondary interlock on every queue it touches; a mistake would corrupt queues only under load (P3).
- **Ethernet on the test host:** the UDP link between two simulators avoids host interfaces; real network access is Ulli's side.

## 7. What this plan does not do

- No DEBNK tape port (section 1): later, as its own step, if wanted.
- No second DEBNA (as with the second KDB50: postponed).
- No ULTRIX (postponed as for the DMB32 and KDB50; ULTRIX has a DEBNA driver, `ni`, so a later test is possible).
- No other BI models than the 8200/8300.

## 8. Decisions for Ulli

| # | Question | Proposal |
|---|---|---|
| D1 | DEBNA or DEBNK | **DEBNA** now; the DEBNK tape port as a later separate step (or never) |
| D2 | Device name | `ET` (VMS ETA0, console boot device ET) |
| D3 | Default node | 7, settable with `SET ET NODE=` |
| D4 | Enabled by default | no (`SET ET ENABLE`) |
| D5 | Patch and branch | patch 10 after patch 9 (KDB50), branch `debna` on `kdb50`; also on `integration` after each tested step |
| D6 | Network boot (P5) | optional, last; do you want it, and do you have a MOP load host (VMS DECnet with a downline-load database, or a host tool)? |
