#!/bin/bash
# open-simh-extended/tests/vax8300/smp55_rep.sh  v1 (2026-11-18; from Claude's workspace rep.sh v2)
# Repeat the VMS V5.5 VAX 8300 run smp55.ini (128MB) with one simulator binary,
# to count failures (SPLRELERR, MACHINECHK, "Exception in interrupt or exception").
# Usage: smp55_rep.sh <binary> <tag> <count> <port base> [speed command, default "echo normal"]
# Environment: IMG = the VMS V5.5 RA82 image to copy (default /mnt/user-data/uploads/VMS055_1.R82).
# Each run gets its own directory W_<tag>_<n> (in the current directory) with a fresh copy of
# the image, which is removed after the run; the result line goes to stdout.
BIN="$1"; TAG="$2"; N="$3"; PORT="$4"; SPEED="${5:-echo normal}"
HERE=$(cd "$(dirname "$0")" && pwd)
IMG="${IMG:-/mnt/user-data/uploads/VMS055_1.R82}"
for i in $(seq 1 "$N"); do
    d="W_${TAG}_${i}"
    rm -rf "${d:?}"; mkdir -p "$d"
    cp "$HERE/smp55.ini" "$d/"; cp "$IMG" "$d/v55.r82"; chmod u+w "$d/v55.r82"
    ( cd "$d" && SDL_AUDIODRIVER=dummy timeout 1000 "$BIN" smp55.ini "set cpu 128m" "$SPEED" $((PORT + i)) > w.out 2> w.err < /dev/null )
    r=$(grep -a -o "FATAL BUG CHECK.*\|Exception in interrupt.*\|\*\*\* h1 done" "$d/w.out" | head -1)
    echo "$TAG run $i: ${r:-no result (timeout)}"
    rm -f "$d/v55.r82"
done
