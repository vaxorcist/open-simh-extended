#!/bin/bash
# open-simh-extended/tests/debna/p2.sh  v3 (v3: the 8200 disk keeps its file name (VMS V5.5: p55_8200.ini with VMS055.R82), reachable check by address, directory check for any size (V5.5 HELPLIB.HLB 5675 blocks); v2: the 8200 script as 5th argument, for P3)
# DEBNA phase P2: DECnet over Ethernet between a VAX 8200 with the DEBNA
# (p2_8200.ini) and a MicroVAX II with its DELQA (p2_mv2.ini), connected
# by a SIMH UDP link (ports 5101/5102 on localhost, no host interface).
#
# Usage:  p2.sh <BIN directory> <8200 disk: VMS047_1.R81, or VMS055.R82 with p55_8200.ini> <VMS047.R81 (MicroVAX II)> [work dir] [8200 script]
#         (the 8200 script: p2_8200.ini by default, p3_8300.ini for phase P3)
#
# The two disks are copied into the work directory (default ./p2run); the
# originals are not written.  Both simulators run at the same time; the
# 8200 drives the test (see p2_8200.ini), the MicroVAX II ends when the
# 8200 has copied DONE.TMP to it.  At the end the script checks the 8200's
# console and prints PASS or FAIL with the failed checks.
set -u
BIN=${1:?BIN directory}; D8200=${2:?8200 disk}; DMV2=${3:?MicroVAX II disk}
W=${4:-./p2run}
INI=${5:-p2_8200.ini}
T=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$W" && cd "$W" || exit 1
cp "$D8200" "$(basename "$D8200")" && cp "$DMV2" VMS047.R81 || exit 1
cp "$T/$INI" "$T/p2_mv2.ini" .
rm -f p2.dbg p3.dbg p2_8200.out p2_mv2.out
SDL_AUDIODRIVER=dummy timeout 2500 "$BIN/microvax2" p2_mv2.ini > p2_mv2.out 2>&1 < /dev/null &
PMV2=$!
sleep 1
SDL_AUDIODRIVER=dummy timeout 2500 "$BIN/vax8200" "$INI" > p2_8200.out 2>&1 < /dev/null &
P8200=$!
wait $P8200
for i in $(seq 1 60); do                        # the partner ends by itself
    kill -0 $PMV2 2>/dev/null || break
    sleep 2
done
kill $PMV2 2>/dev/null
# The 8200 console without the UDP link's socket messages (they come while
# the partner is not yet listening)
tr -d '\r' < p2_8200.out | grep -av "_eth_\|^Mon Nov" > p2_8200.txt
tr -d '\r' < p2_mv2.out | grep -av "_eth_\|^Mon Nov" > p2_mv2.txt
fail=""
chk() { grep -aq "$2" "$3" || fail="$fail $1"; }
chk adjacency   "adjacency up"                         p2_8200.txt
chk reachable   "1\.2 .*reachable"                     p2_8200.txt
chk copy        "Number of difference sections found: 0" p2_8200.txt
chk directory   "Total of 1 file, [0-9][0-9]* blocks"  p2_8200.txt
chk sethost     "on node MV2"                          p2_8200.txt
chk logout      "control returned to node"             p2_8200.txt
chk done8200    "\*\*\* 8200 done"                     p2_8200.txt
chk donemv2     "\*\*\* MV2 done"                      p2_mv2.txt
grep -aq "circuit fault" p2_8200.txt && fail="$fail circuitfault"
grep -aq "Messages not looped" p2_8200.txt && fail="$fail loop"
if [ -z "$fail" ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL:$fail"; fi
