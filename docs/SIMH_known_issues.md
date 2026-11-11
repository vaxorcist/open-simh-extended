<!-- open-simh-extended/docs/SIMH_known_issues.md  v20 (v20: case 47 (fixed), VMS MOUNT of a double density RX02 diskette, and case 48; v19: cases 45 (fixed) and 46, RX02 Set Media Density; v18: cases 43 and 44, RX/RY Initialize; summary rows 41-42 added; v17: case 42, RX18 tracks per group; v16: case 41 DONE, SIMH is right; v15: case 41, RA80 capacity; v14: case 38 widened, cases 39 and 40; v13: case 38, TQ position beyond the recorded data stops the simulator; v12: case 37, asynchronous tape I/O and sim_activate; v11: cases 29-36, TS11/TSV05; v10: case 28; v9: case 27; v8: cases 18, 23, 24 done, 22 narrowed down, case 26; v7: case 25; v6: cases 22-24, case 18 confirmed by the RC25 User Guide) -->
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
| 37 | FIXED (patch 5) | Pitfall | `scp.c` AIO migration, `sim_sound_activate_us` | SCP activates a unit before calling its asynchronous I/O completion callback, so `sim_activate` in the callback is ignored; `sim_activate_abs` replaces the activation |
| 38 | FIXED (patch 5) | Bug | `PDP11/pdp11_tq.c` `tq_svc`, OP_POS | Spacing beyond the last record (end of medium) or into an invalid record (invalid record length, format error) returns SCPE_IOERR and stops the simulator, e.g. a VMS DIRECTORY of a tape with an interrupted BACKUP |
| 39 | FIXED (patch 5) | Bug | `sim_tape.c` `sim_tape_rdlntf` | After an invalid record length the position is set 2 GB too far back when the bad record marker has bit 31 set; the attach scan reports "2149429608 bytes of unexamined data" for a 2 MB image |
| 40 | FIXED (patch 5) | Bug | `sim_timer.c` `sim_throt_svc` | Throttle without warm-up (THROT_DELAY 0, set by the sound realism mode): a 10 ms measurement while idling or starting up disables the throttle ("Host CPU is too slow"); the too-fast recalibration loops forever without a measured peak rate |
| 41 | DONE (no change) | Question | `PDP11/pdp11_rq.c` `RA80_LBN` | RA80 capacity 237,212 blocks in SIMH, 236,964 in the User Guide: SIMH matches a real RA80 |
| 42 | OPEN | Suspicious | `PDP11/pdp11_rq.c` `RX18_TPG` | RX18 "tracks per group" is `RD33_SURF` (7), a copy/paste slip |
| 43 | OPEN | Suspicious | `PDP11/pdp11_ry.c` `ry_svc` (INIT_COMPLETE) | Initialize reads track 1 sector 1 of drive 0 with single-density size and offset, also from a double-density diskette |
| 44 | OPEN | Suspicious | `PDP11/pdp11_rx.c`, `pdp11_ry.c` Initialize | A drive without a diskette is reported as "failed to see home" (codes 0010/0020); the RX01 without a diskette in drive 0 also does not set Initialize Done |
| 45 | FIXED (patch 5) | Bug | `PDP11/pdp11_ry.c` `ry_svc` (SDXFR) | Set Media Density on a drive without a diskette writes through a NULL buffer: SIMH crashes (segmentation fault) |
| 46 | OPEN | Limitation | `scp.c` `detach_unit` with `PDP11/pdp11_ry.c` | A new RX02 image formatted with Set Media Density stays an empty file (the zeroed buffer equals the original, so nothing is written); its density is not recorded |
| 47 | FIXED (patch 5) | Bug | `PDP11/pdp11_ry.c` Read Status, `ry_done` | Read Status never sets the density error, so VMS DYDRIVER reads a double density diskette in single density: MOUNT fails with %MOUNT-F-DRVERR; error bits left in RX2ES by an earlier function also set RX2CS Error on the next Read Status/Read Error Code |
| 48 | OPEN | Suspicious | `PDP11/pdp11_ry.c` `ry_svc` (ESXFR) | Extended status word 4 bit 5 (0040) is set when the drive has a diskette; the RX02 manual defines it as the head load bit |

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

### 37 — FIXED IN PATCH 5 (sim_sound) — Pitfall: sim_activate from an asynchronous I/O completion callback is ignored
- **Where:** `scp.c` / `sim_defs.h`, the AIO migration (`AIO_UPDATE_QUEUE`): for a completed asynchronous `sim_tape`/`sim_disk` operation SCP first activates the unit (`a_activate_call`, usually with delay 0) and only then calls the device's completion callback (e.g. `tq_io_complete`). A plain `sim_activate` inside the callback finds the unit already queued and does nothing.
- **Effect:** a device cannot replace the completion delay from its callback with `sim_activate`; the bottom half runs at once. In the sound realism mode of the TK50 every tape command ended immediately (rewinds took no time, the guest did not wait for the tape), while the transport model's clock ran ahead.
- **Fix (patch 5):** `sim_sound_activate_us` uses `sim_activate_abs`, which replaces an existing activation. The other callers (RL, RP, RQ, HK, RB) call it in their service or command-start paths, where the unit is not queued, so nothing changes for them. Plain SIMH is not affected (its devices do not reschedule from the callback); worth knowing for any future device that does.
- **Found:** 2026-11-08 (TK50 realism mode under VMS V4.7, with `SET TQ DEBUG=TRACE`).

### 38 — FIXED IN PATCH 5 — Bug: a TQ position beyond the recorded data stops the simulator
- **Where:** `PDP11/pdp11_tq.c`, `tq_svc`, OP_POS bottom half: every `sim_tape_position` status other than OK, TMK, BOT and LEOT goes to `tq_mot_err`, which returns `SCPE_IOERR`. `MTSE_EOM` (spacing ran off the last record) is one of them.
- **Effect:** "%SIM-ERROR: Unexpected internal error while processing event for TQ0 which returned 66 - I/O error"; the simulator stops. Seen by Ulli on the MicroVAX II (VMS V4.7): MOUNT/OVERRIDE=IDENTIFICATION, then DIRECTORY of a TK50 whose BACKUP had been interrupted with Ctrl/Y (no closing tape marks: DIRECTORY skips 2 files and runs beyond the data). Plain Open SIMH 87eb7d5 has the same code; the normal mode fails the same way.
- **Fix (patch 5):** MTSE_EOM ends the command normally with the mapped status (ST_DAT, serious exception, position lost), as a read there does. 2026-11-09: widened to MTSE_INVRL and MTSE_FMT (spacing into the remains of an older, longer recording behind the data, which SIMH does not erase; Ulli's `test-interrupt.tap`); only MTSE_IOERR and MTSE_RECE (as in the read path) still go to `tq_mot_err`. VMS then reports `-SYSTEM-F-TAPEPOSLOST, magnetic tape position lost` and lists the save set; the simulator keeps running.
- **Found:** 2026-11-08 (Ulli, TK50 tests).

### 39 — FIXED IN PATCH 5 — Bug: sim_tape position 2 GB off after an invalid record length
- **Where:** `sim_tape.c`, forward record read, SIMH format: the position is advanced by the masked, padded record length (`MTR_L`, without the error flag bit 31); on a mismatch of the trailing length (MTSE_INVRL) it was set back by the unmasked marker value without the padding (`pos -= 4 + *bc + 4`).
- **Effect:** for a garbage marker with bit 31 set (MTR_ERF) the position ends 2^31 bytes before the record. The attach scan prints e.g. "After processing 184400 bytes of tape data … invalid record length … 2149429608 bytes of unexamined data remain" for a 2,130,360 byte image (2^31 + the true 1,945,960 bytes). The guest's next command after such an error starts from that position as well. Plain Open SIMH 87eb7d5 has the same code.
- **Fix (patch 5):** back to the leading record marker, `pos = saved_pos - sizeof (t_mtrlnt)` (position not updated, as the PNU flag says). Checked with a synthetic image (an interrupted BACKUP and a marker 0x80001000 behind it): old build "2147995652 bytes … remain", fixed "512004"; VMS MOUNT/OVERRIDE=IDENTIFICATION + DIRECTORY: TAPEPOSLOST, then the save set listed, in the normal and the realism mode.
- **Found:** 2026-11-09 (Ulli: "the tap file is 2 MB only").

### 40 — FIXED IN PATCH 5 — Bug: throttle without warm-up disabled or looping
- **Where:** `sim_timer.c`, `sim_throt_svc`. The sound realism mode sets THROT_DELAY 0 (realistic speed from power-on; patch 5 already took a fresh start reading in that case).
- **Effect 1:** the only measurement covered about 10 ms. When the CPU idled in it (console ROM at `>>>`) or the host was busy starting (SDL, threads), the rate came out low: "Host CPU is too slow to simulate 450,000 instructions per second … Throttling disabled." on fast hosts (Ulli's MicroVAX II, 2026-11-09; also sporadically in our tests with several simulators).
- **Effect 2:** when a later 10 s recalibration found the CPU much too fast, the restart loop computed the wait from `sim_throt_peak_cps`, which no warm-up ever set (0): the wait stays 0 and the loop never ends (a test run wrote 20 GB of debug output).
- **Fix (patch 5):** without a warm-up the measurement lasts at least 100 ms; one that idled, was below the wanted rate or below 1.25 times it is repeated with a longer window (at most 10 s of the wanted rate); a warning only after 30 repetitions without idling, and the throttle keeps trying. The measured rate feeds the peak; the restart loop uses the current rate when no peak above the wanted one exists. Checked: four MicroVAX II boots in parallel, measurements of 244K and 286K repeated, throttle 450K reached, no warning.
- **Found:** 2026-11-09.

### 41 — DONE (no change needed) — RA80 capacity: 237,212 blocks in SIMH, 236,964 in the User Guide
- **Where:** `PDP11/pdp11_rq.c`, `RA80_LBN 237212`, with the old comment `/* 31*14*546 */` (31 × 14 × 546 is 236,964).
- **Published:** RA80 User Guide EK-0RA80-UG-001, Table 1-1: 31 sectors, 14 heads, 561 tracks per head, 546 logical cylinders, **236,964 LBNs**; the DEC RA60/RA80/RA81 brochure (Aug 1982): 31 user sectors, 14 tracks per logical cylinder, 546 logical cylinders.
- **Real drives:** a real RA80 reports **237,212** LBNs in its MSCP unit status (Usenet comp.periphs: "the RA80 reports 237212 … 237212-236964 == 248 == 31*8"); a 4.3BSD site used 237,212 blocks for the whole disk and read the end without problems (comp.bugs.4bsd, Dec 1986, "4.3/newfs/RA80 sizes"). The 4.2/4.3BSD disktab (nc#559, 242,606 blocks) used the R80 Massbus geometry and was wrong for the RA80 (comp.unix.wizards, Jul 1986, "4.3 UDA50 problem with RA80").
- **Same HDA:** the R80 drive is the RM80 on the Massbus, the RA80 on SDI and the RB80 on the RB730 (Computer History Wiki, R80): 561 tracks per head; RM80/RB80 in SIMH 31 × 14 × 559 = 242,606 blocks (no DSA replacement and table areas), the RA80 546 logical cylinders + 8 tracks = 237,212 LBNs.
- **Result:** SIMH matches the real drive; no change. The comment now says `31*14*546 + 248, as a real RA80 reports`, and the drive table comment explains the difference (sound patch v24).
- **Found:** 2026-11-10; **closed:** 2026-11-10 (Ulli: "try to find out, e.g. using RM80 RB80 R80 information").

### 42 — OPEN — Suspicious: RX18 "tracks per group" copied from the RD33
- **Where:** `PDP11/pdp11_rq.c`, `#define RX18_TPG RD33_SURF` (= 7); the drive table comment above says `tpg 9`, the RX18 has 1 surface (`RX18_SURF 1`).
- **Effect:** GET UNIT STATUS reports 7 tracks per group for the RX18. Every other drive uses its own value (mostly its `_SURF`); 7 matches neither the RX18's surfaces (1) nor the comment (9). Likely a copy/paste slip from the RD33 entry added in the same commit (2396fd03, Nov 2023). Harmless for most host software, which ignores the group size of a diskette.
- **Not changed:** the real value is not documented (the RX18 is the VT180 "Robin" RX180 diskette, which the MSCP specification only lists as possible); `RX18_SURF` (1) would be consistent. Decision for Ulli.
- **Found:** 2026-11-10 (question about the RX18).

### 43 — OPEN — Suspicious: RX02 Initialize reads the boot sector in single density
- **Where:** `PDP11/pdp11_ry.c`, `ry_svc`, case `INIT_COMPLETE`. `ry_reset` clears `ry_csr`, so `bps` in `ry_svc` is `RX_NUMBY` (128) and `CALC_DA (1, 1, bps)` computes the single-density offset 26 × 128 = 3328.
- **Manual:** RX02 Technical Manual EK-0RX02-TM-001, 4.3.2.1 (RX2CS bit 14): after a successful Initialize the RX02 "will also read sector 1 of track 1 on drive 0 into the buffer" — in the diskette's density.
- **Effect:** with a double-density diskette in drive 0 the buffer gets 128 bytes from track 0 sector 14 (offset 3328 of a 256-byte-sector image) instead of the 256 bytes of track 1 sector 1. Only software that empties the buffer right after Initialize without a Read Sector sees it (SIMH's own bootstrap issues Read Sector).
- **Probable intent:** use drive 0's density (`UNIT_DEN`) for `bps` in INIT_COMPLETE.
- **Found:** 2026-11-11 (floppy sound preparation).

### 44 — OPEN — Suspicious: Initialize without a diskette reports "failed to see home"
- **Where:** `PDP11/pdp11_rx.c` (`rx_reset`, `rx_svc` INIT_COMPLETE) and `PDP11/pdp11_ry.c` (`ry_reset`, `ry_svc` INIT_COMPLETE).
- **Code:** drive 0 without a diskette → error code 0010 ("Drive 0 failed to see home on Initialize"); drive 1 without a diskette → error code 0020 set *after* `rx_done`/`ry_done` (no error bit). On the RX01 the drive-0 case calls `rx_done (0, 0010)` without `RXES_ID`, so Initialize Done is not set; the RX02 sets `RYES_ID`.
- **Real drive:** the home (track 0) sensor works without a diskette, so the heads do find home; what fails is the read of track 1 sector 1 (no index pulses, drive not ready). The RX211 error code list has codes for that kind of failure (0110 "More than 40 µs and no SEP clock seen", 0120 "A preamble could not be found"); which one a real RX02 gives without a diskette is not documented and not verified on real hardware. SIMH itself uses 0110 for a Read/Write Sector on an empty drive.
- **Effect:** small: only a program that reads the error code after Initialize sees a misleading code; RT-11 and VMS just look at drive ready.
- **Found:** 2026-11-11 (floppy sound preparation).


### 45 — FIXED IN PATCH 5 — Bug: RX02 Set Media Density on an empty drive crashes SIMH
- **Where:** `PDP11/pdp11_ry.c`, `ry_svc`, states SDCNF/SDXFR: the erase loop `for (i = 0; i < capac; i++) fbuf[i] = 0;` runs without checking `UNIT_BUF`; `capac` is set for an unattached unit (`RY_SIZE` in the unit definition), `filebuf` is NULL.
- **Effect:** a guest that issues Set Media Density (function 4, confirmed with "I") on a drive without a diskette kills the simulator with a segmentation fault. Reproduced with plain Open SIMH (the unchanged sound branch): `tests/rx02/sd0.ini` (PDP-11/70) exit status 139.
- **Fix (patch 5 v27):** without a diskette the command ends with error 0110 (as a Read Sector on an empty drive does), in both modes; in the realism mode after the time a real RX02 needs to give up (head load, two revolutions). `tests/rx02/sd1.ini`: RYCS 104050, RYERR 110.
- **Found and fixed:** 2026-11-11 (RX02 realism work).

### 46 — OPEN — Limitation: a freshly formatted RX02 image stays an empty file
- **Where:** `scp.c`, `detach_unit`: a buffered unit (`UNIT_BUFABLE`/`UNIT_MUSTBUF`) is written back only if the buffer differs from the copy taken at ATTACH. A new image (ATTACH -N) is all zeros, and Set Media Density writes zeros, so nothing is written: the file stays 0 bytes. The density chosen by Set Media Density (`UNIT_DEN`) is not stored anywhere; the next ATTACH autosizes a 0-byte file to the unit's current setting.
- **Effect:** after formatting a new diskette in single density and re-attaching it, SIMH may treat it as double density (or the reverse) until data has been written. Same in plain Open SIMH (`tests/rx02/sd2.ini`, old and new build).
- **Possible fix:** write the whole image when Set Media Density ran (e.g. mark the buffer as changed), so the file gets its full size, which autosize recognizes.
- **Found:** 2026-11-11 (RX02 realism work).

### 47 — FIXED IN PATCH 5 — Bug: VMS cannot mount a double density RX02 diskette (MOUNT-F-DRVERR)
- **Where:** `PDP11/pdp11_ry.c`, Read Status (function 5, state CMD_COMPLETE) and `ry_done`.
- **How VMS finds the density:** DYDRIVER V4.7 (disassembled from the running system with SDA and the SIMH examine command) issues at PACKACK (MOUNT) a **single density** Read Status and switches the unit to double density only when RX2ES bit 4 (density error) comes back set; bit 1 then selects double sided. This matches the RX02 manual 4.1.4.6: Read Status loads the head, reads the first data mark and "possibly sets the density error bit".
- **Bug:** the RY never set the density error on Read Status, so VMS read the home block in single density, got density error 0240 on every retry and gave up: `INITIALIZE/DENSITY=DOUBLE DYA0: RX02T` worked (it sets the density explicitly), `MOUNT DYA0: RX02T` failed with `%MOUNT-F-DRVERR, fatal drive error`. Same in plain Open SIMH (normal mode). A second fault made it worse: `ry_done` set RX2CS Error whenever error bits were in RX2ES, and only Read/Write cleared them, so after the density error the following Read Error Code and Read Status also ended with Error.
- **Fix (patch 5 v29):** Read Status starts with fresh error bits and sets the density error when a diskette is in the drive and its density differs from the function density (RX2CS Error is not asserted for it). RX2CS Error now reports the errors of the current function only. Fill/Empty Buffer and Set Media Density clear RX2ES at initiation (manual 4.3.3.1, 4.3.3.2, 4.3.3.5).
- **Test:** `tests/rx02/vms2.ini` (normal) and `tests/rx02/vms3.ini` (realism) with VMS V4.7 on the VAX-11/750: INITIALIZE double (DYA0) and single (DYA1), MOUNT, COPY LOGINOUT.EXE, DIRECTORY, DISMOUNT; both mounts succeed, no errors. RT-11 tests n1, r1, t1, t2, sd0, sd1 unchanged.
- **Found and fixed:** 2026-11-11 (Ulli's VMS V4.7 test of the RX02 realism work).

### 48 — OPEN — Suspicious: extended status word 4 bit 5 is "diskette in drive", not "head loaded"
- **Where:** `PDP11/pdp11_ry.c`, `ry_svc` state ESXFR: `estat[6]` bit 0040 is set from `UNIT_ATT` of the selected unit.
- **Manual:** RX02 manual 4.3.3.8, Read Error Code: Word 4 <5> is the Head Load Bit (<7> unit select, <6>/<4> drive densities, <0> density of the Read Error Code command).
- **Effect:** none seen (VMS DYDRIVER and RT-11 do not use the bit for decisions as far as tested); diagnostics that check it may complain. In the realism mode the real head state is known (`SND_FLOPPY`) and could be reported.
- **Found:** 2026-11-11 (diagnosis of case 47).
