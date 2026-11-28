open-simh-extended/tests/listen_reuse/README.txt  v1

Tests of patch 31 (known issue 132): a listening port can be attached again
at once after a simulator that had connections on it exited.  Before, the
second ATTACH failed for about a minute with
    Sockets: bind error 98 - Address already in use
    %SIM-ERROR: Can't open network port: 127.0.0.1:port
(ATTACH -U helped only when the earlier simulator had used -U as well).

  ./r.sh <build dir> [L1 L2 L3 L4]     (build dir holds BIN/vax780, BIN/pdp11)

  L1  vax780 DMP0: A listens, runs, a client connects, A exits, the client
      closes at once (A's side in TIME_WAIT); B attaches the port: PASS
  L2  the same with the client's end open 5 s longer (FIN_WAIT2): PASS
  L3  A still listening: B's ATTACH of the same port must be refused
  L4  as L1 on the PDP-11 (11/73) with a DMV11

Each run picks fresh ports (an earlier run's TIME_WAIT sockets would get in
the way).  Linux only (reads /proc/net/tcp to show the TCP states).

Results (2026-11-28): integration 3d38be70 (before): L1, L2, L4 FAIL, L3
PASS; listen-reuse 4655653e and integration 19c5fa7a: 4 of 4 PASS.
