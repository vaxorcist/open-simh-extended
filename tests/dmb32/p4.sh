#!/bin/bash
# open-simh-extended/tests/dmb32/p4.sh  v2 (v2: the 8200 script as 6th argument: p4a_8200.ini = VMS V4.7 with the DMF32)
# DMB32 phase P4: DECnet over the DMB32 synchronous line (DDCMP) between a
# VAX 8200 with VMS V5.5 (p4_8200.ini, line DMB-0) and a partner with
# VMS V4.7: a MicroVAX II with its DMV11 (p4_mv2.ini, line DMP-0) or a
# VAX-11/780 with its DMF32 (p4_780.ini, line DMF-0).  The two DDCMP
# devices are connected by a SIMH TCP link on localhost (ports 4100/4101).
#
# Usage:  p4.sh <BIN directory> <8200 disk> <VMS047.R81 (partner)> [work dir] [partner: mv2 | 780] [8200 script]
#         8200 script p4_8200.ini (default): VMS055.R82, DMB32 line DMB-0;
#         p4a_8200.ini: VMS047_1.R81, the DMF32 on the 8200's Unibus, line DMF-0
#
# The two disks are copied into the work directory (default ./p4run); the
# originals are not written.  Both simulators run at the same time; the
# 8200 drives the test (see p4_8200.ini), the partner ends when the 8200
# has copied DONE.TMP to it.  At the end the script checks the 8200's
# console and prints PASS or FAIL with the failed checks.
set -u
BIN=${1:?BIN directory}; D8200=${2:?8200 disk}; DPART=${3:?partner disk}
W=${4:-./p4run}
P=${5:-mv2}
INI=${6:-p4_8200.ini}
case $P in
    mv2) PSIM=microvax2; PINI=p4_mv2.ini;;
    780) PSIM=vax780;    PINI=p4_780.ini;;
    *)   echo "partner must be mv2 or 780"; exit 1;;
esac
T=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$W" && cd "$W" || exit 1
cp "$D8200" "$(basename "$D8200")" && cp "$DPART" VMS047.R81 || exit 1
cp "$T/$INI" "$T/$PINI" .
rm -f p4.dbg p4_8200.out p4_part.out
SDL_AUDIODRIVER=dummy timeout 2500 "$BIN/$PSIM" "$PINI" > p4_part.out 2>&1 < /dev/null &
PP=$!
sleep 1
SDL_AUDIODRIVER=dummy timeout 2500 "$BIN/vax8200" "$INI" > p4_8200.out 2>&1 < /dev/null &
P8200=$!
wait $P8200
for i in $(seq 1 60); do                        # the partner ends by itself
    kill -0 $PP 2>/dev/null || break
    sleep 2
done
kill $PP 2>/dev/null
tr -d '\r' < p4_8200.out > p4_8200.txt
tr -d '\r' < p4_part.out > p4_part.txt
fail=""
chk() { grep -aq "$2" "$3" || fail="$fail $1"; }
chk circuitup   "circuit up"                           p4_8200.txt
chk reachable   "1\.2 .*reachable"                     p4_8200.txt
chk copy        "Number of difference sections found: 0" p4_8200.txt
chk directory   "Total of 1 file, [0-9][0-9]* blocks"  p4_8200.txt
chk sethost     "on node MV2"                          p4_8200.txt
chk logout      "control returned to node"             p4_8200.txt
chk done8200    "\*\*\* 8200 done"                     p4_8200.txt
chk donepart    "\*\*\* MV2 done"                      p4_part.txt
grep -aq "circuit fault" p4_8200.txt && fail="$fail circuitfault"
grep -aq "Messages not looped" p4_8200.txt && fail="$fail loop"
if [ -z "$fail" ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL:$fail"; fi
