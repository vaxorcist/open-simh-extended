<!-- open-simh-extended/docs/SIMH_known_issues.md  v11 (v11: cases 29-36, TS11/TSV05; v10: case 28; v9: case 27; v8: cases 18, 23, 24 done, 22 narrowed down, case 26; v7: case 25; v6: cases 22-24, case 18 confirmed by the RC25 User Guide) -->
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
| 18 | DONE | Bug | `PDP11/pdp11_rq.c` RC25/RCF25 definitions | Geometry 50 sectors × 8 surfaces × 1260 cylinders did not match the 50,902 LBNs (31 × 2 × 821) |
| 19 | DONE | Suspicious | `VAX/vax730_rb.c` `rb_svc` | RB02 without a cartridge ends commands with INCMP only, which VMS reports as "parity error" or "fatal controller error" instead of "medium offline" |
| 20 | DONE | Bug | `VAX/vax730_rb.c` RB80 geometry | SIMH's RB80 has 32 sectors per track, VMS uses 31: the image is not in logical block order even in the standard layout |
| 21 | DONE | Bug | `VAX/vax730_rb.c` `rb_svc` (seek done) | A finishing seek re-reads the controller's current command: overlapped seeks can execute another drive's command on the wrong drive |
| 22 | OPEN | Suspicious | MicroVAX 3900 (KA655) console / SIMH timing, **not the RQ** | Self-test error `?53 2 xx` at BOOT CPU after a failed VMB boot (also on plain Open SIMH with nothing attached) |
| 23 | DONE | Bug | `sim_sound.c` `sim_spindle_init` | More than 32 spindles failed silently; the drive then never became ready (now no fixed limit) |
| 24 | DONE (realism mode) | Minor | `PDP11/pdp11_rq.c` RC25 | RC25/RCF25 accepted on every MSCP controller type; KLESI specifics not simulated |
| 25 | OPEN | Suspicious | `PDP11/pdp11_rq.c` `rq_avl` (normal mode) | DISMOUNT/UNLOAD of an RC25 cartridge detaches it while the fixed platter on the same spindle keeps running |
| 26 | OPEN | Suspicious | `PDP11/pdp11_rq.c` `rq_set_plug` | A unit number is "in use" while a disabled unit holds it: on the VAX, `SET RQ0 UNIT=8` is refused because of the disabled RQ8 |
| 27 | DONE | Bug | `sim_sound.c` unit loops, run hooks | Fixed tables of 64 unit loops and 8 run hooks: a unit loop beyond 64 got no power-on restore |
| 28 | DONE | Bug | Visual Studio `MicroVAX2.vcxproj`, `VAX.vcxproj` (+ `.filters`) | The DMV11 patch added `pdp11_dmc.c` only to the old `.vcproj` files: a Visual Studio 2010+ build of the MicroVAX II and 3900 could not link `dmv_dev` |
| 29 | OPEN | Suspicious | `PDP11/pdp11_ts.c` `ts_svc` (FNC_INIT) | The Initialize command rewinds the tape; both manuals say it is a no-op (a status update) unless microdiagnostic errors are shown |
| 30 | OPEN | Suspicious | `PDP11/pdp11_ts.c` `ts_svc` (FNC_WRIT), `fnc_mod` | Write Data Retry is mode 2 (0010) in both manuals; SIMH runs it as mode 1 and rejects mode 2 as an illegal command |
| 31 | OPEN | Suspicious | `PDP11/pdp11_ts.c` `ts_svc` (FNC_CTL mode 4) | Control mode 4 is the TSV05's "rewind with immediate interrupt" (not in the TS11 repertoire); SIMH accepts it on the Unibus TS11 too |
| 32 | OPEN | Suspicious | `PDP11/pdp11_ts.c` `ts_svc` (FNC_CTL mode 1) | Rewind and Unload calls `sim_tape_detach` directly, bypassing `ts_detach`: no attention message when the drive goes off-line (normal mode) |
| 33 | OPEN | Minor | `PDP11/pdp11_ts.c` `ts_description` | The Qbus description says "TSV11/TSV05"; there is no TSV11 |
| 34 | OPEN | Minor | `PDP11/pdp11_ts.c` `ts_help` | HELP TS mentions "type options" and a "bad block option" that the TS device does not have (copied from a disk driver) |
| 35 | OPEN | Suspicious | `scp.c` SET (and help listing) | For a single-unit device, HELP lists unit-only modifiers as `SET dev ...` (e.g. `set TS LOCKED`), but SET at device level refuses them ("Invalid argument"); `SET TS0 LOCKED` works |
| 36 | OPEN | Minor | `PDP11/pdp11_ts.c` `ts_rd`, `ts_wr` debug | The REG debug lines name the registers the wrong way round (the base address, TSBA/TSDB, is shown as TSSR and vice versa) |

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

### 18 — DONE — Bug: RC25/RCF25 geometry does not match the capacity
- **Where:** `PDP11/pdp11_rq.c`, `RC25_SECT 50`, `RC25_SURF 8`, `RC25_CYL 1260` (the same for RCF25). These are reported to the host in GET UNIT STATUS (track, group, cylinder sizes).
- **Problem:** 50 × 8 × 1260 = 504,000 blocks, while `RC25_LBN` is 50,902. The comment table in the same file, and the Computer History Wiki, give 31 sectors × 2 surfaces × 821 cylinders = 50,902, matching the LBN count. The define's own comment says `/* ? 50*8*1260 ? */`.
- **Effect:** wrong geometry in GET UNIT STATUS, which the host may use for layout or bad block handling. The realism timing for the RC25 (next step) uses the real geometry.
- **Found:** 2026-11-04 (RC25 realism preparation).
- **Confirmed 2026-11-06** by the RC25 Disk Subsystem User Guide (EK-ORC25-UG-002): 26,061,824 bytes per platter = 50,902 blocks of 512 bytes; spiral read 50 s per platter = 1642 tracks (821 × 2) × 31 ms. (The guide writes "50,902 1Kb blocks" for both platters together; that is a slip, it is 50,902 blocks of 512 bytes per platter.) The RC25 realism timing (sound patch v12) uses 31 × 2 × 821; the geometry reported to the host is still unchanged.

- **Fixed:** 2026-11-07, decided by Ulli: `RC25_SECT`/`RCF25_SECT` 31, `_SURF` 2, `_CYL` 821 (sound patch v13; both modes, since it describes the drive). VMS V3.5 `F$GETDVI` on the RCF25: before 50/8/128 sectors/tracks/cylinders, now 31/2/821 (MAXBLOCK 50902 both times); the VMS tests N1 and R1 still pass.

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

### 22 — OPEN — Suspicious: console self-test error after a failed VMB boot (not the RQ)
- **Where:** MicroVAX 3900 (KA655) console with an MSCP controller (`PDP11/pdp11_rq.c`).
- **Observed:** a VMB boot fails (`?4D DEVOFFLINE`), the controller stays initialized. Then a unit becomes available (ATTACH, or SET RQn ONLINE of an RA60/RC25), and BOOT CPU runs the self-test again: `?53 2 05 FF 00 0000` (also test 07 or 0A seen). The next VMB boot works.
- **Seen with:** the RA60 in normal mode with ATTACH (no RUN/STOP button, no sound: test `tests/rc25/c9b.ini`), so the code path is the one of plain Open SIMH; with SET RQ0 ONLINE (`c9.ini`) the same.
- **Possible cause:** the "available" attention message of the unit (UNIT_ATP, sent by `rq_tmrsvc` while the controller is up) is posted and interrupts while the console self-test runs. Not proven.
- **Also:** SET DEBUG output during the self-test causes `?53` errors by itself (timing); the RC25 tests switch debugging on after the self-test.
- **Found:** 2026-11-06 (RC25 tests).

- **Narrowed down 2026-11-07 (tests e1–e3, `tests/rc25/README.txt`):**
  - Reproduced with **plain Open SIMH 87eb7d5** (no patches) and **nothing attached**: BOOT DUA0 fails (`?4D DEVOFFLINE`), BOOT CPU → `?53 2 07`. So it has nothing to do with the RQ, ATTACH, the RUN/STOP button or the sound work.
  - The "available" attention theory is wrong: `rq_reset` clears UNIT_ATP on every unit.
  - The failing test number changes from run to run (05, 07, 0A), and the error also came on a *first* self-test when the host was busy (a build running at the same time) or with SET DEBUG output: it looks timing dependent, e.g. the KA655 self-test measuring the clock or SSC timers against SIMH's calibrated timer after the long VMB wait. `sysd_reset` does reset the SSC timers; the memory controller registers are only reset at power-up. Not proven.
  - Proposal: a separate investigation of the KA655 self-test timing (sim_timer calibration after BOOT CPU), outside the sound work. The console continues after the error and the next boot works.
### 23 — DONE — Bug: more than 32 spindles failed silently
- **Where:** `sim_sound.c`, `sim_spindle_init`: at most 32 spindles (`SND_MAX_SPINDLES`); for the next one `sp->uptr` stayed NULL without a message.
- **Effect:** such a drive's spindle state is always "stopped": an RA60 or RC25 would report offline even in the normal mode. A PDP-11 uses 20 at reset (HK 8, RL 4, RP 8), each RQ unit with a RUN/STOP button one more.
- **Fix (Claude's own code):** limit 64 and a message naming the unit; the RQ treats a drive without a spindle slot as one without realism (ready when attached). HK, RL, RP and RB set up their spindles at reset and stay below the limit. Sound patch v12.
- **Found and fixed:** 2026-11-06.

- **Final fix 2026-11-07 (decided by Ulli: "limits are given by the controller"):** spindles are registered in chunks of 32, each chunk with its own internal timer device (INT-SPINDLE, INT-SPINDLE2, ...), so a timer unit never moves while it is on the event queue; the timer finds its spindle through `up7`. Tested with 120 spindles on the PDP-11 (HK 8, RL 4, RP 8, 100 RA60 on RQ and RQB): the last one spins up. Sound patch v13.
### 24 — DONE (realism mode) — Minor: RC25 on any MSCP controller type
- **Where:** `PDP11/pdp11_rq.c`. The header (29-Jan-11, HUH) says the KLESI controller and port types are "not yet implemented"; `SET RQ KLESI` only sets the name and type.
- **Effect:** RC25/RCF25 units can be set on an RQDX3, UDA50, etc.; a real RC25 only works with its KLESI adapter. The KLESI specifics in Appendix A of the RC25 User Guide (command limit 14, vector 154, controller timeout 15 s, no NV bit in step 1, ...) are not simulated. VMS does not seem to care; untested.
- **Found:** 2026-11-06 (RC25 realism work).

- **Fixed for the realism mode 2026-11-07 (decided by Ulli: SET SOUND ENABLED means "as realistic as possible"; normal mode unchanged):**
  - RC25 drives only on a KLESI, which only connects RC25 drives, at most 4 units; RC25 pairs at even/odd units with even/odd unit numbers. SET SOUND ENABLED refuses other configurations; while sound is enabled SET RQ <ctype>, SET RQn <type>, SET RQn UNIT=, DRIVES= and ATTACH keep to the rules (details in the handover).
  - KLESI: controller timeout 15 s (SET CONTROLLER CHARACTERISTICS), ABORT only of queued commands.
  - Already as the RC25 before (no change needed): step 1 without NV, with DI and (Q-bus) QB; 15 command credits (= 14 + 1 immediate); DETERMINE ACCESS PATHS, COMPARE CONTROLLER DATA and FLUSH are NOPs; CSR 172150.
  - Not simulated (no effect in SIMH): error log format and limits, multi-host, the burst field, host bus error handling.
  - Sound patch v13.
### 25 — OPEN — Suspicious: RC25 cartridge unloaded while its fixed platter runs (normal mode)
- **Where:** `PDP11/pdp11_rq.c`, `rq_avl`: AVAILABLE with spin-down on a removable unit calls `sim_disk_unload` (detach). The RC25 is a removable type (`RQDF_RMV`), the RCF25 is not.
- **Observed (VMS V3.5 on the VAX-11/730, test N1):** with the system disk on the fixed platter DUA1, `DISMOUNT/UNLOAD DUA0:` (and plain `DISMOUNT`, which unloads by default) detaches the cartridge image while DUA1 keeps working.
- **Real drive:** the cartridge can only be ejected with the spindle stopped, and that would stop the fixed platter too; with the system disk on it, the drive would rather stay running (see the realism mode).
- **Realism mode (Claude's code):** sound patch v12 spun the drive down only if the other unit was not online (nothing stopped here); since sound patch v13 (decided by Ulli 2026-11-07) the whole drive stops as on the real one, the system disk DUA1 goes into mount verification until SET RQ0 ONLINE, and the cartridge stays attached (test R1 v3).
- **Effect:** none harmful; the old normal-mode behavior (an RC25 is unloaded like an RX50) is kept on purpose.
- **Found:** 2026-11-06 (RC25 VMS tests).

### 26 — OPEN — Suspicious: unit numbers held by disabled units
- **Where:** `PDP11/pdp11_rq.c`, `rq_set_plug`: the uniqueness check looks at all units of the controller, including the disabled ones (units 4–253). On the VAX their unit numbers equal their index, so `SET RQ0 UNIT=8` is refused with "Unit Plug 8 Already In Use on RQ8" although RQ8 is disabled.
- **Effect:** small unit numbers above 3 cannot be used on the VAX without first renumbering the disabled unit that holds them (on the PDP-11 the disabled units have numbers from 4 × 4 on, so it is less visible). For an RC25 drive numbered 8/9 (realism mode) the same applies; Ulli: hardly anyone would want that.
- **Found:** 2026-11-07 (RC25 realism rules, test c10).

### 27 — DONE — Bug: fixed tables of unit loops and run hooks
- **Where:** `sim_sound.c`, `SND_MAX_UNITLOOPS` 64 and `SND_MAX_RUNHOOKS` 8 (Claude's own code).
- **Problem:** every spindle registers a unit loop when it loads its sounds; beyond 64 the loop was silently not registered, so `SET SOUND POWER=ON` (or RUN/BOOT after a power off) did not bring its running sound back. The same pattern as case 23. The run hooks (one per device type) were far below 8, but fixed as well.
- **Fix:** both registries are arrays of pointers that grow as needed (nothing in use moves). Sound patch v14.
- **Found and fixed:** 2026-11-07 (default sounds work, while checking for more fixed limits after case 23).

### 28 — DONE — Bug: pdp11_dmc.c missing from the Visual Studio .vcxproj files
- **Where:** `Visual Studio Projects/MicroVAX2.vcxproj`, `VAX.vcxproj` and their `.vcxproj.filters` (Claude's own change in the DMV11 patch).
- **Problem:** the DMV11 patch (up to v10) added `pdp11_dmc.c` to `MicroVAX2.vcproj` and `VAX.vcproj` (Visual Studio 2008 format) only. Visual Studio 2010 and later use the `.vcxproj` files, which lacked it, while `vax630_syslist.c` and `vax_syslist.c` reference `dmv_dev`.
- **Effect:** a Visual Studio build of the MicroVAX II and MicroVAX 3900 simulators fails to link (unresolved `dmv_dev`). makefile and CMake builds were fine. Not tested on Windows (no Windows host here).
- **Fix:** `pdp11_dmc.c` in the `.vcxproj` and `.filters` of MicroVAX2 and VAX (and MicroVAX1 with the new DMV11). DMV11 patch v11.
- **Found and fixed:** 2026-11-07 (adding the DMV11 to the MicroVAX I).

### 29 — OPEN — Suspicious: the TS Initialize command rewinds the tape
- **Where:** `PDP11/pdp11_ts.c`, `ts_svc`, case `FNC_INIT`: it rewinds the tape (`sim_tape_rewind`, MOT set) and then sends the end packet like Get Status.
- **Manuals:** TS11 Technical Manual EK-OTS11-TM-003, 5.2.2.8: "The drive initialize command is a no-op. It results in a message update, just like a get status, if there are no microdiagnostic or runaway errors. However, if errors are displayed, the command does the same thing as a write to the TSSR." TSV05 User's Guide EK-TSV05-UG-003, 5.3.4.9: the same. A write to the TSSR (subsystem initialize) does rewind ("auto-load sequence and positions the tape at BOT", if on-line).
- **Effect:** a driver that issues the Initialize command loses its tape position. Not changed: in the realism mode the rewind takes its real time (as SIMH's semantics stand).
- **Found:** 2026-11-07 (TS11 sound work).

### 30 — OPEN — Suspicious: Write Data Retry mode
- **Where:** `PDP11/pdp11_ts.c`, `ts_svc`, case `FNC_WRIT` (mode 1 = "rewrite": space reverse, write) and `fnc_mod[5] = 2` (modes 0 and 1 allowed).
- **Manuals:** Write command modes 0000 = write data, 0010 = write data retry (space reverse, erase, write data) in both manuals (TS11 table 5-10 and figure 5-11, TSV05 table 5-2 and the mode table). Write Tape Mark Retry is mode 0010 in SIMH and the manuals alike.
- **Effect:** a driver's write retry (mode 2) gets an illegal command (ILC) termination; mode 1 is accepted though undefined. Drivers retry only after errors, which SIMH never produces, so this is hardly ever seen.
- **Found:** 2026-11-07.

### 31 — OPEN — Suspicious: Control mode 4 (rewind) on the TS11
- **Where:** `PDP11/pdp11_ts.c`, `ts_svc`, case `FNC_CTL` mode 4 (and `fnc_mod[012] = 5`).
- **Manuals:** the TS11 manual lists Control modes 0 (message buffer release), 1 (rewind and unload) and 2 (clean) only. The TSV05 manual adds mode 4, "rewind with immediate interrupt" (marked "extended function, not part of the TS11/TS04 repertoire"): the end packet comes at the start of the rewind, a following motion command waits for BOT.
- **Effect:** the Unibus TS11 accepts a TSV05-only function. Not changed; in the realism mode mode 4 behaves as the TSV05 function on both buses (ends at once, the tape rewinds, a following motion command waits for BOT).
- **Found:** 2026-11-07.

### 32 — OPEN — Suspicious: Rewind and Unload bypasses ts_detach
- **Where:** `PDP11/pdp11_ts.c`, `ts_svc`, case `FNC_CTL` mode 1: `sim_tape_detach (uptr)` instead of `ts_detach`.
- **Effect:** in the normal mode the drive goes off-line without the attention message that `ts_detach` sends (when attention interrupts are enabled); the same pattern as case 10 on the RQ. In the realism mode the command no longer detaches (the tape rewinds and unloads, the transport goes off-line with an attention message).
- **Found:** 2026-11-07.

### 33 — OPEN — Minor: ts_description says "TSV11/TSV05"
- **Where:** `PDP11/pdp11_ts.c`, `ts_description`: the Qbus text is "TSV11/TSV05 magnetic tape controller". There is no TSV11; the Qbus subsystem is the TSV05 (M7196 controller, TS05 transport).
- **Found:** 2026-11-07.

### 34 — OPEN — Minor: HELP TS mentions options the device does not have
- **Where:** `PDP11/pdp11_ts.c`, `ts_help`: "The type options can be used only when a unit is not attached to a file.  The bad block option can be used only when a unit is attached to a file." The TS device has neither (text from a disk driver).
- **Found:** 2026-11-07.

### 35 — OPEN — Suspicious: SET of unit modifiers on a single-unit device
- **Where:** `scp.c`, `set_cmd`: a device-level SET (`SET TS LOCKED`) refuses an extended modifier that has only `MTAB_VUN` (`((lvl & mptr->mask) & ~MTAB_XTD) == 0` → "Invalid argument"), while the help listing (`fprint_set_help_ex`) prints such modifiers as `set TS LOCKED` for single-unit devices. `SET TS0 LOCKED` works.
- **Effect:** the documented command fails for TS LOCKED, WRITEENABLED, FORMAT, CAPACITY (and the same pattern on other single-unit devices). The new TS panel modifiers (LOAD, ONLINE, OFFLINE, REWIND, UNLOAD, REEL, TRANSPORT) have `MTAB_VDV` as well, so `SET TS ONLINE` works.
- **Found:** 2026-11-07 (TS11 sound work, first panel test).

### 36 — OPEN — Minor: TS debug output names the registers the wrong way round
- **Where:** `PDP11/pdp11_ts.c`, `ts_rd` and `ts_wr`: `((PA >> 1) & 01) ? "TSBA" : "TSSR"` (and `"TSDB" : "TSSR"`), while the code below decodes PA<1> = 0 as TSBA/TSDB and PA<1> = 1 as TSSR.
- **Effect:** `SET TS DEBUG=REG` shows a write of the command pointer (TSDB, base address) as "TSSR" and a read of TSSR as "TSBA"; misleading when tracing a driver. Behaviour is not affected.
- **Found:** 2026-11-07 (TS realism debugging under VMS V4.7).
