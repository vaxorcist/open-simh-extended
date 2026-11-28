$! open-simh-extended/tests/dup_vms/SET_DMCPOINT-DUP.COM  v1
$!
$! DECnet set-up of a VAX with a DMC11 (line and circuit DMC-0) as the
$! load host of PDP-11 node PDPNET (1.20) with a DUP11 (BOOT DUP0),
$! point-to-point. Made from Ulli's SET_DMPPOINT-DUP.COM.
$!
$  SET VER
$!
$! Stop DECnet (and give it time to finish shutting down)
$  MCR NCP SET EXECUTOR STATE OFF
$  WAIT 00:00:05
$!
$! Remove old node PDPNET
$  MCR NCP PURGE NODE PDPNET ALL
$!
$! Remove old circuits and line
$  MCR NCP PURGE KNOWN CIRCUITS ALL
$  MCR NCP PURGE KNOWN LINES ALL
$!
$! Define new line and circuit data (a DMC11 line is always point-to-point;
$! service on the circuit)
$  MCR NCP DEFINE LINE DMC-0 STATE ON
$  MCR NCP DEFINE CIRCUIT DMC-0 SERVICE ENABLED STATE ON
$!
$! Define new node PDPNET data
$  MCR NCP DEFINE NODE 1.20 NAME PDPNET
$  MCR NCP DEFINE NODE PDPNET SERVICE CIRCUIT DMC-0
$  MCR NCP DEFINE NODE PDPNET SECONDARY LOADER SYS$SYSTEM:SECDUP.SYS
$  MCR NCP DEFINE NODE PDPNET TERTIARY LOADER SYS$SYSTEM:TERDUP.SYS
$  MCR NCP DEFINE NODE PDPNET LOAD FILE SYS$SYSTEM:BRUSYS.SYS
$!
$  SET NOVER
$!
$  @SYS$MANAGER:STARTNET
$!
