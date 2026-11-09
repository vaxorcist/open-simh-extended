$! open-simh-extended/tests/tk50/MKSMALL.COM  v1
$!
$! Creates test files of one block each (default 1000) for the TK50
$! repositioning test: BACKUP of many small files gives a much lower data
$! rate than the same amount in large files (a seek and a file header per
$! file), so the tape should reposition much more often.
$!
$! Usage:   @MKSMALL [count] [directory]
$!          @MKSMALL                         1000 files in SYS$SYSDEVICE:[SMALLTEST]
$!          @MKSMALL 500 DUA1:[TEST]         500 files in DUA1:[TEST]
$!
$! Each file SMALLnnnn.DAT holds 5 records of 87 characters: 5 * (87 + 2
$! byte count + 1 pad) = 450 bytes, less than one block (512 bytes).
$!
$! Then, for example:
$!          BACKUP SYS$SYSDEVICE:[SMALLTEST]*.DAT MUA0:SMALL.BCK/SAVE_SET/REWIND
$! Delete the files again with:
$!          DELETE SYS$SYSDEVICE:[SMALLTEST]*.DAT;*
$!
$ ON CONTROL_Y THEN GOTO STOP                   ! Ctrl/Y: close the open file
$ ON ERROR THEN GOTO STOP
$ count = 1000                                  ! number of files
$ IF P1 .NES. "" THEN count = F$INTEGER (P1)
$ dir = "SYS$SYSDEVICE:[SMALLTEST]"             ! target directory
$ IF P2 .NES. "" THEN dir = P2
$ IF F$PARSE (dir) .EQS. "" THEN CREATE/DIRECTORY 'dir'   ! create it if needed
$ text = "THIS IS TEST DATA FOR THE TK50 REPOSITIONING TEST. 0123456789ABCDEF"
$ n = 0
$FILE_LOOP:
$ n = n + 1
$ IF n .GT. count THEN GOTO DONE
$ num = F$FAO ("!4ZL", n)                       ! 0001, 0002, ...
$ OPEN/WRITE testfile 'dir'SMALL'num'.DAT
$ r = 0
$RECORD_LOOP:                                   ! 5 records = one block
$ r = r + 1
$ WRITE testfile "FILE ", num, " RECORD ", r, " ", text
$ IF r .LT. 5 THEN GOTO RECORD_LOOP
$ CLOSE testfile
$ IF (n / 100) * 100 .EQ. n THEN WRITE SYS$OUTPUT n, " files created"
$ GOTO FILE_LOOP
$DONE:
$ WRITE SYS$OUTPUT "Created ", count, " files in ", dir
$ DIRECTORY/SIZE/GRAND_TOTAL 'dir'SMALL*.DAT
$ EXIT
$STOP:
$ IF F$TRNLNM ("TESTFILE") .NES. "" THEN CLOSE testfile
$ WRITE SYS$OUTPUT "Stopped after ", n - 1, " files"
$ EXIT
