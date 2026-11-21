#!/bin/bash
# open-simh-extended/tests/dmv11_boot/run_tests.sh  v5 (v5: checks 774-776 instead of 400-402 (start trap at SP 1000); v4: T5 BOOT refusals, T6 switch mode and address with the host interface; v3: also 400-402; v2: mopctl.py connects to the ATTACH port)
# Runs the DMV11 boot tests T1-T4: for each, the MOP control station
# (mopctl.py) is started in the background, then the PDP-11 simulator with
# the test's ini file.  A test passes when mopctl.py reports PASS, the
# console shows "DMV BOOT OK", the simulator stops at the test program's
# HALT (PC 001032) and the program's words at 24-40 are its own.
# T5 checks the refusals of BOOT DMV0, T6 the mode and tributary address
# switches with the host interface (debug log).
# usage: run_tests.sh BIN/pdp11 [T1 T2 T3 T4 T5 T6]
BIN=$1; shift
TESTS=${*:-T1 T2 T3 T4 T5 T6}
cd "$(dirname "$0")"
fail=0
for t in $TESTS; do
    if [ $t = T5 ]; then
        SDL_AUDIODRIVER=dummy timeout 30 "$BIN" t5.ini < /dev/null > T5.out 2>&1
        ok=1
        for m in "power-on boot switch (E101 SW4) is off" "needs the station mode set by the mode switches" \
                 "control station is not booted" "boots with its address in the tributary switches"; do
            grep -q "$m" T5.out || ok=0
        done
        if [ $ok = 1 ]; then echo "T5 PASS"; else echo "T5 FAIL"; fail=1; fi
        continue
    fi
    if [ $t = T6 ]; then
        rm -f t6.dbg
        SDL_AUDIODRIVER=dummy timeout 30 "$BIN" t6.ini < /dev/null > T6.out 2>&1
        ok=1
        for m in "Mode 7 from the mode switches" "Tributary address 9 overridden by the switches (5)" \
                 "Established tributary 5" "Procedural error 104" "Mode: full-duplex point-to-point"; do
            grep -q "$m" t6.dbg || ok=0
        done
        grep -q "HALT instruction" T6.out || ok=0
        if [ $ok = 1 ]; then echo "T6 PASS"; else echo "T6 FAIL"; fail=1; fi
        continue
    fi
    case $t in
        T1) port=21101; opts="--ignorefirst";;
        T2) port=21102; opts="--addr 5 --mp --dupload";;
        T3) port=21103; opts="--trigger 123 --wrongfirst";;
        T4) port=21104; opts="--addr 9 --mp";;
    esac
    ini=$(echo $t | tr T t).ini
    pkill -f "BIN/pdp11 $ini" 2>/dev/null
    rm -f $t.mop $t.out
    python3 -I mopctl.py $port $opts > $t.mop 2>&1 &
    mpid=$!
    sleep 0.2
    SDL_AUDIODRIVER=dummy timeout 90 "$BIN" $ini < /dev/null > $t.out 2>&1
    wait $mpid
    ok=1
    grep -q "^PASS" $t.mop || ok=0
    grep -q "DMV BOOT OK" $t.out || ok=0
    grep -q "HALT instruction, PC: 001032" $t.out || ok=0
    for w in "24:	100012" "26:	100013" "34:	100016" "36:	100017" "40:	100020" "774:	100376" "776:	100377"; do
        grep -q "$w" $t.out || ok=0
    done
    if [ $ok = 1 ]; then echo "$t PASS"; else echo "$t FAIL"; fail=1; fi
done
exit $fail
