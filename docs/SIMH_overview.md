<!-- open-simh-extended/docs/SIMH_overview.md  v3, 2026-11-05 (fork and branches; patches 5 and 6; v2: 2026-11-01) -->
# SIMH project – current state

Read this first in every new session. It says what is current; the other docs
are background or history.

## Working rules (Ulli)

- The work lives in Ulli's fork **vaxorcist/open-simh-extended** on GitHub
  (forked from open-simh/simh on 2026-11-05). Claude pushes there; nothing is
  pushed to open-simh/simh or to any other repository. Work on Mark
  Pizzolato's simh fork stays out of it.
- **Patch files:** no path or version comment line at the top, and no
  path/version marker lines added to the source files.
- **Other files** Claude creates or changes: complete path and version number as
  a comment in the first line, and detailed comments.
- When naming files, always give the **complete path**.
- **Every file created or changed is delivered.** Before sending, compare the
  modification times in the output folder with what was already sent. The
  closing summary lists exactly the files sent in that turn.
- NCP commands are given **without the `NCP>` prompt**, ready to paste.

## Base and patch chain

Base: Open SIMH commit **87eb7d5** (= `master` of the fork). The sources use
CR LF, so apply with `git am --keep-cr`, in this order (the files are in
`patches/` of this branch):

| # | Patch file | Content | Applies to |
|---|---|---|---|
| 1 | `opensimh_1_dmv11_v10.patch` | DMV11 (Qbus) / DMP11 (Unibus) host interface, multipoint with listening control station, DMP device on all Unibus VAXen and the PDP-11, select flag discipline and polling of tributaries | 87eb7d5 |
| 2 | `opensimh_2_dmf32_v6.patch` | DMF32, all four functions (units DMF-ASYNC, DMF-PRINT, DMF-SYNC, DMF-PAR) | after 1 |
| 2s | `opensimh_2_dmf32_v6_standalone.patch` | the same DMF32, without patch 1 | 87eb7d5, instead of 1 + 2 |
| 3 | `opensimh_3_vax8200_uba_fix.patch` | VAX 8200 DWBUA interrupt delivery and Unibus init (2 commits) | 87eb7d5, independent |
| 4 | `opensimh_4_dmc_ultrix_v8.patch` | DMC11/DMR11 fixes for the ULTRIX-32 V1.1 `if_dmc` driver | after 1 |
| 6 | `opensimh_6_rb730_v1.patch` | RB730: DEC logical block order, SIMH disk library, status, seek and range fixes | 87eb7d5, independent |
| 5 | `opensimh_5_sound_v11.patch` | common SDL2 sound support and the realism mode (RK07, RL01/02, Massbus packs, RA60, RB02/RB80); without the Visual Studio project files | after 6 |

Patch 6 comes before patch 5. The whole series:

```
git checkout -b <branch> 87eb7d5
git am --keep-cr opensimh_1_dmv11_v10.patch opensimh_2_dmf32_v6.patch opensimh_3_vax8200_uba_fix.patch \
                 opensimh_4_dmc_ultrix_v8.patch opensimh_6_rb730_v1.patch opensimh_5_sound_v11.patch
```

The same work as git branches of the fork:

| Branch | Content | Built on |
|---|---|---|
| `master` | Open SIMH 87eb7d5, unchanged | |
| `dmv11-dmp11` | patch 1 | `master` |
| `dmf32` | patch 2 | `dmv11-dmp11` |
| `dmc-ultrix` | patch 4 | `dmv11-dmp11` |
| `vax8200-uba` | patch 3 | `master` |
| `rb730` | patch 6, as 5 separate commits | `master` |
| `sound` | patch 5, as 9 separate commits (with the Visual Studio project changes) | `rb730` |
| `integration` | patches 1, 2, 3, 4, 6, 5 | `master` |
| `notes` | these docs, tools, tests and patch files (no SIMH code) | |

A failed `git am` leaves `.git/rebase-apply`; clear it with `git am --quit`
(not `--abort`, which moves the branch back) before trying again.

## Device facts settled so far

- **DMV11** only on Qbus (MicroVAX II, MicroVAX 3900, Qbus PDP-11): `SET DMV ENABLE`.
  **DMP11** only on Unibus (VAX-11/730/750/780, 8200, 8600, Unibus PDP-11):
  `SET DMP ENABLE`. There is no DMP11 on Qbus VAXen. The DMC device has only
  `TYPE={DMC,DMR}`; `TYPE=DMP` was removed.
- **Tributary limits:** DMV11 12, DMP11 32 (VMS reports DEVICEFULL beyond).
  VMS NCP `DEFINE EXECUTOR MAXIMUM CIRCUITS` defaults to 16.
- **No SET DROPS.** A DMP/DMV line attached without a PEER listens for as many
  stations as the controller has tributaries.
- **Select flag:** a selected tributary owns the line until it sends a message
  with the select flag; meanwhile the control station sends to it without
  selecting it again. Selection timer 10 s (VMS V3.5 with a DMF32 holds the
  line about 3 s after a poll, so 3 s was too short). Without this, a DMF32
  tributary under VMS V3.5 restarted the line on every new logical link
  ("Line synchronization lost"). Patch 1 v9.
- **Polling:** the control station polls a tributary that has returned the
  line (ACK with select flag) after 0.5 s. A software DDCMP tributary may only
  transmit when selected; without polling its hellos could only follow the
  control station's, and every ~2 hours (beat of the two 15 s hello timers)
  the control station's 30 s listen timer expired ("adjacency down, listener
  receive timeout"). Patch 1 v10.
- VMS V3.5 does not adapt its listen timer (30 s) to the neighbour's hello
  timer: raising the control station's hello timer beyond 30 s makes the
  tributary drop the adjacency.
- **DMF32:** sync line and parallel interface poll once per clock tick (the
  former 1 ms poll prevented `SET CPU IDLE`). `SHOW DMF-SYNC` shows its own
  connection. VMS DMF32 sync driver: XGDRIVER, circuit DMF-0.
- **Idle:** VMS V3.5 needs `SET CPU IDLE=VMS`; `IDLE=OLDVMS` does not idle
  (unpatched Open SIMH too).
- `SET <dev> CORRUPTION=5` makes VMS DECnet copies fail on DMC, DMP and DMV
  alike (general behaviour, not investigated).
- SIMH prints `SHOW <unit>` without a space after a unit name
  (`DMF-SYNCattached`, `DZ-RCV-CONattached`): core behaviour, left alone.

## Tests that passed with the current chain

- dmctest kit (VMS V4.7 DMC/DMR/DMP/DMF/DMF+DMC, V3.7 DMC/DMR/DMP): PASS
  (DMP and DMF rerun on patch 1 v9).
- Multipoint mp3 (control station listening, tributaries 2 and 3, routing,
  copies, circuit off/on) on VAX-11/780 DMP11 and MicroVAX II DMV11: PASS.
- Two DMV11s in one MicroVAX II: PASS.
- VAX-11/730 DMF32 tributary under VMS V3.5 with a VAX-11/780 DMP11 control
  station under VMS V4.7: node hopping with SET HOST, 10 rounds, no restart;
  remote copies with 0 differences; idle line with the tributary's hello
  timer at 25 s: no adjacency loss in 10 min with polling (a loss without).
  Test scripts in Claude's workspace: hop.sh, hello25.sh, restart1.sh,
  restart4.sh (delivered to Ulli).
- VMS V3.5 on the VAX-11/730 with DMF-SYNC attached: 1.2 % host CPU.

## Other docs on this branch

| File | What it is |
|---|---|
| `docs/SIMH_known_issues.md` | the numbered list of known bugs and suspicious code (DONE cases kept) |
| `docs/SIMH_Sound_design.md`, `docs/SIMH_Sound_handover.md` | the sound system: design and current state |
| `docs/SIMH_Sound_devices.ods` | the devices prepared for sound, with rotation speed and access times (generated by `tools/make_sound_devices_ods.py`) |
| `docs/VMS35_4MB_pool_stall.md` | finding: the VMS V3.5 4 MB stall (a hand-tuned WSMAX, not a SIMH bug) |
| `tests/dmctest_kit.txt` | the two-node DECnet test kit (run_pair.sh, vcon.py, dmctest.conf, README) as text |
| `docs/DMV11_OpenSIMH_handover.md` | background: how the DMV11 was built (older patch names) |
| `docs/DMF32_OpenSIMH_handover.md` | background: how the DMF32 was built (older unit names DMF0–DMF4) |
| `docs/DMV11_similarity_report.md` | comparison with Mark Pizzolato's DMV11-Test branch |
| `docs/DMP11_VMS37_tributary_finding.md` | finding: DMP11 tributary with VMS V3.7 |
| `docs/DMC_v7_VMS_regression.md` | DMC v7 regression with VMS V4.7 / V3.7 |
| `docs/ULTRIX11_VAX780_install_log.md` | ULTRIX-32 V1.1 install on the VAX-11/780 |
| `tests/vms_pair_test_harness.txt`, `tests/dmv11_test_harness.txt` | earlier test harnesses, replaced by the dmctest kit |

The manual `simh_new_comm_devices.odt` (revision 1.6, 01-Nov-2026) is not
here; Ulli has the delivered copy.
