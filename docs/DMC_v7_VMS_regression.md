<!-- open-simh-extended/docs/DMC_v7_VMS_regression.md  v1 (30 Oct 2026) -->
# DMC v7 fixes: VMS V4.7 and V3.7 regression test

Date: 30 Oct 2026. Tested for Ulli (vaxorcist) by Claude.

**Result:** the v7 patch (`opensimh_dmc_ultrix_v7.patch`, the three DMC/DMR fixes found with ULTRIX V1.1) does not change anything for VMS:
- All 12 configurations pass on VMS V4.7 and V3.7: 0 differences, no data or buffer errors, no timeouts, and the circuit comes back after off/on.
- Every DMC/DMR case behaves the same with v6 and with v7.
- The only failure seen is a VMS V3.7 resource limit (REMRSRC). It occurs equally with v6 and v7, and it is not a fault of the device emulation.

## Builds

| Name | Contents |
|---|---|
| `vax780.v7` | Open SIMH 87eb7d5 + DMV11 v6 + DMF32 v3 + VAX 8200 DWBUA fix + DMC v7 fixes |
| `vax780.v6` | the same without the DMC v7 fixes (`pdp11_dmc.c` at DMV11 v6), as the baseline |

## Disks

| Disk | System | Nodes |
|---|---|---|
| `VMS047-1.R81`, `VMS047-2.R81` | VAX/VMS V4.7 on RA81 | MV1 = 1.1, MV2 = 1.2 |
| `VMS037-1.RK07`, `VMS037-2.RK07` | VAX/VMS V3.7 on RK07 | VAX1 = 1.1, VAX2 = 1.2 |

Every run starts from fresh copies of the disks.

## Test procedure (`run_pair.sh` v6)

Two VAX-11/780 simulators run with one line each. The DMC device uses `SET DMC0 TYPE=DMC|DMR|DMP`; the DMF32 synchronous line is unit DMF3. The line connects over TCP ports 1111/2222. On each node:

1. Log in as SYSTEM.
2. Replace the permanent DECnet database line and circuit (the disks were left in the multipoint DMP test configuration):

   ```
   MC NCP PURGE KNOWN CIRCUITS ALL
   MC NCP PURGE KNOWN LINES ALL
   MC NCP DEFINE LINE <line> STATE ON PROTOCOL DDCMP POINT
   MC NCP DEFINE CIRCUIT <line> STATE ON
   MC NCP DEFINE EXECUTOR STATE ON
   ```

3. Run `@SYS$MANAGER:STARTNET`.

Then the test itself:

1. Wait until each node reaches the other over the circuit (`SHOW NODE`).
2. Both nodes copy `SYS$HELP:HELPLIB.HLB` to the other node at the same time: 2235 blocks on V4.7, 1734 on V3.7.
3. Each node fetches its copy back and runs `DIFFERENCES` against the original.
4. Node 1 sets the circuit OFF, waits 10 s, then sets it ON. When the circuit is back, it copies the file there and back again and compares.
5. Both nodes show their circuit counters, and the master clears in the DMC trace are counted.

Line names:
- VMS calls the DMR11 line `DMC-0`, like the DMC11. Both use XMDRIVER (XMA0); `DMR-0` is not accepted.
- The DMP11 is `DMP-0` (XDA0) and the DMF32 synchronous line is `DMF-0` (XGA0).

## Results

| VMS | Line (node 1 – node 2) | Build | Circuit up | Copies + DIFFERENCES | Off/on | Counters (errors, timeouts) |
|---|---|---|---|---|---|---|
| V4.7 | DMC11 – DMC11 | v7 | yes | 3 × 0 differences | ok | 0 |
| V4.7 | DMC11 – DMC11 | v6 | 12 s | 3 × 0 | ok, 6 s | 0 |
| V4.7 | DMR11 – DMR11 | v7 | 6 s | 3 × 0 | ok, 12 s | 0 |
| V4.7 | DMR11 – DMR11 | v6 | 6 s | 3 × 0 | ok, 6 s | 0 |
| V4.7 | DMP11 – DMP11 | v7 | 6 s | 3 × 0 | ok, 12 s | 0 |
| V4.7 | DMF32 – DMF32 | v7 | 7 s | 3 × 0 | ok, 6 s | 0 |
| V4.7 | DMF32 – DMC11 | v7 | 6 s | 3 × 0 | ok, 7 s | 0 |
| V3.7 | DMC11 – DMC11 | v7 | 12 s | 3 × 0 | ok, 6 s | 0 |
| V3.7 | DMC11 – DMC11 | v6 | 17 s | 2 × 0, 1 × REMRSRC | ok, 6 s | 0 |
| V3.7 | DMR11 – DMR11 | v7 | 12 s | 3 × 0 | ok, 6 s | 0 |
| V3.7 | DMR11 – DMR11 | v6 | 29 s | 2 × 0, 1 × REMRSRC | ok, 6 s | 0 |
| V3.7 | DMP11 – DMP11 | v7 | 12 s | 3 × 0 | ok, 17 s | 0 |

Repeat runs, V3.7 DMC11 – DMC11:

| Run | Result |
|---|---|
| v6 #1 | 3 × 0 |
| v6 #2 | 3 × 0 |
| v7 #1 | 2 × 0, 1 × REMRSRC |
| v7 #2 | 2 × 0, 1 × REMRSRC |

Notes:
- The first V4.7 DMC v7 run used an early version of the circuit check, which reported "DOWN" wrongly. The console shows the circuit up 16 s after STARTNET (event 4.10) and 0 differences in all three comparisons.
- The master clears (from the DMC trace) are the same with v6 and v7: V4.7 DMC 20/19, DMR 22/21.
- **Copy times are rough.** The simultaneous copies took 10–14 s for the DMC/DMR/DMP. The 3 s measured for DMF32 – DMF32 is real data (the file came back with 0 differences), but the timing method is not exact.

## The V3.7 REMRSRC failure

In 4 of 8 V3.7 runs, node 2's copy to VAX1 failed while VAX1's own outgoing copy was running:

```
%COPY-E-OPENOUT, error opening _VAX1"SYSTEM"::SYS$LOGIN:FROM2.HLB; as output
-RMS-E-CRE, ACP file create failed
-SYSTEM-F-REMRSRC, insufficient system resources at remote node
```

What is known:
- It happened 2 of 4 times with v6 and 2 of 4 times with v7, and never on V4.7.
- The circuit stays up, with no event and no error counter.
- The copies just before and after it work.

So this is a resource limit of the V3.7 system on VAX1 when an inbound network file access arrives during an outbound one. It is not the DMC emulation. The exact limit (a process or quota parameter) was not identified: V3.7's SYSGEN does not accept a command on the `MC` line, so this needs an interactive look.

## Files

In `/home/claude/vmstest`:
- `run_pair.sh` v6, `run_all.sh` v2, `run_rep.sh` v1, `vcon.py` v2;
- `r<VMS>_<line>_<build>.txt` (results) and `..._n1.log` / `..._n2.log` (consoles).
