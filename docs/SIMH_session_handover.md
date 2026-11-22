<!-- open-simh-extended/docs/SIMH_session_handover.md  v16, 2026-11-22 (v16: patch 21 DMP11/DMR11 switches and boot, known issue 90 fixed, 99, 100, DMC11 waits for a manual; v15: patch 20 HELP BOOT and HELP DMV modes; v14: patch 19 (RQ XTIME 1000), known issues 97 fixed, 98; v13: cause of known issue 97; v12: DMV/DMP regressions PASS with SET CPU IDLE=VMS, tests/dmv_regress; v11: patch 18 v3 (SET DMVn PROGRAM, SP 1000), remwrt runs, known issue 97, regressions not passed yet; v10: patch 18 v2, VMS V4.7 loads a PDP-11 over the DMV11, DMV11 technical manual, remwrt, macro11; v9: patch 18, DMV11 switch packs and primary MOP boot (PDP-11), branch dmv-boot; push access via the session's add_repo; v8: patch 17, TS ON-LINE switch visible, branch ts-online; v7: SIDRIVER backported to VMS V4.7; v6: patch 16, DMB32 sync line as DDCMP tributary (CTS follows RTS), branch dmb32-trib; v5: patch 15, KA820 clock IPL 16 and IPIR; VMS055_1.R82; v4: patch 14, VAXBI configuration and memory boards; v3: patches 11 v3 and 13, SET REALISM, topic branches restacked; v2: state after the VMS V5.5 session: patches 9 v4, 10 v2, 12; open points; v1: handover after the KLESI-B session: workspace setup, rules, state, test images, test methods, open points) -->
# Session handover (2026-11-22, after patch 21)

For the next Claude session working with Ulli on the SIMH project. Read this first, then `docs/SIMH_overview.md` (current state, patch chain, device facts) and `docs/SIMH_known_issues.md` (cases 1–100).

**A new session starts with an empty workspace.** Everything Claude had locally (worktrees, built simulators, test directories, Ulli's uploads) is gone. The code, docs, tests and tools are in the fork on GitHub. Ulli's images must be uploaded again (section 4).

## 1. Setting up the workspace

The fork is `vaxorcist/open-simh-extended` (GitHub). The project's `claude/*` docs are copies of the docs in the `notes` branch; the `notes` branch is the reference.

```
cd /home/claude
git clone https://github.com/vaxorcist/open-simh-extended oss
cd oss
git worktree add -b integration-wip ../integ origin/integration
git worktree add -b notes-tmp ../notes origin/notes
git worktree add ../dmvb origin/dmp-dmr-boot  # topic branch of the last work (detached; branch with -b if needed)
cd ../integ
make vax8200 microvax2 vax780 vax750 pdp11  # build only what the change affects, plus representatives
```

- **Edits of the CR LF sources:** use small Python scripts with `notes/tools/crlfedit.py` (`F(path)`, `.rep(old, new)` stops unless the old text occurs exactly once, `.save()` keeps CR LF). Strings in the scripts use plain `\n`.
- **Push targets:** `integration-wip:integration`, `notes-tmp:notes`, the topic branch by its name. Push only to `vaxorcist/open-simh-extended`, never to open-simh/simh or Mark Pizzolato's repository.
- **Push access:** if a push is refused by the git proxy ("not in this session's authorized repository set"), add the repository to the session (add_repo `vaxorcist/open-simh-extended`, access push); the existing clone can then push.
- **Commit messages** end with:
  ```
  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  Claude-Session: <the session URL of the new session>
  ```
  Commits of patch content use the author "Claude" (`git -c user.name=Claude -c user.email=noreply@anthropic.com commit ...`).

## 2. Ulli's working rules (all still valid)

- **Delivery:** every file created or changed is delivered with SendUserFile, including working scripts. Compare the files' modification times with what was already sent. The closing summary lists exactly the files sent in that turn, with complete paths.
- **Patch files:** no path or version comment line at the top, and no path/version marker lines in the sources (SIMH-style history lines in the file header are fine).
- **Other files:** complete path and version number as a comment in the first line, plus detailed comments. Always name files with their complete path.
- **Commands:** complete, ready-to-use commands instead of descriptions. NCP commands without the `NCP>` prompt.
- **Building:** only the affected simulators plus a few representatives. A full build only when build files or every simulator's link change.
- **Bugs:** report every bug and any suspicious code found, in any file, in `docs/SIMH_known_issues.md` (numbered; DONE cases are kept; new cases at the end). Fixing right away is fine, especially in code being worked on.
- **Testing:**
  - test systematically, and run at least one boot test for every bootable device under test;
  - Qbus changes are tested on the **microvax2**; changes for specific CPUs on those CPUs;
  - avoid long trial and error: ask Ulli for the expected guest prompts and behaviour.
- **Decisions:** ask Ulli when a decision is his (AskUserQuestion), and **ask all open questions, not only some**. Say clearly when waiting for a go-ahead and when not. Work does not continue between turns.
- **Saving:** save intermediate results (commit and push as work progresses); keep the project docs (`claude/*.md`) in step with the notes branch (`project_read` first, then `project_write` with `local_path`).
- **ODS table:** `docs/SIMH_Sound_devices.ods` lists only the sound devices; keep it up to date when sound devices change (generated by `tools/make_sound_devices_ods.py`).

## 3. State of the work

- **Patch chain:** patches 1–21 on Open SIMH 87eb7d5, applied with `git am --keep-cr` in the order 1, 2, 3, 4, 6, 5, 7, 8, 9 (v4), 10 (v2), 11 (v3), 12, 13, 14, 15, 16, 17, 18, 19, 20, 21. The result equals the `integration` branch except `README.md` and `.github/workflows/cmake-builds.yml`.
- **How a patch is made** (patch 11 as the example):
  - apply the series in a scratch worktree on 87eb7d5;
  - cherry-pick the topic commits (`git cherry-pick <integration commits>`; plain `git am` without `--keep-cr` breaks the CR LF files), squash them with `git reset --soft`, and commit as Claude; leave README.md out (fork only);
  - `git format-patch -1 --stdout` into `notes/patches/opensimh_<n>_<topic>_v<k>.patch`;
  - `git diff --stat integration <scratch>` must show only README.md and the workflow file.
- **Last commits (2026-11-22):** `integration`: see `git log origin/integration` (patch 21 merged as a work commit), `dmp-dmr-boot` 05520299 (on `help-boot` f261469a, on `rq-xtime` 1a9dd525, on `dmv-boot` 328b9fca) (on `ts-online` 318276b3, on `dmb32-trib` e7b98c1b, on `ka820-clock` 6f6cdd91, on `vaxbi-config` ab555b9e, on `realism` 3174cd71), `kdb50` 86631fc2, `debna` f7cacad4, `klesib` 9cd59d1c, `dmb32-sync` e901fc9c, `notes` with this handover. The topic chain is kdb50 → debna → klesib → dmb32-sync → realism → vaxbi-config → ka820-clock → dmb32-trib → ts-online → dmv-boot; a fix to a lower topic branch needs the branches above it rebased (`git rebase --onto <new base> <old base> <branch>`), and the top must equal the patch series.
- **Work of 2026-11-17 (VMS V5.5):** KDB50 boot (known issues 71-73, patch 9 v3/v4, also from the KDB50 microcode listing), DEBNA with V5.5 (74, 75, patch 10 v2), DMB32 sync line with V5.5's SIDRIVER and the DMF32 sync line on the shared code (patch 12).
- **Latest (2026-11-22, patch 21, `dmp-dmr-boot`):** Ulli asked to fix known issue 90 and to add switches and booting for the DMP11, DMR11 and DMC11 like the DMV11, following DRY, with five new manuals (DMP11 UG, DMR11 UG and TM, M8207 TM, DMC11 IPL maintenance manual; DEC copyright: facts only). One board model (`DMX_BOARD`) now drives the switches and boot of the DMV11, DMP11 and DMR11 (`docs/DMV11_switches_boot_design.md` section 7). Ulli's decisions: DMC11 waits for a line unit manual (known issue 99); DMR11 faithful plus built-in (restart into a boot ROM at 173000+offset, else its own MOP load); DMP11 defaults boot features off. Tests `tests/dmv11_boot/` T1-T6, P1-P4, R1-R3 PASS; VMS pair regressions see section 6.
- **Earlier on 2026-11-22:** Ulli asked which devices boot on Unibus and Qbus PDP-11s and for two help texts: patch 20 (`help-boot`) adds `HELP BOOT` to the PDP-11 simulator (table generated from the devices) and a full table and explanation of `SET DMVn MODE=` in `HELP DMV`.
- **2026-11-21, afternoon:** `SET DMVn PROGRAM={SECONDARY|TERTIARY|SYSTEM}` (Ulli: configurable, default secondary loader); with SYSTEM, VMS sends remwrt from `LOAD FILE` and it runs (banner, prompt for the disk unit). The started program now gets SP 774 (routine SP 1000; with 404 remwrt had no stack). Patch 18 v3, tests T1-T6 and `tests/dmv11_vmsload` (two cases). The DMV/DMP DECnet regressions (`tests/dmv_regress/pair.sh`) PASS: DMV11 and DMP11, point-to-point and multipoint (control station + tributary 3). Known issues 97 (evening, Ulli: "find the real cause"): VMS V4.7 freezes after the date (SYSINIT in RWAST) when its 60 KB read of F11BXQP.EXE completes within a few hundred instructions; always with SET NOASYNCH (also plain Open SIMH), sometimes when the host is fully loaded; `SET CPU IDLE=VMS` or `DEP RQ XTIME 1000` avoids it; Ulli decided to raise the RQ default: patch 19 (`rq-xtime`, `XTIME` 1000 on the VAX) fixes it (`tests/vms_boot_freeze/`). The realism tests R1, U1, T1 of `tests/ra/ratest.py` fail before and after patch 19: the script predates the SDI rule (known issues 98). The test disks need SYSTEM password MANAGER (`setpw.cmd`). The DMV11 secondary loader was not written (not needed with PROGRAM=SYSTEM).
- **Earlier on 2026-11-21:** VMS V4.7 on a MicroVAX II as the MOP load host of a PDP-11/73 over a DMV11 (`tests/dmv11_vmsload/`, PASS with the 72-byte PIC test program HELLO; `docs/DMV11_switches_boot_design.md` section 6). Found and fixed: a sent DDCMP maintenance message was never returned to the host, for all DMC/DMR/DMP/DMV types, also upstream (known issues 92); DTR rule of the DMV11 technical manual EK-DMV11-TM-001 Appendix D (patch 18 v2). VMS loads a secondary loader at address 6. Ulli's remwrt.mac (LOCALWRITE client, DEQNA + RQDX3) assembled with macro11 v0.3 (Ulli allowed github.com/shattered/macro11; local parse.c fix) into remwrt.tsk (66 blocks), but it cannot be a secondary loader: it needs a DMV11 secondary loader (not written). Findings in remwrt.mac and macro11: known issues 95, 96. Line flapping with PEER on both DMV11s: known issues 93 (use PEER on one side). One "fatal hardware error" at STARTNET: 94. A stray remote branch `integration-wip` (same commit as `integration`) was pushed by mistake; deleting it was not allowed in the session: Ulli may delete it.
- **Work before (2026-11-20):** DMV11 switch packs E101/E113 and the primary MOP boot in the PDP-11 simulator, from Ulli's real DMV11 and its User's Guide EK-DMVQM-UG-003 (patch 18, `docs/DMV11_switches_boot_design.md`, tests `tests/dmv11_boot/` T1-T6 PASS; known issues 88-91). Ulli's decisions: the address switch is `TRIBUTARY=n` (ADDRESS is the CSR); boot only on the PDP-11 (no DMV11 boot on a MicroVAX, Ulli checked); BOOT refuses with the power-on boot switch off; manual defaults. Before that: TS realism mode: the ON-LINE switch made visible (patch 17, known issues 87). Before that: VMS V5.5's SIDRIVER backported to VMS V4.7 (`docs/SIDRIVER_V47_backport.md`). Before that: the DMB32 sync line as a DDCMP multipoint tributary under VMS V5.5 (`tests/dmb32/dmb_trib.com`, against a VAX-11/780 DMP11 control station) stayed in -starting: SIDRIVER does multipoint in software with an RTS/CTS handshake and waited for CTS to drop after its first transmission; SIMH kept CTS on. Patch 16: CTS follows RTS (known issues 84). Before that: Ulli's VAX 8300 with VMS V5.5 stopped with "Exception in interrupt or exception" in the realism mode; reproduced as VMS bugcheck SPLRELERR at THROTTLE 400K: the KA820 interval timer was at IPL 18 instead of 16 (patch 15, known issues 81; IPIR fix 82; EVKAA skipped in the 8200 diagnostic test). Before that: the VAXBI configuration (patch 14, `docs/VAXBI_config_VAX8200_design.md`): configurable BI nodes, twelve MS820 boards, `SHOW BI`, rules.
- **VAX 8200/8300 devices done:** second KA820 (patch 7), DMB32 (8), KDB50 (9), DEBNA (10), KLESI-B and VAXBI-only mode (11).

## 4. Test images and other material from Ulli

None of these is in the repository; Ulli uploads them again when needed.

| File | What | Rules |
|---|---|---|
| `VMS047_1.R81` | VMS V4.7 on an RA81, for the VAX 8200/8300 tests (KDB50 unit 0: `set kdb0 ra81`, `attach kdb0 VMS047_1.R81`, `boot kdb0`); login SYSTEM / MANAGER | Tests always work on a copy |
| `VMS047.R81` | VMS V4.7 for the MicroVAX II and other VAXen (tktest.py, `set rq0 ra81`) | The test copy had the SYSTEM password set to MANAGER with `AUTHORIZE MODIFY SYSTEM/NETWORK/PASSWORD=MANAGER/NOPWDEXPIRED`; ask Ulli if a fresh upload does not accept SYSTEM/MANAGER |
| `VMS055_1.R82` (2026-11-18) | Ulli's current VMS V5.5 RA82 image (his 8300 configuration; the reproducer of known issues 81) | Same rules as VMS055.R82: confidential, copies in the workspace only |
| **`VMS055.R82`** (new, 2026-11-16) | VMS V5.5 on an RA82, for testing | **Confidential: it contains license data. It must not go anywhere else**: not into the fork, not into the project, not into any delivered file or doc, and no license data may be quoted. Work on a copy inside the workspace only. Its purpose has not been discussed yet: ask Ulli. |
| `ult020src.tap` | ULTRIX V2.0 sources (VAXBI drivers: bla.c, bua.c, buareg.h, ...) | DEC copyright: stays out of the fork and the project; extracts go to Ulli only |
| `KDB50_Microcode_Listing_Sep88_O.pdf` (new, 2026-11-17) | KDB50 microcode listing (version 19), OCR text | DEC copyright: facts only, in our own words; stays out of the fork and the project |
| `EK-DMVQM-UG-003_QMA_DMV11_Synchronous_Controller_Users_Guide_1988_Ot.pdf` (2026-11-20) | DMV11 User's Guide (switch packs: Figure 2-5, 2-6, Table 2-6, 4-5; boot: section 4.7; registers chapter 3) | DEC copyright: facts only, in our own words; stays out of the fork and the project |
| `EK-DMV11-TM-001` (PDF, 2026-11-21) | DMV11 Technical Manual (boot: 4.7; modem control: Appendix D) | DEC copyright: facts only, in our own words; stays out of the fork and the project |
| `remwrt.mac` (2026-11-21) | Lee Gleason's LOCALWRITE client for a PDP-11 with DEQNA and RQDX3 (rsx11.blogspot.com, January 2024) | Not in the fork; the built remwrt.tsk went to Ulli only |
| `EK-DMB32-UG-001`, `EK-DMB32-TD-001` (PDF) | DMB32 User Guide and Technical Description (sync line: UG 3.3.28-3.3.43, 3.3.52, 3.4.8, 3.4.11; TD 2.7) | Upload again when needed |
| Standalone BACKUP kits (`STABA047-*.RX50`, `.RX1`, `.T58`), RT-11 images, DEC manuals (KA820, DMB32, KDB50, DEBNX, RX02) | earlier tests and sources | Upload again when needed |

## 5. Test methods that work

- **VMS scripts:**
  - SIMH `expect` rules, each rule fires once;
  - **one DCL command per step**, each followed by a marker:
    ```
    WRITE SYS$OUTPUT "=S1","END="
    ```
    and the next rule waits for `=S1END=` (it cannot match the echoed command line);
  - several commands in one SEND overrun the VMS type-ahead buffer (DATAOVERUN);
  - generator example: `tests/klesib/mkini.py`.
- **VMS V4.7 TMSCP ports** (TQ, TB) start at the end of the startup: wait about 90 s (`WAIT 00:01:30`) after the login before using MUA0/MUB0.
- **SYSGEN** in V4.7 does not take a command on the MC line: send `RUN SYS$SYSTEM:SYSGEN`, `SHOW/ADAPTER`, `SHOW/CONFIGURATION`, `EXIT` in one step.
- **Realism mode:** the SDI rule of patch 9 v2 disables non-SDI units on SDI controllers at SET SOUND ENABLED. Set the controller and drive types (`set rq kda50`, `set rq0 ra81`) before `set sound enabled`, and before any ATTACH (tktest.py v4).
- **Before runs:** `pkill -f "BIN/<simulator>"` (stale processes hold UDP ports and images). Run with `SDL_AUDIODRIVER=dummy` and `< /dev/null`.
- **Regression sets:**
  - `tests/tk50/tktest.py` K1–K3 (TK50, TQ): `python3 -I tktest.py <BIN> <VMS047 copy> <dir> K1 K2 K3`;
  - `tests/klesib` b1–b4;
  - `tests/dmb32/p3.ini` (VMS from the UDA50 on the DWBUA, LP11 and DMB32 printers);
  - `tests/kdb50`, `tests/debna` (DECnet to a MicroVAX II), `tests/vax8300`;
  - the RA, RX, TS, TU58 and RB730 sets.
  
  The test READMEs give the commands.
- **A VMS V4.7 boot plus test** takes about 8 minutes. Run long tests in the background and do other work meanwhile.

## 6. Open points

| Point | State |
|---|---|
| Known issues 70: Unibus devices can be enabled while the UBA is disabled | Decision for Ulli: leave it, or refuse `SET <dev> ENABLE` for Unibus devices while the UBA is disabled |
| Known issues 69: no network boot on the 8200 (DEBNA) | Open until further notice (Ulli). VMS V4.7's VMB has no DEBNA driver. **Idea:** VMS V5.5's VMB.EXE (on VMS055.R82) may have the AIE_NI boot driver (device code 98). Only with Ulli's go-ahead. |
| VMS V5.5 RA82 image | In use: boots from the KDB50 (8200 and 8300), DECnet over the DEBNA and the DMB32 sync line; tests `tests/kdb50/v55_*.ini`, `tests/debna/p55_8200.ini`, `tests/dmb32/p4*.ini`. SYSTEM has no password. Further tests: ask Ulli. |
| ULTRIX tests of the VAXBI devices (KLESI-B, KDB50, DMB32, DEBNA) | Not done: no ULTRIX system for the 8200 is available. ULTRIX V2.2 and UETP tests are postponed until further notice. |
| DMB32 sync line | Done with VMS V5.5's SIDRIVER (patch 12, patch 16). VMS V4.7: V5.5's SIDRIVER backported by binary patching (2026-11-19, `docs/SIDRIVER_V47_backport.md`, tools in `tools/sidriver_v47/`); POINT and TRIBUTARY pass; the patched SIDRIVER.EXE is DEC software from Ulli's VMS055.R82: Ulli only, never in the fork or the project. Known issues 86 (circuit fault at circuit OFF) open |
| Second KDB50, DEBNK tape port | Postponed by Ulli |
| MicroVAX II THROTTLE results | Waiting for Ulli's results |
| DMC11 switches and remote load detect | Waiting for a DMC11 line unit manual from Ulli (known issue 99): the meaning of the M8201/M8202 switch packs 2 and 3 |
| DMV11 after patch 18 v2 | VMS DECnet regressions with the DMV11/DMP11 (point-to-point, multipoint; known issues 88 and 92) not run yet; VMS047.R81 is needed again. remwrt loads with `SET DMV0 PROGRAM=SYSTEM` (patch 18 v3); a DMV11 secondary loader was not written. Regressions after patch 18 v3: PASS (use SET CPU IDLE=VMS, known issues 97). Still untested with VMS: multipoint MOP (known issues 88) and NCP LOOP CIRCUIT over a DMC/DMP/DMV after the fix of 92. `NCP LOOP CIRCUIT` over a DMC/DMP/DMV not re-tested after the fix of 92. Known issues 89 stays open (the TM says nothing either), 93 and 94 not investigated |

## 7. Facts worth keeping in mind

- **VAX 8200 BI nodes (defaults since patch 14, all `SET dev NODE=n`):**
  - DWBUA 0;
  - KA1 1 (the 8300's attached processor), KA0 2 (VMS: primary CPU 02);
  - KDB50 4;
  - DMB32 5;
  - KLESI-B 6;
  - DEBNA 7;
  - memory boards MCTL0-MCTL7 on 15-8 (MCTL8-11 without a default node).
  
  `SET dev NODE=n` refuses a node in use (`bi_get_node`); `SHOW BI` lists them; BI devices supply their own vectors via `nexusA` (patch 8). Test scripts that name nodes (the diagnostic supervisor's ATTACH lines) use these numbers.
- **DMP11 and DMR11 switches (patch 21):** the DMP11's power-on boot switch (E121 SW10) enables the boot when ON (default OFF); the DMR11's remote load detect is on when its PASSWORD is not 377 and needs a boot ROM at 173000+BOOTOFFSET (ROM device) or else loads the system itself (MOP device type 40). A test "operating system" needs a stack (an 11/70 stops with a red stack trap at SP 0 when the board takes the CPU over).
- **DMV11 switches (patch 18):** `SET DMVn MODE=...` acts at the next master clear (a running guest master-clears first; a test program without a master clear sees the old mode). Booting needs the mode from the switches and an attached line; T1-T6 in `tests/dmv11_boot/` connect the Python control station to the ATTACH port (known issues 91).
- **VMS device names:**
  - TQ and TB together: PTA/MUA = TQ, PTB/MUB = TB;
  - VAXBI-only: TB is PTA/MUA;
  - DEBNA: ETA0, DECnet line BNT-0 (VMS V4.7) or BNA-0 (VMS V5.5);
  - DMB32 sync line: SIA0, DECnet line DMB-0 (VMS V5.5 SIDRIVER); the DMV11 on VMS V4.7: DMP-0.
- **Realism mode and fixed RA drives:** ATTACH leaves the drive stopped; `SET <dev>n ONLINE` (RUN/STOP) spins it up. A boot script in the realism mode needs it before BOOT, or VMB says "%BOOT-F-Failed to initialize device" (expected, `tests/ra/ra2`).
- **Throttled SMP tests:** timing bugs of the 8300 show at THROTTLE 400K / in the realism mode (few instructions per clock tick), not at host speed: test SMP changes there too (`tests/vax8300/smp55_rep.sh`: repeated runs; one run proves little).
- **VMS and memory boards:** VMS (V4.7, V5.5) uses at most 8 MS820 boards; more give a warning (known issues 80).
- **VMS error count 1** on PTA0, PUA0 and PUB0 after the startup is normal (port start); it is not a device fault.
- **The pdp11_tq.c code serves several controllers.** The per-controller state is `TQ_CTX` (`tq_ctx0` TQ, `tq_ctx1` TB), the names are macros for the current controller (`tqc`), and every entry point selects the controller. New code in pdp11_tq.c must keep this up: set `tqc` in any new entry point.
