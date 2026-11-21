open-simh-extended/tests/vms_boot_freeze/README.txt  v1 (2026-11-21)

Known issue 97: VMS V4.7 freezes after the date at boot when SYSINIT's
60 KB read of F11BXQP.EXE (MSCP READ lbn=34113 bc=F000) completes within a
few hundred instructions.  Needs diskA.vhd/diskB.vhd as in tests/dmv_regress.

  single.py BIN/microvax2 nodmvx_a.ini diskA.vhd
        one node, SET NOASYNCH: prints FROZE (every time, also plain
        Open SIMH 87eb7d5)
  single.py BIN/microvax2 nodmvt_a.ini diskA.vhd
        the same with DEP RQ XTIME 1000: prints finished
  single.py BIN/microvax2 healthy_a.ini diskA.vhd
        asynchronous I/O (default): finished
  single2.py BIN/microvax2 nodmvx_a.ini diskA.vhd "TRACE;REQ"
        RQ debug from the date prompt on (single2.dbg)
  hangprobe.py BIN/microvax2 ROUNDS [nodmvx]
        two nodes at once (nodmv_a/b.ini, no IDLE); a node that freezes is
        stopped and probed: clocks, event queue, Qbus interrupts, RQ state
        and rings, console, and the VMS processes (states, wait masks)
  sda.cmd, lbn2.cmd   vmsrun.py command lists (tests/dmv_regress/vmsrun.py)
        that gave the VMS symbol addresses and found the file at LBN 34113

Result: SYSINIT (PID 00010002) waits in MWAIT/RWAST, everything else idle;
the read's response came about 200 instructions after the poll (frozen)
instead of about 2,800 (healthy).  XTIME 400 and more: no freeze.
