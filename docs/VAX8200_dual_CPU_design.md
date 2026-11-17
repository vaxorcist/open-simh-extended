<!-- open-simh-extended/docs/VAX8200_dual_CPU_design.md  v2, 2026-11-15 (v2: implemented, results, KA820 manual findings; v1: first design plan for Ulli's decision) -->
# VAX 8200/8300: a running second KA820 (design plan)

Prepared with Claude for Ulli (vaxorcist), 2026-11-14. Status: **implemented 2026-11-15** (patch 7, branch `vax8300-mp`); Ulli accepted all proposals of section 9. Results in section 10. Background: known issues case 65.

## 1. Goal

`SET KA1 ENABLE` makes the simulated VAX 8200 a VAX 8300: two KA820 processors on the VAXBI. VMS V4.7 with its multiprocessing product key can start the second processor (`START/CPU`), show it (`SHOW CPU`), run processes on it and stop it again (`STOP/CPU`). Without `SET KA1 ENABLE` the simulator behaves exactly as today, and no other VAX simulator changes.

## 2. What exists today (Open SIMH 87eb7d5, unchanged in our patches)

- **KA1 is only a BI node.** `VAX/vax820_ka.c` answers reads of its BIIC registers (device type KA820, CSR, BER, EICR, IDEST). VMS therefore sees a second processor.
- **Prepared for two CPUs, never finished (Matt Burke, 2019):**
  - per-CPU bit masks for the interval timer interrupt (`tmr_int`), the inter-processor interrupt (`ipir`, IPR IPIR) and the console output (`tto_csr[KA_NUM]`);
  - `ka_pcsr[2]`, `ka_biic[2]`, `mchk_flag[KA_NUM]`;
  - the console channel between the processors (RXCD register): the primary's commands `I` (initialize), `D/G n value` (deposit a register) and `S address` (start) are parsed.
- **Missing:**
  - `cpu_setreg` and `cpu_start` are called only under `#if defined (VAX_MP)`. Nothing defines `VAX_MP`, and the functions do not exist.
  - `cur_cpu` is never set, so it is always 0.
  - The VAX CPU core (`vax_cpu.c` and friends) has exactly one set of processor state.
- **Result:** VMS sends the start commands and gets the echo, but the secondary never runs, never reports in, and `START/CPU` ends with `%SYSTEM-F-DEVOFFLINE`.

## 3. How VMS V4 multiprocessing on the 8300 works (what the simulator must deliver)

- **VMS V4.4 and later:** asymmetric multiprocessing (ASMP) on the VAX 8300 and 8800, enabled by a product key shipped with the VMS/8300 kit. Ulli's image has it.
- **The two processors:** the primary does all I/O, interrupts and most kernel work. The attached processor runs mainly user-mode work of compute-bound processes and asks the primary for everything else.
- **CPU IDs:** `START/CPU` takes the CPU ID, which on the 8300 is the processor's VAXBI node number (VMS V5.4 help text); here that is KA0 node 0 and KA1 node 1.
- **Starting the secondary:** the primary drives it through the console channel RXCD with console commands (initialize, deposit registers, start). NetBSD's `ka820.c` (`ka820_startslave`, `ka820_txrx`) does the same.
- **Running:** the processors then signal each other with the KA820 inter-processor interrupt (IPIR, SCB vector 0x80) and share memory with interlocked instructions.
- **Owner's manual:** chapter 5 of the VAX 8200/8300 Owner's Manual (AZ-GN4AC-TE, Sep 1986) covers booting with multiprocessing and the attached processor's console. Details still to read: section 9, item 6.

## 4. Design

### 4.1 Execution model: two processors take turns on one host thread (proposal)

- **Turns:** the simulator runs CPU 0 for a number of instructions (the quantum), then CPU 1, and so on.
  - The switch happens only at an instruction boundary, at the top of the instruction loop in `sim_instr`. No instruction is ever half done, so VAX interlocked instructions (BBSSI/BBCCI, ADAWI, INSQHI/REMQHI and the other interlocked queue instructions) are atomic for free.
  - The quantum is a register, default 100 instructions (to be tuned).
  - A processor that is halted, or not yet started, is skipped.
- **Why not real host threads** (Sergey Oboguev's VAX MP does that for a MicroVAX 3900):
  - it needs a thread-safe SIMH core, memory barriers and interlocks emulated with host atomics;
  - it is far more code and many more failure modes;
  - two KA820s at 0.8 VUP each need no host parallelism.
  - Interleaving is deterministic, so problems can be reproduced.
- **Time:** simulated time advances with the primary's instructions only.
  - The secondary's instructions cost no simulated time, so in guest time both processors run at full speed, as on a real 8300.
  - The realism THROTTLE (400K) then means 400K instructions per second per processor, i.e. up to 800K for the host. This is no problem on today's hosts.
  - Device timing and the sound realism mode are unchanged.

### 4.2 Processor state: one context per CPU, swapped at the switch

The CPU core keeps its state in globals, used everywhere for speed. Instead of rewriting every access, the state of the processor not running is kept in a context structure, and the switch exchanges it with the globals:

- **Registers:** `R[16]`, `STK[5]`, `PSL`.
- **Memory management and process control:** `SCBB`, `PCBB`, `P0BR/P0LR`, `P1BR/P1LR`, `SBR/SLR`, `mapen` and the dynamic copies in `vax_mmu.c` (`d_p0br` …).
- **Interrupt and exception state:** `SISR`, `ASTLVL`, `trpirq`, `in_ie`, `hlt_pin`, `mem_err`, `crd_err`, `recq[]`, `fault_PC`, `p1/p2`, `mchk_va/mchk_ref`, `badabo`, `mxpr_cc_vc`, `pme`.
- **Instruction prefetch:** `ibufl/ibufh/ibcnt/ppc` are flushed at the switch.
- **TLB:** `stlb[]`/`ptlb[]` are large (2 × VA_TBSIZE entries). They become pointers to one array pair per processor, so the switch moves two pointers and copies no entries.
- **Cost:** about 50 words per switch. At a quantum of 100 instructions that is well below 5 % of the simulation time.
- **Scope:** all of it under `#if defined (VAX_820)` (or a run-time flag that only the 8200 sets), so the other VAX simulators compile and run exactly as before. The single-processor 8200 never swaps.

### 4.3 Per-processor devices and registers (KA820)

| Item | Today | Needed |
|---|---|---|
| Interval timer (ICCS/NICR/ICR) | one timer, interrupt bit per CPU | one timer per KA820 (each processor has its own clock interrupt) |
| TODR / watch chip | one | stays one (VMS reads the primary's) |
| Console terminal (TXCS/RXCS) | `tto_csr[2]`, input to CPU 0 only | stays: the secondary has no terminal of its own, only RXCD |
| RXCD (console between processors) | parsed, `D` and `S` without effect | `D`: set the secondary's register; `S`: start it (PC, PSL kernel mode, IPL 31, memory management off); `I`: initialize it; halt → console prompt `>>>` back to the primary |
| IPIR (inter-processor interrupt) | bit mask | stays; delivered to the CPU it is meant for through `cur_cpu` |
| PCSR (port controller CSR) | `ka_pcsr[2]` | stays; check restart halt / run bits per CPU |
| BI interrupts (IDEST) | all to CPU 0 | by the interrupt destination register; with VMS V4 ASMP everything goes to the primary |
| Machine check | `mchk_flag[2]` | stays |
| HALT instruction, halt by the primary | stops the simulator | on CPU 1: only CPU 1 stops (console mode of the secondary); the simulator keeps running |
| SID, boot RAM, EEPROM | one | SID the same; EEPROM per CPU only if VMS reads it (check) |

### 4.4 SCP (operator interface)

- `SET KA1 ENABLE`/`DISABLE` as today; `SET CPU QUANTUM=n` (or a register `QUANTUM`).
- `SHOW KA1` shows the secondary's state (not started, running, halted, PC); `EXAMINE KA1 R0`, `PC`, `PSL` (registers of the KA1 device).
- `EXAMINE`/`DEPOSIT` of CPU registers and virtual addresses keep working on CPU 0.
- The CPU history (`SET CPU HISTORY`) gets a CPU column; a breakpoint can be limited to one CPU (later, if needed).
- Idle: the simulator idles only when both processors are idle.
- Debug: `SET KA1 DEBUG=...` for start/stop, RXCD and IPI traffic.

## 5. Steps (each one tested and delivered on its own)

1. **State split, no behaviour change:**
   - the context structure, TLB pointers and swap function, all under the 8200 guard;
   - tests: all VAX diagnostics, VMS V4.7 on the 8200, 750 and MicroVAX II as before, single-processor 8200 timing unchanged.
2. **Scheduler and start:**
   - turns in `sim_instr`, time accounting, the RXCD commands `I`, `D`, `S`, the secondary's HALT;
   - test: a small program on the primary starts the secondary through RXCD; both count in memory.
3. **Per-processor timer and interrupts:**
   - the interval timer per KA820, IPIR delivery, IDEST routing;
   - test: both processors take clock interrupts; IPIs both ways.
4. **SCP:** SHOW/EXAMINE KA1, quantum, history column, idle, debug.
5. **VMS V4.7 multiprocessing with Ulli's image:**
   - `START/CPU`, `SHOW CPU`, compute-bound jobs on both, `STOP/CPU`, `START/CPU` again, SHUTDOWN;
   - in normal and realism mode;
   - plus a boot test (single and dual) and the 8200 regression (console RX50, STABACKIT).
6. **Patch, docs, known issues:** case 65 closed, handover, overview, device table if relevant.

## 6. Effort (honest estimate)

- Steps 1 and 2 are the bulk: careful work in the CPU core, about one long session each.
- Steps 3 and 4: about one session together.
- Step 5 is the unknown: VMS may expect KA820 details that are documented only in the technical manual (secondary self-test status, EEPROM contents, PCSR bits). One to several sessions, depending on what turns up.
- **Total:** roughly four to six sessions of the kind we have had, with Ulli's test runs in between.

## 7. Risks

- **Undocumented KA820 behaviour** that VMS ASMP relies on: the secondary's self test and power-up state, the console protocol details, the restart halt bit. Mitigation: the KA820/KA825 Technical Manual and the Owner's Manual chapter 5, Ulli's knowledge.
- **The CPU core is shared by all VAX simulators:** a mistake in step 1 would hurt every VAX. Mitigation: compile guard, full regression after step 1.
- **Quantum effects:** a processor spinning on a lock waits up to one quantum for the other to release it. That is correct but slower. If VMS spins visibly, the switch can come early when a processor loops on a failed interlocked instruction (optional optimization).
- **VMS V4.7 ASMP specifics** (e.g. how it times out the start, `MULTIPROCESSING`-type SYSGEN parameters, MP.EXE loading) are not known here yet: Ulli's image and documents will tell.

## 8. Where it goes

- **New patch 7** (`opensimh_7_vax8300_mp`): it touches `vax_cpu.c`, `vax_mmu.c` (shared core) and the 8200 files.
- **Because the 8200 files are also changed by patch 3** (DWBUA, BOOT CS) and **patch 5** (console RX50, THROTTLE), patch 7 is built on top of the whole series: order 1, 2, 3, 4, 6, 5, 7, the `integration` branch.
- **Branch `vax8300-mp`.**

## 9. Decisions for Ulli

1. Execution model: processors take turns on one host thread (recommended), or real host threads (VAX MP style, much larger)?
2. Time: simulated time advances with the primary only, so both processors run at full speed in guest time (recommended)?
3. Quantum default 100 instructions as a register (`SET CPU QUANTUM=n`): OK?
4. Patch 7 on top of the whole series (1, 2, 3, 4, 6, 5, 7): OK?
5. Model name: with KA1 enabled, should SHOW CPU / the banner say VAX 8300 (and KA825 / 8350 later)?
6. Documents: do you have the **KA820/KA825 Processor Technical Manual** (EK-KA820-TM-003) and VMS V4.4 multiprocessing documentation? The Owner's Manual (AZ-GN4AC-TE, chapter 5) is online.
7. Your VMS setup for multiprocessing: which SYSGEN parameters or startup commands does your image use (e.g. loading MP.EXE)? A console log of the boot and `SHOW CPU` would help.
8. Single-processor 8200 timing must stay exactly the same: agreed?

## Sources

- Computer History Wiki, VAX 8300 (VMS V4.4 support); VAXcluster Systems Rev F1 (Jul 1986): the 8300 is an asymmetric dual processor, multiprocessing through a product key in the VMS/8300 kit.
- Gordon Bell's DEC timeline: VMS V4.4 with ASMP support for the VAX 83xx and 88xx.
- VMS V5.4 HELP START/CPU: on the VAX 8300 the CPU ID is the VAXBI node number.
- NetBSD `sys/arch/vax/vax/ka820.c` (`ka820_startslave`, `ka820_txrx`, IPI vector 0x80).
- Sergey Oboguev, VAX MP (oboguev.net/vax_mp): multiprocessor SIMH VAX with host threads, MicroVAX 3900 based.
- VAX 8200/8300 Owner's Manual AZ-GN4AC-TE (Sep 1986), chapter 5 "Multiprocessing with the VAX 8300".

## 10. Implementation and results (2026-11-15)

### What VMS V4.7 does (traced)

- **Enabling:** `DEFINE/SYSTEM/EXEC MP MP8SS` (Ulli), and `SYS$SYSTEM:MP8SS.EXE` must exist (on Ulli's kit it was `MP_8SS.EXE`: copy it). Without the logical name `SHOW CPU` and `START/CPU` give `%SYSTEM-F-DEVOFFLINE`.
- **START/CPU:** the primary sends to KA1's RXCD: CTRL/P, `I`, `D/G E 00000200` (SP), `D/G C 6` (AP), `S 00000100`.
  - At physical 100 VMS has `JMP @FC`; FC holds the start of the attached processor's code.
  - That code:
    - writes `F03` to its own TXDB;
    - sets bit 16+node in the primary's BIIC IPINTR mask (BBSSI on 2000 0014) and writes its own mask through node private space (2008 0014);
    - takes the restart parameter block at SP − 200 (physical 0), loads SBR (RPB+AC), SLR (RPB+B8), SCBB, P0BR/P0LR, turns on memory management and jumps into system space.
  - `SHOW CPU`: `Attached processor is in the IDLE state`; under load `EXECUTE`.
- **STOP/CPU:** the attached processor executes HALT (halt code 06). VMS restarts it at once with the same command sequence into a park loop: `JMP @FC` with FC = 100, a jump to itself at IPL 31. The next `START/CPU` points FC at the start code again. While stopped, VMS V4.7 answers `SHOW CPU` with DEVOFFLINE.

### KA820 Technical Manual (EK-KA820-TM-003) facts used

- RXCD: bit 15 busy, bits 11:8 sender node, 7:0 data. MTPR RXCD writes the remote register (V set if busy); MFPR RXCD reads the local one (V set if empty).
- An attached processor halts into console mode and sends "?nn", "PC = xxxxxxxx" and ">>>" to the logical console (the primary's RXCD); halt codes ?02 console halt, ?06 HALT instruction.
- IPIR (IPR 16): write a node mask to send an inter-processor interrupt. PCSR bit 3 IP INTR, bit 4 clears it; delivered at IPL 14.
- Every KA820 has its own interval clock (ICCS/NICR/ICR). RSTRT HLT (PCSR bit 31) is 1 on attached processors (halt, never auto-restart).

### Implementation

- `VAX/vax_defs.h`: `VAX_MP` for the 8200 (before the idle-loop macro); a secondary's jump to itself is no infinite-loop stop and no idle.
- `VAX/vax_cpu.c` (all under `VAX_MP`):
  - `CPU_CTX cpu_ctx[]` and `cpu_mp_switch` (registers, PSL, the IPR copies, the TLB pointers);
  - the turn at the top of the instruction loop (register CPU QUANTUM, default 100), with `fault_PC` set for the processor switched to;
  - the primary's instructions alone count simulated time;
  - `cpu_setreg`/`cpu_start`/`cpu_mp_stop`;
  - HALT on a secondary halts only it;
  - no host idle while a secondary runs;
  - SCP always gets processor 0 back; a stop during the secondary's turn prints its PC;
  - hidden register MPTEST (switch even to itself, for testing).
- `VAX/vax_mmu.c`, `vax_mmu.h`: one translation buffer per processor, `stlb`/`ptlb` point to the running one's.
- `VAX/vax820_defs.h`: `CPU_CTX`.
- `VAX/vax820_ka.c`:
  - RXCD commands act on the secondary; halt message to the primary;
  - BIIC IPINTR mask, destination and source registers; node private space 2008 0000-00FF maps the local BIIC;
  - KA1 registers (EXAMINE KA1 PC ...), SHOW KA1 STATE, HELP KA1, SET KA1 DEBUG=RXCD;MP;
  - the RXCD command echo `>>> ...` on the simulator console became debug output.
- `VAX/vax820_bi.c`: a secondary only gets its own clock, IPI, console output and software interrupts (device, RXCD, floppy and terminal interrupts go to the primary); console output interrupt cleared per processor; console halt on a secondary never reboots; node private BIIC range.
- `VAX/vax820_stddev.c`: KA1's own ICCS/NICR (registers ICCS1, NICR1), ticking with the primary's calibrated timer; timer interrupt bit per processor.

### Tests (`tests/vax8300/`, Ulli's VMS V4.7 image with the multiprocessing key)

| Test | Result |
|---|---|
| mp3 (normal mode): START/CPU, SHOW CPU, STOP/CPU, SHOW CPU, START/CPU, SHOW CPU (twice) | IDLE; DEVOFFLINE while stopped; IDLE again |
| mp2 (realism mode, THROTTLE 400K): a CPU-bound loop (4,000,000 SOBGTR) | 10.6 s without, 10.8 s with the attached processor |
| mp2: two loops at once (one in a subprocess) | 12.1 s and 13.1 s (one after the other: about 21 s); SHOW CPU: EXECUTE |
| 8200 diagnostics, normal and with a switch after every instruction (MPTEST, QUANTUM 1) | passed |
| VMS V4.7 boot with a switch every 3 instructions (no KA1) | boots, works |
| SHOW CPU: model VAX 8200, with KA1 VAX 8300, with MODEL=8250 VAX 8350 | ok |
| VAX 780, 750, 730, 8600 diagnostics, MicroVAX 3900 core test; all VAX simulators and pdp11 built | passed, no warnings |

Normal mode (no throttle): both processors share one host thread, so two CPU-bound jobs take as long as one after the other. Only with a throttle (e.g. the realism mode) does the second processor add throughput.

### Not done / open

- KA1's interval timer ticks with the primary's (VMS uses 10 ms on both); single step and ICR reads come from the primary's timer.
- No breakpoint per processor; the CPU history does not mark the processor.
- Model name (decision 5): with KA1 enabled SHOW CPU says VAX 8300 (VAX 8350 with SET CPU MODEL=8250); the start-up banner is printed before the configuration and still says VAX 8200.
