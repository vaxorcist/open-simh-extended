open-simh-extended/tests/dmc_boot/README.txt  v2 (2026-11-23; v2: PDPPEER, by default only the VAX's connection is used; system loads 3 of 3)

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
                         default 21399, where nobody listens, so only the
                         VAX's connection is used; 21301 (the VAX) gives
                         crossing connections (known issue 116)
    PASS when the PDP-11 console shows "HELLO from the VMS load host over
    the DMC11".  The run directory run_<program>_<type>/ holds the disk
    copy and the logs (never in the repository).
hello.mac   the test program: prints its line and halts; R2 keeps the R1
            it was started with (the CSR).

Results (2026-11-23, patch 24):
  PROGRAM=SECONDARY (DMC11): PASS (HELLO at 6, R2 = CSR 160070).
  PROGRAM=SYSTEM (DMC11, HELLO.TSK), PEER on both sides: 2 of 4 PASS; the
    others "Load, Line protocol error": old Request Programs that had waited
    in the PDP-11's connection reached VMS during the load (known issue 116).
  PROGRAM=SYSTEM (DMC11, HELLO.TSK), PDPPEER=21399 (one connection): 3 of 3
    PASS.
  PROGRAM=SYSTEM TYPE=DMR LOADFILE=remwrt.tsk: VMS "Load, Successful",
    remwrt's banner and prompt (the script says FAIL only because it looks
    for HELLO's line).
See docs/DMV11_switches_boot_design.md section 10.
