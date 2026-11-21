# open-simh-extended/tests/vms_boot_freeze/single.py  v2 (v2: the disk copy goes to the file the .ini attaches (read from its 'attach rq0' line), so every run starts fresh)
# Known issue 97: boot ONE MicroVAX II (no second simulator), type the date
# and report whether "job terminated" comes within 180 s.
#   usage: python3 -I single.py SIMULATOR INI DISK
import pexpect, sys, time, shutil
sim, ini, disk = sys.argv[1:4]
import re
target = re.search(r"^attach rq0 (\S+)", open(ini).read(), re.M).group(1)
shutil.copy(disk, target)
p = pexpect.spawn(sim, [ini], encoding="latin-1", timeout=600,
                  env={"SDL_AUDIODRIVER": "dummy", "PATH": "/usr/bin:/bin"})
p.logfile_read = open("single.log", "w")
p.expect(r"\(DD-MMM-YYYY +HH:MM\) +")
time.sleep(1)
p.send("21-NOV-2026 12:00\r")
i = p.expect([r"job terminated", pexpect.TIMEOUT], timeout=180)
print("finished" if i == 0 else "FROZE")
p.close(force=True)
