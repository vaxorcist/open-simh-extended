<!-- open-simh-extended/docs/SIMH_overview.md  v28, 2026-11-12 (v28: patch 5 v35, TU58; v27: patch 5 v34, VAX 8200 console floppy track 0 fix; v26: patch 5 v33, VAX 8600 console RL02; v25: patch 5 v32, VAX 8200 console RX50; v24: patch 5 v31, RX50, RX50 controller rules; v23: patch 5 v30, RX01 (RX, 11/780 console floppy); v22: patch 5 v29, RX02 density error on Read Status (VMS MOUNT); v21: patch 5 v28, RY debug flags; v20: patch 5 v27, RX02 floppy; v19: patch 5 v26, RA81 start 20 s, RA80 40 s; v18: patch 5 v25, SDI bus rule; v17: patch 5 v24, SDI rules, RA80 capacity; v16: patch 5 v23, the fixed RA drives, RA facts; v15: patch 5 v22; next the RA disks; v14: patch 5 v21; v13: patch 5 v20, smooth TK50 sounds; v12: patch 5 v19, TQ position fix; v11: patch 5 v18, TK50, MicroVAX II THROTTLE; v10: patch 5 v17; v9: patch 5 v16, tapes TS11/TSV05, TS facts; v8: patch 6 v2: one RB80, patch 5 v15, RB730 facts; v7: patch 1 v11: DMV11 on the MicroVAX I; v6: patch 5 v14, default sounds; v5: patch 5 v13; v4: 2026-11-06 RC25, patch 5 v12; v3: 2026-11-05 fork and branches; v2: 2026-11-01) -->
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
| 1 | `opensimh_1_dmv11_v11.patch` | DMV11 (Qbus, also the MicroVAX I) / DMP11 (Unibus) host interface, multipoint with listening control station, DMP device on all Unibus VAXen and the PDP-11, select flag discipline and polling of tributaries | 87eb7d5 |
| 2 | `opensimh_2_dmf32_v6.patch` | DMF32, all four functions (units DMF-ASYNC, DMF-PRINT, DMF-SYNC, DMF-PAR) | after 1 |
| 2s | `opensimh_2_dmf32_v6_standalone.patch` | the same DMF32, without patch 1 | 87eb7d5, instead of 1 + 2 |
| 3 | `opensimh_3_vax8200_uba_fix.patch` | VAX 8200 DWBUA interrupt delivery and Unibus init (2 commits) | 87eb7d5, independent |
| 4 | `opensimh_4_dmc_ultrix_v8.patch` | DMC11/DMR11 fixes for the ULTRIX-32 V1.1 `if_dmc` driver | after 1 |
| 6 | `opensimh_6_rb730_v2.patch` | RB730: DEC logical block order, SIMH disk library, status, seek and range fixes, at most one RB80 | 87eb7d5, independent |
| 5 | `opensimh_5_sound_v35.patch` | common SDL2 sound support and the realism mode (RK07, RL01/02, Massbus packs, RA60, RA70–RA92, RC25/RCF25, RB02/RB80; tapes: TS11/TSV05, TK50 cartridge streamer; floppies: RX02, RX01, VAX-11/780 console RX01, RX50, VAX 8200 console RX50; VAX 8600 console RL02; TU58 (PDP-11 TDC, VAX-11/730/750 console); MicroVAX II THROTTLE 450K); without the Visual Studio project files | after 6 |

Patch 6 comes before patch 5. The whole series:

```
git checkout -b <branch> 87eb7d5
git am --keep-cr opensimh_1_dmv11_v11.patch opensimh_2_dmf32_v6.patch opensimh_3_vax8200_uba_fix.patch \
                 opensimh_4_dmc_ultrix_v8.patch opensimh_6_rb730_v2.patch opensimh_5_sound_v35.patch
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
| `sound` | patch 5 (with the Visual Studio project changes): a root snapshot commit 2f3bbe8 (the `rb730` tree plus sound up to patch 5 v15), then one commit each for the TS tapes, the TS fixes, the TK50, the TQ position fix, the smooth TK50 sounds and the fixes of 2026-11-09 (sim_tape position, calibration marker, throttle), SHOW TRANSPORT during moves, the audible reposition and the STRSHIFT register, the fixed RA drives, the SDI rules, the SDI bus rule, the RA81/RA80 start times, the RX02 floppy; its tree is `rb730` + patch 5 | `rb730` (tree, not history) |
| `integration` | patches 1, 2, 3, 4, 6, 5 | `master` |
| `notes` | these docs, tools, tests and patch files (no SIMH code) | |

A failed `git am` leaves `.git/rebase-apply`; clear it with `git am --quit`
(not `--abort`, which moves the branch back) before trying again.

## Device facts settled so far

- **RB730 (VAX-11/730 IDC)**, from EK-RB730-TD-001 (Sep 1982): at most **one R80 and up to three RL02, or four RL02** (figure 3-2); SIMH refuses a second RB80 (patch 6 v2). Drive addresses: the 2-bit drive select (control word bits 8-9) gives addresses 0-3; each RL02 gets its address from the address plug in the drive, the R80 from the logic plug on its operator panel (reported over R80 SELECT ADDRESS 1/2, valid only with R80 PLUG VALID). No address is reserved for the R80: it may be any of 0-3. If the R80 and an RL02 have the same address the IDC selects the R80 (address compare); the RL02 status has a Drive Select Error bit for "multiple drives responding to one address". In SIMH the unit number is the address, so duplicates cannot occur.
- **TS device (sound realism mode):** on the Unibus a TS11 (TS04 transport, EK-OTS11-TM-003: 45 in/s, start/stop for every record, rewind 150 in/s), on the Qbus a TSV05 (TS05 transport, EK-TSV05-UG-003: 25 in/s streaming, rewind 130–180 in/s, load 30 s, unload 15 s). VMS V4.7 names it MSA0 on both. A subsystem initialize with the ON-LINE switch on rewinds the tape (TS11 manual). The TS11 manual's scan lacks pages 4-4/4-5 (loading); TS11 load/unload 8 s each are provisional.
- **TK50 (TQ device, sound realism mode):** TQK50 on the Qbus, TUK50 on the Unibus (EK-OTK50-TM-002, EK-TUK50-OM-001): 75 in/s, 45 KB/s, 22 serpentine tracks on 600 ft, rewind about 80 in/s (up to 90 s), load 10–15 s, calibration of a new cartridge about 2 min, unload 9 s; 32 KB buffer and 60 in back-up after Ulli's TZ30 recording (provisional). In SIMH the Unibus VAXen default to the TU81: `SET TQ TK50` selects the TUK50. VMS V4.7 starts the TMSCP port only at the end of the startup (MUA0 about 30 s after the login).
- **RA drives (RQ device, sound realism mode):** every RA drive has a RUN/STOP switch, the fixed ones too (RA80/RA81 User Guides EK-0RA80-UG-001, EK-0RA81-UG-001; RA90/RA92 User Guide EK-ORA90-UG-002; RA7x/SA7x Pocket Reference Guide EK-RSA7X-PG-002). Published: RA80 3600 rpm, seek 6/25/50 ms, start 40 s (UG max., 15 s typical; brochure 50 s), stop 20 s; RA81 3600 rpm, 7/28/50 ms (UG maxima; brochure 6/28/52), start 20 s, stop 20 s (UG maxima; brochure start 50 s); the drive manuals win over the brochure; RA82 (Computer History Wiki) 3600 rpm, 6/24/38 ms, 50 s / 20 s; RA90 3600 rpm, 5.5/18.5/31.5 ms, start 40 s; RA92 3405 rpm, 3/16/29 ms, start 40 s; RA70 4000 rpm, average 19.5 ms, start 59 s, stop 15 s (CHW); RA71/RA73 3600 rpm (RA72 not stated), spindle at speed within 15 s; no RA71/72/73 seek times found (provisional from the RF72: average 13.3 ms). SIMH's RA80 has 237,212 LBNs, 248 more than the User Guide, as a real RA80 reports (known issues 41, DONE). In the realism mode RA (SDI) drives only on a UDA50, UDA50A or KDA50, at most 4 drives each, the UDA50/UDA50A only on the Unibus and the KDA50 only on the Qbus (Ulli, 2026-11-10). VMS V4.7 cannot mount its system disk on an RA90 or RA92 (also unmodified SIMH, normal mode). In the realism mode RX50 drives only on an RQDX1/RQDX3/RQDX4 or RUX50, a RUX50 only with RX50 drives; RX50 units on other controllers are disabled with a message; two RX50 units with consecutive unit numbers are one drive (Ulli, 2026-11-11). A PDP-11 keeps the RQDX3 after SET CPU 11/70 (known issues 50), so there is no bus rule for the RX50 controllers.
- **VAX 8600 console RL02 (CS):** an RL02 on the RLV12 of the console processor (a T-11); the VAX moves one 512-byte block per transfer, a word at a time, through STXCS/STXDB; VMS calls it CSA1 (SYSGEN CONNECT CONSOLE). Standard RL02 pack format (4.3BSD crl(4)). In the realism mode it has the RL02's LOAD button (SET CS ONLINE/OFFLINE, 40 s / 30 s); with the drive stopped VMS MOUNT asks the operator and completes when it comes up.
- **TU58 (TDC, VAX-11/730/750 console TD):** 512 blocks on two tracks of 1024 records of 128 bytes, read/write 30 in/s, search 60 in/s, 38.4 kbaud (TU58 User's Guide EK-0TU58-UG-004); in the realism mode a random block takes about 9 s, an RT-11 boot from TU58 about 3 min (record order inferred: 1:2 interleave). VMS V4.7 reaches the console TU58 as CSA1 after SYSGEN CONNECT CONSOLE. RT-11 needs DD.SYS (in the V5.3 kit, not in Ulli's V5.7 RX02 kit); `tests/tu58/mkdd.ini` builds a bootable RT-11 TU58.
- **DMV11** only on Qbus (MicroVAX I, MicroVAX II, MicroVAX 3900, Qbus PDP-11): `SET DMV ENABLE`.
- **Test CPUs (Ulli, 2026-11-07):** Qbus changes are tested on the **microvax2** (the one Ulli uses most); a change for specific CPUs (e.g. the DMV11 on the MicroVAX I) is tested on the affected CPU(s).
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
| `docs/SIMH_Sound_devices.ods` | the devices prepared for sound, with rotation speed and access times; sheet Tapes (generated by `tools/make_sound_devices_ods.py`) |
| `tools/make_tape_sounds.py`, `tests/ts11/` | the tape sound generator; the TS11/TSV05 VMS tests (tstest.py) |
| `tools/make_spindle_sounds.py`, `tests/ra/` | the spindle sound generator; the RA console tests and VMS tests (ratest.py) |
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
