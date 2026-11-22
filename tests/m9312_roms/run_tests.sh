#!/bin/bash
# open-simh-extended/tests/m9312_roms/run_tests.sh  v1
# Runs DEC's M9312 console/diagnostic and boot ROMs in the SIMH PDP-11
# (ROM device: ROM0 = console/diagnostic ROM at 17765000, ROM1 = boot ROMs
# at 17773000).  The ROM images are DEC's and not part of the repository;
# they are built with tools/m9312_lst2bin.py from AK6DN's listings (see
# README.txt) and must be in this directory.  A test whose images are
# missing reports SKIP.
#
#   C1  23-248F1 console on an 11/34: diagnostics, prompt, L/D/E/S
#   C2  the same on an 11/04
#   C3  23-248F1 console command XM0: boot over a DMR11 (mopctl.py --rom)
#   C4  23-616F1 on an 11/70: switch register boot with diagnostics (case 106)
#   C5  the same on an 11/60
#   C6  23-446F1 on an 11/44: diagnostics called by the XM ROM, boot
#   C7  23-774F1 on an 11/24: diagnostics called by the XM ROM, boot (cases 107, 108)
#   P1  'PR/TT' 23-760A9, PC11 reader, from the console (PR0)
#   P2  the same from the ROM's entry with diagnostics (173006)
#   P3  'PR/TT', console DL11 as the reader (TT0, the stream typed by SEND)
#   Z1  'ZZ' 23-ZZZA9 continuous diagnostics with the 23-248F1, 3 passes
#   X1  'XE' 23-E32A9/E33A9 on a DEUNA: address printout, BOOT refused (case 109)
#
# The boot tests start the MOP load host ../dmv11_boot/mopctl.py --rom
# (without --trigger: the ROM asks by itself) in the background.  Each test
# uses its own TCP ports (21041-21045 / 21141-21145; UDP 22101/22102 for
# X1); after a run, wait about 60 s before running the same test again
# (TIME_WAIT of the simulator's listening port).
#
# usage: run_tests.sh BIN/pdp11 [C1 ... C7 P1 P2 P3 Z1 X1]
BIN=$(readlink -f "$1"); shift
TESTS=${*:-C1 C2 C3 C4 C5 C6 C7 P1 P2 P3 Z1 X1}
cd "$(dirname "$0")"
MOPCTL=../dmv11_boot/mopctl.py
fail=0

# grep_all FILE MESSAGE...: all messages are in FILE
grep_all () {
    local f=$1; shift
    for m in "$@"; do
        grep -q -- "$m" "$f" || { echo "  missing: $m"; return 1; }
    done
    return 0
}

result () {     # result TEST OK
    if [ "$2" = 1 ]; then echo "$1 PASS"; else echo "$1 FAIL"; fail=1; fi
}

# the ROM images each test needs
images () {
    case $1 in
    C1|C2|C3) echo m9312_248F1.bin m9312_xm.bin;;
    C4|C5)    echo m9312_616F1.bin m9312_xm.bin;;
    C6)       echo m9312_446F1.bin m9312_xm.bin;;
    C7)       echo m9312_774F1.bin m9312_xm.bin;;
    P1|P2|P3) echo m9312_248F1.bin m9312_pr_tt.bin;;
    Z1)       echo m9312_248F1.bin m9312_zz.bin;;
    X1)       echo m9312_248F1.bin m9312_xe.bin;;
    esac
}

# the bootstrap tape and its console stream (P1-P3)
python3 -I mkbstape.py pr.tap "PR BOOT OK" > /dev/null
python3 -I mkbstape.py tt.tap "TT BOOT OK" --sim-send > /dev/null
cp tt.tap.send tt_send.ini
sed -i 's/^send "/send delay=20k,"TT0\\r/' tt_send.ini     # the console command first

for t in $TESTS; do
    missing=""
    for f in $(images $t); do [ -f $f ] || missing="$missing $f"; done
    if [ -n "$missing" ]; then echo "$t SKIP (missing$missing, see README.txt)"; continue; fi
    ini=$(echo $t | tr CPZX cpzx).ini
    rm -f $t.out $t.mop
    mpid=""
    case $t in
    C3|C4|C5|C6|C7)                     # boot over the DMR11
        port=$((21141 + $(echo $t | tr -d C) - 3))
        python3 -I $MOPCTL $port --rom --devtype 12 --text "XM ROM BOOT OK" > $t.mop 2>&1 &
        mpid=$!
        sleep 0.2;;
    esac
    if [ $t = X1 ]; then                # the CPU waits at 2 for ever: stop after 5 s
        ( sleep 5; pkill -INT -f "pdp11 x1.ini" ) &
    fi
    SDL_AUDIODRIVER=dummy timeout 90 "$BIN" $ini < /dev/null 2>&1 | tr -d '\r' > $t.out
    [ -n "$mpid" ] && wait $mpid
    ok=1
    case $t in
    C1|C2) grep_all $t.out "@E 001000 012345" "@E 001002 054321" "HALT instruction, PC: 002002" \
                           "1000:	012345" "1002:	054321" || ok=0;;
    C3)    grep_all $t.out "@XM0" "XM ROM BOOT OK" "HALT instruction, PC: 000036" || ok=0;;
    C4|C5) grep_all $t.out "HALT instruction, PC: 165554" "XM ROM BOOT OK" \
                           "HALT instruction, PC: 000036" "704:	173020" || ok=0;;
    C6)    grep_all $t.out "XM ROM BOOT OK" "HALT instruction, PC: 000036" "704:	173020" || ok=0
           grep -q "HALT instruction, PC: 165" $t.out && { echo "  a diagnostic halted"; ok=0; };;
    C7)    grep_all $t.out "Breakpoint, PC: 165626" "0100000" "XM ROM BOOT OK" \
                           "HALT instruction, PC: 000036" || ok=0;;
    P1|P2) grep_all $t.out "PR BOOT OK" "HALT instruction, PC: 157636" "157776:	177550" || ok=0;;
    P3)    grep_all $t.out "@TT0" "TT BOOT OK" "HALT instruction, PC: 157636" "157776:	177560" || ok=0;;
    Z1)    grep_all $t.out "@ZZ0" "End Pass 000001" "End Pass 000003" || ok=0;;
    X1)    grep_all $t.out "@XE0" "ETHERNET PHYSICAL ADDRESS = 08-00-2B-12-34-56" \
                           "BOOT command not implemented" "PC: 000002" || ok=0;;
    esac
    if [ -n "$mpid" ]; then grep -q "^PASS" $t.mop || { echo "  mopctl: $(tail -1 $t.mop)"; ok=0; }; fi
    result $t $ok
done
exit $fail
