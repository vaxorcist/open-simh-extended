open-simh-extended/tests/deuna_boot/README.txt  v2 (2026-11-22; v2: B9, VMS V4.7 as the load host: vmshost.sh, mknettest.py, mktap.py)

DEUNA/DELUA BOOT port command (SIMH XU device, patch 23, known issue 109)
=========================================================================

Design and facts: docs/DEUNA_boot_design.md.  The BOOT port command makes
the board ask a MOP load host for a secondary loader; SIMH then does what
DEC's UNA secondary loader (SECUNA.SYS) does on the board's own processor:
it asks for the tertiary loader, copies it into host memory and starts the
host, which waits in BR . at location 2 with the CSR address at 4 (DEC's
M9312 'XE' ROM, or any program, sets that up): JMP @#transfer at 6/10,
then MOV #(4),R1 at 2 -- the tertiary starts with R1 = CSR.

run_tests.sh BIN/pdp11 [B2 ... B8]   runs the tests; PASS, FAIL or SKIP
mophost.py PORT SIMPORT [options]     the MOP load host (UDP transport)

Tests
-----
B2  a program sets up 2/4 and gives BOOT (no ROM image); mophost.py serves
    a dummy secondary and its test tertiary loader (prints DEUNA BOOT OK,
    HALT at PC 002034); checks R1 = 174510 and the words at 2-10
B3  as B2, the first Request Program is ignored: the board repeats it
    (5 s of simulated time)
B4  as B2, one Memory Load is sent twice: stored once, the next is asked
    for again
B5  as B2 on a DELUA: device type 11 in the board's own request
B6  through DEC's M9312 ROMs: 23-248F1 console, XE0 ('XE' ROMs print the
    Ethernet address and give BOOT)
B7  DEC's UNA loaders: SECUNA.SYS as the secondary (SIMH does its work),
    TERUNA.SYS as the tertiary; TERUNA runs on the PDP-11, drives the
    DEUNA through its normal port and loads mophost's test system
    (SYSTEM_BOOT_OK, HALT at PC 002034)
B8  BOOT without an attached network: refused (message, PCSR0 bit 14)

Files that are not in the repository (DEC firmware/software, Ulli's copies):
  m9312_248F1.bin, m9312_xe.bin   built as ../m9312_roms/README.txt says
  SECUNA.SYS, TERUNA.SYS          DECnet-11M V4.2 kit tape BB-AS90B-BC
                                  (DOS-11 format), UIC [136,54]
B6 and B7 report SKIP without them.

What TERUNA showed in SIMH's DEUNA (all fixed in patch 23):
  known issue 111  STOP outside the running state (and START when running)
                   gave a port command error instead of DNI
  known issue 112  Read (and Clear) Counters wrote 68 bytes whatever UDB
                   length the PCB gave (it overwrote TERUNA's message)
  known issue 113  Read Load Server Address returned the multicast address
                   exactly when a load server was set
  known issue 114  SET XU MAC= changed the receive address only at the
                   next reset (a GO right after ATTACH missed own frames)

Results 2026-11-22 (deuna-boot, patch 23): B2-B8 PASS.

B9: VMS V4.7 as the load host (vmshost.sh BINDIR DISK; DISK = a VMS V4.7
system disk with SYSTEM/MANAGER, e.g. ../dmv_regress/diskA.vhd; needs
vax780 and pdp11 in BINDIR, the ROM images and SECUNA/TERUNA here).  A
VAX-11/780 with a DEUNA copies the loaders and a test load file from a tape
(mktap.py), defines UNA-0 with SERVICE ENABLED and node PDPNET (1.20,
hardware address 08-00-2B-12-34-56, service device UNA, CPU PDP11,
SECONDARY LOADER SECUNA.SYS, TERTIARY LOADER TERUNA.SYS, LOAD FILE
NETTEST.SYS); the PDP-11 boots with XE0.  State 2026-11-22:
  - VMS answers the board's request: "Load, Successful ... SECUNA.SYS,
    Secondary loader" and "... TERUNA.SYS, Tertiary loader" (OPCOM);
    SIMH's boot engine loads TERUNA and starts it;
  - TERUNA asks VMS for the operating system through SIMH's DEUNA port,
    VMS answers ("Load, Requested ... NETTEST.SYS, Operating system") and
    TERUNA receives and acknowledges every Memory Load;
  - but VMS treats the LOAD FILE as an RSX-11S system image: it reads the
    downline load parameters stored in the executive (set by DECnet-RSX's
    VNP) and sent zeros from 0 to 173777 for the test file (mknettest.py),
    until TERUNA refused a load at 174000 ("Line communication error").
    A file without a task label gave "File open error ... %RMS-E-EOF".
  So B9 needs a real RSX-11S system image (built with RSX-11S SYSGEN/VMR
  and prepared with VNP); with a test file it FAILs at the system load.
  The test with mophost.py (B7) covers the same SIMH paths with DEC's
  loaders and a test system.
