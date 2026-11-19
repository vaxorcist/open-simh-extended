<!-- open-simh-extended/docs/SIDRIVER_V47_backport.md  v1, 2026-11-19 (v1: VMS V5.5 SIDRIVER backported to VMS V4.7 by binary patching; tests POINT, TRIBUTARY, OFF/ON; CONTROL refused as in V5.5) -->
# SIDRIVER (DMB32 synchronous line) for VMS V4.7

Ulli asked on 2026-11-19 to backport the VMS V5.5 SIDRIVER.EXE to VMS V4.7. VMS V4.7 has the DMB32 async (YIDRIVER) and printer (LIDRIVER) drivers, but no driver for the sync line. DECnet-VAX V4.7 already knows the DMB32: NETACP maps device SI to line DMB-n, and STARTNET handles DMB lines.

**Result:** the patched driver loads and works under VMS V4.7 on the VAX 8200:
- SYSGEN configures SIA0;
- DECnet runs over line DMB-0 as DDCMP POINT and as DDCMP TRIBUTARY.

The driver is DEC software taken from Ulli's VMS V5.5 disk. The patched SIDRIVER.EXE and its tape image are therefore **not** in the fork or the project: they go to Ulli only. The fork has the tools that build it and this description, both in our own words.

## 1. The original

- `SYS$LDR:SIDRIVER.EXE` on VMS055.R82, 50 blocks, image ident "SYNC V1.2-089", linked 2-FEB-1992 with Linker V05-00 (VAX WAN Device Drivers V1.2).
- One image section of 48 pages at 0; DPT size 5E95.
- The same code base as XGDRIVER (DMF32). V4.7's own XGDRIVER served as the model for the V4 conventions.
- SIDRIVER.STB (from the same disk) gives the driver's routine names. They were used for the code map.

## 2. Method

The tools are in `tools/sidriver_v47/`. They run on Claude's workspace paths; the scripts say which.

1. **Material taken off the disk images** (`ods2_fh.py`, `vms_varrec.py`, `vms_stb.py`, `vms_isd.py`):
   - SIDRIVER.EXE and SIDRIVER.STB from V5.5;
   - SYS.MAP and SYSDEF.STB of V5.5 and V4.7: system addresses and structure offsets;
   - V4.7's XGDRIVER.EXE, YIDRIVER.EXE and SYS.EXE.
2. **Code map** (`vaxdis.py`, a VAX decoder; `sidmap.py`):
   - a recursive traversal from the entry points: DDT (DDT-relative), FDT (entry-relative routine addresses, as in V4.7), CRB init table, DPT deliver routine, and the STB symbols;
   - the routine SI_FATAL takes an inline argument byte;
   - result: 5914 instructions, no overlaps.
3. **Base structure of every operand:**
   - Traces first: a temporary trace build of vax8200, never committed, logged the base register and its DYN$C type byte for every memory operand of the driver. It ran under VMS V5.5 in tributary mode and in point-to-point mode with the HELPLIB copy (`sidtype.py`).
   - Then static propagation over the code (`sidflow.py`): entry conventions, learned pointer loads, and interior pointers such as UCB+AC. It agrees with 95 % of the observations; the differences are interior pointers.
   - The remaining operands were checked by hand.
4. **Patching** (`sidport.py`, which writes `sidport.log` and `sidport_offsets.txt`):

| Area | Change |
|---|---|
| System routines and cells | 24 V5.5 addresses → V4.7 addresses by name (EXE$ABORTIO, EXE$FORK, IOC$REQCOM, COM$DRVDEALMEM, EXE$INSTIMQ, MMG$GL_SBR, ...) |
| SMP | 170 × `BLBC @#SMP$GL_FLAGS,x` → `BRW x`. In the non-SMP paths: device lock → `MTPR #14,#IPL` (the DMB32's device IPL); fork lock → `MTPR R0,#IPL`, because V4.7 keeps an IPL in UCB/FKB$B_FIPL where V5 keeps a spinlock index. The spinlock index stored in a fork block becomes an IPL (QUEUEAST → 6, IOLOCK8 → 8) |
| Delay loops | the per-CPU loop counts (V5 CPU database at ISP & ~SMP$GL_BASE_MSK, +270) → EXE$GL_TENUSEC / EXE$GL_UBDELAY |
| Timer "no repeat" | V5's EXE$AR_TQENOREPT → a zeroed dummy TQE appended to the driver. V4.7's EXE$SWTIMINT requeues the TQE in R5 only if its TQE$V_REPEAT bit is set |
| Byte count quota | the V5 routines EXE$DEBIT_BYTCNT_ALO, EXE$DEBIT_BYTCNT_NW and EXE$CREDIT_BYTCNT → small appended routines doing what V4.7's XGDRIVER does (EXE$BUFFRQUOTA + EXE$ALLOCBUF, EXE$BUFQUOPRC, PCB$L_JIB → JIB$L_BYTCNT) |
| UCB fixed part (0-A3) | 207 operands translated by field name (V5.5 inserted UCB$L_DLCK and other fields: most offsets +14, DEVCHAR 3C → 38, DEVCLASS 4C → 40, DEVDEPEND 50 → 44, UNIT 68 → 54, DEVSTS 7C → 68, ...). The driver's own UCB extension starts at A4 and keeps its offsets (the UCB stays 21A bytes). The UCB$L_DLCK loads are left alone (only the SMP paths use them) |
| Data table | the parameter table at 158 holds the UCB offset of the buffer size (UCB$W_DEVBUFSIZ 4E → 42) |
| CRB, IDB, PCB | CRB$L_INTD 50 → 24 (VEC$L_IDB at +8, VEC$L_ADP at +14); IDB$L_ADP 18 → 14, IDB$L_UCBLST 1C → 18; PCB$Q_PRIV 88 → 84 |
| DPT | V4.7 layout (version 4, size with the appended code 5F1B), DPT$B_FLAGS at 0D (the driver's NOUNLOAD bit) |
| Init table | structure codes UCB 2 → 10, DDB 4 → 6; offsets as above; UCB$B_FLCK IOLOCK8 → FIPL 8 |
| DDT | IOC$RETURN and IOC$MNTVER entries mapped |
| Image header | the header of V4.7's XGDRIVER.EXE with one ISD of 48 pages and the name SIDRIVER |

## 3. Installing (Ulli)

On the VAX 8200 simulator with VMS V4.7 (the DMB32 enabled, a TK50 on the DWBUA):

```
SET DMB ENABLE
SET TQ TK50
ATTACH -R TQ0 sidriver47.tap
```

In VMS (VMS V4.7 starts the TMSCP port at the end of the startup: wait about 90 s after the login):

```
$ MOUNT/FOREIGN/BLOCKSIZE=512/RECORDSIZE=512 MUA0:
$ COPY MUA0: SYS$SYSTEM:SIDRIVER.EXE
$ DISMOUNT MUA0:
$ MC SYSGEN AUTOCONFIGURE ALL
$ SHOW DEVICE SI
```

After that the boot-time autoconfiguration loads SIDRIVER as well. DECnet, for example point-to-point:

```
DEFINE LINE DMB-0 STATE ON
DEFINE CIRCUIT DMB-0 STATE ON
```

and as a tributary:

```
DEFINE LINE DMB-0 STATE ON PROTOCOL DDCMP TRIBUTARY
DEFINE CIRCUIT DMB-0 STATE ON TRIBUTARY 4
```

## 4. Tests

All on the VAX 8200 with VMS V4.7 (copy of VMS047_1.R81 with the patched SIDRIVER), `tests/dmb32/p4.sh` v3:

| Test | Mode | Partner | Result |
|---|---|---|---|
| `p4b_8200.ini` | POINT | MicroVAX II DMV11 (VMS V4.7) | **PASS**: circuit up, HELPLIB.HLB (2235 blocks) there and back with 0 differences, remote DIRECTORY, NCP LOOP NODE 50 × 1000, SET HOST; counters without errors |
| `p4g_8200.ini` | POINT, circuit and line OFF/ON | MicroVAX II DMV11 | **PASS**: as p4b, then circuit and line off and on, circuit up again, SHOW LINE/CIRCUIT CHARACTERISTICS, ZERO LINE COUNTERS, LOOP 20 × 500, SET HOST |
| `p4c_8200.ini` + `p4c_780.ini` | TRIBUTARY 4 | VAX-11/780 DMP11 control station (VMS V4.7) | **PASS** (as p4b) |
| CONTROL | — | MicroVAX II DMV11 as tributary 3 | SIDRIVER refuses it: NCP "Invalid parameter value, Protocol" for line DMB-0, on V4.7 and the same on V5.5. The DMB32 supports POINT and TRIBUTARY only |
| `p4h_8200.ini` | V5.5 reference for p4g | MicroVAX II DMV11 | **PASS**, the same events as the backport |

With both versions, turning the circuit off logs "DECnet event 4.7, circuit down, circuit fault, Line synchronization lost" (known issues 86).

## 5. Limits

- Only the DDCMP paths were tested. The X.21 and other protocol paths (HDLC, SDLC, BISYNC, general byte) are patched the same way, but they are untested; the SIMH DMB32 does DDCMP only.
- The code reached neither by the traces nor by the propagation is patched by the same rules, but it ran in no test (error paths).
- VAX 8300 with VMS V4.7 multiprocessing: the driver runs the uniprocessor paths, as all V4.7 drivers do; not tested.
