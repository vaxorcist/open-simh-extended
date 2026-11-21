# open-simh-extended/tests/vms_boot_freeze/single2.py  v1
# Known issue 97: boot one MicroVAX II with SET NOASYNCH (freezes every
# time after the date).  At the date prompt switch on RQ debugging, type
# the date, wait 60 s, stop and probe.
#   usage: python3 -I single2.py SIMULATOR INI DISK DEBUGFLAGS
import pexpect, sys, time, shutil
sim, ini, disk, flags = sys.argv[1:5]
shutil.copy(disk, "nodmv_a.vhd")
p = pexpect.spawn(sim, [ini], encoding="latin-1", timeout=600,
                  env={"SDL_AUDIODRIVER": "dummy", "PATH": "/usr/bin:/bin"})
p.logfile_read = open("single2.log", "w")
p.expect(r"\(DD-MMM-YYYY +HH:MM\) +")
p.send("\x05"); p.expect("sim> ")
p.sendline("set debug single2.dbg"); p.expect("sim> ")
p.sendline("set rq debug=%s" % flags); p.expect("sim> ")
p.sendline("continue")
time.sleep(1)
p.send("21-NOV-2026 12:00\r")
i = p.expect([r"job terminated", pexpect.TIMEOUT], timeout=60)
print("finished" if i == 0 else "FROZE")
p.send("\x05"); p.expect("sim> ")
p.sendline("show queue"); p.expect("sim> ")
p.close(force=True)
