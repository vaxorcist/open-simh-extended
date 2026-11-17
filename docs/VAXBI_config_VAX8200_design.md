<!-- open-simh-extended/docs/VAXBI_config_VAX8200_design.md  v1, 2026-11-17 (v1: configurable VAXBI nodes, twelve MS820 memory boards, SHOW BI, rules; patch 14 v1; VMS results) -->
# VAXBI configuration of the VAX 8200/8300 (patch 14)

Prepared with Claude for Ulli (vaxorcist), 2026-11-17. Status: **done** (patch 14 `opensimh_14_vaxbi_config_v1.patch`, branch `vaxbi-config` on `realism`, on `integration`).

## 1. Request and decisions

Ulli, 2026-11-17: "a compromise between the standard SIMH for the VAX 8200/8300 and real-world conditions":

| No. | Item | Decision |
|---|---|---|
| D1 | VAXBI nodes | Up to 16 (the big 24-slot backplane); every node ID configurable |
| D2 | Memory boards | Up to twelve, MCTL0–MCTL11, each with its own size and VAXBI node; `SHOW MCTL` shows all twelve |
| D3 | `SET CPU nM` | Still works, with default boards: 4M 2 × 2MB, 8M 2 × 4MB, 16M 4 × 4MB, 32M 2 × 16MB, 48M 3 × 16MB, 64M 4 × 16MB, 128M 8 × 16MB. A later `SET MCTLn TYPE=` overrides it; `SET MCTLn NODE=` works after `SET CPU` |
| D4 | Above 128M | Realism mode: `SET CPU` stops at 128M, more only board by board. Normal mode as before (256M, 512M) on SIMH-only 128MB boards |
| D5 | Default nodes | KA0 2, KA1 1, DWBUA 0 (field service rules); KDB50 4, DMB32 5, KLESI-B 6, DEBNA 7, MCTL0–MCTL7 on 15–8 (accepted with the plan) |
| D6 | Rules | At most 3 KLESI-B, 2 DEBNT/DEBNA, 4 storage options (KDB50, CIBCA, CIBCI): rules now, extra instances later. Every DEBNT/DEBNA on a higher node than any KLESI-B: realism mode only |
| D7 | `SHOW BI` | All VAXBI nodes |
| D8 | More than 8 boards | VMS uses only 8 (section 6): allowed up to 12, with a warning |
| D9 | Patch | New patch 14 on top, branch `vaxbi-config` |

Source of the rules: a DEC field service handbook chapter "82/83x0 and VAXBI" (Digital internal, January 1993; Ulli's copy, facts only): configuration 1 (12 slots: 8200, 8300, 8250, 8350) and configuration 2 (24 slots: 8220, 8320), the module list with BI device types, the console self-test that assigns contiguous memory addresses, the memory lock bit (CSR1 bit 13).

## 2. Design

### 2.1 Nodes

- Every BI device keeps its node in its DIB's base address; `SET dev NODE=n` (decimal 0–15, or a hex digit A–F as in DEC's documents) goes through `bi_get_node` (`vax820_bi.c`), which refuses a node used by an enabled BI device or a present memory board. The devices' own checks (DMB32, DEBNA, KDB50: only "not 0 or 1"; KLESI-B: a scan) are gone.
- The fixed nexus assignments `TR_KA0`, `TR_KA1`, `TR_MCTL0/1`, `TR_UBA` are replaced by the DIBs and the defaults `TR_UBA_DEF` 0, `TR_KA1_DEF` 1, `TR_KA0_DEF` 2.
- **KA820:** `ka_node (cpu)` and `ka_cpu_of_node (node)` map processors and nodes: the BIIC register access (node of the address), node private space (running processor's node), the BIIC CSR node, the RXCD destination node (bits <11:8> of MTPR RXCD; a node without a KA820 sets V) and the sender node of MFPR RXCD, the IPIR value (a node mask) and BINID. `SET KA0 NODE=n`, `SET KA1 NODE=n`.
- **DWBUA:** its Unibus window is its node's window (`uba_winbase`, `ADDR_IS_IO`, the BIIC start/end address). The Unibus devices' addresses (DIBs, autoconfiguration, `SHOW dev ADDRESS`) stay constants in the canonical window of node 0 (`UBADDRBASE`); `ReadIO`/`WriteIO` move an access in the real window there. `SET UBA NODE=n`; BOOT gives VMB the DWBUA's node in R1.
- The DMB32's INTRDES debug note compares with the primary's node.

### 2.2 Memory boards (`vax820_mem.c`, rewritten)

- One device `MCTL` with twelve units MCTL0–MCTL11 (the boards); per board: type (MS820-AA 2MB T1008, MS820-BA 4MB T1019, MS820-CA 16MB T1040, SIMH-128MB), present, node (16 = none).
- `SET MCTLn TYPE=x|NONE`, `NODE=n`, `ENABLED`, `DISABLED` (unit-level modifiers; the units are never SCP-disabled, so SET works on boards that are out). A board put in needs a free node. The memory size is the sum of the present boards (`cpu_set_size` with `MCTL_RESIZE`); a refused change (no board, more than 512MB, "Really truncate memory" answered no) is undone.
- `SET CPU nM` (`cpu_set_size`, hook for the 8200) sets the table of D3 (256M = 2 × SIMH-128MB, 512M = 4 × SIMH-128MB), all boards on their default nodes; sizes outside the table (e.g. from RESTORE) get boards of one type if possible. In the realism mode sizes above 128M are refused, and SIMH-128MB boards are refused by `SET MCTLn TYPE` and by `SET REALISM ENABLED`.
- Addresses: contiguous in board order, MCTL0 from 0 (the console assigns them contiguously).
- Registers of a board: DTYPE 0001; BIIC CSR (node, self-test passed), BER, EICR, INTRDES; start and end address (end exclusive, as before); CSR1 with the size in 256KB units (<28:18>), the RAM type (<17:16>: -AA and -BA 256K chips, -CA and SIMH-128MB 1M chips, provisional), writable and write-one-to-clear bits after NetBSD's `ka820.c` (interlock <13>, broke, ECC, error bits, error summary); CSR2 error flags (write one to clear).
- `SHOW MCTL`: one line per board (type, size, node, addresses); `SHOW CPU MEMORY`: the present boards.

### 2.3 SHOW BI and the rules

- `SHOW BI` (a modifier of the BI device): the 16 nodes with device, BI device type, description and details (memory: type, size, addresses; KA820: primary or attached; DWBUA: window), conflicts, the disabled BI devices with their nodes, and the rule check.
- `bi_count_rules` (always): at most 3 KLESI-B (DTYPE 0103), 2 DEBNT/DEBNA (010F/410F), 4 storage options (KDB50 010E, CIBCA 0108, CIBCI 010B). Checked at BOOT.
- `bi_realism_rules` (realism mode): every DEBNT/DEBNA on a higher node than any KLESI-B. Checked at `SET ET NODE`, `SET TB NODE` (the device being changed counts even while disabled), BOOT and `SET REALISM ENABLED`.
- The device kinds are found by their BI device type (read from the device), so later instances (TB2, ET2, KDB2…) are counted without changes.

## 3. Changed files

`VAX/vax820_defs.h`, `vax820_bi.c`, `vax820_ka.c`, `vax820_mem.c`, `vax820_uba.c`, `vax820_syslist.c`, `vax820_klesib.c`, `vax_dmb.c`, `vax_debna.c`, `vax_cpu.c` (hooks in `cpu_set_size`, VAX 8200 only), `PDP11/pdp11_rq.c` (KDB50), `PDP11/pdp11_tq.c` (TB help), `VAX/tests/vax-diag_test.ini` (the diagnostic supervisor attaches the KA820 at node 2 and the DWBUA at node 0).

## 4. Results (2026-11-17)

| Test | Result |
|---|---|
| VAX 8200 diagnostics (EVKAA, EVKAB, EVKAC) | **Pass** (with the new attach nodes; with the old ones EVKAB took a machine check, the DWBUA no longer being at node 4) |
| `tests/vaxbi/c1.ini` (console: defaults, SET CPU, boards, nodes, refusals, 12 boards, SIMH boards, realism rules) | **Pass**, nine refusals as expected |
| VMS V4.7, KLESI-B `tests/klesib/b1.ini`, `b1r.ini` (realism) | **Pass**; SYSGEN SHOW/ADAPTER: UB0 at nexus 0, the CPU at 2, KDB50 4, LESI 6, BI Memory 12–15 |
| VMS V5.5 `tests/kdb50/v55_1.ini` (8200), `v55_2.ini` (8300 SMP) | **Pass**; SHOW CPU: "PRIMARY CPU = 02", active CPUs 01 02 |
| VMS V4.7 `tests/kdb50/p4.ini` v3 (8300, START/CPU, two CPU loops, COPY) | **Pass** (v2 of the test typed up to 110 characters ahead in one step and overran VMS's type-ahead buffer, also once with the build before patch 14: test fixed, one command per step) |
| VMS V4.7 `tests/vaxbi/mem47.ini` with `do m54.do` (3 × CA, BA, AA) and `set cpu 128m` | **Pass**: Main Memory 54.00Mb and 128.00Mb, one "BI Memory" per board |
| VMS V5.5 `tests/vaxbi/mem55.ini` with `set cpu 256m` and `set cpu 512m` (SIMH-128MB boards) | **Pass**: Main Memory 256.00Mb / 512.00Mb, "BI Memory (128Mb)" per board |
| VMS V5.5 `mem55.ini` with `do m192.do` (12 × MS820-CA) | Boots and runs, but see section 6 |
| VMS V4.7 `tests/dmb32/p3.ini` (VMS from the UDA50 behind the DWBUA, LP11 and DMB32 printer) at the DWBUA's default node 0 and with `SET UBA NODE=3` | **Pass**, the printer files identical, also between the two runs |
| DEBNA `tests/debna/p2.sh` (VMS V4.7) and with `p55_8200.ini` (V5.5) | **Pass** |
| DMB32 sync line `tests/dmb32/p4.sh` (V5.5 to the MicroVAX II DMV11) | **Pass** |
| `tests/kdb50/sdi_rule.sh` | **Pass** |
| Builds | vax8200, microvax2, microvax3900, vax780, vax750, pdp11 without warnings; their tests pass |

## 5. What VMS sees

- The primary is CPU 02, the attached processor CPU 01 (`START/CPU 1` as before).
- Controller letters follow the node order (VMS scans the BI by node): with the defaults (DWBUA 0, KDB50 4, KLESI-B 6) unchanged, the UDA50 disks DUA, the KDB50 disks DUB, TQ MUA, TB MUB.
- The memory boards appear as "BI Memory" at their nexus (V5.5 with the size from the board).

## 6. Finding: VMS uses at most 8 memory boards

With twelve MS820-CA (192MB), VMS V5.5 gave the sizes of only 8 boards (the 8 lowest nodes) in SYSGEN SHOW/ADAPTER, reported Main Memory 128.00Mb, and its page accounting broke (SHOW MEMORY: more free pages than memory, overflowed fields), since VMB's page bitmap covers the whole 192MB. The field service handbook gives the same limit for VMS V4 ("limits the number of memory controllers to 8, limiting the maximum memory … to 128 Mb"). The size is no limit: V5.5 runs with 512MB on four SIMH-128MB boards. Decision (Ulli, D8): up to 12 boards stay allowed; `SET MCTLn` (when a board goes in) and BOOT warn while more than 8 are present.

## 7. Open, known limitations

- `SHOW dev ADDRESS` and `SHOW UBA IOSPACE` show the Unibus device addresses in the canonical window of node 0, also when the DWBUA is on another node (known issues 79).
- The extra instances (up to 3 KLESI-B, 2 DEBNA, 4 KDB50; CIBCA/CIBCI) are a later step; the rules already count by BI device type.
- The RAM type each MS820 option reports and the DTYPE revision field are provisional (no MS820 technical manual).
