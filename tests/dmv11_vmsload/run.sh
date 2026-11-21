#!/bin/bash
# open-simh-extended/tests/dmv11_vmsload/run.sh  v2 (2026-11-21; v2: second case with SET DMV0 PROGRAM=SYSTEM, the load file sent in Memory Loads)
# VMS V4.7 (MicroVAX II) loads a PDP-11/73 over a DMV11 line.
# usage: run.sh BINDIR [setup]
#   BINDIR: directory with pdp11 and microvax2 (built from patch 18 v2)
#   setup:  first run only: assemble hello.mac, write hello.tap, copy it into
#           VMS and define the node database (needs vms047.vhd here, a
#           writable copy of VMS047.R81, and macro11 with the local diff)
# Result: PASS per case if the PDP-11 console shows the program's HELLO line.
BIN=$1; MODE=$2
cd "$(dirname "$0")" || exit 1
if [ "$MODE" = setup ]; then
    macro11 -ysl 8 hello.mac -o hello.obj -l hello.lst || exit 1
    python3 -I obj2tsk.py hello.obj HELLO || exit 1     # hello.bin, hello.tsk
    python3 -I mktap.py hello.tsk hello.tap || exit 1
    timeout 600 python3 -I vmsrun.py "$BIN/microvax2" mv.ini setup.log setup.cmd 500
fi
# case 1: pdp.ini, the DMV11 asks for a secondary loader (one message)
# case 2: pdp_system.ini, PROGRAM=SYSTEM, the load file (several messages)
for case in pdp pdp_system; do
    rm -f $case.dbg mv.dbg
    # the PDP-11 waits in the background (it ends after 600 s or at the HALT)
    SDL_AUDIODRIVER=dummy timeout 700 "$BIN/pdp11" $case.ini < /dev/null > $case.out 2>&1 &
    sleep 2
    timeout 600 python3 -I vmsrun.py "$BIN/microvax2" mv.ini load_$case.log load.cmd 400
    wait
    if tr -d '\r' < $case.out | grep -q "HELLO from the DMV11 secondary loader"; then
        echo "VMS load $case PASS"
    else
        echo "VMS load $case FAIL"
    fi
    grep -a "Boot:" $case.dbg
done
