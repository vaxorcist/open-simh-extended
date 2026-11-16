<!-- open-simh-extended/docs/KLESIB_VAX8200_design.md  v1, 2026-11-16 (v1: KLESI-B (TB) and SET UBA DISABLE: design, decisions, implementation, VMS V4.7 results) -->
# KLESI-B (TB) and the VAXBI-only mode on the VAX 8200/8300

Prepared with Claude for Ulli (vaxorcist), 2026-11-16. Status: **done for VMS V4.7** (patch 11 `opensimh_11_klesib_v1.patch`, branch `klesib`, on `integration`).

## 1. Request and decisions

Ulli, 2026-11-16: "implement the KLESI-B according to the ULTRIX driver (and NetBSD if necessary). Additionally implement a SIMH command to disable the DWBUA for a VAXBI-only mode."

Decisions (Ulli, 2026-11-16):

| No. | Question | Decision |
|---|---|---|
| D1 | One TMSCP device or a second one? | A **separate KLESI-B controller with shared code**: "We still need the KLESI-U for UNIBUS and there are the TUK50 and TUK70 UNIBUS controllers as well." The TQ device stays the Unibus/Qbus controller. |
| D2 | Device name | **TB**, units TB0-TB3 |
| D3 | Default VAXBI node | **8** (`SET TB NODE=n`, 2-15) |
| D4 | Default state | **Disabled**, drives **TU81** (`SET TB ENABLE`) |
| D5 | `SET UBA DISABLE` with enabled Unibus devices | **Auto-disable**: refused while a Unibus unit is attached; otherwise all enabled Unibus devices are disabled and listed; `SET UBA ENABLE` brings them back |
| D6 | Patch and branch | Patch 11, branch `klesib` (from `debna`) |

## 2. Sources

- **ULTRIX V2.0 sources** (Ulli's tape `ult020src.tap`; DEC copyright, not in the repository):
  - `sys/vaxbi/bla.c`: the KLESI-B attach routine `blainit`. It treats the node as a DWBUA (`struct bua_regs`, "looks alot like BUA"), puts the TMSCP port at Unibus address 0174500, sets the vector offset register to the BI's SCB page (`bua_vec = SCB_BI_OFFSET`), and lets `unifind` probe the internal Unibus. The adapter type is `UBABUA|UBABLA`.
  - `sys/vaxbi/buareg.h`: the register layout (BIIC; control/status at 0x720; vector offset; failed Unibus address; micro-diagnostic registers; data path registers; buffered data paths; 512 map registers at 0x800).
  - `sys/vaxbi/bua.c` `buainit`: the self-test bit of the BIIC (`bisst`) resets the adapter, then the vector offset and the interrupt destination are written again.
  - `sys/vaxbi/bireg.h`: `BI_BLA = 0x0103` (the device type).
  - `sys/vaxuba/tmscp.c`: the KLESI-B is a BI tape interface (`DEV_KLESI`).
- **NetBSD:** not needed. Its KLESI-B support is a workaround (Ulli: "not a real VAXBI device driver").

## 3. Design

### 3.1 The adapter (`VAX/vax820_klesib.c`, new)

- **BI node:** device type 0103; BIIC registers; the DWBUA's own registers at the same offsets: control/status, vector offset, failed Unibus address, failed BI address, micro-diagnostic, data paths, buffered data paths, 512 map registers (the last 16 not usable).
- **Window:** the node's window (`WINADDR(node)`, 256 KB) is the internal Unibus. Only the port's IP (774500) and SA (774502) respond. Byte and aligned-word accesses are accepted, as on the DWBUA. Any other address is a Unibus timeout: the adapter sets its TO bit and the failed Unibus address, and raises the error interrupt (if enabled).
- **Memory access** of the port goes through the adapter's map. It uses the DWBUA's buffer routines, which now take the adapter's map function (`bua_rb`, `bua_rw`, `bua_wb`, `bua_ww` in `vax820_uba.c`).
- **Interrupts:** the port's request is at IPL 15 (BR5) on the node's nexus bit. The vector is the vector offset register ORed with the port's vector from TMSCP step 1, as on the DWBUA. The adapter's error interrupt uses the BIIC's error interrupt control register. Both are acknowledged through `nexusA` (patch 8); if both are pending, the one at the higher level is served first.
- **Self-test** (BIIC CSR bit 10) and **Unibus power init** (CSR bit 17) reset the port.
- **`SET TB NODE=n`** refuses a node that an enabled BI device already uses (UBA 4, memory 2 and 3, KDB50, DEBNA, ...).

### 3.2 The port (`PDP11/pdp11_tq.c`, shared with TQ)

- **Shared state:** the per-controller state (SA, rings, packets, init state, credits, host timer, drive type, plug numbers, TK50 streamer models) moved into a `TQ_CTX` structure. The code keeps its names as macros for the current controller (`tqc`).
- **Controllers:** controller 0 is TQ (unchanged); controller 1 is TB (VAX 8200 only).
- **Entry points:** every entry point (register access, unit services, interrupt acknowledge, reset, attach/detach, SET/SHOW, callbacks) selects the controller first.
- **Hooks:** memory access and interrupts go through hooks in the context (`Map_*` and `SET_INT(TQ)` for TQ; the adapter's `kb_*` for TB).
- **TB settings:** the TB device has its own units, registers (port and adapter), SET/SHOW (TU81 default, `NODE`, adapter `VECTOR`) and help. Its port is always a Unibus port (SA step 1 without Q22).

### 3.3 VAXBI-only mode (`VAX/vax820_uba.c`, `VAX/vax820_defs.h`)

- **The UBA can be disabled:** `uba_dev` gets `DEV_DISABLE`. `uba_reset` acts on a change of the UBA's enable state.
- **Disabling** is refused while a unit of an enabled Unibus device is attached; SCP has already set the flag, so it is cleared again. Otherwise every enabled Unibus device is disabled and remembered, and a message lists them. The DWBUA's node leaves the BI (`build_nexus_tab` skips disabled devices), and its window leaves the I/O space.
- **Enabling** enables the remembered devices again.
- **`ADDR_IS_IO`** covers the DWBUA window only while the UBA is enabled, plus the KLESI-B window (`kb_winbase`, 0 while TB is disabled). `ReadIO` and `WriteIO` pass the KLESI-B window to the adapter.

## 4. Results (VMS V4.7, Ulli's image VMS047_1.R81 on the KDB50)

Test scripts: `tests/klesib/mkini.py` (generates b1-b3).

| Test | Configuration | Result |
|---|---|---|
| b1 | 8200, DWBUA + KLESI-B, new tape on TB0 | SYSGEN SHOW/ADAPTER: node 8 "BI - LESI Adapter". SHOW/CONFIGURATION: PTB, nexus 8, CSR 774500; the TQ (TUK50) stays PTA on the DWBUA. MUB0 = TB0. INITIALIZE, BACKUP SYS$HELP:*.HLB (24 files, 4324 blocks), BACKUP/LIST, restore to [TBR], DIFFERENCES HELPLIB.HLB: 0 differences. **Pass** |
| b2 | 8200, `SET UBA DISABLE` (VAXBI-only), the b1 tape | Boot from the KDB50. No UB0 in SYSGEN; the port is PTA (nexus 8) and the TU81 is MUA0. BACKUP/LIST of the b1 save set (24 files, 4324 blocks); a second save set (SYSGEN.EXE) appended and listed. **Pass** |
| b3 | 8300 (`SET KA1 ENABLE`), VAXBI-only, the b1 tape | SYSGEN lists both CPUs (nexus 0 and 1); otherwise as b2. **Pass** |
| b4 | 8200, TB disabled, the b1 tape on TQ0 (TUK50, TU81, on the DWBUA) | Regression of controller 0 on the 8200: MUA0, BACKUP/LIST 24 files, 4324 blocks. **Pass** |

Regression:

- **TK50 (TQ, controller 0):** `tests/tk50/tktest.py` v4, K1 (MicroVAX II), K2 (realism mode, MicroVAX II) and K3 (realism mode, VAX-11/750, TUK50): **pass**.
- **DWBUA:** `tests/dmb32/p3.ini` (VMS booted from the UDA50 on the DWBUA, LP11 and DMB32 printer ports, outputs identical): **pass**.
- **Builds:** pdp11, vax780, vax750, vax730, microvax2, microvax3900 and vax8200 build without warnings.

## 5. Findings

- **VMS V4.7 knows the KLESI-B** ("BI - LESI Adapter"). Its TMSCP port is configured like a Unibus port on nexus 8 and started late in the startup, about 30 s after the login, like the other TMSCP ports.
- **VMS probes the KLESI-B's internal Unibus** for the standard CSRs (UDA50 at 772150 and others). Each probe is a Unibus timeout, which gives an adapter error interrupt that VMS handles. This is the same behaviour as on the DWBUA.
- **Port error count 1:** after the startup, SHOW DEVICE shows an error count of 1 on PTA0 (TB, b2/b3), and also on the TQ's PTA0 and the KDB50's and UDA50's PUA0/PUB0 (b4). VMS logs this for every MSCP/TMSCP port it starts; it is not a KLESI-B effect.
- **The VMS names** depend on the order: with the DWBUA, TQ is PTA/MUA and TB is PTB/MUB; without it (VAXBI-only), TB is PTA/MUA.
- **No boot from TB:** the 8200's VMB (built in, and VMS V4.7's VMB.EXE) has no tape boot driver (see case 69 for its driver table). `BOOT TB` is not offered.
