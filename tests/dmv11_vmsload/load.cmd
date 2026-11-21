# open-simh-extended/tests/dmv11_vmsload/load.cmd  v3 (v3: defines the loader files again first, so an older definition on the disk cannot interfere; v2: comments start with #, which vmsrun.py skips; with ! they went to DCL) (2026-11-21)
# DCL commands for vmsrun.py: start DECnet, then load the PDP-11.  NCP
# sends Enter MOP Mode, the PDP-11's DMV11 answers with Request Program
# (secondary loader) and VMS sends HELLO.TSK as one Memory Load with
# Transfer Address.  NCP then waits for the next request, which this test
# program never sends, so LOAD NODE does not return here.
MC NCP DEFINE NODE PDP SECONDARY LOADER SYS$SYSTEM:HELLO.TSK LOAD FILE SYS$SYSTEM:HELLO.TSK
@SYS$MANAGER:STARTNET
!WAIT 20
MC NCP SHOW KNOWN CIRCUITS
MC NCP LOAD NODE PDP
!WAIT 30
