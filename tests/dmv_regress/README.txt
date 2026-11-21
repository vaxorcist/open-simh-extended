open-simh-extended/tests/dmv_regress/README.txt  v1 (2026-11-21)

VMS V4.7 DECnet regression for the DMV11 (microvax2) and the DMP11 (vax780),
two nodes, point-to-point and multipoint (control station + tributary 3).

Files
  pair.sh       ./pair.sh microvax2|vax780 [BINDIR]; MP=1 ./pair.sh ... for
                multipoint.  Node A (VAX4, 1.4) and node B (VAX3, 1.3) boot
                one after the other with SET CPU IDLE=VMS (known issue 97),
                start DECnet on DMP-0, copy SYS$HELP:HELPLIB.HLB both ways,
                fetch it back and run DIFFERENCES; A also turns its circuit
                off and on and copies again.  PASS = 3 compares without
                differences and no error after the login.
  bootpair.sh   boot-only check of two nodes at once (known issue 97)
  vmsrun.py     pexpect console driver (boot, date, login, DCL lines)
  setupB.cmd    once on diskB.vhd: executor address 1.3
  setpw.cmd     once on both disks: SYSTEM password MANAGER (remote access)
  alone.cmd     only log in (for bootpair.sh)

Needs diskA.vhd and diskB.vhd in this directory: copies of Ulli's
VMS047.R81 (not in the repository; node 1.4 VAX4, knows VAX1-VAX4), diskB
changed with setupB.cmd, both with setpw.cmd, e.g.
  python3 -I vmsrun.py BIN/microvax2 setup.ini setup.log setpw.cmd

Results 2026-11-21 (patch 18 v3 build):
  microvax2 DMV11 point-to-point   PASS
  vax780    DMP11 point-to-point   PASS
  microvax2 DMV11 multipoint       PASS
  vax780    DMP11 multipoint       PASS
