<!-- open-simh-extended/docs/DEBNA_VAX8200_design.md  v5, 2026-11-17 (v5: section 10, VMS V5.5 (known issues 74, 75), patch 10 v2; v4: network boot open until further notice (Ulli); v3: P5: VMS V4.7's VMB.EXE tried, no DEBNA driver; v2: decisions D1-D6 accepted by Ulli; section 9, results P0-P4, P6, VMS findings, network boot (P5) open; v1: first implementation plan for Ulli's decision: DEBNA or DEBNK, sources, phases, open points) -->
# DEBNA (and later DEBNK) on the VAX 8200/8300 (implementation plan and results)

Prepared with Claude for Ulli (vaxorcist), 2026-11-16. Status: **done for VMS V4.7 and V5.5** (patch 10 `opensimh_10_debna_v2.patch`, branch `debna`, on `integration`); network boot (P5) open, see section 9; the DEBNK tape port maybe later (Ulli).

Sources:
- EK-DEBNX-IN-001 *DEBNA/DEBNK Installation Guide* (Aug 1987), supplied by Ulli. Appendix B has the registers, appendix C the network boot, appendix D the Ethernet address.
- NetBSD `sys/dev/bi/if_ni.c` and `if_nireg.h` (Ludd, 2000; BSD licence with advertising clause): the BVP NI port as a host driver uses it. These are used only for interface facts, and the code is not copied (the same rule as for the DMV11 and the KDB50).
- The VMS V4.7 disk: `ETDRIVER.EXE` (the DEBNA/DEBNK Ethernet port, VMS device `ETA0`, DECnet line `BNT-0`, see section 9), `PBDRIVER.EXE` (BVP port), the SDL names `AT$_BVP`, `AT$_BVP_NIP`, `ADP$L_BVP...`, and the message "BVP failure - Status/Error/Data". Traced on a running VMS where the NetBSD driver does not settle a point.
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

## 8. Decisions (all accepted by Ulli, 2026-11-16)

| # | Question | Decision |
|---|---|---|
| D1 | DEBNA or DEBNK | **DEBNA** now; the DEBNK tape port maybe later |
| D2 | Device name | `ET` (VMS ETA0, console boot device ET) |
| D3 | Default node | 7, settable with `SET ET NODE=` |
| D4 | Enabled by default | no (`SET ET ENABLE`) |
| D5 | Patch and branch | patch 10 after patch 9 (KDB50), branch `debna` on `kdb50`; also on `integration` after each tested step |
| D6 | Network boot (P5) | optional, last. Ulli doubted that the big VAXen could boot from the network; checked: the 8200 family can (the guide's own example `B ET50` on a VAX 8250, VMB code `BTD$K_AIE_NI` = 98 in VMS V4.7, the ET boot program in the 8350's EEPROM, port-vax mailing list); see section 9 for what SIMH would need |

## 9. Results (2026-11-16)

Branch `debna` (on `kdb50`), also on `integration`; patch 10 `opensimh_10_debna_v1.patch` (after patch 9 v2). Tests in `tests/debna/` (README there).

| Phase | Result |
|---|---|
| P0 | Settled with short VMS traces and a disassembly of ETDRIVER (SIMH `EXAMINE -V -M` of the loaded driver, 801F79B0-801FD2F0), see the findings below |
| P1 | **Pass** (`p1.ini`). `SYSGEN SHOW/ADAPTER`: "7  BI - AIE Adapter with NI port only"; `ETA0` online; DECnet line/circuit **BNT-0** (VMS V4.7 names the DEBNA line after the DEBNT; `ET-0` is refused with "invalid device name"); hardware address = `SHOW ET MAC`; circuit up, hellos sent, no failures, also without a partner |
| P2 | **Pass** (`p2.sh` with `p2_8200.ini` and `p2_mv2.ini`). VAX 8200 (MV1 = 1.1, BNT-0) and MicroVAX II with DELQA (MV2 = 1.2, QNA-0) over a SIMH UDP link: adjacency up, MV2 reachable, `HELPLIB.HLB` (2235 blocks) to MV2 and back with 0 differences, remote DIRECTORY, `NCP LOOP NODE MV2` 50 x 1000 bytes, `SET HOST MV2` (login, LOGOUT), line counters on both sides consistent (about 3400 frames, 1.4 MB each way, no failures) |
| P3 | **Pass** (`p2.sh ... p3_8300.ini`). VAX 8300: `START/CPU`, two CPU-bound loop processes (60 s CPU each, attached processor in the EXECUTE state), the P2 DECnet test at the same time |
| P4 | **Pass.** KDB50 boot test (`tests/kdb50/p3.ini`): system disk DUB0, 0 differences; DMB32 printer test (`tests/dmb32/p3.ini`, boot from the UDA50 on the DWBUA): DMB32 and LP11 output identical; the DELQA (unchanged SIMH code) was the partner of every P2/P3 run. `vax8200` and `microvax2` build without warnings |
| P5 | **Open (decision for Ulli).** SIMH's built-in VMB of the 8200 has no DEBNA driver: VMB halts at once with R0/R2 = 98. A real 8200 boots from the network with its console EEPROM boot program "ET", which SIMH does not have. Ways: (a) VMS V4.7's own `VMB.EXE` (if it has the AIE_NI driver; it has to be taken off the disk image first) with SIMH's external VMB loading, or (b) a MOP client in SIMH; both need a MOP load host and an image to load. `BOOT ET` is therefore not in the boot table yet (known issues case 69). **Tried (a):** VMS V4.7's VMB.EXE (taken off the disk image with `tools/vhd_read_blocks.py`, loaded by hand at 200) boots VMS from the KDB50, but halts with code 98 like the built-in one: its boot driver table has the device codes 0, 1, 2, 3, 17, 33, 64 and a generic entry, no DEBNA. **Ulli: open until further notice** |
| P6 | `HELP ET`; this document, the overview, known issues (case 69); patch 10 verified: the series 1, 2, 3, 4, 6, 5, 7, 8, 9 v2, 10 on 87eb7d5 gives the `integration` sources. No ODS change (the DEBNA makes no sound) |

**Findings with VMS V4.7 (where the NetBSD driver left a point open or was misleading):**

| Point | Finding |
|---|---|
| PQB vector and level word (0x28) | `<15:2>` vector, `<1:0>` level; VMS writes 0572: vector 15C (the BR5 slot of node 7), level 2 = BR5 |
| Free queues | 4 free queues; the FQB gives each its entry length: messages 96 bytes, datagrams 80 bytes. The port must not write past them (a 512-byte write-back corrupted VMS pool) |
| Port instruction timing | VMS gives PS OWN back right after it writes PC, so the port takes an instruction a little later (50 instructions), as the board's MicroVAX does; otherwise the ENABLE status interrupt is lost and VMS gives up after 10 s |
| PTDB index | any longword (VMS: an address in its UCB); received datagrams carry it back |
| WPARAM | the second address field (offset 8) is the address to use (DECnet AA-00-04-00-01-04), the first stays zero; the answer carries the hardware address in the first field (NetBSD reads it from there) |
| Transmit status | VMS reads a transmit's completion status from **byte 12** of the entry (0 = success) and does not clear it when it reuses an entry (a former receive entry came back with 31 there): the port must write it. A missing write was the cause of "circuit down, circuit fault, Line synchronization lost" |
| Receive status | byte 0D; byte 13 selects the VMS user of the entry |
| Counters | READ COUNTERS returns the 64-byte DEC Ethernet counter block (layout found with a byte pattern and NCP SHOW LINE COUNTERS; in `vax_debna.c`) |
| Host transport errors | `eth_write` fails on a UDP link while the partner is not listening; reporting that as a send failure made VMS drop the circuit. A real DEBNA on an empty cable sends successfully, so such errors are only traced |
| Frames | VMS builds the whole frame including DECnet's length word; the port adds nothing but the padding to 60 bytes |

## 10. VMS V5.5 (2026-11-17, patch 10 v2)

Ulli: DECnet start under VMS V5.5 failed on the DEBNA (`%SYSTEM-F-TIMEOUT` on line BNA-0; V5.5 names the line and circuit BNA-0, V4.7 BNT-0). Two differences of V5.5's ETDRIVER, found with `SET ET DEBUG=PORT;MSG;QUE;XMT`:

| Case | V5.5 does | Fix |
|---|---|---|
| 74 | No message free queue: free queue 0 has entry length 0, the message entries are V5.5's own; free queue 2's length is set only after INIT | Lengths read again before each command queue; a response is clamped to the command entry when there is no message free queue |
| 75 | Immediate datagrams (opcode 3): the frame is in the queue entry from 0x1C, length = LEN - 10 | Opcode 3 sends the frame from the entry |

Test `tests/debna/p55_8200.ini` (with `p2.sh` v3; partner: MicroVAX II with VMS V4.7, node MV2 = 1.2): adjacency up, copies there and back with 0 differences, remote DIRECTORY, NCP LOOP NODE, SET HOST; about 3.4 MB each way, no failures. VMS V4.7 P2 passes unchanged.
