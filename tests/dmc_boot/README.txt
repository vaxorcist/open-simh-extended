open-simh-extended/tests/dmc_boot/README.txt  v4 (2026-11-25; v4: vmsdup.sh, BOOT DUPn with VMS V4.7 as the load host (patch 28); v3: deadconn.py and d1.ini (known issue 116, patch 25); PDPPEER default 21301 again; v2: PDPPEER, by default only the VAX's connection is used; system loads 3 of 3)

SIMH's BOOT DMCn with a real load host (patch 24, docs/DMV11_switches_boot_design.md
section 10).  BOOT DMCn starts a small MOP bootstrap written for SIMH
(PDP11/pdp11_dmc_boot.mac) in place of a DECnet boot ROM such as DEC's M9312
'XM': it master-clears the DMC11/DMR11, puts it into maintenance mode, asks
the load host for the program (Request Program with device type 12 or 40
and the program type of SET DMCn PROGRAM), stores the Memory Loads and
starts the program with R1 = CSR.  The tests with a Python load host are
R8-R10 in ../dmv11_boot.

vmsdmc.sh BINDIR DISK
    VMS V4.7 on a VAX-11/780 (a copy of DISK: VMS V4.7, node VAX4 1.4,
    SYSTEM/MANAGER) with a DMC11 is the load host of a PDP-11/34 with a
    DMC11 (TYPE=DMR: DMR11), joined by a SIMH TCP line (VAX 21301, PDP-11
    21302).  VMS gets HELLO.TSK and the load file from a tape, defines line
    and circuit DMC-0 with SERVICE ENABLED and node PDPDMC (1.21) with
    SERVICE CIRCUIT DMC-0, SERVICE DEVICE DMC, CPU PDP11, SECONDARY LOADER
    SYS$SYSTEM:HELLO.TSK, LOAD FILE SYS$SYSTEM:DMCLOAD.TSK.  Then the PDP-11
    gives BOOT DMC0.  VMS answers the bootstrap's Request Program by itself
    (the circuit goes "on -autoloading"; no NCP LOAD NODE needed).
    Variables:
      PROGRAM=SECONDARY  (default) VMS sends HELLO.TSK in one Memory Load
                         with Transfer Address (at address 6)
      PROGRAM=SYSTEM     VMS sends the load file in Memory Loads and a
                         Parameter Load with Transfer Address
      LOADFILE=file.tsk  the load file (default HELLO.TSK), e.g. Ulli's
                         remwrt.tsk (not in the repository)
      TYPE=DMR           DMR11 instead of DMC11 on both sides
      WAITLOAD=1         NCP LOAD NODE PDPDMC instead of waiting
      VAXDEBUG=, PDPDEBUG=  SET DMC DEBUG= flags (logs vax.dbg, pdp.dbg)
      PDPPEER=port       the PDP-11's PEER (a DMC needs one to ATTACH);
                         default 21301 (the VAX: both sides connect);
                         21399 (nobody listens): only the VAX's connection
    PASS when the PDP-11 console shows "HELLO from the VMS load host over
    the DMC11".  The run directory run_<program>_<type>/ holds the disk
    copy and the logs (never in the repository).
hello.mac   the test program: prints its line and halts; R2 keeps the R1
            it was started with (the CSR).
deadconn.py PORT [NDEAD], d1.ini
            known issue 116 (patch 25): the PDP-11 of d1.ini runs BOOT DMC0
            with its line listening on 21397; deadconn.py makes NDEAD
            connections and closes them, then a live one, which must get
            the Request Program within 3.5 s (the closed ones are discarded
            in one poll).  Start deadconn.py first:
                python3 -I deadconn.py 21397 3 &
                BIN/pdp11 d1.ini

Results (2026-11-23, patch 24):
  PROGRAM=SECONDARY (DMC11): PASS (HELLO at 6, R2 = CSR 160070).
  PROGRAM=SYSTEM (DMC11, HELLO.TSK), PEER on both sides: 2 of 4 PASS; the
    others "Load, Line protocol error": old Request Programs that had waited
    in the PDP-11's connection reached VMS during the load (known issue 116).
  PROGRAM=SYSTEM (DMC11, HELLO.TSK), PDPPEER=21399 (one connection): 3 of 3
    PASS.
  With patch 25 (known issue 116 fixed), PEER on both sides: 4 of 4 PASS.
  d1.ini/deadconn.py: 2.9 s with patch 25, 15.5 s before (FAIL).
  PROGRAM=SYSTEM TYPE=DMR LOADFILE=remwrt.tsk: VMS "Load, Successful",
    remwrt's banner and prompt (the script says FAIL only because it looks
    for HELLO's line).
See docs/DMV11_switches_boot_design.md section 10.

vmsdup.sh (patch 28): BOOT DUPn with a real load host.  VMS V4.7 on a
VAX-11/780 (DMC11, circuit DMC-0) loads a PDP-11/34 over its DUP11 (SIMH's
MOP bootstrap for the DUP11).  Needs DEC's SECDUP.SYS and TERDUP.SYS
(DECnet-11M V4.2 DECKIT [136,54]; never in the repository) in LOADERS=dir.

  LOADERS=dir PROGRAM=SYSTEM ./vmsdup.sh BINDIR DISK
      VMS sends the load file (HELLO) to the bootstrap directly.
  LOADERS=dir PROGRAM=SECONDARY ./vmsdup.sh BINDIR DISK
      VMS sends SECDUP.SYS, it asks for TERDUP.SYS, that for the load file.
      DEC's loaders use the DUP11 with CRC inhibit (known issue 124) and
      load characters into TXDBUF with byte writes (known issue 126).
  BRURL=../deuna_boot/brusys.rl01 LOADERS=dir ./vmsdup.sh BINDIR DISK
      the load file is BRUSYS.SYS (Ulli's RSX-11M standalone BRU).

Results (2026-11-25, patch 28): SYSTEM PASS; SECONDARY PASS (VMS: Load
Successful for SECDUP.SYS, TERDUP.SYS and the load file; HELLO's line);
SECONDARY with BRURL PASS (BRU's banner "RSX-11M/RSX-11M-PLUS STANDALONE
COPY SYSTEM V4.8" and "Enter first device:").  Before the 126 fix
SECONDARY failed: SECDUP never asked for TERDUP ("aborted service request,
Line open error").
