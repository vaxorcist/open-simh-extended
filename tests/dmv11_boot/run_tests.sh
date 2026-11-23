#!/bin/bash
# open-simh-extended/tests/dmv11_boot/run_tests.sh  v9 (v9: R8-R10, SIMH's BOOT DMCn MOP bootstrap (patch 24): DMC11 with a repeated request, DMR11 with PROGRAM=TERTIARY and 64 KB, refusals and help; v8: R6, R7 boot through DEC's M9312 'XM' DECnet ROM (DMR11, DMC11; image m9312_xm.bin, see README.txt; SKIP without it); v7: DMC11 tests R4, R5, R3 checks the DMC11's switch packs (known issue 99); v6: DMP11 tests P1-P4, DMR11 tests R1-R3; common_check.ini gets the unit; v5: checks 774-776 instead of 400-402 (start trap at SP 1000); v4: T5 BOOT refusals, T6 switch mode and address with the host interface; v3: also 400-402; v2: mopctl.py connects to the ATTACH port)
# Runs the boot tests of the DMV11 (T1-T6), DMP11 (P1-P4), DMR11 (R1-R3) and
# DMC11 (R4, R5), and through DEC's M9312 DECnet boot ROM (R6 DMR11, R7 DMC11);
# SIMH's BOOT DMCn bootstrap (R8 DMC11, R9 DMR11, R10 refusals and help).
# For a load test the MOP control station (mopctl.py) is started in the
# background, then the PDP-11 simulator with the test's ini file.  A load
# test passes when mopctl.py reports PASS, the console shows the program's
# text, the simulator stops at the test program's HALT (PC 001032) and the
# program's words at 24-40 and 774-776 are its own.
# T5, P4 and R3 check refusals (messages), T6 the DMV11 mode and tributary
# address switches with the host interface (debug log), R2 the DMR11's
# restart into a boot ROM, R5 the DMC11's (offset in words).
# usage: run_tests.sh BIN/pdp11 [T1 ... T6 P1 ... P4 R1 ... R10]
BIN=$1; shift
TESTS=${*:-T1 T2 T3 T4 T5 T6 P1 P2 P3 P4 R1 R2 R3 R4 R5 R6 R7 R8 R9 R10}
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
    T5|P4|R3|R10)                           # refusals: messages only
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
        R10) msgs=("Unit not attached" "Non-existent unit" "1.3.1 BOOT command"
                   "starts a small MOP bootstrap written for SIMH" "DMC     yes"
                   "BOOT DMCn start a MOP bootstrap" "R10 END");;
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
        R8) port=21127; opts="--listen --ignorefirst --mingap 1.0 --devtype 12"; text="DMC BOOT OK";;
        R9) port=21128; opts="--listen --devtype 40 --progtype 1"; text="DMR BOOT OK";;
        R6|R7)                              # DEC's M9312 'XM' ROM (not in the repository)
            if [ ! -f m9312_xm.bin ]; then echo "$t SKIP (no m9312_xm.bin, see README.txt)"; continue; fi
            port=$([ $t = R6 ] && echo 21125 || echo 21126)
            opts="--trigger 83 --dmrpw --rom --devtype 12"; text="XM ROM BOOT OK";;
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
    if [ $t = R6 -o $t = R7 ]; then           # the ROM started the program at 6, HALT at 34
        grep_all $t.out "boot ROM at 173004: present" "$text" "HALT instruction, PC: 000036" || ok=0
    elif [ $t = R2 -o $t = R5 ]; then
        grep_all $t.out "HALT instruction, PC: 173026" "24:	173020" "26:	000000" "R0:	123456" \
                 "boot ROM at 173020: present" || ok=0
    else
        if [ $t = R8 -o $t = R9 ]; then       # BOOT DMCn: start registers at the breakpoint
            grep_all $t.out "Breakpoint, PC: 001000" "R1:	160070" "SP:	001000" || ok=0
        fi
        grep -q "$text" $t.out || ok=0
        grep -q "HALT instruction, PC: 001032" $t.out || ok=0
        grep_all $t.out "24:	100012" "26:	100013" "34:	100016" "36:	100017" "40:	100020" \
                 "774:	100376" "776:	100377" || ok=0
    fi
    result $t $ok
done
exit $fail
