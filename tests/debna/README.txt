open-simh-extended/tests/debna/README.txt  v1 (2026-11-16)

Tests of the DEBNA VAXBI Ethernet adapter (device ET, patch 10) on the
VAX 8200/8300 with VMS V4.7.  The 8200 boots from the KDB50 (a copy of
Ulli's VMS047_1.R81, SYSTEM/MANAGER).  The DECnet partner is a MicroVAX II
with its DELQA (XQ) and VMS047.R81, connected by a SIMH UDP link on
localhost (ports 5101/5102), so no host interface is needed.

VMS V4.7 names the DEBNA line and circuit BNT-0 (after the DEBNT, the
DEBNA's earlier name); ET-0 is refused ("invalid device name").

Files

  p1.ini        single 8200, DEBNA on a UDP port without a partner:
                SYSGEN SHOW/ADAPTER, SHOW DEVICE ETA0, DECnet on BNT-0,
                line characteristics and counters after 40 s
  p2_8200.ini   P2, VAX 8200 side (node MV1 = 1.1, line BNT-0): drives the
                test (see below)
  p2_mv2.ini    P2/P3, MicroVAX II side (node MV2 = 1.2, line QNA-0):
                sets the SYSTEM password to MANAGER (the disk has none)
                and network access, starts DECnet, waits for DONE.TMP
  p3_8300.ini   P3: the P2 test on a VAX 8300 with both CPUs busy
                (START/CPU, two CPU-bound loop processes)
  p2.sh         runs P2 or P3: both simulators, then PASS/FAIL

Running

    SDL_AUDIODRIVER=dummy BIN/vax8200 p1.ini > p1.out < /dev/null
        (VMS047_1.R81 in the current directory)

    tests/debna/p2.sh BIN VMS047_1.R81 VMS047.R81 [workdir] [p3_8300.ini]
        (copies both disks into the work directory, ./p2run by default;
        takes 5-8 minutes; prints RESULT: PASS or FAIL with the checks)

What P2 does (8200 side)

  - DECnet on BNT-0, then a loop (R.COM, up to 2 minutes) until a remote
    DIRECTORY on MV2 works;
  - NCP SHOW KNOWN NODES / ADJACENT NODES;
  - COPY SYS$HELP:HELPLIB.HLB (2235 blocks) to MV2 and back, DIFFERENCES;
  - remote DIRECTORY; NCP LOOP NODE MV2 COUNT 50 LENGTH 1000;
  - SET HOST MV2 (login, SHOW LOGICAL SYS$NODE, LOGOUT);
  - NCP SHOW LINE BNT-0 COUNTERS; DONE.TMP to MV2 (ends the partner).

Test   Expected                                             Result 2026-11-16
-----  ---------------------------------------------------  -----------------
p1     "7  BI - AIE Adapter with NI port only", ETA0        pass
       online, circuit up, hardware address = SHOW ET MAC,
       frames sent, no send failure, no circuit fault
p2     adjacency up, MV2 reachable, 0 difference sections,  PASS (about 3400
       2235 blocks on MV2, LOOP without "not looped",       frames, 1.4 MB
       SET HOST login and "control returned", no circuit     each way)
       fault, both sides end
p3     as p2 on the 8300; SHOW CPU: attached processor in   PASS (both loops
       the EXECUTE state                                     60 s CPU)

Regression (P4): tests/kdb50/p3.ini (boot from the KDB50) and
tests/dmb32/p3.ini (DMB32 printer = LP11 output) pass with the DEBNA build.

Notes

  - The 8200's UDP link prints "Sockets: _eth_reader(udp): error ..."
    lines while the partner is not yet listening; p2.sh removes them from
    the console copies (p2_8200.txt, p2_mv2.txt).  Such host side errors
    are not send failures for VMS (the frames are lost, as on a cable with
    nobody listening).
  - Network boot (BOOT ET) is not possible yet: known issues case 69.
