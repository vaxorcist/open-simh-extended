# open-simh-extended/tests/dmv_regress/setpw.cmd  v1
# Give SYSTEM the password MANAGER, so remote file access works with
# NODE"SYSTEM MANAGER"::  (the account had no password on this disk).
SET DEFAULT SYS$SYSTEM
MC AUTHORIZE MODIFY SYSTEM/PASSWORD=MANAGER/NOPWDEXPIRED
SET DEFAULT SYS$LOGIN
