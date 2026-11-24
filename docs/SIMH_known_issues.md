<!-- open-simh-extended/docs/SIMH_known_issues.md  v73 (v73: case 117, the realism mode's waits at the prompt could not be interrupted, FIXED in patch 26; v72: case 116 FIXED in patch 25 (sim_tmxr.c: closed incoming connections discarded at once), case 93 probably the same cause; v71: case 116 newly analysed: crossing PEER connections, not the receive path; v70: cases 115 (DMC transmit buffer too many crashed SIMH, FIXED in patch 24) and 116, found with BOOT DMCn; v69: case 109, patch 23 v2 adds SIMH's BOOT XU and a HELP XU Booting topic; v68: case 109 FIXED (patch 23, DEUNA/DELUA BOOT), cases 111-114 (XU STOP/START, counter length, load server address, SET XU MAC), found with DEC's TERUNA and fixed in patch 23; v67: case 109, option 1 decided, UNA loaders found, design doc; v66: case 109, the manuals' description of BOOT; v65: cases 106, 107, 108 and 110 FIXED (patch 22, Ulli's decisions), case 109 waits for the DEUNA/DELUA manuals (uploaded), what the manuals say about BOOT; v64: cases 106-110 (11/60-70 hit/miss register, console DL11 maintenance loopback, console character lost at RESET, DEUNA/DELUA BOOT port command, TTI debug of a NUL aborts), found running DEC's M9312 console/diagnostic and boot ROMs (tests/m9312_roms); heading of case 103 corrected to FIXED; v63: case 103 FIXED (Ulli: keep DTR at master clear), case 105 (maintenance mode request started DDCMP on a connected line, fixed with it); v62: cases 101-104 (HELP BOOT DMP description, DMC connection poll starved by RESET, master clear drops DTR, ROM device byte order), found while booting DEC's M9312 DECnet ROM; v61: case 99 FIXED (patch 21 v2, DMC11 switch packs from the line unit manuals and the DMC11 diagnostics), case 100: DMC11 INIT; v60: case 90 FIXED (patch 21), cases 99 and 100, DMP11/DMR11/DMC11 switches and boot; v59: case 97 FIXED (patch 19, RQ XTIME 1000), case 98; v58: case 97 cause found: a disk read that completes too fast; v57: case 97: no hang with SET CPU IDLE=VMS, also when node B boots after A has logged in; v56: case 95 effect seen with remwrt running (patch 18 v3); v55: case 97, two MicroVAX II booting VMS V4.7 at once sometimes hang after the date; case 94 may be related; v54: cases 92-96, VMS V4.7 as load host for a PDP-11 over the DMV11 (patch 18 v2); v53: cases 88-91, DMV11 switch packs and MOP boot work (patch 18); v52: case 87, TS realism mode ON-LINE switch not visible (patch 17); v51: case 86, circuit fault when DMB-0 is turned off (SIDRIVER, V5.5 and the V4.7 backport); summary rows 84 and 85 in number order; v50: case 85, the fix done the AUTOGEN way (Ulli); v49: case 85, TMSCPLOAD SERVE_ERR at VMS V5.5 startup (VMS parameters, not SIMH), remark in case 72 corrected; v48: case 84 FIXED, DMB32 sync line CTS follows RTS (patch 16); v47: case 83, SEND string overflow; case 84, DMB32 sync line as DDCMP tributary under VMS V5.5; v46: cases 81 and 82, KA820 clock IPL and IPIR (patch 15); v45: cases 78-80, VAXBI configuration and memory boards (patch 14); v44: cases 76 and 77, realism mode checks and the KLESI-B TU81 rule; the realism mode is now switched with SET REALISM ENABLED/DISABLED (SET SOUND ENABLED/DISABLED, named in the older cases, still works); v43: cases 74 and 75, DEBNA with VMS V5.5; v42: case 73, KDB50 per its microcode listing; v41: case 72 answered by the KDB50 microcode listing; v40: case 72 open question, tests; v39: case 9 FIXED, cases 71 and 72, VMS V5.5 boot from the KDB50; v38: case 70, KLESI-B work; v37: case 69 stays open (Ulli); v36: case 69, VMS V4.7's VMB.EXE has no DEBNA driver either; v35: case 69, no network boot on the VAX 8200 (DEBNA); v34: case 68 FIXED (patch 9 v2), SDI rule in the realism mode; v33: case 68, SDI controllers accept non-SDI drives in the realism mode; v32: cases 66 and 67 (fixed in patch 8, DMB32 work); v31: case 65 DONE, the second KA820 runs (patch 7); v30: case 65, VAX 8200 second CPU (KA1) never runs; v29: case 64 (fixed), BOOT CS unit on the VAX-11/780; v28: case 63 (fixed), realism throttle measurement and VMS delay loops; v27: case 62 checked by Ulli; v26: case 62 (fixed), BOOT CS on the VAX 8200; v25: cases 55-61, TU58 realism work; v24: case 54 (fixed), VAX 8200 console floppy physical track 0; v23: cases 51 (fixed), 52 and 53, VAX 8600 console RL02; v22: case 50, PDP-11 RQ controller type after SET CPU; v21: case 49, RY boot with the RX enabled; v20: case 47 (fixed), VMS MOUNT of a double density RX02 diskette, and case 48; v19: cases 45 (fixed) and 46, RX02 Set Media Density; v18: cases 43 and 44, RX/RY Initialize; summary rows 41-42 added; v17: case 42, RX18 tracks per group; v16: case 41 DONE, SIMH is right; v15: case 41, RA80 capacity; v14: case 38 widened, cases 39 and 40; v13: case 38, TQ position beyond the recorded data stops the simulator; v12: case 37, asynchronous tape I/O and sim_activate; v11: cases 29-36, TS11/TSV05; v10: case 28; v9: case 27; v8: cases 18, 23, 24 done, 22 narrowed down, case 26; v7: case 25; v6: cases 22-24, case 18 confirmed by the RC25 User Guide) -->
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
| 9 | FIXED | Bug | `PDP11/pdp11_rq.c` `rq_svc` (COMPARE) | Wrong bus address stored after a memory error during Compare |
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
| 49 | OPEN | Pitfall | `PDP11/pdp11_io_lib.c` autoconfig, `pdp11_ry.c` `ry_boot`, RT-11 boot block | With RX and RY both enabled the RX gets CSR 177170 and the RY a floating CSR (160270); BOOT RY0 loads the RT-11 boot block, which hardcodes 177170 and so talks to the empty RX: the simulator stops with "Unexpected I/O error … RX0 - Unit not attached" (vanilla 87eb7d5 the same) |
| 50 | OPEN | Suspicious | `PDP11/pdp11_rq.c` `rq_reset` (DEFAULT_CTYPE), PDP-11 `SET CPU` | The default MSCP controller type is resolved at the first reset with the default CPU (11/73, Qbus): after `SET CPU 11/70` the RQ stays an RQDX3, a Qbus controller on a Unibus machine (plain Open SIMH too) |
| 51 | FIXED (patch 5) | Bug | `sim_disk.c` `sim_disk_pdp11_bad_block` (path without a sim_disk context), `VAX/vax860_stddev.c` `rlcs_attach` | A new VAX 8600 console RL02 image created with ATTACH -Y gets its bad block table in the middle of the pack (byte 5,237,760 of 10,485,760; the ten copies overlap): the capacity in 16-bit words is used as a byte offset |
| 52 | OPEN | Pitfall | `scp.c` `get_yn` with `pdp11_bad_block` / `sim_disk_attach` | `ATTACH -N` (new file) also answers the "Overwrite last track?" question with No (switch N), so a new RL/RK/RP/console RL02 image never gets a bad block table that way; ATTACH -Y or an interactive Y does |
| 53 | OPEN | Suspicious | `VAX/vax860_stddev.c` `rlcs_svc` (RL_STATUS, RL_MP) | The console RL02's drive status never has the RL02 bit (0200) nor the volume check bit (kept in `STAT` but not reported) |
| 54 | FIXED (patch 5) | Bug | `VAX/vax820_stddev.c` `CALC_SC`, `CALC_DA` | VAX 8200 console RX50: a Read/Write Sector of physical track 0 (the last logical track, blocks 790-799) computes a negative disk address and SIMH dies with a segmentation fault (VMS @SYS$UPDATE:STABACKIT CSA1:, Ulli) |
| 55 | FIXED (patch 5) | Bug | `sim_sound.c` `sim_sound_rt_us` | The realism clock divided all instructions since the last start by the current rate estimate: on unthrottled simulators it jumped by seconds after a long run (TU58 records taken as passed: 13-20 s searches) |
| 56 | FIXED (patch 5) | Bug | `PDP11/pdp11_td.c` `td_process_packet` | A TU58 command for a unit number the controller does not have (2-255) schedules `ctlr->uptr + unitno`: another controller's unit, or memory outside the unit array of the VAX console TD (1 unit) |
| 57 | FIXED (patch 5) | Suspicious | `PDP11/pdp11_td.c` `td_svc` (READ1, WRITE2) | A cartridge removed (DETACH) during a TU58 transfer leaves `filebuf` NULL; the next data packet is built from / written to the NULL buffer |
| 58 | OPEN | Bug | `VAX/vax730_stddev.c`, `vax750_stddev.c` `td_reg` | Every register of the console TD points to the same dummy `td_regval`: EXAMINE shows nothing real, DEPOSIT (e.g. CTIME, XTIME) changes nothing |
| 59 | OPEN | Limitation | `PDP11/pdp11_td.c` `td_process_packet` | The TU58 command modifier byte is ignored: the special address mode (record numbers 0-2047 instead of blocks) is not supported, write/verify only gets its time in the realism mode |
| 60 | OPEN | Limitation | `VAX/vax730_sys.c` BOOT TD with the internal VMB | A VAX-11/730 console cartridge (RT-11 volume, e.g. the diagnostic supervisor) cannot be booted: VMB looks for a standalone boot file, `%BOOT-F-Unable to locate BOOT file` |
| 61 | OPEN | Pitfall | RT-11 V5.7 with the PDP-11 TDC | RT-11 V5.7 installs the serial printer handler LS at CSR 176500, the TDC's default address |
| 62 | FIXED (patch 3) | Bug | `VAX/vax820_bi.c` `vax820_boot_parse` | BOOT CS0 on the VAX 8200 fails with "Invalid argument": the parser rejects devices without a DIB, although CS is in the boot table |
| 63 | FIXED (patch 5) | Bug | `sim_timer.c` `sim_throt_svc` (INIT, no warm-up) | The realism throttle measured the host by running the guest at full speed; VMS calibrated its delay loops meanwhile: TIMEWAIT 10-100 times too long (SYSGEN CONNECT CONSOLE on the 750 hung for minutes) |
| 64 | FIXED (patch 5) | Bug | `VAX/vax780_sbi.c` `vax780_boot_parse` | BOOT CS passes unit 0 in R3; the VMS console floppy driver takes its console ID from it: standalone VMS writes floppy commands with ID 8 and data to the terminal (garbage after the banner, loops) |
| 65 | DONE (patch 7) | Limitation | `VAX/vax820_ka.c` (KA1, `VAX_MP`), VAX CPU core | VAX 8200: `SET KA1 ENABLE` only adds the second KA820 as a BI node; it never executes instructions, so VMS cannot start it (`START/CPU`: %SYSTEM-F-DEVOFFLINE) |
| 66 | FIXED (patch 8) | Bug | `VAX/vax_bi.h` `BICSR_HES` | The hard error summary bit is defined as 0x4000, the same as the soft error summary `BICSR_SES`; it is bit 15 (0x8000). Not used anywhere yet |
| 67 | FIXED (patch 8) | Limitation | `VAX/vax820_bi.c` `get_vector` | Every BI node other than the DWBUA got a fixed vector `100 + level*40 + node*4`; a BI node that supplies its own vector (DMB32: `100 + type*40 + node*4`, type = function) could not be simulated |
| 68 | FIXED (patch 9) | Suspicious | `PDP11/pdp11_rq.c` `rq_rlz_rules` (realism mode) | The SDI rule only keeps SDI drives (RA, ESE) off non-SDI controllers; an SDI controller (UDA50, UDA50A, KDA50, KDB50) accepts RD, RX33 and other non-SDI drives, which a real one cannot connect |
| 69 | OPEN | Limitation | `VAX/vax820_bi.c` boot, built-in VMB of the `vax8200` | No network boot: the built-in VMB has no driver for the DEBNA (VMB device code 98, `BTD$K_AIE_NI`) and halts; a real 8200 boots with its console EEPROM boot program ET (`B ETx0`). `BOOT ET` is not offered |
| 70 | OPEN | Limitation | `VAX/vax820_uba.c` `uba_enbdis`, SCP `SET dev ENABLE` | After SET UBA DISABLE a Unibus device can still be enabled by hand; it has no adapter and SET UBA ENABLE does not know it |
| 71 | FIXED | Bug | `PDP11/pdp11_rq.c` `rq_getpkt`, `rq_putpkt` (KDB50) | Ring descriptor addresses masked to 22 bits (Qbus width) on the VAXBI: VMS V5.5 cannot boot from the KDB50 (EXECINIT error loading PROCESS_MANAGEMENT.EXE, reboot loop) |
| 72 | FIXED | Bug | `PDP11/pdp11_rq.c` `kdb_wr` (KDB50) | A write to IP did not poll: VMS V5.5's PUDRIVER starts the port by writing IP and hung, resetting the node every 50 s |
| 73 | FIXED | Suspicious | `PDP11/pdp11_rq.c` KDB50 | DTYPE microcode byte, reported versions and the poll after each ring interrupt differed from the KDB50 microcode |
| 74 | FIXED | Bug | `VAX/vax_debna.c` `et_message`, `et_load_pqb` (DEBNA) | VMS V5.5 has no message free queue (length 0): every message response got a negative length and was never written; DECnet start: %SYSTEM-F-TIMEOUT on line BNA-0 |
| 75 | FIXED | Bug | `VAX/vax_debna.c` `et_transmit` (DEBNA) | Immediate datagrams (BVP opcode 3, the frame in the queue entry) read as a buffer list: VMS V5.5's hellos went out as garbage, no DECnet adjacency |
| 76 | FIXED | Bug | `sim_sound.c` `snd_set_enabled`, enable checks of RQ and TB | A refused SET REALISM ENABLED (KLESI-B TU81 rule) left the RQ units disabled that the SDI rule had already disabled |
| 77 | FIXED | Bug | `PDP11/pdp11_tq.c` `tb_reset` (KLESI-B) | SET TB ENABLE kept a TK50 type chosen while TB was disabled, in the realism mode |
| 78 | FIXED | Limitation | `VAX/vax820_mem.c`, `vax820_defs.h` (VAX 8200) | Memory was two fixed controllers at nodes 2 and 3, each half the memory, with CSR1's size field always all ones; all BI nodes fixed at compile time; SHOW CPU MEMORY empty |
| 79 | OPEN | Limitation | `VAX/vax820_uba.c` (VAX 8200, patch 14) | With the DWBUA on another node than 0, SHOW dev ADDRESS and SHOW UBA IOSPACE show the Unibus addresses in node 0's window |
| 80 | DONE (warning) | Pitfall | VMS V4.7/V5.5 with more than 8 MS820 boards | VMS uses at most 8 memory boards; with 12 x MS820-CA V5.5 counted 128MB and its SHOW MEMORY broke |
| 81 | FIXED | Bug | `VAX/vax820_defs.h` `IPL_CLKINT`, `vax820_bi.c` (VAX 8200/8300) | KA820 interval timer at IPL 18 instead of 16: VMS V5.5 SMP on the 8300 crashes (SPLRELERR, or "Exception in interrupt or exception") with THROTTLE 400K / realism mode |
| 82 | FIXED | Bug | `VAX/vax820_bi.c` `WriteIPR` (IPIR) | A write of IPIR replaced the pending inter-processor interrupt requests of both processors instead of adding to them |
| 83 | OPEN | Bug | `scp.c` `send_cmd` | A SEND (also as an EXPECT action) with a quoted string longer than CBUFSIZE (about 4.2 KB on Linux) overflows stack buffers: "stack smashing detected", SIMH dies |
| 84 | FIXED | Bug | `VAX/vax_dmb.c` sync line (DMB-SYNC), VMS V5.5 SIDRIVER | As a DDCMP multipoint tributary the DMB-0 circuit stayed in -starting: CTS did not follow RTS, so SIDRIVER waited for CTS off after its first transmission. Fixed in patch 16 |
| 85 | DONE (no SIMH change) | Pitfall | VMS V5.5 parameters TMSCP_LOAD / VAXCLUSTER | `%TMSCPLOAD-E-SERVE_ERR, error serving device MUAn:, R0 status = 00000084` at every startup: TMSCP_LOAD 1 on a node with VAXCLUSTER 0; the TMSCP server is cluster code and is not loaded, CONFIGURE still tries to serve the tapes |
| 86 | OPEN | Suspicious | `VAX/vax_dmb.c` sync line, VMS SIDRIVER | Turning the DMB-0 circuit off logs "circuit down, circuit fault, Line synchronization lost" instead of an operator stop; VMS V5.5 and the V4.7 backport alike |
| 87 | FIXED | Limitation | `PDP11/pdp11_ts.c` realism mode (patch 17) | After ATTACH and SET TS LOAD the transport stays off-line (as a real TS04/TS05), but neither SHOW TS nor SET TS LOAD said so: VMS MOUNT MSA0: only asked the operator to mount the device |
| 88 | FIXED | Bug | `PDP11/pdp11_dmc.c` `dmp_mp_send_session` (patch 18) | Multipoint DMP/DMV: maintenance (MOP) messages kept DDCMP address 1, so a receiving multipoint station handed them only to tributary 1 |
| 89 | OPEN | Limitation | `PDP11/pdp11_dmc.c` DMV11 boot | The unit number for booting (E101 SW2, `SET DMVn BOOTUNIT`) is not passed to the loaded program: the manual does not say how the program learns it |
| 90 | FIXED | Suspicious (cosmetic) | `PDP11/pdp11_dmc.c` DMP/DMV shared units, PDP-11 simulator (patch 21) | `SHOW DMV0` printed the unit as `DMP0`: the DMP and DMV devices share their units, and the unit name was taken from the DMP device; the units now take the device in use |
| 91 | OPEN | Suspicious | `sim_tmxr.c` / `sim_sock.c` outgoing connections | A line's outgoing connection (PEER) to a plain listening TCP socket (a Python test script) was reported "Outgoing Line Connection ... failed" and closed, although the script had accepted it; seen in Claude's workspace only, not investigated |
| 92 | FIXED | Bug (also upstream) | `PDP11/pdp11_dmc.c` `dmc_complete_transmit` (patch 18 v2) | DMC/DMR/DMP/DMV: a sent DDCMP maintenance (MOP) message stayed on the ack-wait queue and was never returned to the host; VMS NCP `LOAD NODE`/`TRIGGER NODE` hung after Enter MOP Mode |
| 93 | OPEN | Suspicious | `sim_tmxr.c` peer connections, DMV11 pair | With `PEER` on both DMV11s (MicroVAX II and PDP-11), the TCP line kept connecting and closing (crossing connections, "aborting outgoing line connection attempt"); with `PEER` on one side only the line is stable |
| 94 | OPEN | Suspicious | VMS V4.7 STARTNET with the DMV11, `PDP11/pdp11_dmc.c` | Once in five runs `SET LINE DMP-0` failed with "Operation failure, fatal hardware error"; the debug log shows only the master clear; not reproduced with REG/CMD debug |
| 95 | OPEN | Finding (not SIMH) | `remwrt.mac` (Lee Gleason, from Ulli) | `mov #^X8000,b.eol` lacks `(r5)` and writes 100000 into location 14 (BPT vector); `crlf` is `.byte 13,15` in octal, i.e. VT CR, not LF CR |
| 96 | OPEN | Finding (not SIMH) | `macro11` v0.3 (github.com/shattered/macro11) | 5-digit local labels (`10000$`) give "Bad branch target" unless `-ysl 8`; `^X0FF` (hex starting with a digit) is read as a global symbol; `.ASCIZ <15><12>/TEXT/` is read as 12 divided by TEXT |
| 97 | FIXED | Bug | `PDP11/pdp11_rq.c` `RQ_XTIME` (VAX), VMS V4.7 boot (patch 19) | VMS froze after the date (SYSINIT in RWAST) when its 60 KB read of F11BXQP.EXE completed within a few hundred instructions (SIMH's XTIME 200; always with SET NOASYNCH, sometimes on a fully loaded host; also plain Open SIMH); XTIME now 1000 |
| 98 | OPEN | Test script | `tests/ra/ratest.py` (realism tests R1, U1, T1) | The VMS realism tests predate the SDI rule (case 68): the RA units are disabled ("Unit disabled: RQ0", "RQ1") and VMB fails; the same with the build before patch 19; N1 (normal mode) passes |
| 99 | FIXED | Limitation | `PDP11/pdp11_dmc.c` DMC11 (TYPE=DMC) (patch 21 v2) | The DMC11's line unit switch packs 2 and 3 (password, boot ROM offset) and its remote load detect were not simulated; now pack 2 (register 15) is the password, pack 3 (register 16) the boot ROM word offset; ROM or own load as on the DMR11 |
| 100 | OPEN | Limitation | `PDP11/pdp11_dmc.c` DMC11/DMR11/DMP11 boot (patch 21) | Not modelled: the DMR11's answers to an invalid Enter MOP Mode (guide Table 3-8), its rule that only the first two units can bootstrap, the BSEL1 lockout switch (M8207 E85 SW1, M8200 E76 SW9), the Unibus INIT of the DMC11's own load, and which DMC11 switch setting selects ROM or own load |
| 101 | FIXED | Bug (cosmetic) | `PDP11/pdp11_dmc.c` `dmp_description` (patch 21 v3) | The DMP/DMV description depended on the CPU's bus, not on the device: `HELP BOOT` on a Qbus CPU listed the DMP device as "DMV11 Synchronous network controller" (Ulli) |
| 102 | FIXED | Bug | `PDP11/pdp11_dmc.c` `dmc_reset` (patch 21 v3) | Every reset restarted the 2-second connection poll: a program that executes RESET more often (DEC's M9312 DECnet boot ROM retrying) starved it, so a dropped line was never connected again |
| 103 | FIXED | Suspicious | `PDP11/pdp11_dmc.c` `dmc_process_master_clear` (also upstream; patch 21 v4) | A master clear (also the bus INIT of a RESET instruction) dropped DTR, which closes the TCP line; the DMC11 manual says the board keeps DTR and asserts it again at master clear. Now DTR is kept (Ulli's decision) |
| 104 | OPEN | Suspicious | `PDP11/pdp11_rom.c` `rom_rd`, `rom_attach` (upstream) | The ROM image is read as host-order 16-bit words (wrong on a big-endian host); a file of odd size makes the last word read one byte past the buffer |
| 105 | FIXED | Bug | `PDP11/pdp11_dmc.c` `ddcmp_UserStartup` (patch 21 v4) | On a connected line a maintenance mode request (CONTROL IN with MAINT, DMP key MAINT) started DDCMP (rule 1 before rule 2); maintenance mode came only with the peer's MOP message |
| 106 | FIXED | Limitation | `PDP11/pdp11_cpumod.c` `HITMISS` (upstream; patch 22) | The 11/60 and 11/70 hit/miss register always reads 0 (no cache model): DEC's M9312 diagnostic ROM 23-616F1 halts in its cache test at 165554 (CONTINUE goes on with the cache off) |
| 107 | FIXED | Limitation | `PDP11/pdp11_stddev.c` `tto_wr` (upstream; patch 22) | The console DL11 has no maintenance loopback (XCSR bit 2 not writable, no loopback): DEC's 11/24 diagnostic ROM 23-774F1 waits for ever in its console test |
| 108 | FIXED | Bug (minor) | `PDP11/pdp11_stddev.c` `tto_reset` (upstream; patch 22) | A bus INIT (RESET instruction) cancels the console character being sent; a real DL11 finishes sending it: the last character before a RESET is lost |
| 109 | FIXED | Limitation | `PDP11/pdp11_xu.c` port command BOOT (upstream; patch 23) | The DEUNA/DELUA BOOT port command is not implemented ("BOOT command not implemented!"): DEC's M9312 'XE' boot ROMs print the Ethernet address and then wait for ever |
| 110 | FIXED | Bug | `PDP11/pdp11_stddev.c` `tti_svc` debug (upstream; patch 22) | With SET TTI DEBUG=DAT, a received NUL byte aborts the simulator: the debug line formats the character with %c, and `sim_debug` aborts on an embedded NUL |
| 111 | FIXED | Bug | `PDP11/pdp11_xu.c` `xu_port_command` STOP/START (upstream; patch 23) | STOP outside the running state and START in the running state gave a port command error; the DELUA UG says they are no-ops that set DNI (DEC's TERUNA failed on it) |
| 112 | FIXED | Bug | `PDP11/pdp11_xu.c` `xu_command` FC_RDCTR/FC_RDCLCTR (upstream; patch 23) | Read (and Clear) Counters always wrote 68 bytes, ignoring the UDB length in PCB+6: it overwrote memory behind a short UDB (TERUNA's MOP message) |
| 113 | FIXED | Bug | `PDP11/pdp11_xu.c` `xu_command` FC_RLSA (upstream; patch 23) | Read Load Server Address returned the load assistant multicast exactly when a load server was set (test inverted: eth_mac_cmp is 0 when equal) |
| 114 | FIXED | Bug (minor) | `PDP11/pdp11_xu.c` `xu_setmac` (upstream; patch 23) | SET XU MAC= changed only the address used after the next reset: a GO right after SET MAC/ATTACH received nothing for the new address |
| 115 | FIXED | Bug | `PDP11/pdp11_dmc.c` `dmc_process_input_transfer_completion` (upstream; patch 24) | DMC11/DMR11: a transmit BA/CC IN when no buffer is free dereferenced NULL: the simulator died with a segmentation fault |
| 116 | FIXED | Bug | `sim_tmxr.c` `tmxr_poll_conn` per-line listener (upstream; patch 25) | A line with PEER and its own port took one pending incoming connection per poll, also attempts the peer had given up long ago (closed): the peer's live connection, already used by the peer, was taken many polls later and what it carried arrived in a burst; with BOOT DMCn VMS V4.7 then gave up 2 of 4 system loads |
| 117 | FIXED | Bug | `sim_sound.c` `sim_spindle_wait`, `sim_reel_wait`, `sim_strm_wait` (ours, sound patch; patch 26) | Realism mode: a SET ONLINE/OFFLINE/LOAD/UNLOAD/REWIND at the sim> prompt waited up to a minute for the spindle or tape without looking at the keyboard: neither Ctrl-E nor Ctrl-C ended it |

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
- **2026-11-12:** `sim_sound_register_storage` is back (sound patch v33) for the VAX 8600 console RL02, which does not use sim_disk and so is not `DEV_DISK`.

### 9 — OPEN — Bug: MSCP Compare stores a wrong bus address after a memory error
- **Where:** `PDP11/pdp11_rq.c`, `rq_svc`, Compare branch, non-existent memory:
  `PUTP32 (pkt, RW_WBCL, bc - i); PUTP32 (pkt, RW_WBAL, bc - i); /* adj ba */`
- **Problem:** the buffer address is set to the byte count.
- **Probable intent:** `ba + i`, as in the read and write branches.
- **Effect:** wrong address in the end packet or error log; only after a memory error during Compare.
- **Found:** 2026-11-04.
- **Fixed:** 2026-11-16, integration commit "RQ: compare host data reports the right bad host address" (`ba + i`), during the KDB50 work (cases 71 and 72), as fixing in code being worked on is fine (Ulli).

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

### 49 — OPEN — Pitfall: BOOT RY0 with the RX also enabled stops in the RX
- **Reported by Ulli (2026-11-11):** PDP-11, `SET RX ENABLED` while RY is enabled, `ATTACH RY0 RT11.RX2`, `BOOT RY0` → `%SIM-ERROR: Unexpected I/O error while processing event for RX0 - Unit not attached`, stop at PC 000352. Same with plain Open SIMH 87eb7d5.
- **What happens:** autoconfig has one slot for RX11/RX211 (`{ "RX", "RY" }`, fixed CSR 177170, vector 264). With both enabled the first, RX, gets the fixed CSR and the RY floats to the next free address (160270, vector 340); `SHOW CPU IOSPACE` shows both, no conflict. SIMH's RY boot ROM is patched with the RY's CSR (160270, word at 2026) and reads the boot block correctly. The RT-11 boot block (block 0 of the diskette) then uses its own CSR: `240: MOV #177170,R5` (and the word at 242 for the polling), i.e. the RX. The RX has no diskette and STOP_IOE set, so its command stops the simulator.
- **So:** the addressing is right (a second RX-type controller floats, as DEC's autoconfiguration rules say); the guest's boot block expects the standard CSR. On real hardware the same boot block would talk to the RX11 too (it would fail, not stop).
- **Workarounds:** `SET RX DISABLED` (the RY then gets 177170 and vector 264), or swap the two controllers by hand (this switches autoconfiguration off; the RT-11 DY handler also expects vector 264): `SET RX ADDRESS=17777150`, `SET RX VECTOR=340`, `SET RY ADDRESS=17777170`, `SET RY VECTOR=264`. Checked: RT-11 then boots from RY0 and lists the directory.
- **Possible SIMH changes (decision for Ulli):** (a) `BOOT RY0`/`BOOT RX0` warn when the controller is not at 177170 ("an RT-11 boot block expects the standard CSR 177170; RX is there"); (b) the RX (like the RY in the realism mode) ends a command on an empty drive with an error instead of stopping the simulator (STOP_IOE default).
- **Found:** 2026-11-11 (Ulli).

### 50 — OPEN — Suspicious: a Unibus PDP-11 keeps the Qbus RQDX3
- **Where:** `PDP11/pdp11_rq.c`, `rq_reset`: `if (cp->ctype == DEFAULT_CTYPE) cp->ctype = (UNIBUS ? UDA50A_CTYPE : MICROVAX1 ? RQDX1_CTYPE : RQDX3_CTYPE);`. The first reset (start-up) runs with the default PDP-11 CPU, an 11/73 (Qbus), so the controller becomes an RQDX3; `SET CPU 11/70` (Unibus) does not change it, nor does a later reset.
- **Seen:** plain Open SIMH 87eb7d5: `SHOW RQ` gives `RQDX3` before and after `SET CPU 11/70` and `RESET`. The VAX simulators are not affected (their bus is fixed).
- **Effect:** the guest sees an RQDX3 (MSCP controller model 19) on a Unibus machine; drivers that look at the model may behave differently from a real UDA50. In the sound realism mode this is why there is no bus rule for the RX50 controllers (RQDXn Qbus, RUX50 Unibus): it would refuse every Unibus PDP-11 configuration (decided 2026-11-11 while adding the RX50).
- **Possible fix:** when the CPU model changes the bus (`cpu_set_model`), a controller still at its resolved default type gets the new bus's default, or keep DEFAULT_CTYPE until a type is set explicitly and resolve it at each reset.
- **Found:** 2026-11-11 (RX50 controller rules).

### 51 — FIXED IN PATCH 5 — Bug: the VAX 8600 console RL02 bad block table lands in the middle of a new image
- **Where:** `sim_disk.c`, `sim_disk_pdp11_bad_block`, the branch for a unit without a sim_disk context (`uptr->disk_ctx == NULL`). Only the VAX 8600 console RL02 (`VAX/vax860_stddev.c` `rlcs_attach`, `attach_unit`, no sim_disk) takes it; every other caller has a context and writes through `sim_disk_wrsect`.
- **Bug:** `da = capac - sec * wds` counts 16-bit words (the unit's capacity is in words), but the branch uses it as the byte offset of `sim_fseek` and steps it by `wds` (128) while writing 256 bytes per copy. A new console RL02 created with `ATTACH -Y CS file` therefore got the table at byte 5,237,760 (the middle of the 10,485,760-byte pack, block 10230), the ten copies overlapping, and the file was only 5.2 MB long.
- **Fix (patch 5 v33):** the branch converts the word offset to bytes (`da * sizeof (uint16)`, not for DEV_SECTORS devices). Checked: the table now starts at byte 10,475,520 (cylinder 511, head 1, sector 0), ten 256-byte copies, as the sim_disk path writes it.
- **Found and fixed:** 2026-11-12 (8600 console RL02 realism work).

### 52 — OPEN — Pitfall: ATTACH -N never writes a bad block table
- **Where:** `scp.c` `get_yn`: switch Y answers yes, switch N answers no, before asking. `pdp11_bad_block` (and `sim_disk_attach` for new images with a PDP-11 track size) asks "Overwrite last track? [N]" with `get_yn`.
- **Effect:** `ATTACH -N` means "create a new file" to attach, but get_yn takes the same N as the answer No: a new image never gets the DEC standard bad block table that way. `ATTACH -Y` (or a plain ATTACH of a new file with Y typed at the prompt) writes it. In a command file without input the default (No) applies too. VMS INITIALIZE then warns `%INIT-W-NOBADDATA` (seen in the RL02/RP06 tests); EXCHANGE does not care.
- **Possible change:** a separate switch for the bad block question, or ask only when the file existed before.
- **Found:** 2026-11-12 (8600 console RL02 realism work).

### 53 — OPEN — Suspicious: the 8600 console RL02 status lacks the RL02 and volume check bits
- **Where:** `VAX/vax860_stddev.c`, `rlcs_svc`, RL_STATUS / RL_MP: an attached, ready drive reports `RLDS_ATT` (heads out, brushes home, lock on = 035); `RLDS_RL02` (0200) is never set; `rlcs_attach` keeps volume check in `STAT`, which is never reported or cleared.
- **Effect:** none seen (VMS V4.7 CSDRIVER mounts and uses the drive). Software that checks the drive type (RL01/RL02) or volume check in the console's RL status would see an RL01 without volume check. The realism mode reports the drive state (load, spin-up, spin-down) while the drive is not up to speed.
- **Found:** 2026-11-12 (8600 console RL02 realism work).

### 54 — FIXED IN PATCH 5 — Bug: the VAX 8200 console RX50 crashes SIMH on physical track 0
- **Reported by Ulli (2026-11-12):** VAX 8200, VMS V4.7, `@SYS$UPDATE:STABACKIT` to CSA1: with copy verification; after "Copying the first 136 blocks of SYS.EXE to SYSTEM_1" SIMH ended with `Segmentation fault`.
- **Where:** `VAX/vax820_stddev.c`, `CALC_SC (t,s)` = `fl_intl[((t) - 1) % FL_INTL][...]` and `CALC_DA (t,s)` = `(((t) - 1) * FL_NUMSC + CALC_SC (t,s)) * FL_NUMBY`, used by Read Sector and Write Sector in `fl_svc`.
- **Bug:** the guest gives physical tracks 0-79; the macros take physical track t as logical track t - 1. For track 0 that is -1: `fl_intl[-1]` is read out of bounds, the disk address is negative (stored in a `uint32`), and `fbuf[da + i]` accesses memory about 4 GB away from the diskette buffer. The RX50 keeps the last logical track (blocks 790-799) on physical track 0 (the RQDX maps it the same way), so VMS reaches it as soon as a diskette is nearly full. Plain Open SIMH 87eb7d5 has the same code and crashes the same way.
- **Fix (patch 5 v34):** physical track t holds logical track (t + 79) % 80; the interleave row follows the logical track. The image is in RX50 block order, the same as an RQ RX50 image.
- **Tests:**
  - `tests/rx50_8200/t0.ini`: one Read Sector of track 0 sector 1 from an image with the block number in every block: plain Open SIMH and the old build exit 139 (segmentation fault), the fixed build reads block 791 (logical track 79, skew 8, as the RQDX).
  - `tests/rx50_8200/stab1.ini`: Ulli's STABACKIT (three diskettes, copy verification) on the old integration build: segmentation fault at the same place; on the fixed one "The kit is complete.", 47 verified copies, no errors. `stab3.ini`: the same in the realism mode (5.5 min VMS time, 20 operations on track 0).
  - `tests/rx50_8200/stab2.ini`: the kit diskette SYSTEM_1 read by VMS on the MicroVAX II as an RX50 on an RQDX3: mounts, ANALYZE/DISK_STRUCTURE only reports the missing QUOTA.SYS and the storage control block of a kit volume, VMB.EXE identical; SYS.EXE's first extent is LBN 727-799 and its blocks on track 0 (LBN 790-799) are identical to the copy STABACKIT verified (host-side comparison with the RA81 image).
- **Found and fixed:** 2026-11-12 (Ulli).

### 55 — FIXED IN PATCH 5 — Bug: the realism clock jumped on unthrottled simulators
- **Where:** `sim_sound.c`, `sim_sound_rt_us` (Claude's own code, since sound patch v16): while the CPU ran it returned `base + (sim_gtime () - gbase) / sim_sound_inst_per_us ()`, i.e. all instructions since the last start divided by the *current* rate estimate.
- **Effect:** without a THROTTLE (VAX-11/730, PDP-11, ...) SIMH's instruction rate estimate changes about every second by up to ±20 %; after a long run a 1 % change moves the clock by seconds. Found with the TU58 model (VAX-11/730 console TU58 under VMS, PDP-11 RT-11): consecutive records were taken as already passed and the tape searched back, 13-20 s instead of 0.1 s. The same clock drives the floppy rotation and the tape models, which could show the same on unthrottled simulators. With a THROTTLE (VAX-11/750 300K, MicroVAX II 450K) the rate is fixed and nothing happened.
- **Fix (patch 5 v35):** piecewise linear: when the rate estimate changes, the time so far is kept (computed with the old rate) and a new segment starts. Tests: `tests/tu58/vms4.ini` (VAX-11/730) and `rt2.ini` (PDP-11) without long searches between consecutive records; RX02 RT-11 realism test unchanged.
- **Found and fixed:** 2026-11-12 (TU58 realism work).

### 56 — FIXED IN PATCH 5 — Bug: TU58 command for a unit the controller does not have
- **Where:** `PDP11/pdp11_td.c`, `td_process_packet`: `ctlr->unitno = ctlr->ibuf[4]` (any value 0-255) and then `sim_activate (ctlr->uptr+ctlr->unitno, ...)`; `td_svc` uses that unit and its `up7`.
- **Effect:** a guest that sends a command for unit 2 or more (a TU58 has drives 0 and 1) schedules another controller's unit (PDP-11 TDC: the units of all controllers are one array) or, on the VAX-11/730/750 console TD (one unit), memory outside the unit array. Plain Open SIMH 87eb7d5 has the same code. Not seen with VMS or RT-11 (they only use valid units).
- **Fix (patch 5 v35):** such a command ends with the TU58's "bad unit number" success code (0370, -8) in its end packet, in both modes.
- **Found and fixed:** 2026-11-12 (code reading for the TU58 realism work).

### 57 — FIXED IN PATCH 5 — Suspicious: a TU58 cartridge removed during a transfer
- **Where:** `PDP11/pdp11_td.c`, `td_svc`, states READ1 and WRITE2: `fbuf = uptr->filebuf` is used without checking that the unit is still attached; `td_test_xfr` checks only at the start of the command.
- **Effect:** a DETACH between the data packets of a transfer (possible from the sim> prompt or a script) frees the buffer; the next data packet would be built from or copied into a NULL buffer. Not reproduced (the window is short in the normal mode); in the realism mode a transfer of several blocks takes seconds.
- **Fix (patch 5 v35):** the transfer ends with "no cartridge" (0367) instead, in both modes (as when a real cartridge is pulled: UG 2.5.2).
- **Found and fixed:** 2026-11-12 (code reading).

### 58 — OPEN — Bug: the console TD registers of the VAX-11/730 and 750 are dummies
- **Where:** `VAX/vax730_stddev.c` and `VAX/vax750_stddev.c`, `td_reg[]`: ECODE, BLOCK, RX_CSR, ..., CTIME, STIME, XTIME, ITIME all point to the same `int32 td_regval` ("temp location used in reg declarations"); the real values live in `pdp11_td.c` (`td_ctlr[TD_NUMCTLR]`, `td_ctime`, ...).
- **Effect:** EXAMINE TD shows the dummy for every register; DEPOSIT TD CTIME etc. has no effect; SAVE/RESTORE of the console TU58 state saves nothing. The new realism registers STARTMS, TURNMS, RUNUP and BAUD do point to the real variables (patch 5 v35).
- **Possible fix:** export the variables from `pdp11_td.c` (as done for the realism registers) or build the console register table there.
- **Found:** 2026-11-12.

### 59 — OPEN — Limitation: TU58 command modifiers ignored
- **Where:** `PDP11/pdp11_td.c`, `td_process_packet`: the modifier byte (command packet byte 3) is not looked at.
- **Effect:** the special address mode (modifier bit 7: the block number field holds a 128-byte record number 0-2047, User's Guide 3.2.3.1) is executed as a block number, so such a request reads or writes the wrong place; write/verify (modifier 1 of the Write command) gets its read-back time only in the realism mode (the data are not verified, which cannot fail in SIMH anyway); read with decreased sensitivity (Read modifier 1) is a plain read (fine).
- **Found:** 2026-11-12.

### 60 — OPEN — Limitation: a VAX-11/730 console cartridge cannot be booted
- **Where:** `VAX/vax730_sys.c` BOOT TDn with SIMH's internal VMB.
- **Seen:** Ulli's 11/725-730 diagnostic supervisor cartridge (BE-T175I-DE, RT-11 volume: DIAGBO.EXE, ENSAA.EXE, DEFBOO.CMD): `BOOT TD0` and `BOOT TD0/R5:10` both end in VMB's standalone-kit dialog ("Insert the first standalone system volume") and then `%BOOT-F-Unable to locate BOOT file`; plain Open SIMH 87eb7d5 the same. A real 730 boots its console cartridge through the console microcode, which reads the RT-11 volume.
- **Effect:** the console and diagnostic cartridges of the 730 can only be used as data volumes (VMS CSA1, `tests/tu58/vms6.ini`).
- **Found:** 2026-11-12.

### 61 — OPEN — Pitfall: RT-11 V5.7 LS handler on the TDC address
- **Seen:** Ulli's RT-11 V5.7 RX02 system (`SHOW DEVICES`): `LS Installed 176500 470 474 300 304`, the serial printer handler on the CSR and vector 300 of the PDP-11 TDC (TU58), which this kit has no handler for (no DD.SYS). RT-11 V5.3 installs XL at 176500.
- **Effect:** a TDC at its default address shares it with an installed handler for another device; use the DD handler (`tests/tu58/mkdd.ini` builds an RT-11 V5.3 TU58) or move the TDC (`SET TDC ADDRESS=`).
- **Found:** 2026-11-12.

### 62 — FIXED IN PATCH 3 — Bug: BOOT CS0 on the VAX 8200 gives "Invalid argument"
- **Reported by Ulli (2026-11-12):** `BOOT CS0` with the STABACKIT diskette SYSTEM_1 attached: `Invalid argument`.
- **Where:** `VAX/vax820_bi.c`, `vax820_boot_parse`: `if (dibp == NULL) return SCPE_ARG;`. The console floppy CS has no DIB, but it is in the boot table (`BOOT_CS`, device code 64), so it could never be booted. Plain Open SIMH 87eb7d5 since the 8200 was added (1d159661). The VAX-11/780 (`vax780_sbi.c`) and 8600 (`vax860_abus.c`) parsers use base address 0 for a device without a DIB.
- **Fix (patch 3, third commit):** the same as on the 780 and 8600. `BOOT CS0` loads VMB from the console RX50; VMB reads SYSTEM_1 and asks for the standalone system volumes.
- **Checked by Ulli (2026-11-12, integration build):** `ATTACH CS0 STABA047_1.RX50`, `BOOT CS0`; at "Insert the first standalone system volume" YES (SYSTEM_1 stays in); at "Insert the next standalone system volume" Ctrl/E, `ATTACH CS0 STABA047_2.RX50`, CONTINUE, YES; date and time; at "Insert the standalone application volume" Ctrl/E, `ATTACH CS0 STABA047_3.RX50`, CONTINUE, YES: `%BACKUP-I-IDENT, Stand-alone BACKUP V4.7`, `$` prompt.
- **Found and fixed:** 2026-11-12.

### 63 — FIXED IN PATCH 5 — Bug: realism throttle measurement makes VMS delay loops far too long
- **Reported by Ulli (2026-11-13):** VAX-11/750, VMS V4.7, sound enabled: `MC SYSGEN`, `CONNECT CONSOLE` hung at PC 8017E73D (`SOBGTR (SP)`), the TU58 did not move; it went on after about 10 minutes.
- **Where:** `sim_timer.c`, `sim_throt_svc`, state INIT with THROT_DELAY 0 (my change of case 40): the host rate was measured by running the guest unthrottled for at least 100 ms (repeated with longer windows when too low).
- **Effect:** VMS calibrates EXE$GL_TENUSEC and EXE$GL_UBDELAY at boot; the part of the boot that ran during the measurement (24-35 MIPS) gave TENUSEC 3-4 and UBDELAY 19-150 instead of 1. The TIMEWAIT that holds BREAK on the console TU58 line for 0.4 s took 8 s to minutes; all other VMS timed waits were too long by the same factor. How much of the boot fell into the measurement varied: time at the `sim>` prompt before the boot made it worse.
- **Fix (patch 5 v36):** with a fixed rate (xK, xM) and no warm-up the throttle starts throttling at once (one 1 ms sleep per 1/1000 of the wanted rate) and recalibrates after 1 s. Checked: 750 (300K) TENUSEC 1, UBDELAY 1, BREAK 0.67 s; MicroVAX II (450K) 1 and 3; Ulli's sequence no longer hangs; TU58 vms2 test 0 differences.
- **Found and fixed:** 2026-11-14.

### 64 — FIXED IN PATCH 5 — Bug: standalone VMS cannot read the VAX-11/780 console floppy after BOOT CS
- **Reported by Ulli (2026-11-14):** VAX-11/780, `ATTACH CS STABA047-1.RX1`, `B CS`; VMB loads SYSTEM_1 to SYSTEM_3 (diskette changes as asked), then after the VMS V4.7 banner the console shows garbage (`+`, later `)(<?9<?9...`) and VMS loops (PC 80008B1F). Plain Open SIMH 87eb7d5 does the same, in both modes.
- **Trace (TXDB writes):** VMB uses floppy commands `0x09xx` (ID 9) and data bytes `0x01xx` (ID 1). After the banner standalone VMS's console floppy driver writes `0x0800` and data bytes `0x00xx`: the same protocol with bit 8 missing, so the command goes to ID 8 (ignored) and the sector/track bytes to the terminal (the garbage: `<` = track 60, `?` 63, `9` 57); VMS retries.
- **Cause:** `vax780_boot_parse` puts the unit number (0 for the single-unit CS device) into R3. The VMS driver builds the ID from the boot unit (unit 1 = CSA1: ID 9 and 1). The 780's console floppy is CSA1.
- **Fix (patch 5 v37):** BOOT CS passes unit 1 in R3. The VAX 8600 (console RL02, R3 0) and 8200 (CS0/CS1) boot standalone BACKUP with their unit numbers (Ulli's tests), so they are unchanged.
- **Tests:** `tests/rx01/stab1.ini` (normal) and `stab2.ini` (realism): four diskettes, `%BACKUP-I-IDENT, Stand-alone BACKUP V4.7`, `$`; VAX-11/780 diagnostics passed.
- **Found and fixed:** 2026-11-14.

### 65 — DONE (patch 7) — Limitation: the VAX 8200's second CPU (KA1) never runs
- **Reported by Ulli (2026-11-14):** VAX 8200 with `SET KA1 ENABLE`, VMS V4.7 with a multiprocessing license: `START/CPU` gives `%SYSTEM-F-DEVOFFLINE, device is not in configuration or not available`.
- **Where:** `VAX/vax820_ka.c`. KA1 is only a BI node: it answers reads of its BIIC registers (device type KA820, CSR, BER, EICR, IDEST), so VMS sees a second processor on the BI. The console commands a primary uses to start a secondary through the RXCD register (`D` deposit a register, `S` start) are parsed, but the calls that would act on them, `cpu_setreg (cpu, ...)` and `cpu_start (cpu, ...)`, are inside `#if defined (VAX_MP)`; `VAX_MP` is defined nowhere and those functions do not exist. `cur_cpu` is never set (always 0). The SIMH VAX CPU core has one register set, one TLB and one instruction loop, so the secondary never executes, never reports in, and VMS leaves it offline.
- **Same in plain Open SIMH 87eb7d5** (the code is Matt Burke's original 8200 support, 2019).
- **What a fix needs:** a second CPU context (registers, PSL, IPRs, TLB, per-CPU interrupt state, interval timer, the KA820 inter-processor interrupt), interleaved execution on the host thread (switching contexts every few instructions or at events), per-CPU console registers (TXCS/RXCS, PCSR already per `cur_cpu`), and `cpu_start`/`cpu_setreg` for the RXCD commands. A large change to the VAX CPU core.
- **Found:** 2026-11-14.
- **Done 2026-11-15 (patch 7 `opensimh_7_vax8300_mp_v1.patch`, branch `vax8300-mp`, design and results in `docs/VAX8200_dual_CPU_design.md` v2):** the CPU core keeps one context per processor (only for the 8200, `VAX_MP`); the processors take turns every CPU QUANTUM instructions; the RXCD start commands, the BIIC IPINTR registers, per-processor interrupts and KA1's interval timer are implemented. VMS V4.7 (`DEFINE/SYSTEM/EXEC MP MP8SS`): START/CPU, SHOW CPU IDLE/EXECUTE, STOP/CPU, START/CPU again; two CPU-bound jobs run in parallel. The "DEVOFFLINE" of Ulli's first try also had a VMS side: the logical name MP and the image MP8SS.EXE were missing.
- **Also found and fixed on the way (in patch 7):** SIMH's infinite-loop stop for a jump to itself at IPL 31 must not apply to an attached processor (VMS parks a stopped one that way).

### 66 — FIXED IN PATCH 8 — Bug: BICSR hard error summary bit
- **Where:** `VAX/vax_bi.h`: `#define BICSR_HES 0x00004000`, the same value as `BICSR_SES` on the next line.
- **Right value:** bit 15, 0x00008000 (DMB32 User Guide 3.3.2, VAXBICSR: <15> HES, <14> SES; same in every BIIC description).
- **Effect:** none so far: no simulator uses `BICSR_HES`.
- **Same in plain Open SIMH 87eb7d5.**
- **Found:** 2026-11-15, while writing the DMB32 BI node. **Fixed** in the DMB32 patch (patch 8).

### 67 — FIXED IN PATCH 8 — Limitation: BI nodes cannot supply their own interrupt vector
- **Where:** `VAX/vax820_bi.c` `get_vector`: the DWBUA gets its vector from `uba_get_ubvector`; every other nexus gets `SCB_NEXUS + (level << 6) + (node << 2)`.
- **Why it matters:** a VAXBI node's BIIC may send any vector (UINTRCSR, or the node's own logic with EX.VECTOR). The DMB32 sends `100 + type*40 + node*4` at BR4, with the type telling the function (0 BIIC error, 1 sync, 2 async, 3 printer); the fixed formula would give every DMB32 interrupt the same vector 100 + node*4, and type 1 would collide with the formula's level-15 slot.
- **Fix (patch 8):** a per-nexus acknowledge routine (`nexusA[]`, filled from the DIB's `ack[0]` in `build_nexus_tab`); `get_vector` asks the node for its vector when it has one. The existing nexus (KA820, memory, DWBUA) have none and keep the formula.
- **Found and fixed:** 2026-11-15 (DMB32 work, design `docs/DMB32_VAX8200_design.md`).

### 68 — FIXED (patch 9 v2) — Suspicious: SDI controllers accept non-SDI drives in the realism mode
- **Where:** `PDP11/pdp11_rq.c`, `rq_rlz_rules` (Claude's realism rules, sound patch v24/v25): an SDI drive (RA family, ESE) needs an SDI controller (UDA50, UDA50A, KDA50, now the KDB50); the reverse is not checked.
- **Effect:** with SET SOUND ENABLED an RD54 (an ST506 drive of the RQDX controllers) is accepted on a UDA50, KDA50 or KDB50, which a real SDI controller cannot connect. Found while testing the KDB50 (tests/kdb50/p5.ini v1 expected a refusal).
- **Decision for Ulli:** add "an SDI controller only connects SDI drives" to the realism rules (the RX50 rule already works that way for the RX50), or keep it permissive.
- **Found:** 2026-11-15 (KDB50 work).
- **Decision (Ulli, 2026-11-16):** add the rule; "disable unattached, refuse attached" (like the RX50 rule), and "keep disabling": a unit the rule disabled is enabled again with SET RQn ENABLED before it gets an SDI type.
- **Fix:** kdb50 e297df39 / integration de132fd0, in patch 9 v2. `rq_rlz_rx50_disable` (called at SET SOUND ENABLED and SET RQ <ctype>) also disables unattached non-SDI units on an SDI controller, with a message such as "RQ0 disabled: a KDA50 only connects SDI drives (RA family, ESE), RQ0 is an RD54"; `rq_rlz_rules` refuses an attached one, SET RQn <non-SDI type> and ATTACH ("a KDB50 only connects SDI drives ..."). HELP RQ describes it.
- **Consequences:** with sound enabled, the default RD54 units of the Unibus VAXen (UDA50A) and of a KDA50 are disabled; enable a unit and retype it one at a time (SET RQ0 ENABLED, SET RQ0 RA81, SET RQ1 ENABLED, ...), because the rules check all enabled units. An unattached RC25 on a UDA50 is now disabled by SET SOUND ENABLED instead of being refused. A unit stays disabled after SET RQ <non-SDI type> or SET SOUND DISABLED.
- **Tests:** tests/kdb50/sdi_rule.sh v1 (new); ra1.ini v2, ra5.ini v2, rx50/c1.ini v2, rx50/c2.ini v2, rc25/c10.ini v3 adapted (output otherwise unchanged).

### 69 — OPEN — Limitation: no network boot on the VAX 8200 (DEBNA)
- **Where:** `VAX/vax820_bi.c` (`vax820_boot_parse`, boot table) and the built-in VMB of the `vax8200` simulator.
- **Effect:** the DEBNA works under VMS (patch 10), but the 8200 cannot boot from the network. A trial with `BOOT ET` (R0 = 98 = `BTD$K_AIE_NI`, R1 = node, as for the KDB50 with 33) halted VMB at once (PC 00005374, R2 = 98): the built-in VMB has no DEBNA driver. `BOOT ET` was therefore taken out of the boot table again.
- **Real hardware:** the 8200 family boots from the network with the console's EEPROM boot program "ET" (EK-DEBNX-IN-001 appendix C: `B ET50` on a VAX 8250; an 8350 EEPROM lists "Boot device type ET - Version 104: DEBNK, DEBNA"; NetBSD port-vax 2000: `>>>T`, then `B ETx0` MOP-boots). VMS V4.7 knows the code (`BTD$K_AIE_NI = 98`, and `BTD$K_AIE_TK50 = 35` for the DEBNK tape).
- **Possible ways (decision for Ulli):** (a) VMS V4.7's `VMB.EXE` from the system disk, loaded instead of the built-in one; (b) a MOP client in the simulator (the console's ET boot program). Both need a MOP load host (VMS with a DECnet downline-load database, or a host `mopd`) and a loadable image.
- **Tried (a), 2026-11-16 (Ulli: "Try the VMB.EXE from my VMS V4.7 disk"):** VMB.EXE (57 blocks, contiguous at LBN 31716 of VMS047_1.R81, taken off the VHD image with `tools/vhd_read_blocks.py`) loaded by hand at 200 (`LOAD -O`, R0..R5, `GO 200`) boots VMS from the KDB50 (R0 = 33, R1 = 6), so the method works; with R0 = 98, R1 = 7 it halts at once (PC 3AE6, R2 = 62) without touching the DEBNA. Its boot driver table (at 380B, entries of 0x28 bytes: CPU type, device code, routines) has the device codes 0, 1, 2, 3, 17, 33 (KDB50), 64 (console) and a generic entry, but no 98: VMS V4.7's VMB has no DEBNA driver. That fits the real 8200, whose network boot is the console EEPROM program ET. Left: (b), or a VMB of a later VMS version that has the AIE_NI driver.
- **Decision (Ulli, 2026-11-16):** network boot stays open until further notice.
- **Found:** 2026-11-16 (DEBNA work, phase P5).

### 70 — OPEN — Limitation: Unibus devices can be enabled while the UBA is disabled
- **Where:** `VAX/vax820_uba.c` `uba_enbdis` (patch 11) and SCP's `SET dev ENABLE`.
- **Effect:** SET UBA DISABLE (VAXBI-only mode) disables every enabled Unibus device. Afterwards SET RQ ENABLE (or any other Unibus device) is still accepted. The device is then enabled but has no adapter: its registers are not reachable (the DWBUA window is gone), so the guest does not see it. SET UBA ENABLE re-enables only the devices it disabled itself; the one enabled by hand simply stays enabled.
- **Possible ways (decision for Ulli):** (a) leave it (harmless: the device is unreachable); (b) refuse SET dev ENABLE for Unibus devices while the UBA is disabled. That needs a check in each Unibus device's reset, or a hook in SCP.
- **Found:** 2026-11-16 (KLESI-B work).

### 71 — FIXED — Bug: KDB50 ring descriptors masked to 22 bits
- **Where:** `PDP11/pdp11_rq.c`, `rq_getpkt` and `rq_putpkt`: `addr = desc & UQ_ADDR` (`UQ_ADDR` = 0x003FFFFE, the Qbus/Unibus width), also for the KDB50 (patch 9).
- **Effect:** VMS V5.5 did not boot from the KDB50: `%EXECINIT-F-Error loading system file - PROCESS_MANAGEMENT.EXE R0 = 00000810`, then a reboot loop. When SYSBOOT relocates the VMB boot driver, the driver initializes the KDB50 again with its rings and packets near the top of memory (PA 00FA8D74 with 16 MB); the KDB50 fetched the command packets from 003A8D84 (zeros) and answered every command with an error. VMS V4.7 was not hit (its packets stay below 4 MB with 16 MB). The same image boots from the UDA50 behind the DWBUA and from a KDA50 on the MicroVAX II.
- **Fix:** `RQ_DESC_ADDR (cp, desc)`: 30 bits (`KDB_DESC_ADDR` = 0x3FFFFFFE) on the KDB50, `UQ_ADDR` on the other controllers. The KLESI-B (TB) is not affected: its port sees 18-bit Unibus addresses through the adapter's map.
- **Commit:** integration "RQ: KDB50 ring descriptors carry 30-bit VAXBI addresses".
- **Found and fixed:** 2026-11-16 (Ulli's report, VMS V5.5 on the VAX 8200).

### 72 — FIXED — Bug: the KDB50 ignored writes to IP
- **Where:** `PDP11/pdp11_rq.c`, `kdb_wr` (patch 9): "IP: writing has no effect".
- **Effect:** after case 71, VMS V5.5 got through EXECINIT but hung without the date prompt. Its PUDRIVER initializes the KDB50 (steps 1-4: communication area at PA 00000610, step 4 = 0403) and then starts it by **writing** 0 to IP (bb+F2). The KDB50 never polled the command ring; PUDRIVER timed out after about 40 s, reset the node (BICSR NRST) and started over, for ever. VMS V4.7's PUDRIVER and the VMB boot driver read IP, so this did not show before.
- **Fix:** a write to IP (a word or byte write to bytes 2-3 of GPR0, or a longword write of GPR0) polls like a read. On the KDB50 the port is re-initialized through BICSR NRST, so an IP write has no initialization meaning (unlike the UDA50).
- **Result:** VMS V5.5 boots from the KDB50 through the startup (date prompt, SYSTEM job terminated).
- **Question answered (2026-11-17):** the KDB50 User Guide (chapter 3) says an IP write has no effect; the KDB50 microcode listing (KDB50 microcode of 22-Apr-1988, listing of Sep 1988, Ulli's copy) shows no periodic ring scan: the idle loop (U.IDLE) reads the command ring only when a hardware POLL condition ("IP accessed") is set, and the BDA code clears that condition when it starts reading the ring. The condition is set by a BI access to the node's IP address that the board detects itself (the microcode's poll self-test sets it with a BI read of its own IP and clears it with a BI abort); it is not the BIIC write-status bit the board uses for SA. The listing does not say whether a host write sets it, but nothing else starts a poll, so VMS V5.5, which only writes IP, can only work on a real KDB50 if a write does. **Conclusion:** "a write polls" fits the listing and stays, unless Ulli decides otherwise. Also seen: after each ring interrupt to the host the microcode reads its own IP to poll again (comment "biicd goof"); SIMH does not need that so far. Tests: `tests/kdb50/v55_1.ini` - `v55_3.ini` (8200 and 8300), see `docs/KDB50_VAX8200_design.md` section 11. The `%TMSCPLOAD-E-SERVE_ERR ... MUA0: ... MUA3:, R0 status = 00000084` messages at the end of the startup are not a KDB50 matter: see case 85 (VMS parameters TMSCP_LOAD 1 with VAXCLUSTER 0).
- **Commit:** integration "RQ: KDB50: a write to IP polls like a read".
- **Found and fixed:** 2026-11-16.

### 73 — FIXED — Suspicious: KDB50 identity and polling differed from its microcode
- **Where:** `PDP11/pdp11_rq.c`, KDB50 (patch 9): `KDB_DTYPE` 0511010E (a guess), versions `RQ_HVER` 1 / `RQ_SVER` 3 for every controller, `rq_ring_int`.
- **Found in the KDB50 microcode listing** (version 19 = V2.0, 22-Apr-1988; Ulli's copy, facts only):
  - at self-test the microcode puts its version into DTYPE<31:24>: 13 hex;
  - it reports software version 19 and, as hardware version, the board revision byte DTYPE<23:16> (SET CONTROLLER CHARACTERISTICS, error log and last fail packets); step 4 sends the version modulo 16 (3, as SIMH already did) with model 18;
  - after each ring interrupt to the host it reads its own IP and so polls the command ring again (a driver may queue a command after an interrupt without touching IP).
- **Effect:** none seen with VMS V4.7 or V5.5; a driver that relies on the poll after an interrupt would stall, and the versions shown differ from a real KDB50.
- **Fix:** DTYPE 1311010E; `RQ_HVER_OF`/`RQ_SVER_OF`: 0x11/19 on the KDB50, 1/3 on the other controllers; `rq_ring_int` polls again after the interrupt on the KDB50 when the port is up.
- **Tests:** V5.5 `tests/kdb50/v55_1.ini` (8200) and `v55_3.ini` (8300), V4.7 P2 and P3: pass. The new VMS047_1.R81 runs START/CPU in its startup, which gives `%SYSTEM-F-DEVOFFLINE` on an 8200 without KA1 (not a KDB50 matter).
- **Commit:** integration "RQ: KDB50 identity and polling as in its microcode (version 19)"; patch 9 v4.
- **Found and fixed:** 2026-11-17.

### 74 — FIXED — Bug: DEBNA message responses with VMS V5.5 (DECnet start times out)
- **Reported by Ulli (2026-11-17):** VMS V5.5, DECnet start on the DEBNA: `%NCP-W-OPEFAI, Operation failure`, `%SYSTEM-F-TIMEOUT, device timeout`, `%NCP-W-UNRCMP, Unrecognized component, Circuit BNA-0`.
- **Where:** `VAX/vax_debna.c` (patch 10): `et_message` clamped every message response to the entry length of free queue 0 (the message free queue), which `et_load_pqb` read once at port INIT.
- **Cause:** VMS V5.5's ETDRIVER does not use free queue 0: its length is 0, and V5.5 allocates its message entries itself (the response goes back into the same entry). The clamp gave a negative length, so the response to WRITE PARAMETERS, WRITE SYSTEM ID and SET PTDB was never written (debug: "memory error 00 at 00000000"); VMS timed out and counted an error on ETA0. V5.5 also sets free queue 2's length only after INIT (with its first PTDB). VMS V4.7 sets all lengths before INIT and uses free queue 0.
- **Fix:** `et_load_fq_len` reads the lengths at INIT and again before each command queue is worked; a message response is clamped to the message free queue's length only when the host gave one, otherwise to the command entry's own length.
- **Commit:** integration "DEBNA: VMS V5.5 message entries and late free queue lengths"; patch 10 v2.
- **Found and fixed:** 2026-11-17.

### 75 — FIXED — Bug: DEBNA immediate datagrams (BVP opcode 3)
- **Where:** `VAX/vax_debna.c`, `et_transmit` (patch 10): opcode 3 (`OPC_DGRAMI`) was handled like opcode 1, reading a buffer list at 0x1C.
- **Cause:** VMS V5.5 sends its small frames (DECnet hellos, Ethernet loopback) as immediate datagrams: the frame (destination, source, type, data) is in the queue entry from offset 0x1C, and its end is 0x12 + the entry's length word, so the frame has LEN - 10 bytes (e.g. 0x35 - 10 = 43 for an endnode hello). Read as a buffer list, these gave garbage frames (type 7FFF to 7F-FF-7F-FF-7F-FF, 768/1024 bytes) or "transmit failed, status 13". After case 74 the circuit came up, but no adjacency: "remote node is not currently reachable". VMS V4.7 only uses opcode 1.
- **Fix:** opcode 3 sends the frame from the entry; opcode 1 keeps the buffer list.
- **Tests:** VMS V5.5 on the 8200, DECnet over the DEBNA to a MicroVAX II (`tests/debna/p55_8200.ini` with `p2.sh` v3): adjacency up, HELPLIB.HLB (5675 blocks) there and back with 0 differences, remote DIRECTORY, NCP LOOP NODE 1.2, SET HOST; line counters: about 3.4 MB each way, no failures. VMS V4.7 P2: PASS.
- **Commit:** integration "DEBNA: immediate datagrams (BVP opcode 3)"; patch 10 v2.
- **Found and fixed:** 2026-11-17.

### 76 — FIXED — Bug: a refused SET REALISM ENABLED left RQ units disabled
- **Where:** `sim_sound.c`, `snd_set_enabled`, with the devices' enable checks (`sim_sound_register_enable_check`): `PDP11/pdp11_rq.c` (`rq_rlz_enable_check`, SDI rule, patch 9 v2) and `PDP11/pdp11_tq.c` (`tb_rlz_enable_check`, KLESI-B TU81 rule, patch 11 v2).
- **Cause:** the checks ran one after the other and each applied its changes at once. On the VAX 8200 with its default RQ (UDA50A with RD54 and RX50 units) and TB set to TK70, the RQ check disabled RQ0–RQ3 with its messages, then the TB check refused the command. SET REALISM ENABLED failed, but the RQ units stayed disabled.
- **Fix:** the check function takes `t_bool apply`. SET REALISM ENABLED first runs every check as a dry run (`apply` FALSE: the RQ check works on a snapshot of its units and restores them, without messages), and runs them again with `apply` TRUE only if none refuses. A refusal now changes nothing.
- **Tests:** `tests/sound/enable_dryrun.ini` (refused: RQ0–RQ3 stay enabled; with TB a TU81: accepted, RQ0–RQ3 disabled with one message each), `tests/kdb50/sdi_rule.sh` unchanged.
- **Commit:** integration "Sound: SET SOUND ENABLED checks the devices as a dry run first"; patch 13 v1 (with the SET REALISM rename).
- **Found and fixed:** 2026-11-17.

### 77 — FIXED — Bug: SET TB ENABLE kept a TK50 TB in the realism mode
- **Where:** `PDP11/pdp11_tq.c`, `tb_reset` (patch 11 v2, KLESI-B TU81 rule).
- **Cause:** TB is disabled by default, and the realism check skips a disabled device. SET TB TK50 while TB was disabled, then SET REALISM ENABLED, then SET TB ENABLE gave an enabled TB with the TK50 type in the realism mode. ATTACH still refused it, so no TK50 could be used, but SHOW TB and the unit data said TK50.
- **Fix:** `tb_reset` (run by SET TB ENABLE and every reset) switches an enabled TB of another type to TU81 in the realism mode and says so ("TB: realism mode: TK50 replaced by TU81"). A reset cannot refuse, so this follows the SDI rule, which disables units instead of refusing.
- **Tests:** `tests/klesib/tu81_rule.ini` step 4; VMS V4.7 on the KDB50 with the TU81 on the KLESI-B in the realism mode (`tests/klesib/b1r.ini`): BACKUP to MUB0, restore, DIFFERENCES 0.
- **Commit:** integration "TQ: realism mode: SET TB ENABLE makes a TB of another type a TU81"; patch 11 v3.
- **Found and fixed:** 2026-11-17.

### 78 — FIXED — Limitation: VAX 8200 memory and VAXBI nodes fixed
- **Where:** `VAX/vax820_mem.c`, `VAX/vax820_defs.h` (Open SIMH 87eb7d5, Matt Burke's 8200 code; unchanged until patch 14).
- **What it was:** two memory controllers MCTL0 and MCTL1 at BI nodes 2 and 3, each answering with half of the memory (`SET CPU nM` up to 512M); their CSR1 size field was always all ones (0x7FF, about 512MB per board), CSR2 always 0, the BIIC CSR 0; `SHOW CPU MEMORY` printed nothing ("TODO"). The nodes of KA0 (0), KA1 (1), the memory (2, 3) and the DWBUA (4) were compile-time constants; a real 8200/8300 has the primary at node 2 and the DWBUA at node 0 (field service rules), and its memory is MS820 boards of 2, 4 or 16MB, one node each.
- **Fix (patch 14, Ulli's compromise of 2026-11-17):** twelve MS820 boards MCTL0–MCTL11 with their own type and node, `SET CPU nM` from a table, contiguous addresses, CSR1 with size and RAM type, `SHOW MCTL`, `SHOW CPU MEMORY`; every BI node configurable (`SET dev NODE=n`), new defaults DWBUA 0, KA1 1, KA0 2, KDB50 4, DMB32 5, KLESI-B 6, DEBNA 7, memory 15 down; `SHOW BI`; rules (see `docs/VAXBI_config_VAX8200_design.md`).
- **Tests:** `tests/vaxbi/` (c1.ini, mem47.ini, mem55.ini), the 8200 regression (design doc section 4).
- **Commit:** integration "VAX8200: configurable VAXBI nodes, twelve MS820 memory boards, SHOW BI"; patch 14 v1.
- **Found and fixed:** 2026-11-17.

### 79 — OPEN — Limitation: Unibus addresses shown in node 0's window
- **Where:** `VAX/vax820_uba.c`, `vax820_defs.h` (patch 14).
- **What:** the Unibus devices' addresses (DIBs, autoconfiguration) are constants in the canonical window of node 0 (`UBADDRBASE`); the DWBUA's real window follows its node (`SET UBA NODE=n`) and `ReadIO`/`WriteIO` translate. VMS and VMB are not affected (they use the real window), but with the DWBUA on another node `SHOW dev ADDRESS`, `SET dev ADDRESS=` and `SHOW UBA IOSPACE` use node 0's window (e.g. 2043F468 for the UDA50's IP, where VMS sees 204FF468 with the DWBUA at node 3).
- **Possible fix:** show and parse Unibus addresses relative to the DWBUA's window in the 8200's address display, or Unibus addresses (774500 style) for the 8200.
- **Found:** 2026-11-17.

### 80 — DONE (warning) — Pitfall: VMS uses at most 8 memory boards
- **Seen:** VMS V5.5 on the VAX 8200 with twelve MS820-CA (192MB, `tests/vaxbi/m192.do`): SYSGEN SHOW/ADAPTER gives the size of only 8 boards (those on the 8 lowest nodes), SHOW MEMORY/PHYSICAL_PAGES says "Main Memory (128.00Mb)" with more free pages than memory and overflowed fields (VMB's page bitmap covers the whole 192MB). The field service handbook gives the same limit for VMS V4 (8 memory controllers, 128MB on the 24-slot systems). Not a size limit: VMS V5.5 runs with 512MB on four SIMH-128MB boards.
- **Decision (Ulli, 2026-11-17):** up to 12 boards stay allowed (the 24-slot backplane takes 12); `SET MCTLn` (when a board goes in) and BOOT warn while more than 8 are present.
- **Found:** 2026-11-17.

### 81 — FIXED — Bug: KA820 interval timer at IPL 18 (VMS V5.5 SMP crashes)
- **Reported by Ulli (2026-11-18):** VAX 8300 (SET KA1 ENABLE), 8 × MS820-CA, SET REALISM ENABLED, VMS V5.5 (`300BI_128M_VMS055_001.log`): while reading SYSGEN HELP PARAMETERS the simulator stopped with `Exception in interrupt or exception, PC: 86CFAB6E (CLRQ -(SP))`.
- **Reproduced:** with Ulli's image and configuration, in the realism mode and in the normal mode with `SET THROTTLE 400K` alone, in about half of the runs: the same stop, or VMS bugcheck **SPLRELERR** ("Spinlock to be released is not owned", CPU 02, PC 86CFAC1E, spinlock at 86CFAFA0: IPL 22). Also with the build before patch 14, so not caused by it. Never at normal speed or THROTTLE 2M, also not with the processors switching every 2 instructions.
- **Analysis:** the PCs are in VMS's SMP synchronization code (spinlock acquire with its wait loop, and release). VMS V5.5 finds the running processor's per-CPU database from the interrupt stack pointer (`MFPR #4` and a mask) and treats an acquire by the owner as a nested one (only a count goes up). The lock at 86CFAFA0 has IPL 22, the clock's level in VMS. SIMH delivered the KA820 interval timer at IPL 18 (24), above that lock: a tick could interrupt the holder on the same processor, its clock code got the lock "again" as the owner, and a later release found it free. At 400K a tick comes every few thousand instructions, so it happens often; at host speed rarely.
- **Source:** KA820/KA825 Processor Technical Manual EK-KA820-TM-003, table 5-2: vector C0 "Interval timer", IPL 16 (table 5-1: VAXBI devices IPL 14-17; IPINTR, RXCD, RCX50, serial lines IPL 14; CRD 1A).
- **Fix (patch 15 `opensimh_15_ka820_clock_v1.patch`, branch `ka820-clock`):** `IPL_CLKINT` 0x16. IPL 16 is also a VAXBI device level: `eval_int` checks the clock inside the hardware level loop (after IPL 17 devices, first within IPL 16), `get_vector` takes IPL 16 as the clock only if the processor's clock is pending.
- **Diagnostic test:** DEC's generic hardcore test EVKAA, test 13.5, raises the IPL to 17 and waits for the interval timer (the 11/7xx's architectural IPL 18), so it fails with IPL 16. Decision (Ulli, 2026-11-18): IPL 16 as in the manual; `VAX/tests/vax-diag_test.ini` skips EVKAA on the VAX 8200 (EVKAB, EVKAC run and pass).
- **Tests (`tests/vax8300/smp55.ini`, `smp55_rep.sh`, Ulli's VMS055_1.R82 copy):** before the fix about 5 of 8 slow runs (THROTTLE 400K or realism mode) crashed; with the clock at IPL 16 all 8 passed (4 × THROTTLE 400K with the clock change alone; with patch 15: 3 × THROTTLE 400K, 1 × realism mode). Also with patch 15: VMS V4.7 8300 `tests/kdb50/p4.ini` (with the clock change), VMS V5.5 8300 `tests/kdb50/v55_2.ini` (with the clock change), VMS V5.5 8200 `tests/kdb50/v55_1.ini`, VMS V4.7 `tests/dmb32/p3.ini` (UDA50 behind the DWBUA, printer files identical to patch 14), the 8200 diagnostics EVKAB and EVKAC.
- **Found and fixed:** 2026-11-18.

### 82 — FIXED — Bug: IPIR replaced pending inter-processor interrupts
- **Where:** `VAX/vax820_bi.c`, `WriteIPR`, MT_IPIR (patch 7; patch 14 added the node to processor mapping): `ipir = 0`, then the target bits (before patch 14 `ipir = val`).
- **Problem:** a write of IPIR sends an IPINTR to the nodes in its mask; it does not cancel anything. SIMH replaced the pending requests, so a request the other processor had not yet taken was lost when a processor sent its own.
- **Effect:** none seen: a counter in the VMS V5.5 SMP runs of 2026-11-18 (case 81) found no such loss. Found while looking for case 81.
- **Fix (patch 15):** requests are added; a request is cleared when its processor takes the interrupt or writes its PCSR (as before).
- **Found and fixed:** 2026-11-18.

### 83 — OPEN — Bug: SEND with a long string overflows SIMH's stack buffers
- **Where:** `scp.c`, `send_cmd`: the quoted string is copied into `char gbuf[CBUFSIZE]` (`get_glyph_quoted`) and decoded into `uint8 dbuf[CBUFSIZE]` (`sim_decode_quoted_string`) without a length check. `CBUFSIZE` is `128 + PATH_MAX` (4224 on Linux).
- **Seen (2026-11-18):** a test script typed a 5 KB DCL file into VMS with one `expect ... send "..."` action: `*** stack smashing detected ***: terminated`, the simulator died. Plain Open SIMH has the same code.
- **Workaround:** split long texts into several SEND actions below 4 KB (each EXPECT rule triggered by the echo of the previous part).
- **Possible fix:** check the length in `get_glyph_quoted`/`send_cmd` and refuse too long strings ("String too long"), or allocate the buffers dynamically.
- **Found:** 2026-11-18 (test of `tests/dmb32/dmb_trib.com`).

### 84 — FIXED — Bug: DMB32 sync line as DDCMP tributary (VMS V5.5) did not come up
- **Reported by Ulli (2026-11-18):** VAX 8200, VMS V5.5, DECnet line/circuit DMB-0 as DDCMP tributary 4 (`tests/dmb32/dmb_trib.com`), on a multipoint line with a VAX-11/780 DMP11 as control station (VMS V4.7, line DMP-0 `DDCMP CONTROL`, circuits DMP-0.2/.3/.4). The TCP links are up; tributary 2 (another VAX-11/780 with a SIMH DMP11) works on the same line; circuit DMB-0 stays `on -starting`.
- **What the logs show** (`SET DMB DEBUG=SYNC;PXMT;PRCV;REG;MDM` on the 8200, `SET DMP DEBUG=INFO;WARN;CONNECT;DATASUM` on the 780):
  - the control station sends STRT (select) to tributary 4 on its drop line 1; SIDRIVER answers STACK (select); the control station switches to Run ("Tributary 4 answers on drop line 1"), sends ACK with the select flag and data message 1 (12 bytes, DECnet routing init) without select (the tributary holds the line);
  - the DMB32 receives both with good CRCs and completes them to SIDRIVER, which only re-arms its receive buffers;
  - SIDRIVER then sends nothing (no ACK of data message 1, no routing init, nothing returning the select); after about 5 s it aborts both receive buffers (error 61), resets the line (LPR1 LINE.RESET) and starts over with STRT, about every 10 s; the control station returns tributary 4 to Halt on that STRT.
- **How SIDRIVER sets up the DMB32 as a tributary** (DMB32 User Guide EK-DMB32-UG-001, 3.3.34, 3.3.35, 3.3.42): LPR1 = 00040E09 (RX.ENABLE, **RX.PRIMARY = 1: secondary station**, 2 sync characters), LPR2 protocol 0 (DDCMP), **RTS (LPR2 bit 4) raised before each transmission and dropped after it**, LPR3 = 00040096 (SYNC.CHAR 96, **ADDRESS1 = 04**, the tributary address for address matching). Modem inputs constant B3 (Rx clock, Tx clock, CTS, DCD, DSR); LPR1 MODEM.SUPPRESS clear.
- **SIMH:** the DMB32 sync line keeps LPR1 RX.PRIMARY and LPR3 ADDRESS1/2 but does not act on them (no secondary station behaviour, no address matching), and its modem inputs do not follow RTS. The User Guide names the bits but does not describe what the hardware does in DDCMP secondary mode. Point-to-point DDCMP on the same line works (patch 12).
- **Cause (found by tracing SIDRIVER, VMS V5.5):** the DMB32 hardware itself does nothing for DDCMP multipoint (it ignores the select flag; DMB32 Technical Description EK-DMB32-TD-001, 2.7.2.3 and 2.7.2.5); SIDRIVER does it in software. SIDRIVER received the ACK and data message 1 correctly and passed the data to DECnet, but its transmit routine only queues a message to the DMB32 in line states that allow transmitting. As a tributary it drives an RTS/CTS handshake through a modem state machine (UCB offset 219, next state chosen from the LPR2 modem inputs): idle → RTS on → wait for CTS on → transmit → RTS off → **wait for CTS off** → idle. SIMH kept CTS on all the time, so after its first transmission (the STACK) SIDRIVER stayed in the "wait for CTS off" state, never sent DECnet's reply, and restarted the line after 5 s.
- **Fix (patch 16, `opensimh_16_dmb32_trib_v1.patch`, branch `dmb32-trib`):** while the sync line is up (connected or looped), CTS follows RTS (LPR2 bit 4), as with a modem or a null-modem cable. A write of LPR2 updates the inputs immediately, and a CTS change posts a modem status entry in SBUF (unless LPR1 MODEM.SUPPRESS is set). Point-to-point operation keeps RTS on and is unchanged.
- **Tests:** VAX 8200, VMS V5.5, tributary 4 (`tests/dmb32/dmb_trib.com`) against a VAX-11/780 VMS V4.7 DMP11 control station: circuit up (event 4.10, adjacency 1.2), `NCP LOOP NODE 1.2 COUNT 20 LENGTH 500`, DIRECTORY and COPY to the 780; no circuit downs, no selection timeouts. DMB32 point-to-point against a MicroVAX II DMV11 (`tests/dmb32/p4.sh`): PASS.
- **Note for file specs:** RMS does not accept `1.2::`; use the node name or `1026::` (area 1 × 1024 + 2).
- **Found:** 2026-11-18.

### 85 — DONE (no SIMH change) — Pitfall: %TMSCPLOAD-E-SERVE_ERR at the VMS V5.5 startup
- **Reported by Ulli (2026-11-18):** VAX 8300, VMS V5.5 (VMS055_1.R82), KLESI-B (TB) with four TU81 units, VAXBI-only: at every startup `%TMSCPLOAD-E-SERVE_ERR, error serving device MUA0:, R0 status = 00000084`, the same for MUA1-MUA3. The tapes work.
- **Checked:**
  - the same with a tape attached to MUA0, and on the MicroVAX II with a TK50 on the TQ (the same disk): not the KLESI-B, the TU81 or the 8200;
  - no TMSCP command reaches the controller while the messages appear (`SET TB DEBUG=TRACE;REQ;RSP`): VMS refuses before any I/O;
  - status 84 is SS$_DEVOFFLINE. It comes from SYS$SYSTEM:CONFIGURE.EXE (the cluster configure process), from the routine that hands a tape to the TMSCP server: it returns DEVOFFLINE when the TMSCP server is not loaded (its system pointer is 0);
  - the system has **TMSCP_LOAD 1 and VAXCLUSTER 0** (MSCP_LOAD 0, no cluster interconnect). TMSCP.EXE is cluster code (class CLUS, like SYS$CLUSTER.EXE and MSCP.EXE), which SYSBOOT loads only when the cluster code is loaded; with VAXCLUSTER 0 it is not. CONFIGURE's start-up call does not load the server, skips the "enabled automatic tape serving" message, but returns with a success status left in R0, so the serving AST still runs and every tape gets DEVOFFLINE. VMS behaviour, independent of SIMH.
- **Fix (VMS side):** TMSCP_LOAD 0 (a TMSCP server is only useful in a VAXcluster). Checked with a conversational boot (`BOOT KDB0/R5:1`, `SYSBOOT> SET TMSCP_LOAD 0`, `CONTINUE`): no TMSCPLOAD messages, MUA0-MUA3 online. Permanently (Ulli, the proper way): `TMSCP_LOAD = 0` in SYS$SYSTEM:MODPARAMS.DAT, then `@SYS$UPDATE:AUTOGEN GETDATA GENPARAMS` and `@SYS$UPDATE:AUTOGEN SETPARAMS REBOOT`. A MODPARAMS.DAT entry alone does nothing until AUTOGEN runs; a SYSGEN `WRITE CURRENT` alone would be overwritten by the next AUTOGEN. Serving tapes would need the node in a cluster (VAXCLUSTER 2), which is not wanted here.
- **Also corrected:** case 72 attributed the messages to empty TK50 drives.
- **Found:** 2026-11-18.

### 86 — OPEN — Suspicious: turning the DMB-0 circuit off logs a circuit fault
- **Seen (2026-11-19, `tests/dmb32/p4g_8200.ini` with VMS V4.7 and the backported SIDRIVER, `p4h_8200.ini` with VMS V5.5):** `NCP SET CIRCUIT DMB-0 STATE OFF` on a running point-to-point circuit logs `DECnet event 4.7, circuit down, circuit fault` with the reason `Line synchronization lost`, not an operator stop. Turning the line and circuit on again brings the circuit up; the data transfers afterwards are clean.
- **Same with both VMS versions**, so it comes from SIDRIVER's handling of the stop together with the SIMH DMB32, not from the backport. When the circuit is turned off, SIDRIVER aborts its receive and transmit buffers (DMA abort in TLNCTRL/RLNCTRL). The SIMH DMB32 completes them with the host abort error code (61, as in the User Guide). The driver may report that to DECnet as a line error.
- **Open question:** what a real DMB32 reports in SBUF for an aborted buffer, and whether SIDRIVER then logs the same event. Not checked: no real hardware and no V5.5 SIDRIVER sources.
- **Effect:** a misleading event in the operator log when the operator stops the circuit; nothing else. `tests/dmb32/p4.sh` v3 accepts a circuit fault right after `STATE OFF`.
- **Found:** 2026-11-19 (SIDRIVER backport to VMS V4.7).

### 87 — FIXED — Limitation: the TS ON-LINE switch was not visible
- **Reported by Ulli (2026-11-19):** VAX 8300, realism mode, `SET TS ENA`, `ATT TS sidriver47.tap`, `SET TS LOAD`. VMS V4.7 `MOUNT/OVERRIDE=IDENTIFICATION MSA0:` answered `%MOUNT-I-OPRQST, Please mount device _MSA0:`; with the TK50 (TQ0) it worked.
- **Reproduced** (`ts1.ini`): `SET TS LOAD` threads the tape to BOT but leaves the transport off-line, as on a real TS04/TS05, where ON-LINE is a separate switch. That is intended (sound patch v16 and later, `HELP TS`: "SET TS ONLINE ... loads the tape first if needed"). `SET TS ONLINE` while MOUNT waits completes the mount (`%MOUNT-I-RQSTDON ... mount completed successfully`), and COPY MSA0: then reads the 49 blocks. But `SHOW TS` showed only the reel, and `SET TS LOAD` said nothing about the switch.
- **Fix (patch 17, `opensimh_17_ts_online_v1.patch`, branch `ts-online`, Claude's own code):** in the realism mode `SHOW TS` ends the reel line with `, on-line` or `, off-line`, and `SET TS LOAD` reports `TS: tape loaded, the transport is off-line (SET TS ONLINE)`. The normal mode is unchanged.
- **Note for tapes without labels:** use `MOUNT/FOREIGN` (with `/BLOCKSIZE=512/RECORDSIZE=512` for a file copied block by block), not `/OVERRIDE=IDENTIFICATION`.
- **Tests:** `ts2.ini` (console: LOAD, SHOW, ONLINE, SHOW, OFFLINE, SHOW); VMS V4.7 on the VAX 8300 in the realism mode (`ts1.ini`): MOUNT, then ONLINE, mount completes, COPY.
- **Found and fixed:** 2026-11-19.

### 88 — FIXED — Bug: multipoint MOP messages carried DDCMP address 1
- **Where:** `PDP11/pdp11_dmc.c` `dmp_mp_send_session()` (patch 1): for maintenance (DLE) messages the tributary address was not put into the DDCMP header; `ddcmp_build_maintenance_packet()` sets address 1.
- **Effect:** a multipoint DMP11/DMV11 control station's MOP messages for tributary n went out with address 1. The receiving emulated station routes every message by its address (`dmp_mp_receive()`), so only tributary 1 got them; with several tributaries on a hub they went to whichever drop answered as 1. A DMV11 tributary must find its own address (and password) in the control station's Enter MOP Mode message (EK-DMVQM-UG-003 4.7.1).
- **Fix (patch 18, `opensimh_18_dmv11_boot_v1.patch`, branch `dmv-boot`):** the tributary address goes into every message of a session, maintenance messages included. Point-to-point is unchanged (address 1).
- **Not tested with VMS yet** (multipoint MOP, e.g. `NCP LOOP CIRCUIT` on a tributary circuit).
- **Found and fixed:** 2026-11-20.

### 89 — OPEN — Limitation: the DMV11 unit number for booting is not passed on
- **Where:** `PDP11/pdp11_dmc.c`, DMV11 primary MOP boot (patch 18).
- **What:** E101 SW2 selects unit 0 or 1 "for booting", so that the loaded program can tell which of two DMV11s booted it (EK-DMVQM-UG-003 Table 2-6, 4.7.4). The manual does not say how the program learns it (a register, a CSR, a MOP field). `SET DMVn BOOTUNIT` keeps and displays the switch only.
- **Needed:** the DMV11 technical manual or a program that uses it (e.g. a DECnet-11 secondary loader for the DMV11).
- **Found:** 2026-11-20.

### 90 — FIXED — Suspicious (cosmetic): SHOW DMV0 says DMP0 in the PDP-11 simulator
- **Seen:** `pdp11`, `SET CPU 11/73`, `SET DMV ENABLE`, `SHOW DMV0` → `DMP0    not attached, peer=unspecified, ...`. `SHOW DMV` names the device correctly.
- **Cause (likely):** the DMP and DMV devices share their units (`dmp_units`, see the comment at `dmp_dev`); SCP finds the DMP device first when it names a unit. Only the PDP-11 simulator has both devices.
- **Effect:** cosmetic.
- **Found:** 2026-11-20 (DMV11 switch work).
- **Cause (confirmed):** `sim_uname()` names a unit after `find_dev_from_unit()`, which takes the first device in the device list that has the unit (DMP) and caches it in `uptr->dptr` and `uptr->uname`.
- **Fix (patch 21, branch `dmp-dmr-boot`):** `dmc_reset()` sets `dptr` and the name of every shared unit (lines, polling and timer unit) to the device in use: DMP on a Unibus, DMV on a Qbus. A `SET CPU` that changes the bus resets the devices and so renames the units.
- **Tests:** PDP-11 `SET CPU 11/73`, `SET DMV ENABLE`, `SHOW DMV0` → `DMV0 ...`; then `SET CPU 11/70`, `SET DMP ENABLE`, `SHOW DMP0` → `DMP0 ...`; MicroVAX II `SHOW DMV0` and VAX-11/780 `SHOW DMP0` name their own device.
- **Fixed:** 2026-11-22.

### 91 — OPEN — Suspicious: outgoing PEER connection to a plain TCP listener reported as failed
- **Seen (Claude's workspace, 2026-11-20):** a DMV11 line with `PEER=127.0.0.1:21001` (and also `localhost`) connected to a Python script listening on that port. The script accepted the connection, but the simulator logged `tmxr_poll_conn() - Outgoing Line Connection to 127.0.0.1:21001 failed` and closed it, again on every retry. A connection from the script to the simulator's ATTACH port works.
- **Possibly:** `sim_check_conn()` sees the socket in the exception set of `select()`, or `getpeername()` fails. It could also be the workspace's network setup. SIMH-to-SIMH pairs work, because each side also accepts the other's incoming connection.
- **Effect:** none for SIMH pairs; test scripts connect to the ATTACH port instead (`tests/dmv11_boot/`).
- **Found:** 2026-11-20, not investigated.

### 92 — FIXED — Bug: DDCMP maintenance messages never completed for the host (also in upstream Open SIMH)
- **Where:** `PDP11/pdp11_dmc.c`, `dmc_complete_transmit()`; the same code is in upstream Open SIMH 87eb7d5.
- **What:** a sent transmit buffer of type `TransmitData` always went to the ack-wait queue. A DDCMP maintenance message (DLE, MOP) has no sequence number and is never acknowledged, so it stayed there: the host never got the transmit completion.
- **Seen:** VMS V4.7 on a MicroVAX II, DMV11 circuit DMP-0, `NCP LOAD NODE PDP` and `NCP TRIGGER NODE PDP`: the DMV sent Enter MOP Mode, the PDP-11's DMV11 answered with Request Program, VMS put it in a receive buffer, but NCP hung. The debug log showed `ACKW:1` from the Enter MOP message on.
- **Fix (patch 18 v2, commit 70290323 on `dmv-boot`):** a sent maintenance message goes straight to the completion queue. Afterwards VMS answered the Request Program with a Memory Load with Transfer Address and the PDP-11 program ran.
- **Also affects:** `NCP LOOP CIRCUIT` over a DMC11/DMR11/DMP11/DMV11 (not re-tested yet; the DMF32 has its own code and worked before).
- **Found and fixed:** 2026-11-21.

### 93 — OPEN — Suspicious: two DMV11s that both have PEER keep reconnecting
- **Seen (2026-11-21):** MicroVAX II (VMS V4.7) `SET DMV0 PEER=127.0.0.1:21202`, `ATTACH DMV0 21201`; PDP-11/73 `SET DMV0 PEER=127.0.0.1:21201`, `ATTACH DMV0 21202`, remote load detect on. The PDP-11 log had nine "Outgoing Line Connection ... established", each followed shortly by `tmxr_close_ln()`; the VMS log had crossing incoming connections ("aborting outgoing line connection attempt"). The Enter MOP Mode message never arrived.
- **Possibly:** the DTR drop of each VMS master clear closes the line, and then both sides connect to each other at the same time.
- **Workaround:** `PEER` on one side only. Without `PEER` on the PDP-11 (it only listens) the line was stable in every run.
- **Found:** 2026-11-21, not investigated.

### 94 — OPEN — Suspicious: "fatal hardware error" at SET LINE DMP-0, once
- **Seen (2026-11-21):** VMS V4.7, MicroVAX II with DMV11 (`SET DMV DEBUG=INFO;WARN;CONNECT;MODEM`), `@SYS$MANAGER:STARTNET` while a PDP-11 simulator ran on the same host: `%NCP-I-NMLRSP, listener response - Operation failure`, `%SYSTEM-F-ABORT, abort, fatal hardware error`; then the circuit was unknown. The debug log ended with "DMV8: Master clear". Four other runs (two of them with `REG;CMD` debug) started the line normally.
- **Possibly:** a timing dependency in the master clear/RUN handshake under host load.
- **2026-11-21:** case 97 shows how a fully loaded host makes asynchronous completions arrive within very few simulated instructions; this case was also seen only with two simulators running without IDLE, so it may be the same kind of timing problem. Not seen with `SET CPU IDLE=VMS`.
- **Found:** 2026-11-21, not investigated.

### 95 — OPEN — Finding (not SIMH): two slips in remwrt.mac
- **Where:** `remwrt.mac` (Lee Gleason, rsx11.blogspot.com, January 2024), as Ulli sent it.
- **Line 728:** `mov #^X8000,b.eol` has no `(r5)`. It assembles to `MOV #100000,14` (PC-relative), so it writes 100000 into location 14, the BPT vector, instead of the buffer descriptor. The neighbouring lines use `b.sw1(r5)` and `b.sw2(r5)`.
- **Line 1411-1413:** `crlf: .byte 13, 15` with the comments "line feed" and "carriage return". The default radix is octal, so 13 is VT, not LF (12): every new line on the console is VT CR.
- **Effect:** on a console, lines are not advanced by LF; the BPT vector is overwritten. Possibly what the blog's warning about the console output refers to.
- **Seen (2026-11-21):** remwrt loaded over the DMV11 by VMS V4.7 (`SET DMV0 PROGRAM=SYSTEM`, patch 18 v3) printed its banner and prompt all on one console line.
- **Found:** 2026-11-21.

### 96 — OPEN — Finding (not SIMH): macro11 v0.3 cross assembler
- **Where:** github.com/shattered/macro11 v0.3, used to assemble remwrt.mac (Ulli allowed it).
- **Local labels:** 5-digit local labels such as `10000$` give "Bad branch target" by default; `-ysl 8` (symbol length 8) fixes it.
- **Hex numbers:** `^X0FF` (hex starting with a digit) was read as the global symbol `0FF`. Fixed locally in `parse.c` (hex digit check before the numeric branch); the diff is in `tests/dmv11_vmsload/macro11_v0.3_local.diff`.
- **Strings:** `.ASCIZ <15><12>/TEXT/` (valid MACRO-11: bytes in angle brackets, then a delimited string) is read as the expression 12 divided by the symbol `TEXT`. Not fixed; worked around with separate `.BYTE` and `.ASCII` lines.
- **Found:** 2026-11-21.

### 97 — FIXED — Bug: VMS V4.7 boot freezes when an MSCP read completes too fast
- **Seen (2026-11-21, Claude's workspace, DMV11 regressions):** two `microvax2` simulators (VMS V4.7, copies of VMS047.R81) started together. After `PLEASE ENTER DATE AND TIME` is answered, one node (sometimes both) prints nothing more; SIGINT shows PC 80008B1F (`BRB 80008B1F`, the VMS null loop). Not the DMV11: also without a DMV11, with the build before patch 18, and with plain Open SIMH 87eb7d5.
- **Reproducible on one node:** `SET NOASYNCH` (synchronous disk I/O) freezes every boot at the same place, without a second simulator (`/home/claude/vmsreg/single.py`; upstream 87eb7d5 too).
- **State at the freeze** (`hangprobe.py`, VMS symbols from SDA on a healthy node): the clock runs and its interrupts are taken (about 100 ticks/s acknowledged), the system time advances, no Qbus interrupt is pending, the console terminal is idle (TTO DONE, IE), the MSCP rings are empty (every command slot host-owned, every response slot controller-owned, no packets busy). The only process besides NULL and SWAPPER, PID 00010002 (SYSINIT), is in MWAIT with resource 1 (RSN$_ASTWAIT, "RWAST"); nothing is computable, so VMS idles for ever.
- **The trigger** (RQ `TRACE;REQ` debug, frozen and healthy boot compared): both boots issue exactly the same 535 MSCP commands. The 535th is `READ lbn=34113 bc=F000` (60 KB): LBN 34113 is block 2 of **F11BXQP.EXE** (the Files-11 XQP), read again after the date. In the healthy boot (asynchronous I/O) its response comes about 2,800 instructions after the poll and VMS goes on with `READ lbn=94CF`; in the frozen one (synchronous) it comes about 200 instructions after the poll, and VMS never issues another command.
- **Timing threshold:** with `SET NOASYNCH`, `DEP RQ XTIME 200` (the default) freezes, 400 and 1000 boot normally. With asynchronous I/O (the default build) the completion time in instructions depends on the host: when both host cores are busy (two simulators without IDLE on the 2-core workspace), the CPU thread gets so little time that the I/O thread's completion arrives within a few hundred simulated instructions, hence the occasional freezes; `SET CPU IDLE=VMS` frees the host and they disappear.
- **Cause:** a timing assumption in VMS V4.7 (MicroVMS) when SYSINIT reads the XQP image: a response within a few hundred instructions of the poll is never possible on a real RQDX3/UDA50 (milliseconds), but SIMH's default RQ timing (`QTIME` 100, `XTIME` 200 instructions) allows it. Which VMS code misses the completion (it then waits in RWAST) was not traced further.
- **Workarounds:** `SET CPU IDLE=VMS` (the usual setting anyway), or a slower RQ: `DEP RQ XTIME 1000`. Not affected: the realism mode, which gives the RQ real drive times.
- **Possible fix (decision for Ulli):** raise the RQ's default `XTIME` (e.g. 1000 instructions, still far below real hardware) or make the minimum completion time per transfer larger; would affect every simulator with an MSCP controller.
- **Fix (patch 19, `opensimh_19_rq_xtime_v1.patch`, branch `rq-xtime`, Ulli's decision):** `RQ_XTIME` 1000 instructions on the VAX (was 200; the PDP-11 keeps 500). `rq_io_complete()` schedules a completion not before `iostarttime + XTIME`, so this is also the minimum with asynchronous I/O.
- **Tests:** SET NOASYNCH 3 of 3 boots (before 0 of 3); two simultaneous boots without IDLE 10 of 10 (before about half froze); with SET NOASYNCH 6 of 6 (before 0 of 6). Disk regressions: RA console tests ra1-ra7 and RC25 c1, c2, c4, c5, c6, c10 give the same results as the build before (c6 only differs in a `?53` self-test error of case 22, on the old build at another place); RA VMS test N1, RX50 VMS test vms1 (both diskettes mounted, 0 differences); DMV11/DMP11 pair runs.
- **Found and fixed:** 2026-11-21.

### 98 — OPEN — Test script: ratest.py realism tests predate the SDI rule
- **Where:** `tests/ra/ratest.py` (v4 or older), tests R1, U1, T1.
- **Seen (2026-11-21, disk regressions for patch 19):** R1, U1 and T1 log `%SIM-ERROR: Unit disabled: RQ0` / `RQ1` many times; VMB then cannot boot, and the script goes on typing DCL lines at `>>>` (T1 waited for ever). N1 (normal mode) passes. The build before patch 19 (`old18`) gives exactly the same R1 log (28 "Unit disabled" lines), so it is the script, not the RQ change.
- **Cause (likely):** the SDI rule of 2026-11-16 (case 68) disables unattached non-SDI units at SET REALISM ENABLED; the console tests ra1 and ra5 were adapted then (`SET RQn ENABLED` before giving a unit an RA type), ratest.py's generated .ini files were not.
- **Needed:** adapt ratest.py like ra1/ra5 (enable each unit again before `SET RQn RA81`, one at a time).
- **Found:** 2026-11-21.

### 99 — FIXED — Limitation: DMC11 line unit switches and remote load detect
- **Where:** `PDP11/pdp11_dmc.c`, lines of the DMC device with `TYPE=DMC`.
- **What:** the DMC11 IPL microprocessor maintenance manual (EK-DMCMP-MM-001) says that the line unit (M8201/M8202) has two 8-switch packs read by the microcode (IBUS registers 15 and 16) whose meaning the microcode defines, and that on an Enter MOP Mode with the right password (4 equal bytes after code 6) the DMC11 either triggers a ROM bootstrap with an offset, starts a down-line load itself (INIT, CPU in a tight loop, Request Program 8,12,1, Memory Load with Transfer Address) or passes the message on, depending on these switches. Which switch means what is not in the uploaded manuals.
- **Now:** `SET DMCn PASSWORD/BOOTOFFSET/PROGRAM` and `SHOW DMCn SWITCHES` refuse on a DMC11 with "The DMC11 line unit switches are not simulated (SET DMCn TYPE=DMR for a DMR11)"; the DMR11 (`TYPE=DMR`) has its switches and boot.
- **Needed:** a DMC11 line unit manual (or the DMC11 user's guide) with the meaning of switch packs 2 and 3; Ulli will upload one.
- **Found:** 2026-11-22.
- **What the line unit manuals say** (EK-DMCLU-OP-001 and EK-DMCLU-MM-001, uploaded 2026-11-22; scanned, read with OCR): switch pack no. 2 is register 15 (E87 on the M8201, E90 on the M8202), no. 3 is register 16 (E88 / E91); both are shipped all OFF (377); they serve the down-line loading, and the microcode (the microprocessor manual, chapter 3) decides their meaning. Neither manual has a bit table.
- **Which pack is which:** the DMC11 diagnostic MAINDEC-11-DZDMG-C-D (CROM and jump tests, bitsavers) reads the packs with two microinstructions, 320 named the line number switch and 340 the BM873 address; 320 and 340 address IBUS registers 15 and 16. Its configuration table keeps them as "switch pack 1 (DDCMP line number)" and "switch pack 2 (BM873 boot address)". So register 15 (pack no. 2) is the value repeated in the Enter MOP Mode message, the password, and register 16 (pack no. 3) the boot ROM offset; the microprocessor manual (3.4.11) calls it an 8-bit word offset. This is the same assignment as on the DMR11 (E134 = register 15 password, E121 = register 16 offset).
- **Not documented:** which switch setting makes the DMC11 start the ROM, load the system itself (the later microcode: INIT, tight loop, Request Program 8,12,1, Memory Load with Transfer Address) or pass the message on.
- **Fix (patch 21 v2, branch `dmp-dmr-boot`, commit 88102133):** the DMC11 is a board of the shared model: `SET DMCn PASSWORD=n`, `BOOTOFFSET=n` (octal; both 377 by default, as shipped), `PROGRAM`, `SHOW DMCn SWITCHES`, registers `SP2`, `SP3`. Password 377 passes the message on. With another password, an Enter MOP Mode with four matching password bytes starts the boot ROM at 173000 + 2 × offset if one answers there (power-fail vector, as the DMR11), otherwise the DMC11 loads the system itself with Request Program `08 0C 01` (MOP device type 12; with `PROGRAM` other than SECONDARY the program type is added). The same rule as Ulli's decision for the DMR11.
- **Tests:** `tests/dmv11_boot/` R3 (defaults 377/377, word offset 010 = 173020), R4 (DMC11 loads the test program itself, wrong password first), R5 (DMC11 with a ROM at 173000, offset 010 words: CPU restarted at 173020); T1-T6, P1-P4, R1-R2 unchanged: PASS. VAX-11/780: SHOW DMC0 SWITCHES works, the boot switches are refused.
- **Fixed:** 2026-11-22.

### 100 — OPEN — Limitation: DMR11/DMP11 boot details not modelled
- **Where:** `PDP11/pdp11_dmc.c`, patch 21.
- **What is not modelled:**
  - the DMR11's answers to an Enter MOP Mode that is not a boot (wrong password, password 377, bad CRC; EK-DMR11-UG-002 Table 3-8: halted or initialized no action; running: halt and report a maintenance message; maintenance mode: a CRC error is counted). In the simulation such a message goes the normal way (the DDCMP engine's maintenance message handling), or is dropped while the host gives no receive buffer;
  - the guide's remark that only DMR11s at the first two unit addresses can use the bootstrap (the M9301-YJ offsets 356/374 are for units 0 and 1); the simulation boots any line;
  - the BSEL1 lockout switch (M8207 E85 SW1 on the DMP11/DMR11, M8200 E76 SW9 on the DMC11): with it OFF the maintenance functions are inhibited and the run bit stays set; the simulation behaves as with lockout off;
  - the DMC11's own load initializes all Unibus peripherals (INIT) before it takes the CPU over (EK-DMCMP-MM-001 3.4.11); the simulation does not reset the other devices (as with the DMV11 boot engine);
  - which DMC11 switch setting selects the boot ROM, the own load or passing the message on (case 99): the simulation starts a ROM that answers at the boot address, else loads itself.
- **Effect:** none for the normal use (VMS, RSX DECnet, the boot itself).
- **Found:** 2026-11-22 (DMP11/DMR11 boot work).

### 101 — FIXED — Bug (cosmetic): HELP BOOT called the DMP device a DMV11
- **Reported by Ulli (2026-11-22):** `HELP BOOT` in the PDP-11 simulator (default CPU 11/73, Qbus) showed `DMP     yes    -    -      -        bus   DMV11 Synchronous network controller`.
- **Where:** `PDP11/pdp11_dmc.c`, `dmp_description()` (patch 1, used by the DMP and the DMV device): it chose the text by the CPU's bus (`UNIBUS`), not by the device. On a Qbus CPU both devices said DMV11, on a Unibus CPU both DMP11. The columns of the line were right (Unibus only, "bus": not on the current Qbus CPU).
- **Fix (patch 21 v3):** the text follows the device (`dptr == &dmv_dev`). `HELP BOOT` also names `BOOT DMPn` and the remote load detect of the DMC11/DMR11 now, and says that SIMH has no boot ROM images of its own.
- **Found and fixed:** 2026-11-22.

### 102 — FIXED — Bug: the DMC/DMP/DMV connection poll was starved by frequent resets
- **Where:** `PDP11/pdp11_dmc.c`, `dmc_reset()`: it cancelled every unit of the device, the connection poll unit included, and started the poll again `DMC_CONNECT_POLL` (2) seconds of simulated time later.
- **Seen (2026-11-22):** DEC's M9312 DECnet boot ROM ('XM', 23-862A9/863A9/864A9) started by a DMR11 remote load detect executes RESET at every retry, about every half second. Each RESET dropped the line (case 103), and as every RESET pushed the poll out again, the load host's new connection was never accepted: the ROM's Request Programs stayed on the transmit queue.
- **Fix (patch 21 v3):** a reset cancels only the line units and keeps a scheduled poll; it starts the poll only when none is scheduled.
- **Tests:** `tests/dmv11_boot/` R6 (DMR11) and R7 (DMC11): the M9312 ROM boots the test program; T1-T6, P1-P4, R1-R5 unchanged; VMS V4.7 DECnet pairs DMV11 and DMP11, point-to-point and multipoint (`tests/dmv_regress/pair.sh`): PASS (3 of 3 compares each).
- **Found and fixed:** 2026-11-22.

### 103 — FIXED — Suspicious (decision for Ulli): a master clear drops DTR and closes the line
- **Where:** `PDP11/pdp11_dmc.c`, `dmc_process_master_clear()`: `dmc_clr_modem_dtr()` unless a DMV11 boot keeps the line; with modem control passed through, DTR off closes the TCP connection. The same in upstream Open SIMH. A bus INIT (RESET instruction, `dmc_reset()`) master-clears the board too.
- **Manual:** DMC11 microprocessor manual EK-DMCMP-MM-001 3.4.14: the microprocessor keeps Data Terminal Ready on all the time, drops it after Data Set Ready goes off or after a procedure error, and asserts it again when the device is master cleared. So a master clear does not hang up the line. (The DMV11 technical manual describes its own DTR rules, used since patch 18 v2.)
- **Effect:** every RESET of a program, e.g. DEC's M9312 DECnet boot ROM at each retry, closes the TCP line to the load host, which must connect again (a SIMH peer with PEER does; the test station `mopctl.py --rom` does). Probably also a few extra line restarts when an operating system master-clears its DMC.
- **Possible change:** keep DTR at a master clear (DMC11/DMR11; the DMP11 and DMV11 by their manuals); would change the behaviour of every DMC/DMR/DMP/DMV line and needs the DECnet regressions. Not done: Ulli decides.
- **Found:** 2026-11-22.
- **Decision (Ulli, 2026-11-22):** keep DTR at master clear, for every DMC/DMR/DMP/DMV line.
- **Fix (patch 21 v4, commit 7a82c0a1):** `dmc_process_master_clear()` no longer drops DTR and asserts it on an attached line (not on a multipoint hub's shared line, which its sessions handle). The deliberate drops stay: the HALT command, the DMP/DMV station halt, the DMC's "drop DTR" microinstruction (used by the VMS driver). DTR was also the DMC11's sign that the host had started the line (`ddcmp_UserStartup`); with DTR kept, that sign is now the host's start itself for every board (case 105).
- **Tests:** `tests/dmv11_boot/` T1-T6, P1-P4, R1-R7 PASS; in R6/R7 the M9312 ROM's RESETs no longer drop the line. VMS V4.7 DECnet pairs (`tests/dmv_regress/pair.sh` v7, two nodes, copies there and back, circuit off/on): DMC11 and DMR11 (vax780, new in v7), DMV11 (microvax2) and DMP11 (vax780) point-to-point and multipoint: all six PASS (3 of 3 compares each).
- **Fixed:** 2026-11-22.

### 104 — OPEN — Suspicious: ROM device image byte order and odd sizes
- **Where:** `PDP11/pdp11_rom.c` (upstream): `rom_rd()` reads `((uint16 *)filebuf)[offset >> 1]`, the file's bytes as host-order words; `rom_attach()` takes the file size as the ROM size.
- **Effect:** on a little-endian host (x86, ARM) the image is little-endian words, as in PDP-11 memory (HELP ROM now says so); on a big-endian host the bytes of every word would be swapped. A file of odd size makes the last word read one byte past the buffer.
- **Possible fix:** build the word from the bytes (`b[off] | (b[off+1] << 8)`), and refuse or pad odd sizes.
- **Found:** 2026-11-22 (Ulli's question about the ROM format).

### 105 — FIXED — Bug: a maintenance mode request started DDCMP on a connected line
- **Where:** `PDP11/pdp11_dmc.c`, the DDCMP state table: in the Halt state rule 1 (`UserStartup && LineConnected` → IStart, send STRT) is tested before rule 2 (`UserMaintenanceMode` → Maintenance). `ddcmp_UserStartup()` was true for a DMC11/DMR11 whenever DTR was on, and for a DMP/DMV whenever the host had started the line, also with a maintenance mode request (CONTROL IN with MAINT, DMP key MAINT).
- **Effect:** on a connected line a request for maintenance mode started DDCMP: the board sent STRT and stayed in IStart; it reached maintenance mode only when the peer's MOP message arrived (rule 8). VMS as a load host (which sends first) did not notice; DEC's M9312 DECnet ROM, which sends its Request Program first, was stuck with its message queued once DTR was kept at master clear (case 103) and the line stayed connected.
- **Fix (patch 21 v4, with case 103):** `ddcmp_UserStartup()` is false while a maintenance mode request is being dispatched, and for every board requires that the host started the line (state Running: CONTROL IN, the first transmit buffer, DMP START), since DTR is no longer a sign of that.
- **Found and fixed:** 2026-11-22.

### 106 — FIXED — Limitation: the 11/60 and 11/70 hit/miss register always reads 0
- **Where:** `PDP11/pdp11_cpumod.c`: `HITMISS` is only cleared and read, never set; SIMH has no cache model. The same in upstream Open SIMH.
- **Seen (2026-11-22):** DEC's M9312 diagnostic ROM 23-616F1 (11/60, 11/70; started by its switch register boot at 165744 with SR = 000006, i.e. the XM boot ROM's entry with diagnostics) writes and reads memory with the cache on and then rotates the hit/miss register (177752), expecting a hit in bit 0. With 0 it halts at 165554. CONTINUE takes the ROM's own way on: it forces cache misses (cache control 177746 = 14) and continues; the boot then works. On the 11/44 the 23-446F1 has no such test and runs through.
- **Possible change:** report hits while the cache is not forced to miss (CCR bits 2-3), e.g. read the hit/miss register as all ones (the six most recent accesses hits). Real hit/miss behaviour would need a cache model; programs that only check for hits would be satisfied.
- **Tests:** `tests/m9312_roms/` C4 (11/70), C5 (11/60) expect the halt and continue.
- **Found:** 2026-11-22.
- **Fixed (patch 22, branch `m9312-roms`, commit 152182d5; Ulli's decision of 2026-11-22):** the 11/44, 11/60 and 11/70 hit/miss register reads 077 (the last six references hits) unless CCR bits 2 and 3 force misses in both cache halves (then 0); the value is also kept in HITMISS for EXAMINE. The J11 keeps its own rule. The 11/70's two cache halves have one force-miss bit each (bits 2, 3); 616F1 expects hits with one half forced and turns both off (CCR 14) after a failure.
- **Tests (patch 22):** C4 and C5 run through without a halt; C1-C7, P1-P3, Z1, X1 and `tests/dmv11_boot/` T1-T6, P1-P4, R1-R7 PASS.

### 107 — FIXED — Limitation: the console DL11 has no maintenance loopback
- **Where:** `PDP11/pdp11_stddev.c`: `TTOCSR_RW` is only the interrupt enable; the DL11's maintenance bit (XCSR bit 2), which connects the transmitter output to the receiver input, is not modelled. The same in upstream Open SIMH.
- **Seen (2026-11-22):** DEC's 11/24 diagnostic ROM 23-774F1 sets the maintenance bit, sends every byte 0-377 and waits for each in the receiver (165626-165702): in SIMH it waits for ever (PC 165652). Skipped with a breakpoint, the rest of the ROM works.
- **Possible change:** a writable maintenance bit; while it is set, a transmitted character goes to the receiver buffer (with receiver done) instead of the terminal. The same applies to the DL11 lines of the DLI device (`pdp11_dl.c`) and probably the VAX console.
- **Tests:** `tests/m9312_roms/` C7 skips the loopback test.
- **Found:** 2026-11-22.
- **Fixed (patch 22, branch `m9312-roms`, commit 152182d5; Ulli's decision of 2026-11-22):** XCSR bit 2 is readable and writable; while it is set, a transmitted character goes to the receiver buffer (receiver done, interrupt if enabled) instead of the terminal, and typed input is dropped (the line is disconnected). Only the console DL11; the DLI lines (`pdp11_dl.c`) and the VAX consoles are unchanged.
- **Tests (patch 22):** C7 without the breakpoint (774F1 runs through); L1 (no image): a program loops 'A' back and reads it, nothing reaches the terminal.

### 108 — FIXED — Bug (minor): a RESET drops the console character being sent
- **Where:** `PDP11/pdp11_stddev.c`, `tto_reset()`: `sim_cancel (&tto_unit)` discards the character in progress. The same in upstream Open SIMH.
- **Manual behaviour:** a DL11's transmitter finishes shifting out the character it has; bus INIT clears the registers, not the line.
- **Seen (2026-11-22):** DEC's 23-774F1 prints the memory size in 8 octal digits (01000000 for 256 KB) and returns to the boot ROM, which executes RESET at once: SIMH shows 0100000, the last digit is lost. Any program that ends with a message and a RESET (or a boot ROM started right after a console message) loses the last character.
- **Possible fix:** at a bus INIT let a pending output event finish (output the character, keep the transmitter "busy" until then) instead of cancelling it; a power-up or SIMH RESET command may still cancel.
- **Tests:** `tests/m9312_roms/` C7 expects 0100000 for now.
- **Found:** 2026-11-22.
- **Fixed (patch 22, branch `m9312-roms`, commit 152182d5; Ulli's decision of 2026-11-22):** `tto_reset()` outputs a character still being sent (unless the line is looped back) before it clears the registers, at every reset.
- **Tests (patch 22):** C7 shows all 8 digits (01000000); L1: 'Z' sent right before RESET appears.

### 109 — FIXED — Limitation: DEUNA/DELUA port command BOOT not implemented
- **Where:** `PDP11/pdp11_xu.c`, `xu_port_command()`, `CMD_BOOT`: prints "BOOT command not implemented!". The same in upstream Open SIMH.
- **Seen (2026-11-22):** DEC's M9312 'XE' boot ROMs (23-E32A9 + 23-E33A9) start the port, read the default physical address (port function 2, PCBB at 700), print ETHERNET PHYSICAL ADDRESS = 08-00-2B-12-34-56 (correct), give BOOT and wait at location 2 (BR .) with the CSR at 4 for the board to load the system. In SIMH nothing comes.
- **Possible change:** implement the board's MOP boot (Request Program on the Ethernet, Memory Loads by DMA into memory, start of the loaded program as the DEUNA/DELUA documentation defines it). Needs the DEUNA/DELUA user's guide for the details (which program is requested, where it goes, how the CPU's loop at 2 is ended) and a MOP load host for tests (a Python one over SIMH's UDP Ethernet transport, or VMS/RSX).
- **Tests:** `tests/m9312_roms/` X1 expects the address printout and the refusal.
- **Found:** 2026-11-22.
- **Decision (Ulli, 2026-11-22):** implement it with the manuals; Ulli uploaded the DEUNA Technical Manual EK-DEUNA-TM-PRE and the DELUA User's Guide EK-DELUA-UG-002 (DEC copyright: facts only).
- **What the manuals say (DELUA UG chapter 5, DEUNA TM 4.3):** BOOT (port command 5) puts the board into the primary load state (DNI set). The board sends a MOP Request Program (code 8; DELUA device type 11, program type 0, processor 0 = system; software ID from write system ID, function 23; to the load server address, function 25, else the load assistant multicast AB-00-00-01-00-00), repeats it every 5 s up to 8 times, then (system software) with software ID 0 to the multicast address every 30 s. The answer is a Memory Load with Transfer Address whose image (the secondary loader) the board stores in **its own internal memory** and runs on its own processor; that program fetches a tertiary loader into host memory and starts it; the tertiary (a DEUNA/DELUA port driver) loads the operating system. For a remote boot frame (MOP remote console 60-02, code 6, verification code) with BOOT SEL = system load the board first takes the host over: it writes a small program at 2-34 (2: BR ., 4: its PCSR0 address, 24/26: power-fail vector 30/340, 30: MOV #1000,SP, 34: BR 2) and asserts ACLO, so the CPU loops at 2. The manuals do not describe the secondary loader's own requests or how the host's loop at 2 is ended.
- **Decision (Ulli, 2026-11-22):** option 1, SIMH plays the board's secondary loader. The UNA loaders SECUNA.SYS and TERUNA.SYS were found on Ulli's DECnet-11M V4.2 kit tape ([136,54]); SECUNA (code for the board's own processor) shows the whole protocol: Request Program type 1 for the tertiary, Memory Loads into host memory answered by Request Memory Load, start of the host by writing 000137/transfer address at 6/10 and 012701 at 2 (R1 = CSR from location 4). Design: `docs/DEUNA_boot_design.md`.
- **Fixed (patch 23, branch `deuna-boot`, commit e2fc1426):** BOOT puts the board into the primary load state and asks for a secondary loader (Request Program type 0, device type 1/11, every 5 s, after 8 tries every 30 s to the multicast with software ID 0); once one arrives SIMH does the UNA secondary loader's work (tertiary into host memory, load host = load server address, host start through 2/4/6). `SET XU DEBUG=BOOT`.
- **Tests (patch 23):** `tests/deuna_boot/` B2-B8 PASS: BOOT from a program and through DEC's 'XE' ROMs, retries, a repeated load, the DELUA, refusal without a network, and DEC's own SECUNA/TERUNA: TERUNA runs on the PDP-11, drives the DEUNA through its normal port and loads a test system. `tests/m9312_roms` X1 v2 and the other ROM tests PASS. VMS V4.7 DECnet over a DEUNA on two VAX-11/780s (`tests/dmv_regress/pair.sh` v8, `DEV=UNA`): PASS (3 of 3 compares).
- **Patch 23 v2 (2026-11-23, Ulli asked for it):** SIMH's `BOOT XU` (and `BOOT XUB`) puts a bootstrap at 002000 that does what the 'XE' ROM does, so no DEC ROM image is needed; HELP XU has a Booting topic (PDP-11) naming the ROM route as the alternative. `tests/deuna_boot` B10-B12 PASS.

### 110 — FIXED — Bug: SET TTI DEBUG=DAT aborts the simulator on a NUL byte
- **Where:** `PDP11/pdp11_stddev.c`, `tti_svc()`: `sim_debug (DBG_DATA, ..., "Received data: 0x%02X '%c'\n", c, c)`; for c = 0 the formatted line has a NUL, and `_sim_vdebug()` (`scp.c`) prints "has an imbedded \0 character. DON'T DO THAT!" and calls `abort()`. The same in upstream Open SIMH.
- **Seen (2026-11-22):** typing the bootstrap tape for the M9312 'PR/TT' ROM's TT entry into the console (SEND of 8-bit data with NULs) with TTI debugging on: the simulator aborted. Reproduced with `send "A\x00B"`.
- **Possible fix:** print the character only when it is printable (`isprint`), e.g. `'%c'` with `(c >= 040 && c < 0177) ? c : '.'`.
- **Found:** 2026-11-22.
- **Fixed (patch 22, branch `m9312-roms`, commit 152182d5; Ulli's decision of 2026-11-22):** the debug line prints '.' for a non-printable character.
- **Tests (patch 22):** D1 (no image): bytes E9, 41, NUL, 42 typed with TTI debugging on; the simulator reads all four, the log shows `0x00 '.'`.

### 111 — FIXED — Bug: DEUNA/DELUA STOP when not running, START when running
- **Where:** `PDP11/pdp11_xu.c`, `xu_port_command()`: CMD_STOP outside the running state and CMD_START in the running state set PCEI (port command error). The same in upstream Open SIMH.
- **Manual:** DELUA User's Guide table 4-1: STOP not in the running state "acts as a NO-OP and sets the done interrupt (DNI)"; START in the running state "does nothing but set the done interrupt".
- **Seen (2026-11-22):** DEC's UNA tertiary loader TERUNA.SYS (DECnet-11M V4.2) gives STOP first; with PCEI its command helper failed and it went into its error loop.
- **Fix (patch 23):** both set DNI as the manual says.
- **Found and fixed:** 2026-11-22.

### 112 — FIXED — Bug: DEUNA/DELUA counters written beyond the UDB
- **Where:** `PDP11/pdp11_xu.c`, `xu_command()`, FC_RDCTR/FC_RDCLCTR: `Map_WriteW(udbb, 68, ...)` whatever UDB length the PCB gives. The same in upstream Open SIMH.
- **Manual:** DELUA User's Guide 4.5.8: PCB+6 <15:01> is the length of the UDB in words, up to 55; with a shorter length the board returns a truncated list.
- **Seen (2026-11-22):** TERUNA reads and clears the counters into a 4-word UDB right below its transmit buffer; SIMH's 68 bytes overwrote its MOP Request Program (the frame went out as zeros with one 04).
- **Fix (patch 23):** the transfer is PCB+6 bytes (bit 0 cleared), at most 110.
- **Found and fixed:** 2026-11-22.

### 113 — FIXED — Bug: DEUNA/DELUA Read Load Server Address inverted
- **Where:** `PDP11/pdp11_xu.c`, `xu_command()`, FC_RLSA: `if (eth_mac_cmp(load_server, eth_mac_any))` returns the multicast address "if not set", but eth_mac_cmp is 0 when equal, so the multicast came back exactly when a load server was set (and the zero address when none was). The same in upstream Open SIMH.
- **Seen (2026-11-22):** after the boot (which sets the load server address as DEC's UNA secondary loader does) TERUNA asked the multicast address for the system instead of its load host (before patch 23: the zero address).
- **Fix (patch 23):** the test is `!eth_mac_cmp(...)`.
- **Found and fixed:** 2026-11-22.

### 114 — FIXED — Bug (minor): SET XU MAC= effective only after a reset
- **Where:** `PDP11/pdp11_xu.c`, `xu_setmac()`: sets the default address (`mac`), which a reset copies to the current physical address (`setup.macs[0]`) and the receive filter; ATTACH applies the filter from `setup.macs`. The same in upstream Open SIMH.
- **Effect:** SET XU MAC=, ATTACH and GO (no reset; BOOT and RUN do reset) left the board receiving on its old address. Seen with the BOOT tests started by a program (`tests/deuna_boot` B2): the load host's answers were not received.
- **Fix (patch 23):** SET XU MAC= also sets the current physical address.
- **Found and fixed:** 2026-11-22.

### 115 — FIXED — Bug: a DMC11/DMR11 transmit buffer too many crashed SIMH
- **Where:** `PDP11/pdp11_dmc.c`, `dmc_process_input_transfer_completion()`, BA/CC IN transmit: `dmc_buffer_queue_add()` returns NULL when no buffer is free, and the code used the result without a check. The same in upstream Open SIMH.
- **Seen (2026-11-23):** the first version of the BOOT DMCn bootstrap repeated its Request Program every few seconds while the line had no peer; each repeat gave the board another transmit buffer; after the 14 buffers were used up the simulator died with a segmentation fault (gdb: `dmc_process_input_transfer_completion`).
- **Real hardware:** the DMC11 takes a limited number of buffers (the microprocessor manual names the limit; a host must not give more). What a real board does with one too many is not described in the manuals we have.
- **Fix (patch 24, branch `dmc-bootstrap`):** the command is dropped (the debug log has the "Failed to queue ... queue full" warning); the bootstrap itself now gives the board at most one transmit buffer.
- **Found and fixed:** 2026-11-23.

### 116 — FIXED — Bug: with PEER on both sides a DMC11 line's messages waited in a connection the other side did not use yet
- **Seen (2026-11-23):** BOOT DMCn (patch 24) on a PDP-11/34 with VMS V4.7 on a VAX-11/780 as the load host over a DMC11 (`tests/dmc_boot/vmsdmc.sh`, `PROGRAM=SYSTEM`), both sides with PEER (VAX: listens 21301, PEER 21302; PDP-11: listens 21302, PEER 21301). In 2 of 4 runs VMS gave up the load with "Load, Line protocol error" and tried again 6 times, always the same; the other runs loaded.
- **What the CONNECT/DATASUM debug logs show:** the PDP-11's outgoing connection to the VAX was "established" at once, and the PDP-11 sent its Request Program on it, again every few seconds. The VAX's own outgoing attempts to the PDP-11 "failed" (the PDP-11 answered "Line connection busy", its line being up), and the VAX read nothing from the PDP-11's connection for a long time while it sent its DDCMP STRTs. When it finally did, all the waiting Request Programs arrived at once; VMS switched the circuit to maintenance mode on the first, started the load, and in the failing runs read one of the old Request Programs where it expected a Request Memory Load.
- **Check:** with the PDP-11's PEER pointing to a port where nobody listens (only the VAX's connection is used), 3 of 3 system loads PASS. So the cause is the crossing connections, as in case 93 (DMV11 pair), not the DMC11 itself.
- **Side note:** first this was put down to `dmc_buffer_fill_receive_buffers()`, which reads a point-to-point line only while the board runs and has a receive buffer, so messages that arrive meanwhile also wait. Ulli then decided to drop such messages as a real line would; the logs above showed that this was not the cause here, so that change was not made (asked again, 2026-11-23).
- **Workaround:** PEER on the load host only, the booting PDP-11's PEER pointing nowhere (a DMC line needs a PEER to ATTACH). `vmsdmc.sh` does so by default (`PDPPEER`).
- **Possible change:** see why the VAX side did not take the PDP-11's incoming connection while its own outgoing attempts failed (`sim_tmxr.c`, `tmxr_poll_conn()`), together with case 93.
- **Found:** 2026-11-23.
- **Cause (Ulli: investigate sim_tmxr, 2026-11-23):** the VAX's listen queue held several connections from the PDP-11: attempts the PDP-11 had made and closed again before its live one (at ATTACH and at the BOOT reset; a test listener that accepts late saw two closed ones before the live one). `tmxr_poll_conn()` takes one pending incoming connection per poll (a DMC line is polled for connections every 2 s of simulated time while it has none); each closed one was taken as the line, sent a STRT, found closed (`tmxr_close_ln`) and dropped, and the next poll took the next. The PDP-11's live connection, which its side had reported "established" at once (the kernel completed the handshake), was taken only after all of them; meanwhile the PDP-11's Request Programs waited in it.
- **Fix (patch 25, branch `tmxr-peer`):** `sim_peek_closed_sock()` (sim_sock.c) tells without taking data whether the peer has already closed or reset a connection; `tmxr_poll_conn()` discards such a per-line incoming connection at once ("Incoming connection already closed by the peer, discarded") and tries the next one in the same poll. The same in upstream Open SIMH.
- **Tests:** `tests/dmc_boot/deadconn.py` with `d1.ini` (three closed connections, then a live one): the Request Program comes after 2.9 s (before the fix 15.5 s). VMS V4.7 system loads over a DMC11 with PEER on both sides (`vmsdmc.sh PDPPEER=21301`): 4 of 4 PASS (before 2 of 4), each with the closed connections discarded in one poll. `tests/dmv11_boot` (20) and `tests/deuna_boot` (10) PASS; VMS V4.7 DECnet pairs (`pair.sh`, PEER on both sides): DMC11, DMR11, DMP11 (vax780) and DMV11 (microvax2) PASS, 3 of 3 compares each.
- **Case 93** (two DMV11s with PEER, crossing connections, reconnecting) probably had the same cause; not tested again.

### 117 — FIXED — Bug: the realism mode's waits at the prompt could not be interrupted
- **Where:** `sim_sound.c`, `sim_spindle_wait()`, `sim_reel_wait()`, `sim_strm_wait()` (our sound patch): a loop of `sim_os_ms_sleep (50)` until the drive is ready or at rest. The console stays in command mode, where the WRU character is an ordinary character, and nothing reads the keyboard.
- **Seen (Ulli, 2026-11-24):** "While a disk is spinning up I cannot interrupt SIMH using CTRL-E" (an RL02 spin-up takes about 40 s).
- **Fix (patch 26, branch `wait-interrupt`):** the three waits put the console into run mode (`sim_ttrun`) and poll the keyboard every 50 ms: the WRU character sets `stop_cpu` (as during RUN), and Ctrl-C, an ordinary character in run mode, is taken as an interrupt too. The wait then ends with e.g. "RL0: wait interrupted, still spinning up (35 s left; SHOW RL0 SPINDLE)"; the drive goes on in the background, and a DO script stops there (`stop_cpu` stays set until the next prompt). Other keys typed during a wait are dropped.
- **HELP:** HELP SET REALISM now describes the waits, the switch -N (return at once), SHOW <unit> SPINDLE/TRANSPORT and the interrupt; HELP SET SOUND points there. Before, SET -N was only in each device's own help.
- **Tests:** `tests/wait_interrupt/w.py` (pseudo terminal, pdp11): W1-W5 PASS (before W1-W3 FAIL); `tests/rl02_8600/c1.ini` (vax8600, no keyboard) unchanged.
- **Found and fixed:** 2026-11-24.
