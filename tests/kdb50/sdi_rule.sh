#!/bin/bash
# open-simh-extended/tests/kdb50/sdi_rule.sh  v2
# Console checks of the realism rule "an SDI controller only connects SDI
# drives" (known issues case 68, decided by Ulli 2026-11-16): unattached
# non-SDI units on an SDI controller are disabled with a message at SET
# REALISM ENABLED (SET REALISM ENABLED needs all disks detached anyway), an
# ATTACH of a non-SDI unit is refused, and SET RQn <non-SDI type> is refused
# while sound is enabled.
# Usage: sdi_rule.sh <repository directory>   (runs in a scratch directory)
set -u
REPO=${1:-/home/claude/kdb}
W=$(mktemp -d)
cd "$W" || exit 1
ln -s "$REPO/sound" sound
run () {    # run <simulator> <commands...>: commands one per argument
    local sim=$1; shift
    printf '%s\n' "$@" exit | SDL_AUDIODRIVER=dummy "$REPO/BIN/$sim" 2>&1 |
        grep -i -E "sdi|disabled|error|^RQ[0-9]|^KDB[0-9]|Sound:  *enabled"
}
echo "== 1. VAX-11/780 defaults (UDA50A, RQ0-2 RD54, RQ3 RX50): RD54s and RX50 disabled"
run vax780 "set realism enabled"
echo "== 2. VAX-11/780 with realism enabled: RQ1 (RD54) enabled again, ATTACH refused"
run vax780 "set realism enabled" "set rq1 enabled" "attach -n rq1 rd.dsk"
echo "== 3. VAX-11/780 with realism enabled: SET RQ0 RD54 refused, RQ0 stays RA81"
run vax780 "set rq0 ra81" "set realism enabled" "set rq0 rd54" "show rq0"
echo "== 4. MicroVAX II (RQDX3, not SDI): no SDI messages"
run microvax2 "set realism enabled"
echo "== 5. VAX 8200 KDB50 with realism enabled: SET KDB2 RD54 refused"
run vax8200 "set realism enabled" "set kdb2 rd54" "show kdb2"
cd /
echo "(scratch directory $W)"
