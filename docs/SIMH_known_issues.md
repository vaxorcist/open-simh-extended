<!-- open-simh-extended/docs/SIMH_known_issues.md  v6 (v6: cases 22-24, case 18 confirmed by the RC25 User Guide) -->
# Open SIMH: known bugs and suspicious code

Started 2026-11-04 for Ulli (vaxorcist).

This list collects every bug, suspicious spot and limitation found while working on the Open SIMH clone (sound project and others). Ulli decides what happens with each case. Nothing is fixed unasked, except problems in Claude's own new code.

How the list is kept:
- Each case keeps its number for good.
- Fixed cases stay in the list, marked **DONE**, with the fix and the patch it is in.
- New cases are added at the end.

Kinds of case:
- **Bug:** wrong behaviour.
- **Suspicious:** looks wrong or fragile, but the effect is not proven.
- **Limitation:** a missing feature that affects users.

Line numbers refer to the sound branch (patch v8 plus work in progress) and are only a guide; the function names are the reliable reference.

## Summary

| No. | Status | Kind | Where | Short description |
|---|---|---|---|---|
| 1 | DONE | Bug | `VAX/vax730_rb.c` `rb_svc` (GET STATUS, RB02) | Operator precedence: the RB02 status bits are never reported |
| 2 | DONE | Bug | `VAX/vax730_rb.c` `rb_svc` (READ HEADER) | Read Header returns no track or cylinder |
| 3 | DONE | Suspicious | `VAX/vax730_rb.c` `rb_rd32`, `rb_wr32` | Drive ready set although no cartridge is loaded |
| 4 | DONE | Bug (confirmed) | `VAX/vax730_rb.c` `GET_DA` | RB image layout head-major, not DEC logical block order: RB02 images are not binary copies of a cartridge and not interchangeable with RL02 images |
| 5 | DONE | Suspicious | `VAX/vax730_rb.c` `rb_svc` | No cylinder range check on seeks and transfers |
| 6 | OPEN | Suspicious | `VAX/vax730_rb.c` `rb_svc` (SEEK) | Undocumented seek with disk address 0xFFFFFFFF |
| 7 | DONE | Limitation | `VAX/vax730_rb.c` | RB730 does not use SIMH's disk library (sim_disk): no VHD, no FORMAT/autosize/metadata |
| 8 | DONE | Bug | `VAX/vax730_rb.c` / `sim_sound.c` | SET SOUND ENABLED/DISABLED allowed while an RB drive is attached |
| 9 | OPEN | Bug | `PDP11/pdp11_rq.c` `rq_svc` (COMPARE) | Wrong bus address stored after a memory error during Compare |
| 10 | OPEN | Suspicious | `PDP11/pdp11_rq.c` `rq_avl` | AVAILABLE with spin-down detaches bypassing `rq_detach` |
| 11 | OPEN | Suspicious | `PDP11/pdp11_rq.c` / `sim_disk.c` | After an unload, the container was reported as RAW format at the next ATTACH (not verified) |
| 12 | OPEN | Suspicious | `PDP11/pdp11_rq.c` `rq_rw_valid` | LBN plausibility check commented out |
| 13 | OPEN | Minor | `PDP11/pdp11_rq.c` `rq_help` | HELP RQ always says "UDA50A", whatever the controller type |
| 14 | OPEN | Minor | `PDP11/pdp11_rl.c` RL seek | Open TBD: head switch sector position |
| 15 | DONE | Bug | `PDP11/pdp11_rl.c` `rl_show_spindle` | SHOW RLn SPINDLE printed no line end |
| 16 | DONE | Bug | `sim_timer.c` `sim_throt_svc` | Throttle started with a stale start time when THROT_DELAY is 0 |
| 17 | OPEN | Suspicious | `sim_timer.c` `sim_activate_after` | During a throttled boot, events can be tied to a much later timer tick |
| 18 | OPEN | Bug | `PDP11/pdp11_rq.c` RC25/RCF25 definitions | Geometry 50 sectors × 8 surfaces × 1260 cylinders does not match the 50,902 LBNs (31 × 2 × 821) |
| 19 | DONE | Suspicious | `VAX/vax730_rb.c` `rb_svc` | RB02 without a cartridge ends commands with INCMP only, which VMS reports as "parity error" or "fatal controller error" instead of "medium offline" |
| 20 | DONE | Bug | `VAX/vax730_rb.c` RB80 geometry | SIMH's RB80 has 32 sectors per track, VMS uses 31: the image is not in logical block order even in the standard layout |
| 21 | DONE | Bug | `VAX/vax730_rb.c` `rb_svc` (seek done) | A finishing seek re-reads the controller's current command: overlapped seeks can execute another drive's command on the wrong drive |
| 22 | OPEN | Suspicious | `PDP11/pdp11_rq.c` / MicroVAX 3900 console | Self-test error `?53 2 xx` at BOOT CPU after a failed VMB boot when a unit becomes available meanwhile |
| 23 | DONE | Bug | `sim_sound.c` `sim_spindle_init` | More than 32 spindles failed silently; the drive then never became ready |
| 24 | OPEN | Minor | `PDP11/pdp11_rq.c` RC25 | RC25/RCF25 accepted on every MSCP controller type; KLESI specifics not simulated |

## Details

### 1 — DONE — Bug: RB02 GET STATUS never reports the drive's status bits
- **Where:** `VAX/vax730_rb.c`, `rb_svc`, GET STATUS for an RB02:
  `rbmp = uptr->STAT | (uptr->flags & UNIT_ATT)? RB02DS_ATT: RB02DS_UNATT;`
- **Problem:** `?:` binds weaker than `|`, so this is `(STAT | ATT) ? RB02DS_ATT : RB02DS_UNATT`.
- **Effect:** volume check, spin error and write gate error never reach the host. A drive with no cartridge but a left-over error in `STAT` reports "locked on, heads out", as if a cartridge were loaded.
- **Probable intent:** `rbmp = uptr->STAT | ((uptr->flags & UNIT_ATT) ? RB02DS_ATT : RB02DS_UNATT);`
- **Caution:** VMS would then see volume check after ATTACH, which it may not see today. A fix needs a test.
- **Found:** 2026-11-04 (RB02 realism work). The realism changes leave the line untouched.
- **Fixed:** branch `rb730` (RB730 patch `opensimh_6_rb730_v1.patch`), commit 6d9c3aa (`rbmp = uptr->STAT | ((ATT) ? RB02DS_ATT : RB02DS_UNATT)`), 2026-11-04. Tested with the regression set `rbtest/rbtest.py` (VMS V3.5 on the 730 and the RL11 of the 750).

### 2 — DONE — Bug: RB READ HEADER returns no track or cylinder
- **Where:** `VAX/vax730_rb.c`, `rb_svc`, READ HEADER: `rbmp = (uptr->TRK & RBDA_TRACK) | GET_SECT (rbda);`
- **Problem:** a seek stores only the cylinder in `TRK` (`uptr->TRK = newc << RBDA_V_CYL`), so `TRK & RBDA_TRACK` is always 0 and the cylinder is masked out.
- **Effect:** the header always reads cylinder 0, track 0. Software that checks the header (diagnostics, some drivers) gets wrong data.
- **Found:** 2026-11-04.
- **Fixed:** branch `rb730` (RB730 patch `opensimh_6_rb730_v1.patch`), commit 6d9c3aa (seeks and transfers keep the head in `TRK`; READ HEADER returns cylinder, head and sector), 2026-11-04. Tested with the regression set `rbtest/rbtest.py` (VMS V3.5 on the 730 and the RL11 of the 750).

### 3 — DONE — Suspicious: RB drive ready without a cartridge
- **Where:** `VAX/vax730_rb.c`, `rb_rd32` and `rb_wr32`, RBCS.
- **Problem:** DRDY depends only on "unit busy" (`sim_is_active`) and `UNIT_DIS`, so a drive with no cartridge reports ready. Commands then fail with a spin error.
- **Effect:** probably harmless (VMS copes with it), but not what the hardware does.
- **Note:** in the realism mode, an RB02 whose cartridge is attached but not up to speed now reports DRDY off (Claude's code). The case of no cartridge at all is unchanged.
- **Found:** 2026-11-04.
- **Fixed:** branch `rb730` (RB730 patch `opensimh_6_rb730_v1.patch`), commit 6d9c3aa (no drive ready without a cartridge), 2026-11-04. Tested with the regression set `rbtest/rbtest.py` (VMS V3.5 on the 730 and the RL11 of the 750).

### 4 — DONE — Bug (confirmed 2026-11-04): RB image layout is not DEC logical block order
- **Where:** `VAX/vax730_rb.c`, `GET_DA`: `track * cylinders * sectors * words + cylinder * sectors * words + sector * words`. The file holds all of surface 0, then all of surface 1 (head-major).
- **The RL11** (`pdp11_rl.c`) stores `(cylinder * 2 + head) * 40 + sector`, which is cylinder-major.
- **Verified with VMS V3.5:**
  1. VAX-11/730, RB02 volume RB02T: `DUMP/HEADER` puts the contiguous file X.TMP at LBN 63..158. Virtual block 21 = LBN 83 = 256-byte sectors 166/167.
     - Cylinder-major decomposition (DEC's usual order): cylinder 2, head 0, sector 6.
     - The bytes of that block (from `DUMP/BLOCK`) are at sector 86 of the image file = `0*512*40 + 2*40 + 6`. So VMS's DQDRIVER maps logical blocks cylinder-major, exactly like an RL11 driver.
     - With a head-major LBN mapping in DQDRIVER, the block would be at sector 166. It is not.
  2. VAX-11/750, RL11, VMS V3.5 (DLDRIVER):
     - the same RB02 image converted to the RL layout (`tools/rl02_rb02_convert.py rb2rl`) mounts as RB02T; DIRECTORY lists all files; virtual block 21 of X.TMP dumps byte for byte the same as on the 730;
     - the unconverted image fails: `%MOUNT-W-IDXHDRBAD`, `%MOUNT-F-FILESTRUCT`.
- **Conclusion:**
  - DEC's drivers for both controllers put a logical block on the same cylinder, head and sector, so real RL02/RB02 cartridges are interchangeable.
  - SIMH's RL11 layout is DEC logical block order (a binary copy of a cartridge read in LBN order). The RB730 layout is not.
  - This matches Ulli's experience that SIMH RL02 images can be written to a real RL02.
- **Also affects the RB80:** its image is head-major as well, so it is not in LBN order. Host tools that read an image in LBN order (ODS-2 readers, SIMH's own file-system detection at ATTACH) cannot read RB images.
- **Workaround:** `tools/rl02_rb02_convert.py {rl2rb|rb2rl}` converts RL02 images between the two layouts; a round trip is identical.
- **Decision (Ulli):** fix the layout, keep old images usable with a temporary `SET RBn LAYOUT=OLD`, and provide the converter. The older `GET_DA` in the comment was also wrong (`*` instead of `+`).
- **Found:** 2026-11-04. **Confirmed:** 2026-11-04 (layout tests l1/l2).
- **Fixed:** branch `rb730` (RB730 patch `opensimh_6_rb730_v1.patch`), commit cfddc9c (standard layout; `SET RBn LAYOUT=OLD` for old images; converter `tools/rb730_layout_convert.py` for the RB02 and RB80, which replaces `rl02_rb02_convert.py`), 2026-11-04. Tested with the regression set `rbtest/rbtest.py` (VMS V3.5 on the 730 and the RL11 of the 750).

### 5 — DONE — Suspicious: no RB cylinder range check
- **Where:** `VAX/vax730_rb.c`, `rb_svc`, seek and transfer.
- **Problem:** cylinders above 511 (RB02) or 558 (RB80) are accepted; `RBDA_M_CYL` is 0xFFFF.
- **Effect:** reads past the end of the file return zeros; writes extend the image file.
- **Found:** 2026-11-04.
- **Fixed:** branch `rb730` (RB730 patch `opensimh_6_rb730_v1.patch`), commit 2bd1b66 (a seek stops at the last cylinder; a track beyond the last head gives a header error), 2026-11-04. Tested with the regression set `rbtest/rbtest.py` (VMS V3.5 on the 730 and the RL11 of the 750).

### 6 — OPEN — Suspicious: RB seek with disk address 0xFFFFFFFF
- **Where:** `VAX/vax730_rb.c`, `rb_svc`, SEEK: `if ((uint32)rbda == 0xFFFFFFFF) swait = rb_swait;`
- **Problem:** the heads don't move and the case is not documented. It may be a recalibrate or drive-select-only seek.
- **Effect:** in the realism mode this seek keeps the old timing and plays no sound.
- **Found:** 2026-11-04.
- **Observed 2026-11-04:** VMS V3.5 (DQDRIVER) issues this seek during error recovery, right after GET STATUS following a failed transfer; the heads stay where they are, and VMS then repeats the transfer. Kept as is.

### 7 — DONE — Limitation: RB730 does not use SIMH's disk library
- **Where:** `VAX/vax730_rb.c` uses `attach_unit` and `sim_fseek`/`sim_fread`/`sim_fwrite` directly.
- **Effect:**
  - no VHD for any RB unit (RB80 or RB02): a VHD file would be misread;
  - no `SET RBn FORMAT`, no autosize, no SIMH metadata footer, no sim_disk debug;
  - the device is not `DEV_DISK` (see case 8).
- Ulli cannot create a VHD RB80 image for this reason and uses flat files.
- **Found:** 2026-11-04.
- **Fixed:** branch `rb730` (RB730 patch `opensimh_6_rb730_v1.patch`), commit 13006f4 (sim_disk: SIMH/VHD/RAW, FORMAT, autosize RB02/RB80, metadata, DISK debug, bad block table on new images, file system detection, DEV_DISK), 2026-11-04. Tested with the regression set `rbtest/rbtest.py` (VMS V3.5 on the 730 and the RL11 of the 750).

### 8 — DONE — Bug: SET SOUND ENABLED allowed while an RB drive is attached
- **Where:** `sim_sound.c`, `snd_attached_storage_unit`, which only looked at `DEV_DISK`/`DEV_TAPE` devices. The RB device is neither (case 7).
- **Fix:** new `sim_sound_register_storage (DEVICE *)`; `rb_reset` registers the RB device.
- **In:** the next sound patch (after v8). This was Claude's own code, so it was fixed directly.
- **Found and fixed:** 2026-11-04.
- **Superseded 2026-11-04:** with the RB730 on sim_disk (case 7) the device is `DEV_DISK`, so the SET SOUND check covers it without the registration. `sim_sound_register_storage` was removed again (sound patch v10).

### 9 — OPEN — Bug: MSCP Compare stores a wrong bus address after a memory error
- **Where:** `PDP11/pdp11_rq.c`, `rq_svc`, Compare branch, non-existent memory:
  `PUTP32 (pkt, RW_WBCL, bc - i); PUTP32 (pkt, RW_WBAL, bc - i); /* adj ba */`
- **Problem:** the buffer address is set to the byte count.
- **Probable intent:** `ba + i`, as in the read and write branches.
- **Effect:** wrong address in the end packet or error log; only after a memory error during Compare.
- **Found:** 2026-11-04.

### 10 — OPEN — Suspicious: MSCP AVAILABLE with spin-down bypasses rq_detach
- **Where:** `PDP11/pdp11_rq.c`, `rq_avl`, `sim_disk_unload (uptr)` for removable media. `sim_disk_unload` calls `sim_disk_detach` directly.
- **Problem:** `rq_detach` is not called, so `UNIT_ATP` is not cleared and the spindle state is not reset.
- **Effect:** the realism mode avoids this path for the RA60, but it still applies in normal mode and to the other removable types (RX50, RX33, CD-ROM).
- **Found:** 2026-11-04.

### 11 — OPEN — Suspicious: format lost after an MSCP unload (not verified)
- **Observed:** an RA60 container created by ATTACH (SIMH format), then unloaded by VMS `DISMOUNT/UNLOAD` in normal mode (case 10's path), was shown as "RAW format" at the next ATTACH.
- **Possible cause:** `sim_disk_unload` sets `media_removed` and detaches, and the SIMH metadata footer may not be written.
- **Effect:** to be checked; the data seemed intact.
- **Found:** 2026-11-04 (test t1/t0, RA60 under VMS 3.5).

### 12 — OPEN — Suspicious: MSCP LBN plausibility check commented out
- **Where:** `PDP11/pdp11_rq.c`, `rq_rw_valid`:
  `// if (lbn & 0xF0000000) return (ST_CMD | I_LBN);`
- **Effect:** probably none, because the range checks that follow catch large LBNs. Worth a look.
- **Found:** 2026-11-04.

### 13 — OPEN — Minor: HELP RQ names the wrong controller type
- **Where:** `PDP11/pdp11_rq.c`, `rq_help`: `"UDA50A MSCP Disk Controller (%s)"`, fixed text.
- **Effect:** wrong title for RQDX3, KDA50, KLESI and the other types; `rq_description` uses the real type.
- **Found:** 2026-11-04.

### 14 — OPEN — Minor: open TBD in the RL11 seek
- **Where:** `PDP11/pdp11_rl.c`, seek: `/* TBD: if a head switch, sector should be RL_NUMSC/2? */`
- **Effect:** none known.
- **Found:** 2026-11-04.

### 15 — DONE — Bug: SHOW RLn SPINDLE printed no line end
- **Where:** `PDP11/pdp11_rl.c`, `rl_show_spindle`, an `MTAB_NMO` entry (SCP adds no line end for those).
- **Effect:** "no cartridgesim>".
- **Fix:** the routine ends the line itself. Patch v8.
- **Found:** 2026-11-03. **Fixed:** 2026-11-04.

### 16 — DONE — Bug: throttle measured with a stale start time
- **Where:** `sim_timer.c`, `sim_throt_svc`, INIT state.
- **Problem:** with `THROT_DELAY 0`, the first measurement used a start time from before the delay.
- **Effect:** wrong throttle rate at the start, and the warning "Host CPU could be too slow".
- **Fix:** take a fresh start reading and measure (state TIME). Patch v3 (sound project; the realism mode sets `THROT_DELAY 0`).
- **Found and fixed:** 2026-11-02.

### 17 — OPEN — Suspicious: sim_activate_after during a throttled boot
- **Where:** `sim_timer.c`, `sim_activate_after` (timer calibration).
- **Problem:** while the throttle and the timer calibration settle (boot), an event scheduled with `sim_activate_after` can be tied to a much later timer tick than asked.
- **Effect:** in the sound project, realistic device times are converted to instructions by the devices themselves (`sim_sound_activate_us`), so the sound devices avoid it. Other devices may still see long delays.
- **Found:** 2026-11-02 (VMS mount verification on the RK07).

### 18 — OPEN — Bug: RC25/RCF25 geometry does not match the capacity
- **Where:** `PDP11/pdp11_rq.c`, `RC25_SECT 50`, `RC25_SURF 8`, `RC25_CYL 1260` (the same for RCF25). These are reported to the host in GET UNIT STATUS (track, group, cylinder sizes).
- **Problem:** 50 × 8 × 1260 = 504,000 blocks, while `RC25_LBN` is 50,902. The comment table in the same file, and the Computer History Wiki, give 31 sectors × 2 surfaces × 821 cylinders = 50,902, matching the LBN count. The define's own comment says `/* ? 50*8*1260 ? */`.
- **Effect:** wrong geometry in GET UNIT STATUS, which the host may use for layout or bad block handling. The realism timing for the RC25 (next step) uses the real geometry.
- **Found:** 2026-11-04 (RC25 realism preparation).
- **Confirmed 2026-11-06** by the RC25 Disk Subsystem User Guide (EK-ORC25-UG-002): 26,061,824 bytes per platter = 50,902 blocks of 512 bytes; spiral read 50 s per platter = 1642 tracks (821 × 2) × 31 ms. (The guide writes "50,902 1Kb blocks" for both platters together; that is a slip, it is 50,902 blocks of 512 bytes per platter.) The RC25 realism timing (sound patch v12) uses 31 × 2 × 821; the geometry reported to the host is still unchanged.

### 19 — DONE — Suspicious: RB02 without a cartridge reports a parity or controller error
- **Where:** `VAX/vax730_rb.c`, `rb_svc`, not attached: `rb_set_done (RBCS_ERR | RBCS_INCMP)`; drive ready stays on (case 3).
- **Observed:** with the drive stopped but treated like this, VMS 3.5 reported `%MOUNT-F-CTRLERR, fatal controller error` for MOUNT, and `-SYSTEM-F-PARITY` for a COPY from a mounted volume, without starting mount verification.
- **What made VMS see "medium offline" in the realism mode:** drive ready off plus drive error (`RBCS_DRE`), as the RL11 does. For a stopped RB02 with a cartridge, MOUNT then asks the operator, and a mounted volume goes into mount verification.
- **Effect:** the case without a cartridge at all is unchanged and presumably still gives the controller or parity error.
- **Found:** 2026-11-04 (RB02 realism tests).
- **Fixed:** branch `rb730` (RB730 patch `opensimh_6_rb730_v1.patch`), commit 6d9c3aa (drive error; VMS now reports %MOUNT-F-MEDOFL, or asks the operator), 2026-11-04. Tested with the regression set `rbtest/rbtest.py` (VMS V3.5 on the 730 and the RL11 of the 750).

### 20 — DONE — Bug: RB80 with 32 sectors per track, VMS uses 31
- **Where:** `VAX/vax730_rb.c`, `RB80_NUMSC 32` (`RB80_SIZE` = 559 × 14 × 32 sectors).
- **Observed with VMS V3.5:**
  - every RB80 transfer in the command trace starts at sectors 0–30, and none touches sector 31;
  - INITIALIZE gave "Max. files allowed 60651" = 242,606 / 4, and 242,606 = 559 × 14 × 31. That is the RM80's capacity, and the R80 is the same drive.
- **Effect:** each track of the image has an unused 512-byte gap, so even in the standard layout the RB80 image is not in logical block order. SIMH's file-system detection does not find the ODS-2 volume, and the image differs from an RM80 image.
- **Proposal (accepted by Ulli):**
  - standard layout with 31 sectors per track (124,214,272 bytes), in logical block order like an RM80 image;
  - the converter drops sector 31 of each track;
  - LAYOUT=OLD keeps 32 sectors.
- **Found:** 2026-11-04 (RB730 step 2 tests).
- **Fixed:** branch `rb730` (RB730 patch `opensimh_6_rb730_v1.patch`), commit c2304ad, 2026-11-04. With the 31-sector standard layout SIMH now finds the ODS-2 volume ("Sectors In Volume: 242606"); `tools/rb730_layout_convert.py` v2 drops the unused sector 31 (it was zero on Ulli's image). Regression set passed with the flat, the VHD and the old-layout image.

### 21 — DONE — Bug: a finishing seek executes the current command of another drive
- **Where:** `VAX/vax730_rb.c`, `rb_svc`. The second activation of a seek ("seek done") first decodes the *current* controller registers (`rbcs` function, `rbda`, `GET_DRIVE (rbcs)`) and only then looks at the unit's seek-in-progress flag (`SIP`). It also sets DONE for the controller.
- **Problem:** VMS overlaps seeks on different drives. If VMS has meanwhile started a transfer on another drive, the seek completion of the first drive runs that transfer with the first drive's unit.
- **Seen:** 3 times in the RB realism test (seek times of 15–100 ms): "Invalid cylinder" with an RB80 address on the RB02, then VMS retries. In normal mode the window is small (300+ instructions), but it exists.
- **Effect:** wrong-drive errors and retries, error log entries; in the worst case a transfer on the wrong drive.
- **Proposal (accepted by Ulli):** when a seek finishes, only set the drive's attention bit (drive = unit number), and request an interrupt only if the controller is idle. Leave the current command alone.
- **Found:** 2026-11-04 (analysis of the RB realism trace testRB/r1.dbg).
- **First attempt (failed):** only raising the attention bit broke the RB02. VMS writes the next command for a drive while it is still seeking, and the controller holds it until the seek is done, which the old code did by accident. INITIALIZE then timed out or gave a parity error.
- **Fixed:** branch `rb730` (RB730 patch `opensimh_6_rb730_v1.patch`), commit c2304ad, 2026-11-04. A finishing seek raises its own drive's attention. It executes the current command only if that command is for the same drive, and signals done only if the controller is idle.
- **Tested:** regression set passed; in the RB realism test (sound branch rebased on the fix) the "Invalid cylinder" errors went from 3 to 0.

### 22 — OPEN — Suspicious: console self-test error after a failed VMB boot
- **Where:** MicroVAX 3900 (KA655) console with an MSCP controller (`PDP11/pdp11_rq.c`).
- **Observed:** a VMB boot fails (`?4D DEVOFFLINE`), the controller stays initialized. Then a unit becomes available (ATTACH, or SET RQn ONLINE of an RA60/RC25), and BOOT CPU runs the self-test again: `?53 2 05 FF 00 0000` (also test 07 or 0A seen). The next VMB boot works.
- **Seen with:** the RA60 in normal mode with ATTACH (no RUN/STOP button, no sound: test `tests/rc25/c9b.ini`), so the code path is the one of plain Open SIMH; with SET RQ0 ONLINE (`c9.ini`) the same.
- **Possible cause:** the "available" attention message of the unit (UNIT_ATP, sent by `rq_tmrsvc` while the controller is up) is posted and interrupts while the console self-test runs. Not proven.
- **Also:** SET DEBUG output during the self-test causes `?53` errors by itself (timing); the RC25 tests switch debugging on after the self-test.
- **Found:** 2026-11-06 (RC25 tests).

### 23 — DONE — Bug: more than 32 spindles failed silently
- **Where:** `sim_sound.c`, `sim_spindle_init`: at most 32 spindles (`SND_MAX_SPINDLES`); for the next one `sp->uptr` stayed NULL without a message.
- **Effect:** such a drive's spindle state is always "stopped": an RA60 or RC25 would report offline even in the normal mode. A PDP-11 uses 20 at reset (HK 8, RL 4, RP 8), each RQ unit with a RUN/STOP button one more.
- **Fix (Claude's own code):** limit 64 and a message naming the unit; the RQ treats a drive without a spindle slot as one without realism (ready when attached). HK, RL, RP and RB set up their spindles at reset and stay below the limit. Sound patch v12.
- **Found and fixed:** 2026-11-06.

### 24 — OPEN — Minor: RC25 on any MSCP controller type
- **Where:** `PDP11/pdp11_rq.c`. The header (29-Jan-11, HUH) says the KLESI controller and port types are "not yet implemented"; `SET RQ KLESI` only sets the name and type.
- **Effect:** RC25/RCF25 units can be set on an RQDX3, UDA50, etc.; a real RC25 only works with its KLESI adapter. The KLESI specifics in Appendix A of the RC25 User Guide (command limit 14, vector 154, controller timeout 15 s, no NV bit in step 1, ...) are not simulated. VMS does not seem to care; untested.
- **Found:** 2026-11-06 (RC25 realism work).

