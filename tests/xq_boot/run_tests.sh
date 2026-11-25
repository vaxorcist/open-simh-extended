#!/bin/bash
# open-simh-extended/tests/xq_boot/run_tests.sh  v2 (v2: Q1-Q5 also check that no frame has source address 0 (known issue 121))
# Tests of patch 27 (known issues 118, 119, 120): PDP-11/83 BOOT XQ / BOOT XQB
# with the DEQNA, DELQA and DELQA-T boot ROMs over SIMH's UDP Ethernet
# transport; no load host, so a passing boot ROM keeps sending MOP Request
# Program frames (watch.py counts them) and does not halt.
#   Q1-Q3  BOOT XQ, TYPE=DEQNA / DELQA / DELQA-T: no HALT within 20 s, MOP
#          Request Program frames (60-01, code 08) seen          (case 120)
#   Q4     BOOT XQB (second board at 174460), DELQA: the same     (case 118)
#   Q5     Q4 in the realism mode with TYPE=DEQNA
#   Q6     BOOT XQ, stopped, DETACH XQ and EXIT with no listener on the
#          other side (writes fail): no "double free"           (case 119), 5 runs
# usage (from this directory): ./run_tests.sh ../../BIN/pdp11
SIM=${1:-../../BIN/pdp11}
export SDL_AUDIODRIVER=dummy
pass=0; fail=0
res() { if [ "$2" = ok ]; then echo "$1 PASS"; pass=$((pass+1)); else echo "$1 FAIL: $3"; fail=$((fail+1)); fi; }

boot_case() {   # name dev type realism
  local n=$1 dev=$2 ty=$3 rl=$4 p=$((21500 + RANDOM % 400 * 2))
  { [ -n "$rl" ] && echo "set realism enabled"
    echo "set cpu 11/83"
    echo "set $dev enabled"
    echo "set $dev type=$ty"
    echo "attach $dev udp:$p:127.0.0.1:$((p+1))"
    echo "boot $dev"; } > $n.ini
  python3 -I watch.py $((p+1)) 22 > $n.frames &
  sleep 1
  timeout 20 $SIM $n.ini < /dev/null > $n.out 2>&1
  wait
  local req=$(grep -c "type 6001 len 60 0b0008" $n.frames)
  if grep -q "HALT instruction" $n.out; then res $n bad "$(grep HALT $n.out)"
  elif [ "$req" -lt 3 ]; then res $n bad "only $req MOP Request Program frames"
  elif grep -q "<- 00-00-00-00-00-00" $n.frames; then res $n bad "frame with source address 0"
  else res $n ok; fi
}
boot_case Q1 xq DEQNA
boot_case Q2 xq DELQA
boot_case Q3 xq DELQA-T
boot_case Q4 xqb DELQA
boot_case Q5 xqb DEQNA realism

printf 'set cpu 11/83\nset xq enabled\nset xq type=delqa\nattach xq udp:21405:127.0.0.1:21406\nbreak 10616\nboot xq\ndetach xq\nexit\n' > Q6.ini
bad=0
for i in 1 2 3 4 5; do
  timeout 20 $SIM Q6.ini < /dev/null > Q6.out 2>&1
  grep -q "Breakpoint" Q6.out || bad=$((bad+1))
  grep -q "double free\|corruption\|Aborted" Q6.out && bad=$((bad+1))
done
[ $bad -eq 0 ] && res Q6 ok || res Q6 bad "$bad of 5 runs failed (see Q6.out)"
echo "passed $pass, failed $fail"
