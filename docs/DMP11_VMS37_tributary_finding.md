<!-- open-simh-extended/docs/DMP11_VMS37_tributary_finding.md  v1 (29 Oct 2026) -->
# VMS V3.7 DMP11 tributary stuck in "synchronizing": finding

**Result:** the cause was not the DMP11 emulation (v6, `SET DMCn TYPE=DMP`). On Ulli's disk `VMS037-2.RK07` (node VAX2, 1.2), the permanent DECnet executor database has no `STATE ON`. `STARTNET` therefore leaves the executor **OFF**, and NETACP never starts a circuit.

## Fix

Run this in NCP on each V3.7 node:

```
SET EXECUTOR STATE ON
DEFINE EXECUTOR STATE ON
```

## Test

Two VAX-11/780 simulators, Open SIMH with opensimh_dmv11_v6.patch, both running VMS V3.7:

- **VAX1 (1.1):** control station, `SET DMC0 TYPE=DMP`, `DROPS=2`, `ATTACH DMC0 1111`.
- **VAX2 (1.2):** tributary 2, `PEER=localhost:1111`, `ATTACH DMC0 2222`.

Results:

- Circuit up straight after `STARTNET` (event 4.10).
- COPY of HELPLIB.HLB (1734 blocks) in both directions, DIFFERENCES = 0.
- No errors in the circuit counters.
- DMP-0.3 stays `-starting`, which is expected with no tributary 3 connected.
- No emulation change was needed.

## How it was found

1. **Emulation trace:** V3.7's XDDRIVER sends the mode command, writes GSS 034-037, posts 4 receive buffers and then stops. It never sends ESTABLISH or START.
2. **QIO trace:** breakpoints on EXE$QIO (80008677) showed that NETACP sends XDA0 only `IO$_SETMODE` 0x23 (define tributary) and 0xA3 (shutdown), never 0x63 (startup).
3. **NETACP code:** NETACP's circuit start routine (V3.7 NETACP, P0 BBC2) gives up in a check at BEB4, which tests the node state byte 61 of the executor block. Its value was 0, and `SHOW EXECUTOR STATUS` showed `State = off`.

## Side notes

- **DTR:** V3.7's XDDRIVER never raises DTR itself; it only clears it (key 021 with SEL4 = 0 at line shutdown). The emulation raises DTR on START, and that is enough.
- **File names:** VMS V3 allows only 9-character file names without underscores. FAL needs access control such as `VAX1"SYSTEM"::`, because these disks have no default DECnet account.
