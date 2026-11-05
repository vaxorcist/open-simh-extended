<!-- open-simh-extended/docs/ULTRIX11_VAX780_install_log.md  v3 (30 Oct 2026): VMS regression of the v7 DMC fixes done (11.5); v2: DMF32 and DMC11 added (section 11), DMC fixes (v7 patch), corrections -->
# ULTRIX-32 V1.1 on a SIMH VAX-11/780: installation log

Date: 30 Oct 2026. Installed for Ulli (vaxorcist) by Claude. All steps were typed at the simulated console; the complete console transcript is in `ULT011-RM05_install_console.LOG`.

**Result:** ULTRIX-32 V1.1 is installed on the RM05 image (`ULT010.RM5`). A kernel configured for this machine (`/vmunix`, with both DZ11s) boots unattended to multi-user mode. fsck is clean on all filesystems, and logins work on the console and over Telnet through the DZ11.

| Item | Value |
|---|---|
| Hostname | `ultrix` (config name `ULTRIX`) |
| root password | `ultrix` (change it with `passwd`) |
| Root | `hp0a`, 7.4 MB |
| Swap / dumps | `hp0b` |
| `/usr` | `hp0g`, 74 MB |
| `/usr/users` | `hp0h`, 137 MB |
| Time zone | MET/MEST (`timezone -1 dst 4` in `/sys/conf/ULTRIX`) |
| Terminals | console, tty00–tty07 on dz0 (Telnet port 6666) |

## 1. Simulator

- Open SIMH 87eb7d5 with the project patches:
  - `opensimh_dmv11_v6.patch`;
  - `opensimh_dmf32_v3.patch`;
  - `opensimh_vax8200_uba_fix.patch` (both commits).
- The Visual Studio `.vcproj` binary hunks were left out. They don't affect the Linux build.
- Built with `make vax780`. The build uses `-Werror` and finished with no warnings.
- Checked after the build:
  - `SET DMC0 TYPE=DMP` places a DMP11 at its floating address (`2013E0D0`, vector E0);
  - `SET DMF ENABLE` shows the DMF32 (async+sync+LP).
- Correction (v2): ULTRIX V1.1 does have drivers for two of these devices, `dmf` (DMF32) and `if_dmc` (DMC11). Both are now in the kernel; see section 11. There is no DMP11 driver.

## 2. Files used

| File | Use |
|---|---|
| `ultrix-32_1.1_sys_sw_1of2.tap` | Distribution tape 1 on TS11 (attached read-only) |
| `ult011_console.rx1` | Console floppy (COPY and BOOT programs) |
| `ULT010.RM5` | System disk (RM05). It already held a V1.0 miniroot in `hp0b`; that was replaced. |
| `vax780_install.ini` | Your ini without the boot lines, tape attached |

Tape 1 layout (from the `.tap` file):

| File | Contents |
|---|---|
| 0 | Boot/tp, 512-byte records |
| 1 | Miniroot, 300 × 10240 bytes |
| 2 | Root dump |
| 3 | `/usr` tar |
| 4 | `/usr/sys` tar |

Tape 2 was not available and was not needed for the base system (see section 9).

**V1.1 differs from the May 1984 (V1.0) manual.** The V1.1 miniroot's `/.profile` is an installation script (`__gettape`, `__getdisk`, `step1`, `__makeusrfs`, `__mkconfig`, `doconfig`). It replaces the manual's `xtr`, the hand-typed `newfs`/`tar` steps and the kernel configuration chapter. The manual's tables don't list RM05/TS11, but the V1.1 scripts accept both.

## 3. Copy the miniroot (manual 2.1.1)

```
sim> FLOAD COPY 0
sim> RUN 2
From: ts(0,1)
To: hp(0,1)
Copy completed: 300 records copied
^E
```

## 4. Boot the miniroot (manual 2.1.2)

```
sim> DEP R10 2
sim> DEP R11 3
sim> FLOAD BOOT 0
sim> RUN 2
Boot
: hp(0,1)vmunix
Ultrix V1.1 System #1: Thu May 9 08:45:41 EDT 1985
...
root device? hp0*
```

The first `Boot:` attempt in the log shows `1)vmunix  Bad device`. That was typed before the boot program was ready, and the retype worked. The generic V1.1 kernel did not report the DZ11s, which the V1.0 kernel had found.

## 5. Restore the root (miniroot `.profile`)

| Prompt | Answer |
|---|---|
| Distribution media drive | `ts11` |
| Root disk drive | `rm05` |
| Alternate unit number | `n` (unit 0) |

The script then runs:

1. `newfs rhp0a rm05`;
2. `fsck`;
3. `restore rsf 3 /dev/ts0`;
4. `fsck` again, with the result: 704 files, 3872 used, 3557 free.

## 6. Update the console floppy

- At the prompt, press RETURN.
- The script runs `make update` in `/sys/floppy`: it extracts the floppy with `arff xf /dev/floppy`, rewrites it with `flcopy -t3`, then adds the ULTRIX boot files (`arff rf`).
- `flcopy` asks two questions:

| Question | Answer |
|---|---|
| `Change Floppy, Hit return when done.` | RETURN |
| `Are you sure you want to clobber the floppy?` | `y` |

- Reading and writing the console RX01 from ULTRIX works with the SIMH `CS` device.
- At "another console update", answer `n`.
- The script prints the reboot steps and halts.

## 7. First boot of the new root; `step1` and `__makeusrfs`

```
sim> DEP R10 2 / DEP R11 3 / FLOAD BOOT 0 / RUN 2
: hp(0,0)vmunix
root device? hp0
```

| Prompt | Answer |
|---|---|
| Date (yymmddhhmm) | `9810301215`. ULTRIX V1.1 has a two-digit year. 1998 has the same weekdays as 2026. |
| root password (twice) | `ultrix` |

The script then:

1. makes the tape device files (`MAKEDEV ts0`);
2. runs `newfs rhp0g rm05`;
3. extracts tape files 3 (`/usr`) and 4 (`/usr/sys`) and runs `fsck` (789 files);
4. runs `newfs hp0h` for `/usr/users`;
5. writes `/etc/fstab` and checks all filesystems.

Superblock backups:
- `hp0g`: 32, 9792, 19552, … (17 groups);
- `hp0h`: 32, 9792, … (30 groups).

Both are in the console log.

## 8. Kernel configuration (`__mkconfig` / `doconfig`)

| Prompt | Answer |
|---|---|
| Automatic crash dumps | `y` |
| System name | `ULTRIX`, confirm `y`, make it the hostname `y` |
| Time zone | `g`, DST `yes`, area `c` (Central Europe) |

`sizer` found uba0, ts0, lp0, mba0 and hp0, but no terminal multiplexer. Its add-device dialogue was answered twice:

| Question | dz0 | dz1 |
|---|---|---|
| Device name | `dz0` | `dz1` |
| Connected to | `uba0` | `uba0` |
| CSR | `0160100` | `0160110` |
| Flags | `0xff` (soft carrier, all 8 lines) | `0xff` |

Finish with `exit`.

To edit the configuration file, answer `y` and pick the editor `ed`:

```
/^timezone/s/0 dst 4/-1 dst 4/p
w
q
```

The prompt only takes hours *west* of Greenwich; `config` accepts `-1` for one hour east.

- The build finished without errors, giving `/newvmunix`:
  - text 173744, data 71940, bss 35696.
- `MAKEDEV lp0 hp0 dz0 dz1 ts0 pty0 pty1 std` ran.
- Boot the new kernel with `: hp(0,0)newvmunix`. It reports `dz0 at uba0 csr 160100 vec 300` and `dz1 ... csr 160110 vec 310`.
- Then install it:

```
# mv /vmunix /oldvmunix
# mv /newvmunix /vmunix
# date                 -> Fri Oct 30 13:24:26 MET 1998
^D                     (multi-user)
```

Verified:
- `df` shows `/`, `/usr` and `/usr/users` mounted.
- `login: root` works on the console.
- A Telnet login on port 6666 lands on `/dev/tty00`.
- `/etc/halt` stops the system cleanly.

## 9. Boot configuration (`vax780.ini` v3)

- **R10 must be 0 for the RM05.** In this boot program, R10 = 2 is `up` (UNIBUS). With R10 = 2 an automatic boot (R11 = 0 or 2) tries `up(0,0)vmunix` and hangs. The interactive boots above worked only because `hp(...)` was typed.
- With R10 = 0:
  - R11 = 0 boots unattended: autoreboot fsck passes, then multi-user and `login:`;
  - R11 = 3 gives the `Boot:` prompt. Answer `hp(0,0)vmunix`.

## 10. Not done / open points

- **Tape 2 of 2** (optional software, manual section 4.5) was not installed. The base system from tape 1 has:
  - 63 commands in `/bin`, 29 in `/usr/bin`, 52 in `/usr/ucb`;
  - the C compiler (`/bin/cc`, `/lib/ccom`) and `csh`;
  - the network daemons (`telnetd`, `ftpd`, `rlogind`, `rshd`) and `/etc/ifconfig`, `/etc/route`, but no network clients (`telnet`, `ftp`, `rlogin`, `rsh`).
  - `uname`, `lptest` and the network clients are missing, presumably on tape 2. (v1 also listed `wc`; it was only missing because `/usr` was not mounted in single-user mode.)
- **dz1 lines** (tty10–tty17) have device files and a kernel driver, but they are not in `/etc/ttys`. Add `18tty10` … `18tty17` to enable logins on them.
- `/oldvmunix` (generic kernel) is kept as a fallback: boot it with `hp(0,0)oldvmunix`.
- The original console floppy is kept as `ult011_console.rx1.before_update` in the working directory. The delivered `ult011_console.rx1` is the updated one.

## 11. DMF32 and DMC11 in the kernel (added 30 Oct 2026, afternoon)

The ULTRIX V1.1 binary kit in `/sys/BINARY.vax` contains two of these drivers:

| Device | Driver | What it supports |
|---|---|---|
| DMF32 | `dmf.o` (`dmf.c` 1.7, 4/24/85) | 8 asynchronous lines and the line printer port |
| DMC11 | `if_dmc.o` | a network interface for IP over DDCMP |
| DMP11 | none | n/a |

### 11.1 Simulator configuration (`vax780.ini` v4)

```
SET DMF ENA
SET DMF CONFIG=PRINTER
ATT DMF 6667                ; 8 lines, Telnet port 6667
ATT DMF1 dmf_printer.txt
SET DMC ENA
SET DMC0 TYPE=DMC           ; ULTRIX speaks only the DMC11 interface
SET DMC0 PEER=localhost:5002
ATT DMC0 5001
```

With the DMC11 present, the floating addresses move. This matches the real Unibus rank order:

| Device | CSR | Vector |
|---|---|---|
| dmc0 | 160070 | 300 |
| dz0 | 160110 (was 160100) | 310 |
| dz1 | 160120 (was 160110) | 320 |
| dmf0 | 160400 | 330 (the driver moves it, see below) |

### 11.2 Kernel (`/sys/conf/ULTRIX`, old version saved as `ULTRIX.v1`)

```
device  dz0   at uba0 csr 0160110 flags 0xff vector dzrint dzxint
device  dz1   at uba0 csr 0160120 flags 0xff vector dzrint dzxint
device  dmf0  at uba0 csr 0160400 flags 0xff vector dmfsrint dmfsxint dmfdaint dmfdbint dmfrint dmfxint dmflint
device  dmc0  at uba0 csr 0160070 vector dmcrint dmcxint
```

The syntax comes from `/sys/conf/LINT`.

`doconfig` (keeping the existing config file) does not rebuild everything. Its makefile has no dependencies on the new `dmf.h`/`dmc.h`, so:

1. The first `make` reported "`vmunix' is up to date".
2. After `rm vmunix`, the link failed with undefined `_Xdmf*`/`_Xdmc*`, because `ubglue.o` was stale.
3. After that, `init` reported "No such device" for every `/dev/ttyh*`, because `conf.o` was stale.

What works:

```
# cd /sys/ULTRIX
# rm -f vmunix ubglue.o ubglue.c ubglue.i conf.o conf_net.o in_proto.o *_data.o ioconf.o param.o
# make vmunix
# cp vmunix /vmunix                  (old one kept as /vmunix.dz)
# cd /dev; ./MAKEDEV dmf0            (/dev/ttyh0-ttyh7, /dev/lph)
```

Then add `18ttyh0` to `18ttyh7` to `/etc/ttys` (the old file is kept as `/etc/ttys.orig`).

The new kernel (text 183512) reports at boot:

```
dz0 at uba0 csr 160110 vec 310, ipl 15
dz1 at uba0 csr 160120 vec 320, ipl 15
dmf0 at uba0 csr 160400 vec 740, ipl 15
dmc0 at uba0 csr 160070 vec 300, ipl 15
```

The `dmf` driver assigns its own vector block (740), the 4.2BSD way, and loads it into CSR0. So this also tests the DMF32 emulation's CSR0 vector base.

### 11.3 Tests

| Test | Result |
|---|---|
| Telnet login on DMF32 line 0 (port 6667): getty, password, `tty` = `/dev/ttyh0`, `stty everything`, 30-line `ls -l`, `who` | ok |
| Printing to `/dev/lph` (text, date, tab, form feed) | ok, in `dmf_printer.txt` |
| Two ULTRIX nodes over the DMC11 (see 11.4) | ok after the simulator fixes in 11.5 |
| TCP connect from node A to node B's `telnetd` | ok: B's login prompt arrives |
| `rshd` on node B runs `cat /vmunix` for node A | 288768 bytes in about 3 s; `sum` 06695 282 on both sides |

Notes on the tests:
- **Telnet banner:** getty sends with even parity. A client that drops 8-bit characters shows a garbled banner; that is not an emulation fault.
- **Printer output:** lines end in a bare LF, because the ULTRIX driver does not use the DMF32 formatting options.

### 11.4 Two-node DMC11 test set-up

- Node B is a copy of the disk in its own directory, with `vax780_nodeb.ini`:
  - DMC ports swapped (listens on 5002, peer 5001);
  - Telnet ports 6676 and 6677;
  - no tape;
  - R11 0.
- On both nodes, with V1.1 `ifconfig` syntax `ifconfig if -dest peer address up`:

```
A# /etc/ifconfig dmc0 -dest 10.1.0.2 10.1.0.1 up ; /etc/route add 10.1.0.2 10.1.0.1 0
B# /etc/ifconfig dmc0 -dest 10.1.0.1 10.1.0.2 up ; /etc/route add 10.1.0.1 10.1.0.2 0
```

- Tape 1 has no network clients, so two small test clients were written and compiled on ULTRIX. Both are in `/usr/users` on node A:
  - `tcpc.c`: TCP connect, prints what the server sends;
  - `tcprsh.c`: the rsh protocol, from a reserved port.
- On B, `/etc/hosts` gets `10.1.0.1 nodea` and `/.rhosts` gets `nodea root`, so that `rshd` accepts A.

### 11.5 Simulator fixes found with ULTRIX (DMV11 work v7, `opensimh_dmc_ultrix_v7.patch`)

These are faults in the original Open SIMH DMC11 code; VMS does not trigger them. Only DMC/DMR mode is changed, and the DMP11/DMV11 paths are untouched.

1. **Master clear left BSEL0/BSEL2 bits set.** ULTRIX sets MCLR with a byte write to BSEL1, so RQI|IEI from the probe survived. The emulation then started an unrequested input transfer and never answered the later BASE IN. Now master clear clears the low bytes of SEL0 and SEL2 and withdraws pending interrupt requests.
2. **No interrupt when IEI was set while RDI was already on** (same for IEO/RDO). `dmcload()` sets RQI first and IEI second.
3. **The DMC became Running only with the first transmit buffer, and reception was enabled only on a DTR change.** Node B, waiting as the TCP server, never read its line. Now CONTROL IN starts the line, and raising DTR always enables reception.

VMS regression (same evening): VMS V4.7 and V3.7, two VAX-11/780 nodes, DMC11, DMR11, DMP11, DMF32 and DMF32–DMC11. All pass with the v7 build and behave like v6. Details are in `DMC_v7_VMS_regression.md`.
