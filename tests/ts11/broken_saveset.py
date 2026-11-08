# open-simh-extended/tests/ts11/broken_saveset.py  v1
#
# Generates a MicroVAX II test (VMS V4.7, TSV05): BACKUP/IMAGE to a
# foreign-mounted tape, interrupted with Ctrl/Y (a save set without tape
# marks), DISMOUNT/NOUNLOAD, MOUNT/FOREIGN, BACKUP again without /REWIND.
# The second BACKUP skips forward beyond the recorded data and fails with
# %BACKUP-F-POSITERR / DRVERR (OPI, class 6), in the normal mode and in the
# realism mode alike (and on a real drive: the TS11 sets OPI after 25 ft
# without data); the realism mode moves the 25 ft first.
# Usage: python3 -I broken_saveset.py normal|real <name>  -> <name>.ini
#        (needs <name>.vhd, a copy of VMS047-1.R81; runs with BIN/microvax2)
import sys
real = sys.argv[1] == "real"
name = sys.argv[2]
s = "; %s.ini v1 (mk.py): interrupted BACKUP, then BACKUP again without /REWIND\n" % name
if real: s += "set sound enabled\n"
s += "set cpu 16m\nset rq0 ra81\nattach rq0 %s.vhd\nattach -n ts %s.tap\n" % (name, name)
if real: s += "set ts online\n"
s += "set debug %s.dbg\nset ts debug=req\nset runlimit 1800 seconds\n" % name
s += 'expect ">>>" send "B DUA0\\r"; continue\nexpect "HH:MM)" send "8-NOV-2026 09:30\\r"; continue\n'
s += 'expect "job terminated" send after=2000000,"\\r"; continue\nexpect "Username:" send "SYSTEM\\r"; continue\n'
s += 'expect "Password:" send "MANAGER\\r"; continue\nexpect "Welcome"\nboot cpu\n'
def step(c, t):
    return 'send after=500000,"%s\\rWRITE SYS$OUTPUT \\"=%s\\",\\"Z=\\"\\r"\nexpect "=%sZ="\ncontinue\n' % (c, t, t)
s += step("INIT MSA0: TEST", "A") + step("MOUNT/FOREIGN MSA0:", "B")
s += 'send after=500000,"BACKUP/IGNORE=INTERLOCK/IMAGE/LOG DUA0: MSA0:TEST.BCK\\r"\nexpect "COPIED"\ncontinue\n'
s += 'send after=1000000,"\\031"\n'          # Ctrl/Y: partial save set
s += step("DISMOUNT/NOUNLOAD MSA0:", "C") + step("MOUNT/FOREIGN MSA0:", "D")
s += "echo ==== second BACKUP\nshow ts\nshow ts transport\n"
s += 'send after=500000,"BACKUP/IGNORE=INTERLOCK/IMAGE/LOG DUA0: MSA0:TEST.BCK\\r"\nexpect "BACKUP-F-"\nexpect "COPIED"\ncontinue\n'
s += "echo ==== result\nshow ts transport\nexit\n"
open(name + ".ini", "w").write(s)
