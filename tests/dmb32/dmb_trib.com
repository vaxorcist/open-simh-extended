$! open-simh-extended/tests/dmb32/dmb_trib.com  v3 (2026-11-18; v3: no executor TYPE (the node type
$! stays DECnet's default); a running DECnet is stopped with SET EXECUTOR STATE OFF and started
$! again with STARTNET instead of asking for a reboot; v2: executor purged before the node
$! entries, which include the executor's own (no "Unrecognized component" warning))
$!
$! Configure DECnet Phase IV on VMS V5.5 (VAX 8200/8300) as a DDCMP
$! TRIBUTARY on the DMB32 synchronous line (SIMH unit DMB-SYNC, VMS
$! SIDRIVER device SIA0, DECnet line/circuit DMB-0):
$!     node 1.4, name VAX4 (node type: DECnet's default, not set here)
$!
$! The whole old DECnet configuration is removed first from the PERMANENT
$! database (circuits, lines, all node entries, executor, logging).
$! DECnet objects (FAL, NML, ...) are NOT purged: they are the standard
$! object definitions DECnet needs to work.
$!
$! At the end DECnet is (re)started with the new configuration: a running
$! DECnet is stopped with NCP SET EXECUTOR STATE OFF (all logical links are
$! aborted), then @SYS$MANAGER:STARTNET loads the new permanent database.
$!
$! Parameter:
$!     P1  tributary address of this station (1-255), default 4.
$!         It must match the tributary the control station defines for
$!         this node (e.g. on a DMP11/DMV11 control station:
$!         DEFINE CIRCUIT DMP-0.4 TRIBUTARY 4 STATE ON).
$!
$! Usage (as SYSTEM):   @DMB_TRIB          or   @DMB_TRIB 4
$!
$! SIMH side (before BOOT): the DMB32 sync line connects to the control
$! station's DDCMP device, e.g.
$!     SET DMB-SYNC PEER=<control host>:<control port>
$!     ATTACH DMB-SYNC <local port>
$!
$ SET NOON                              ! purges of absent entries only warn
$ ON CONTROL_Y THEN GOTO done
$ NCP := $SYS$SYSTEM:NCP
$!
$! ---- check the tributary address -------------------------------------------
$ trib = P1
$ IF trib .EQS. "" THEN trib = "4"
$ IF F$TYPE(trib) .NES. "INTEGER" THEN GOTO bad_trib
$ IF F$INTEGER(trib) .LT. 1 .OR. F$INTEGER(trib) .GT. 255 THEN GOTO bad_trib
$!
$! ---- privileges and the DMB32 sync device ------------------------------------
$ old_priv = F$SETPRV("SYSPRV,OPER,NETMBX,TMPMBX,SYSNAM,CMKRNL")
$ IF .NOT. F$PRIVILEGE("SYSPRV,OPER") THEN GOTO no_priv
$ IF .NOT. F$GETDVI("SIA0","EXISTS") THEN -
     WRITE SYS$OUTPUT "%DMB_TRIB-W-NOSIA, SIA0: (DMB32 sync line) not configured; check SET DMB ENABLE in SIMH"
$!
$! ---- 1. remove the old DECnet configuration (permanent database) -----------
$ WRITE SYS$OUTPUT "Removing the old DECnet configuration ..."
$ NCP PURGE KNOWN CIRCUITS ALL
$ NCP PURGE KNOWN LINES ALL
$ NCP PURGE KNOWN LOGGING ALL
$ NCP PURGE EXECUTOR ALL                ! first: KNOWN NODES also removes
$ NCP PURGE KNOWN NODES ALL             ! the executor's own node entry
$!
$! ---- 2. executor: node 1.4 VAX4 ---------------------------------------------------
$ WRITE SYS$OUTPUT "Defining executor 1.4 VAX4 ..."
$ NCP DEFINE EXECUTOR ADDRESS 1.4
$ NCP DEFINE EXECUTOR NAME VAX4
$ NCP DEFINE EXECUTOR MAXIMUM ADDRESS 1023
$ NCP DEFINE EXECUTOR STATE ON
$!
$! ---- 3. line and circuit DMB-0: DDCMP tributary --------------------------------
$ WRITE SYS$OUTPUT "Defining line and circuit DMB-0 as DDCMP tributary ''trib' ..."
$ NCP DEFINE LINE DMB-0 PROTOCOL DDCMP TRIBUTARY
$ NCP DEFINE LINE DMB-0 STATE ON
$ NCP DEFINE CIRCUIT DMB-0 TRIBUTARY 'trib'
$ NCP DEFINE CIRCUIT DMB-0 STATE ON
$!
$! ---- 4. event logging to the operator console (useful for the line tests) -----
$ NCP DEFINE LOGGING MONITOR KNOWN EVENTS
$ NCP DEFINE LOGGING MONITOR STATE ON
$!
$! ---- 5. show the result ----------------------------------------------------------
$ WRITE SYS$OUTPUT ""
$ NCP LIST EXECUTOR CHARACTERISTICS
$ NCP LIST LINE DMB-0 CHARACTERISTICS
$ NCP LIST CIRCUIT DMB-0 CHARACTERISTICS
$ NCP LIST KNOWN NODES
$!
$! ---- 6. (re)start DECnet with the new configuration ---------------------------
$! A running DECnet (process NETACP) is stopped first; STARTNET then loads the
$! permanent database defined above.
$ DEFINE/USER_MODE SYS$OUTPUT NL:
$ DEFINE/USER_MODE SYS$ERROR NL:
$ SHOW PROCESS NETACP
$ IF $STATUS
$ THEN
$     WRITE SYS$OUTPUT "Stopping DECnet (SET EXECUTOR STATE OFF) ..."
$     NCP SET EXECUTOR STATE OFF
$     n = 0
$ wait_off:                             ! wait until NETACP has gone (max. 60 s)
$     WAIT 00:00:02
$     n = n + 1
$     DEFINE/USER_MODE SYS$OUTPUT NL:
$     DEFINE/USER_MODE SYS$ERROR NL:
$     SHOW PROCESS NETACP
$     IF $STATUS .AND. n .LT. 30 THEN GOTO wait_off
$ ENDIF
$ WRITE SYS$OUTPUT "Starting DECnet ..."
$ @SYS$MANAGER:STARTNET
$ WAIT 00:00:05
$ NCP SHOW EXECUTOR
$ NCP SHOW CIRCUIT DMB-0 STATUS
$ GOTO done
$!
$ bad_trib:
$ WRITE SYS$OUTPUT "%DMB_TRIB-E-BADTRIB, tributary address must be 1-255, not ''P1'"
$ GOTO done
$ no_priv:
$ WRITE SYS$OUTPUT "%DMB_TRIB-E-NOPRIV, needs SYSPRV and OPER (run it as SYSTEM)"
$ done:
$ IF F$TYPE(old_priv) .NES. "" THEN old_priv = F$SETPRV(old_priv)
$ EXIT
