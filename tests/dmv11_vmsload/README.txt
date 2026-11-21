open-simh-extended/tests/dmv11_vmsload/README.txt  v2 (2026-11-21; v2: second case pdp_system.ini, SET DMV0 PROGRAM=SYSTEM)

VMS V4.7 on a MicroVAX II loads a PDP-11/73 over a DMV11 line (MOP).
Design notes: docs/DMV11_switches_boot_design.md, section 6.

Files
  run.sh        runs the test: ./run.sh BINDIR [setup]
  pdp.ini       PDP-11/73, DMV11 FDXPOINT, remote load detect, password 123,
                listens on port 21202 (no PEER, see known issue 93); the
                DMV11 asks for a secondary loader (default)
  pdp_system.ini  the same with SET DMV0 PROGRAM=SYSTEM: the DMV11 asks for
                the system image, VMS sends the LOAD FILE in Memory Loads
  mv.ini        MicroVAX II, VMS V4.7 from vms047.vhd, DMV11 PEER=127.0.0.1:21202
  setup.cmd     once: HELLO.TSK from tape to SYS$SYSTEM, NCP node database
  load.cmd      STARTNET, NCP LOAD NODE PDP
  vmsrun.py     pexpect driver: boots VMS, logs in, types the .cmd lines
  hello.mac     72-byte position-independent PDP-11 test program
  obj2tsk.py    macro11 object -> RSX task image without header (.tsk, .bin)
  mktap.py      file -> SIMH tape image with 512-byte records
  macro11_v0.3_local.diff   local fix for github.com/shattered/macro11 v0.3

Needs: vms047.vhd (a writable copy of Ulli's VMS047.R81; not in the
repository), python3 with pexpect, pdp11 and microvax2 built with patch 18 v2,
and for "setup" macro11 in the PATH.

Each case passes when the PDP-11 console shows
"HELLO from the DMV11 secondary loader".  In the default case NCP LOAD NODE
does not return, because the test program is not a loader and never asks
for the load file.  With PROGRAM=SYSTEM Ulli's remwrt (not in the
repository) was also loaded this way and ran (2026-11-21).
