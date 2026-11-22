#!/bin/bash
# open-simh-extended/tests/dmv11_boot/run_tests.sh  v7 (v7: DMC11 tests R4, R5, R3 checks the DMC11's switch packs (known issue 99); v6: DMP11 tests P1-P4, DMR11 tests R1-R3; common_check.ini gets the unit; v5: checks 774-776 instead of 400-402 (start trap at SP 1000); v4: T5 BOOT refusals, T6 switch mode and address with the host interface; v3: also 400-402; v2: mopctl.py connects to the ATTACH port)
# Runs the boot tests of the DMV11 (T1-T6), DMP11 (P1-P4), DMR11 (R1-R3) and
# DMC11 (R4, R5).
# For a load test the MOP control station (mopctl.py) is started in the
# background, then the PDP-11 simulator with the test's ini file.  A load
# test passes when mopctl.py reports PASS, the console shows the program's
# text, the simulator stops at the test program's HALT (PC 001032) and the
# program's words at 24-40 and 774-776 are its own.
# T5, P4 and R3 check refusals (messages), T6 the DMV11 mode and tributary
# address switches with the host interface (debug log), R2 the DMR11's
# restart into a boot ROM, R5 the DMC11's (offset in words).
# usage: run_tests.sh BIN/pdp11 [T1 ... T6 P1 ... P4 R1 ... R5]
BIN=$1; shift
TESTS=${*:-T1 T2 T3 T4 T5 T6 P1 P2 P3 P4 R1 R2 R3 R4 R5}
cd "$(dirname "$0")"
fail=0

# grep_all FILE MESSAGE...: all messages are in FILE
grep_all () {
    local f=$1; shift
    for m in "$@"; do
        grep -q -- "$m" "$f" || return 1
    done
    return 0
}

result () {     # result TEST OK
    if [ "$2" = 1 ]; then echo "$1 PASS"; else echo "$1 FAIL"; fail=1; fi
}

for t in $TESTS; do
    ini=$(echo $t | tr TPR tpr).ini
    case $t in
    T5|P4|R3)                               # refusals: messages only
        SDL_AUDIODRIVER=dummy timeout 30 "$BIN" $ini < /dev/null > $t.out 2>&1
        case $t in
        T5) msgs=("power-on boot switch (E101 SW4) is off" "needs the station mode set by the mode switches"
                  "control station is not booted" "boots with its address in the tributary switches");;
        P4) msgs=("power-on boot switch (E121 SW10) is off" "SET DMPn MODE"
                  "control station is not booted" "Unit number for booting must be 0-15"
                  "SW1-4  unit number          15" "SW10   power-on boot        ON:  enabled");;
        R3) msgs=("PASSWORD=377" "BOOTOFFSET=377" "377 octal words: boot address 173776"
                  "010 octal words: boot address 173020" "switch pack no. 2 (register 15"
                  "The value must be octal 0-377" "BOOTOFFSET=356");;
        esac
        ok=1
        grep_all $t.out "${msgs[@]}" || ok=0
        grep -q "HALT instruction" $t.out && ok=0          # no BOOT may start the CPU
        result $t $ok
        continue;;
    T6)
        rm -f t6.dbg
        SDL_AUDIODRIVER=dummy timeout 30 "$BIN" t6.ini < /dev/null > T6.out 2>&1
        ok=1
        grep_all t6.dbg "Mode 7 from the mode switches" "Tributary address 9 overridden by the switches (5)" \
                 "Established tributary 5" "Procedural error 104" "Mode: full-duplex point-to-point" || ok=0
        grep -q "HALT instruction" T6.out || ok=0
        result T6 $ok
        continue;;
    esac
    text="DMV BOOT OK"
    case $t in
        T1) port=21101; opts="--ignorefirst";;
        T2) port=21102; opts="--addr 5 --mp --dupload";;
        T3) port=21103; opts="--trigger 123 --wrongfirst";;
        T4) port=21104; opts="--addr 9 --mp";;
        P1) port=21111; opts="--ignorefirst --devtype 18"; text="DMP BOOT OK";;
        P2) port=21112; opts="--addr 5 --trigger 5 --wrongfirst --devtype 18"; text="DMP BOOT OK";;
        P3) port=21113; opts="--devtype 18"; text="DMP BOOT OK";;
        R1) port=21121; opts="--trigger 83 --dmrpw --wrongfirst --devtype 40"; text="DMR BOOT OK";;  # 83 = 123 octal
        R2) port=21122; opts="--trigger 83 --dmrpw --noload";;
        R4) port=21123; opts="--trigger 83 --dmrpw --wrongfirst --devtype 12 --shortreq"; text="DMC BOOT OK";;
        R5) port=21124; opts="--trigger 83 --dmrpw --noload";;
    esac
    if [ $t = R2 -o $t = R5 ]; then         # the boot ROM: MOV #123456,R0 and HALT at offset 020
        python3 -I -c 'import struct,sys; sys.stdout.buffer.write(bytes(16) + struct.pack("<3H", 0o012700, 0o123456, 0) + bytes(10))' > r2rom.bin
    fi
    rm -f $t.mop $t.out
    python3 -I mopctl.py $port $opts --text "$text" > $t.mop 2>&1 &
    mpid=$!
    sleep 0.2
    SDL_AUDIODRIVER=dummy timeout 90 "$BIN" $ini < /dev/null > $t.out 2>&1
    wait $mpid
    ok=1
    grep -q "^PASS" $t.mop || ok=0
    if [ $t = R2 -o $t = R5 ]; then
        grep_all $t.out "HALT instruction, PC: 173026" "24:	173020" "26:	000000" "R0:	123456" \
                 "boot ROM at 173020: present" || ok=0
    else
        grep -q "$text" $t.out || ok=0
        grep -q "HALT instruction, PC: 001032" $t.out || ok=0
        grep_all $t.out "24:	100012" "26:	100013" "34:	100016" "36:	100017" "40:	100020" \
                 "774:	100376" "776:	100377" || ok=0
    fi
    result $t $ok
done
exit $fail
