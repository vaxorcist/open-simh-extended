# open-simh-extended/tests/dmv_regress/setupB.cmd  v1
# Run once on diskB.vhd (a copy of Ulli's VMS047.R81 with the DMV11 boot
# test setup): make it node 1.3 VAX3, so diskA (1.4 VAX4) and diskB form a
# two-node DECnet pair.  Both disks already know VAX1-VAX4.
MC NCP DEFINE EXECUTOR ADDRESS 1.3
MC NCP LIST EXECUTOR
