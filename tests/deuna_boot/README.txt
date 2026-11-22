open-simh-extended/tests/deuna_boot/README.txt  v1 (2026-11-22)

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
