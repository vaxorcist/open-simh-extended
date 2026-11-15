<!-- open-simh-extended/docs/KDB50_VAX8200_design.md  v4, 2026-11-15 (v4: section 10, results P0-P6, patch 9; v3: D1-D6 accepted by Ulli, DWBUA disable as a later wish; v2: Ulli's answers: ULTRIX postponed, KDB50 is the 8200's default system disk controller, UDA50 behind the DWBUA the alternative; v1: first implementation plan for Ulli's decision) -->
# KDB50 on the VAX 8200/8300 (implementation plan)

Prepared with Claude for Ulli (vaxorcist), 2026-11-15. Status: **done for VMS** (patch 9 `opensimh_9_kdb50_v1.patch`, branch `kdb50`, on `integration`); ULTRIX postponed (section 10). Later wish (Ulli): the DWBUA can be disabled, which takes all Unibus options with it (section 8).

Sources:
- EK-KDB50-UG-001 *KDB50 Controller User Guide*, supplied by Ulli. Chapter 3 is the programming information.
- The VMS V4.7 disk VMS047_1.R81: SYSGEN, the KDB50 error-log texts, `DT$_BDA50`, and the VMB boot codes (`BTD$K_BDA = 33`).
- 4.3BSD and NetBSD `kdbreg.h`/`kdb.c`. These are used only for interface facts, and the code is not copied (the same rule as for the DMV11).
- In SIMH: the RQ (MSCP) device, `PDP11/pdp11_rq.c`, and the DMB32 groundwork on the 8200: the BI node, device vectors (known issues case 67) and the DMA engine with VAX page tables.

The KDB50 technical manual is not available.

## 1. What the KDB50 is

The KDB50 is the VAXBI version of the KDA50/UDA50: an SDI disk controller for up to **4 DSA drives** (RA60, RA70, RA80, RA81, RA82, RA90 ...). It has two modules, T1002 (SDI) and T1003 (processor), and talks MSCP over UQSSP. VMS uses DUDRIVER through PUDRIVER, so the disks are `DUA0`... and VMS shows the controller as "BI - Disk Adapter (KDB50)".

What the User Guide settles (chapter 3):

| Item | KDB50 |
|---|---|
| Node space | `2000 0000 + node*2000` |
| DTYPE | `XXYZ 010E` (XX microcode, Y SDI board, Z processor board revision) |
| BICSR after the self-test | `XX01 080n`: STS set, BROKE clear, n = node |
| IP | `bb+F2`, the upper half of GPR0. A read starts command polling (after a connection) and a write has no effect |
| SA | GPR1: SAr (read) at `bb+F4`, SAw (write) at `bb+F6`. ER is bit 15 and the step bits are 11-14, as on the UDA50 |
| Interrupts | The host sets the vector, which is used both for the BIIC error interrupt and for the controller's interrupt (UINTRCSR force bit). INTRDES = the host's node |
| Command limit | 21 (20 MSCP commands + 1 immediate-only) |
| Formats | 512-byte sectors only; MSCP and UQSSP; purge/poll and wrap diagnostics; maintenance read and write of the KDB50 RAM; last-fail packets |
| Self-test | After it, the controller waits for the host's initialization (step 1). LED codes 2/3/4 = stuck in step 2/3/4; 8 = wrap bit (SA bit 14) set |
| Drives | 4, unit numbers 0-254, which must be unique |
| Boot | VMB device code `BTD$K_BDA = 33` (VMS V4.7) |

## 2. What is still open, and where it comes from

| Open point | Source |
|---|---|
| Exact SA step contents on the BI: where the vector goes in step 1, the communication-area address in steps 2/3 (physical? 30 bits?), purge/poll | Trace PUDRIVER on VMS V4.7 (registers and SA values with debug output), then 4.3BSD `kdb.c` |
| **Buffer descriptors:** how a mapped MSCP buffer on the BI is described (virtual address plus page-table pointer) | Trace VMS DUDRIVER/PUDRIVER: what is written into the command packets; the BSD driver points the controller at the page tables, VMS shows the format |
| How VMB on the 8200 starts the controller and reads the boot block (`BTD$K_BDA`) | Boot traces with VMB (`BOOT` with R0 = 33) |
| Error-log and last-fail packets | Not needed for operation; only if VMS insists |

Ulli's rule: no long trial and error. Each open point gets a short, targeted trace; if VMS needs a dialog, I ask Ulli for the prompts.

## 3. Simulator structure (proposal)

| Item | Plan |
|---|---|
| Where | In `PDP11/pdp11_rq.c` as a new controller type `KDB50_CTYPE`, only for the VAX 8200 (`VAX_820`). The MSCP server, drive table, realism (spindles, seek times, sounds) and SDI rules are all reused |
| Device | A separate RQ controller instance with a BI DIB (nexus): device name **`KDB`** (decision D1), units `KDB0`–`KDB3` (4 drives), default **BI node 6** (decision D2; 5 is the DMB32) |
| Bus access | The RQ code reaches memory in 14 places (`Map_ReadW`/`Map_WriteW`/...). These go through two small per-controller routines: Unibus/Qbus map as now, or BI DMA with page tables for the KDB50 |
| DMA translation | The DMB32's page-table walk moves into a shared BI DMA helper, `VAX/vax_bi_dma.c` (used by the DMB32 and the KDB50; decision D3) |
| BI node | BIIC block (DTYPE 010E, BICSR, BER, EINTRCSR, INTRDES, BCICSR, UINTRCSR), node reset/self-test (BROKE while testing, as the DMB32), IP/SA in GPR0/GPR1 |
| Interrupt | `nexusA` acknowledge (case 67) returns the host-set vector; IPL from the host's level setting (BR4/5 as set in step 1 or UINTRCSR) |
| Boot | `vax820_bi.c` boot table: `{ "KDB", BOOT_BDA (33), ... }` with the node in the boot registers as VMB expects |
| Default configuration | **KDB enabled by default** on the `vax8200` (system disk on the KDB50, as on a real 8200). The UDA50 behind the DWBUA (`RQ`) stays available as the alternative (decision D6) |

## 4. Sound

The drives are the same RA drives as on the UDA50/KDA50, so their realism mode (RUN/STOP button, spin-up, seek times, sounds) comes along unchanged. The realism rule "SDI drives only on SDI controllers, at most 4" gets the KDB50 added to the SDI controllers. The controller itself makes no sound.

## 5. Phases and tests

The tests use VMS V4.7 on the `vax8200` (Ulli's VMS047_1.R81). Per Ulli's rule, there is at least one boot test per bootable device.

| Phase | Content | Test |
|---|---|---|
| P0 | Short traces for section 2 (in parallel with P1) | none |
| P1 | BI node and self-test, `KDB50_CTYPE`, BI DMA layer in the RQ code, the KDB device | VMS boots from the **RQ on the DWBUA** as now; SYSGEN shows the KDB50 at node 6; no BROKE, no machine check |
| P2 | UQSSP initialization steps 1-4, rings, interrupts, MSCP with BI buffer descriptors | A second disk on `KDB1` (RA81): `INITIALIZE`, `MOUNT`, `COPY`, `DIRECTORY`, `ANALYZE/DISK`, `BACKUP` of a directory, `DISMOUNT` |
| P3 | **Boot from the KDB50** (VMB `BTD$K_BDA`) | The system disk moved to `KDB0`; `BOOT KDB0` → VMS up, login (boot test) |
| P4 | 8300: KA1 started, disk load while both CPUs are busy | `START/CPU`, copy jobs on both CPUs |
| P5 | Realism mode: RA drives on the KDB50 (SDI rule, spindle buttons) | `SET SOUND ENABLED`, `SET KDB0 ONLINE`, boot; VMS SHUTDOWN spin-down |
| P6 | RQ regression (the bus layer touches all RQ controllers) | MicroVAX II (KDA50/RQDX3) and VAX-11/780 (UDA50) VMS boot and copy tests, as before |
| P7 | Help, docs, ODS device table, known issues, patch 9 | `HELP KDB`; full patch series on 87eb7d5 |

Estimated size: about 600-900 lines (BI front end, BI DMA layer, boot) plus the RQ changes.

## 6. Risks

- **Buffer descriptor format** (section 2) is the main unknown; the VMS trace decides it. If VMS uses a form the DMB32's page-table walk does not cover, that is extra work but no dead end.
- **RQ regression:** `pdp11_rq.c` is large and well tested; the bus layer is small, but every RQ controller on every simulator uses it (P6).
- **Diagnostics** (EVRLF/EVRLG etc. in the User Guide) may check KDB50 details that are not documented here; not a goal.

## 7. Questions for Ulli (answered 2026-11-15)

1. ULTRIX on the KDB50: postponed, as with the DMB32.
2. 8200 configuration: both options; the default puts the system disk on the KDB50 (as on a real 8200), the alternative is the UDA50 behind the DWBUA.

## 8. What this plan does not do

- **Later (Ulli's wish, 2026-11-15):** `SET UBA DISABLE` on the 8200, which removes the DWBUA and with it all Unibus options (RQ, DZ, LP, DMF, ...). Not part of this patch.

- No HSC/CI, no BVP storage ports (`BTD$K_BVPSSP`), no other BI models (D5 of the DMB32 plan applies: other VAX 8xxx models later).

## 9. Decisions

| # | Decision | Decided (Ulli, 2026-11-15) |
|---|---|---|
| D1 | Device name | `KDB` (units KDB0-KDB3); VMS still calls the disks DUAn/DUBn |
| D2 | Default node | 6, settable with `SET KDB NODE=` |
| D3 | Shared BI DMA helper | Move the DMB32's page-table walk into `VAX/vax_bi_dma.c` and use it in both (DRY; DMB32 tests P2/P3 rerun) |
| D4 | Implementation place | Inside `pdp11_rq.c` as a controller type, not a separate copy of the MSCP server |
| D5 | Patch number and branch | Patch 9 after patch 8 (DMB32), branch `kdb50`; also on `integration` after each tested step |
| D6 | How "KDB50 by default" is done | `KDB` enabled by default; `RQ` (UDA50 on the DWBUA) **stays enabled** too, so existing 8200 scripts with `ATTACH RQ0` keep working; `HELP KDB`, the docs and the test scripts show the KDB50 as the standard. Alternative: disable `RQ` by default (then `SET RQ ENABLE` is needed for the DWBUA disk) |

## 10. Results (2026-11-15)

Branch `kdb50` (on `dmb32`), also on `integration`; patch 9 `opensimh_9_kdb50_v1.patch` (after patch 8). Tests in `tests/kdb50/`.

| Phase | Result |
|---|---|
| P0 | Settled without long traces. The **buffer descriptor**: bit 31 of the buffer field = mapped; the map field then holds the **physical address of the buffer's PTEs**, the buffer field the byte offset (4.3BSD/NetBSD `KDB_MAP`, confirmed by VMS: all 669 transfers of P2 are mapped this way, no errors). The **vector** comes from UINTRCSR (VMS writes 118 there; its step 1 has no vector). VMS: MSCP model 18 (`MSCP$K_CM_KDB50`), boot code 33 (`BTD$K_BDA`) |
| P1 | **Pass.** `SYSGEN SHOW/ADAPTER`: "6 BI - Disk Adapter (KDB50)"; `PUB0`, `DUB0`-`DUB3`. Found on the way: PUDRIVER resets the node and then **waits about 20 s** (its self-test timeout: "tim,int" in SDA) before it programs INTRDES, UINTRCSR, EINTRCSR and starts step 1; nothing else is needed from the controller (`p1.ini`) |
| P2 | **Pass.** RA81 on KDB1 (DUB1): INITIALIZE, MOUNT, COPY of 223 files (10,891 blocks), DIFFERENCES 0, ANALYZE/DISK (only the missing QUOTA.SYS), BACKUP/VERIFY, DISMOUNT (`p2.ini`) |
| P3 | **Pass (boot test).** `BOOT KDB0`: VMS V4.7 from the KDB50, system disk DUB0, COPY/DIFFERENCES on it; also with the `integration` build (`p3.ini`) |
| P4 | **Pass.** VAX 8300: START/CPU, two CPU-bound loop processes (attached processor in the EXECUTE state), INITIALIZE/MOUNT/COPY of 224 files to DUB1, DIFFERENCES 0 (`p4.ini` v2) |
| P5 | **Pass.** Realism mode: RA drives on the KDB50 accepted, `SET KDB UDA50` refused, KDB0 spins up in 20 s (SET KDB0 ONLINE), VMS boots from it (`p5.ini` v3). An RD54 is accepted too, as on the UDA50/KDA50: known issues case 68 |
| P6 | **Pass.** RQ regression after the per-controller memory access: MicroVAX II (KDA50 system disk, RQDX3 with two RX50, `tests/rx50/vms1.ini`): mounts, DIFFERENCES 0; VAX-11/780 (UDA50 boot, `tests/dmb32/p4a_lp.ini`): printer output byte for byte the same as before. `pdp11`, `microvax3900`, `microvax2`, `vax780`, `vax750`, `vax8200` build without warnings |
| P7 | `HELP KDB`; this document, the overview, known issues (case 68), the device table; patch 9 verified: the series 1, 2, 3, 4, 6, 5, 7, 8, 9 on 87eb7d5 gives the `integration` sources |

D3 (shared BI DMA): the DMB32's DMA engine is now `VAX/vax_bi_dma.c`; the DMB32 tests P2 and P3 pass unchanged.

**Correction found on the way:** the DMB32 test P5 (v1) assumed `LOOP.EXE` on the disk; it was not there, so its CPU loop never ran. Both 8300 tests now build the loop themselves (DMB32 `p5.ini` v2: all async checks pass with both CPUs busy).
