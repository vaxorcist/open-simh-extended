#!/bin/bash
# open-simh-extended/tests/listen_reuse/r.sh  v1
# Known issue 132: a listening port could not be attached again for about a
# minute after a simulator that had connections on it exited ("Sockets: bind
# error 98 - Address already in use").  Tests (Linux):
#   L1  A listens (ATTACH DMP0 127.0.0.1:port), runs, a client connects; A exits
#       and the client closes at once (A's side in TIME_WAIT); B attaches the
#       same port: must succeed without -U
#   L2  the same, but the client keeps its end open for 5 s (A's side in
#       FIN_WAIT2): B must succeed
#   L3  A listens and stays; B attaches the same port at the same time: must be
#       refused (two listeners on one address and port are never allowed)
#   L4  the same as L1 on the PDP-11 with a DMV11 (another simulator, same core)
# usage: r.sh <dir with BIN/vax780 and BIN/pdp11> [L1 ... L4]
# Each simulator runs from a generated .ini that ends with EXIT; A runs a
# 2-instruction loop until SIGINT (VAX: INCL R0, BRB .-2; PDP-11: INC R0, BR .-2).
D=$1; shift; T=${*:-L1 L2 L3 L4}
W=$(mktemp -d); cd $W
port=$((20000 + (RANDOM % 1000) * 8))   # fresh ports each run: an earlier run's
                                         # TIME_WAIT sockets would get in the way
pass=0; fail=0
states () {     # print the TCP states of the port (local or remote side)
python3 - $1 <<'PY'
import sys
p="%04X"%int(sys.argv[1]); st={'01':'ESTABLISHED','05':'FIN_WAIT2','06':'TIME_WAIT','08':'CLOSE_WAIT','0A':'LISTEN'}
s=[st.get(l.split()[3],l.split()[3]) for l in open('/proc/net/tcp').readlines()[1:] if l.split()[1].endswith(':'+p)]
print("   port %s on this side: %s" % (sys.argv[1], ", ".join(s) or "nothing"))
PY
}
a_ini () {      # $1 simulator, $2 port
if [ $1 = vax780 ]; then
  printf 'set dmp enable\nattach dmp0 127.0.0.1:%s\ndeposit 0 FC1150D6\ngo 0\nexit\n' $2
else
  printf 'set cpu 11/73\nset dmv enable\nattach dmv0 127.0.0.1:%s\ndeposit 1000 005200\ndeposit 1002 000776\ngo 1000\nexit\n' $2
fi
}
b_ini () {
if [ $1 = vax780 ]; then printf 'set dmp enable\nattach dmp0 127.0.0.1:%s\nshow dmp0\nexit\n' $2
else printf 'set cpu 11/73\nset dmv enable\nattach dmv0 127.0.0.1:%s\nshow dmv0\nexit\n' $2; fi
}
dmx () { [ $1 = vax780 ] && echo DMP0 || echo DMV0; }
exitcase () {   # $1 sim, $2 hold seconds of the client after A's exit
sim=$1; hold=$2; port=$((port+1))
a_ini $sim $port > a.ini; b_ini $sim $port > b.ini
$D/BIN/$sim a.ini < /dev/null > a.log 2>&1 & P=$!; sleep 1.5
python3 -c "
import socket,time
s=socket.create_connection(('127.0.0.1',$port)); s.settimeout(30)
try:
  while s.recv(100): pass
except Exception: pass
time.sleep($hold); s.close()" & C=$!
sleep 3; kill -INT $P; wait $P; sleep 0.8
states $port
timeout -s KILL 10 $D/BIN/$sim b.ini < /dev/null > b.log 2>&1
wait $C 2>/dev/null
grep -q "$(dmx $sim)    attached to 127.0.0.1:$port" b.log
}
for t in $T; do
  case $t in
  L1) echo "L1: vax780 DMP0, TIME_WAIT left by A"; exitcase vax780 0 ;;
  L2) echo "L2: vax780 DMP0, FIN_WAIT2 left by A"; exitcase vax780 5 ;;
  L3) echo "L3: vax780 DMP0, A still listening"; port=$((port+1))
      a_ini vax780 $port > a.ini; b_ini vax780 $port > b.ini
      $D/BIN/vax780 a.ini < /dev/null > a.log 2>&1 & P=$!; sleep 1.5
      timeout -s KILL 10 $D/BIN/vax780 b.ini < /dev/null > b.log 2>&1
      kill -INT $P; wait $P
      grep -q "Can't open network port: 127.0.0.1:$port" b.log ;;
  L4) echo "L4: pdp11 DMV0, TIME_WAIT left by A"; exitcase pdp11 0 ;;
  *) echo "unknown test $t"; false ;;
  esac
  if [ $? = 0 ]; then echo "$t PASS"; pass=$((pass+1)); else echo "$t FAIL"; sed 's/^/   b: /' b.log | grep -v "^   b: *$" | tail -4; fail=$((fail+1)); fi
done
echo "$pass passed, $fail failed"; rm -rf $W; [ $fail = 0 ]
