#!/bin/bash
# open-simh-extended/tests/dmv11_vmsload/run.sh  v1 (2026-11-21)
# VMS V4.7 (MicroVAX II) loads a PDP-11/73 over a DMV11 line.
# usage: run.sh BINDIR [setup]
#   BINDIR: directory with pdp11 and microvax2 (built from patch 18 v2)
#   setup:  first run only: assemble hello.mac, write hello.tap, copy it into
#           VMS and define the node database (needs vms047.vhd here, a
#           writable copy of VMS047.R81, and macro11 with the local diff)
# Result: PASS if the PDP-11 console shows the program's HELLO line.
BIN=$1; MODE=$2
cd "$(dirname "$0")" || exit 1
if [ "$MODE" = setup ]; then
    macro11 -ysl 8 hello.mac -o hello.obj -l hello.lst || exit 1
    python3 -I obj2tsk.py hello.obj HELLO || exit 1     # hello.bin, hello.tsk
    python3 -I mktap.py hello.tsk hello.tap || exit 1
    timeout 600 python3 -I vmsrun.py "$BIN/microvax2" mv.ini setup.log setup.cmd 500
fi
rm -f pdp.dbg mv.dbg
# the PDP-11 waits in the background (it ends after 600 s or at the HALT)
SDL_AUDIODRIVER=dummy timeout 700 "$BIN/pdp11" pdp.ini < /dev/null > pdp.out 2>&1 &
sleep 2
timeout 600 python3 -I vmsrun.py "$BIN/microvax2" mv.ini load.log load.cmd 400
wait
if tr -d '\r' < pdp.out | grep -q "HELLO from the DMV11 secondary loader"; then
    echo "VMS load PASS"
else
    echo "VMS load FAIL"
fi
grep -a "Boot:" pdp.dbg
