#!/bin/bash
# open-simh-extended/tests/deuna_boot/run_tests.sh  v1
# Tests of the DEUNA/DELUA BOOT port command of SIMH's XU device (patch 23,
# known issue 109; docs/DEUNA_boot_design.md).  Each test starts the MOP
# load host mophost.py (SIMH's UDP Ethernet transport: the simulator has
# ATTACH XU udp:LP:127.0.0.1:HP, mophost.py listens on HP) and then the
# PDP-11 simulator with the test's ini file.
#
#   B2  BOOT given by a program (no ROM image), test tertiary loader
#   B3  as B2, first request ignored: repeated (after 5 s of simulated time)
#   B4  as B2, a Memory Load sent twice: stored once, next asked again
#   B5  as B2 on a DELUA: device type 11 in the first request
#   B6  through DEC's M9312 ROMs (248F1 console, XE0; images needed)
#   B7  DEC's SECUNA.SYS/TERUNA.SYS: TERUNA loads a test system through
#       the normal port (DEC files needed; also known issues 111-113)
#   B8  BOOT without an attached network: refused (PCEI)
# Needs for B6/B7: m9312_248F1.bin, m9312_xe.bin (see ../m9312_roms/README.txt);
# for B7 also SECUNA.SYS and TERUNA.SYS (DECnet-11M kit, [136,54]).  Missing
# files: SKIP.  UDP ports 22241-22292.
#
# usage: run_tests.sh BIN/pdp11 [B2 ... B8]
BIN=$(readlink -f "$1"); shift
TESTS=${*:-B2 B3 B4 B5 B6 B7 B8}
cd "$(dirname "$0")"
fail=0

grep_all () {            # grep_all FILE MESSAGE...
    local f=$1; shift
    for m in "$@"; do
        grep -q -- "$m" "$f" || { echo "  missing: $m"; return 1; }
    done
    return 0
}
result () { if [ "$2" = 1 ]; then echo "$1 PASS"; else echo "$1 FAIL"; fail=1; fi; }

for t in $TESTS; do
    ini=$(echo $t | tr B b).ini
    need=""
    case $t in
    B6) need="m9312_248F1.bin m9312_xe.bin";;
    B7) need="m9312_248F1.bin m9312_xe.bin SECUNA.SYS TERUNA.SYS";;
    esac
    missing=""
    for f in $need; do [ -f $f ] || missing="$missing $f"; done
    if [ -n "$missing" ]; then echo "$t SKIP (missing$missing)"; continue; fi
    rm -f $t.out $t.mop
    n=$(echo $t | tr -d B)
    lp=$((22221 + 10 * n)); hp=$((lp + 1))
    case $t in
    B2|B6) opts="";;
    B3) opts="--dropfirst";;
    B4) opts="--resend";;
    B5) opts="--devtype 11";;
    B7) opts="--secondary SECUNA.SYS --tertiary TERUNA.SYS --system SYSTEM_BOOT_OK";;
    esac
    mpid=""
    if [ $t != B8 ]; then
        python3 -I mophost.py $hp $lp $opts > $t.mop 2>&1 &
        mpid=$!
        sleep 0.3
    fi
    SDL_AUDIODRIVER=dummy timeout 120 "$BIN" $ini < /dev/null 2>&1 | tr -d '\r' > $t.out
    [ -n "$mpid" ] && wait $mpid
    ok=1
    case $t in
    B2|B3|B4|B5|B6)
        grep_all $t.out "DEUNA BOOT OK" "HALT instruction, PC: 002034" "R1:	174510" \
                        "2:	012701" "4:	174510" "6:	000137" "10:	002000" || ok=0;;
    B7) grep_all $t.out "SYSTEM_BOOT_OK" "HALT instruction, PC: 002034" || ok=0;;
    B8) grep_all $t.out "BOOT needs an attached network" "R0:	04" || ok=0;;
    esac
    [ $t = B6 -o $t = B7 ] && { grep -q "ETHERNET PHYSICAL ADDRESS = 08-00-2B-12-34-56" $t.out || { echo "  no address printout"; ok=0; }; }
    # B3: the board repeats its request (5 s of simulated time; the wall
    # clock time depends on the host): two requests for the secondary
    [ $t = B3 ] && { [ $(grep -c "send MOP code 08 (30 bytes)" b3.dbg) -ge 2 ] && grep -q "PASS: request repeated" $t.mop \
                     || { echo "  not repeated"; ok=0; }; }
    [ $t = B4 ] && { grep -q "PASS: repeated load" $t.mop || ok=0; }
    if [ -n "$mpid" ]; then
        grep -q "^FAIL" $t.mop && { echo "  mophost: $(grep ^FAIL $t.mop)"; ok=0; }
        grep -q "PASS: tertiary loader sent" $t.mop || { echo "  tertiary not sent"; ok=0; }
    fi
    result $t $ok
done
exit $fail
