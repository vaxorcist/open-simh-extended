# open-simh-extended/tests/vms_boot_freeze/lbn2.cmd  v3 (v3: DUMP takes no wildcards here: a DCL loop over F$SEARCH)
# Known issue 97: retrieval pointers of every file on the system disk, to
# find the file that holds LBN 34113.
OPEN/WRITE F SYS$LOGIN:L.COM
WRITE F "$ OPEN/WRITE O SYS$LOGIN:H.LIS"
WRITE F "$ CLOSE O"
WRITE F "$LOOP:"
WRITE F "$ F = F$SEARCH(""SYS$SYSDEVICE:[000000...]*.*;*"")"
WRITE F "$ IF F .EQS. """" THEN EXIT"
WRITE F "$ DUMP/HEADER/BLOCKS=COUNT:0/OUTPUT=SYS$LOGIN:H1.LIS 'F'"
WRITE F "$ APPEND SYS$LOGIN:H1.LIS SYS$LOGIN:H.LIS"
WRITE F "$ DELETE SYS$LOGIN:H1.LIS;*"
WRITE F "$ GOTO LOOP"
CLOSE F
@SYS$LOGIN:L.COM
SEARCH/OUTPUT=SYS$LOGIN:H2.LIS SYS$LOGIN:H.LIS "File name:","LBN:"
TYPE SYS$LOGIN:H2.LIS
