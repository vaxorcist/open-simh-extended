<!-- open-simh-extended/README.md  v5 (branch notes, 2026-11-07; v5: patch 1 v11, DMV11 on the MicroVAX I; v4: patch 5 v14; v3: patch 5 v13; v2: RC25, patch 5 v12) -->
# open-simh-extended: notes, tools and tests

This is the `notes` branch of **vaxorcist/open-simh-extended**, a fork of
[Open SIMH](https://github.com/open-simh/simh) with new devices and features
for the DEC simulators. The branch holds no SIMH code. It has the
documentation of the work, the tools that go with it, the test scripts and
the patch files. The code is on the other branches.

The work was done by Ulli (vaxorcist) with Claude (Anthropic). Everything is
meant to go back to Open SIMH as separate pull requests, one per topic.

## What's new compared to Open SIMH

| Topic | Branch | Simulators |
|---|---|---|
| DMV11 (Qbus) and DMP11 (Unibus) synchronous interfaces, with multipoint (control station and tributaries) | `dmv11-dmp11` | MicroVAX I, MicroVAX II, MicroVAX 3900, VAX-11/730/750/780, VAX 8200, VAX 8600, PDP-11 |
| DMF32 multi-function interface: 8 asynchronous lines, synchronous line, line printer, parallel interface | `dmf32` | VAX-11/730/750/780, VAX 8200, VAX 8600 |
| DMC11/DMR11 fixes for the ULTRIX-32 V1.1 `if_dmc` driver | `dmc-ultrix` | all with the DMC device |
| VAX 8200 DWBUA: interrupts at all BI levels, Unibus init on self-test (VMS V4.7 boots) | `vax8200-uba` | VAX 8200 |
| RB730: DEC logical block order (RB02 images interchangeable with the RL11), SIMH disk library (VHD, autosize), status, seek and range fixes | `rb730` | VAX-11/730 |
| Common SDL2 sound system and a realism mode (`SET SOUND ENABLED`): power and blower sounds, RUN/STOP buttons, spin-up/spin-down, realistic seek and transfer times and seek sounds for the RK06/07, RL01/02, Massbus packs (RP04–RP07, RM03/05/80), RA60, RC25/RCF25 and RB02/RB80 | `sound` | all (sound for the disks above) |

The branch `integration` has all of it together, in the right order. It is
the one to build and run:

```sh
git clone -b integration https://github.com/vaxorcist/open-simh-extended
cd open-simh-extended
make vax780              # or any other simulator; SDL2 development files for sound
```

`master` is kept identical to Open SIMH (currently commit 87eb7d5).

## Branches

| Branch | Content | Built on |
|---|---|---|
| `master` | Open SIMH 87eb7d5, unchanged | |
| `dmv11-dmp11` | DMV11/DMP11 (patch 1) | `master` |
| `dmf32` | DMF32 (patch 2) | `dmv11-dmp11` |
| `dmc-ultrix` | DMC/ULTRIX fixes (patch 4) | `dmv11-dmp11` |
| `vax8200-uba` | VAX 8200 DWBUA fixes (patch 3) | `master` |
| `rb730` | RB730 fixes (patch 6, as separate commits) | `master` |
| `sound` | sound system (patch 5, as separate commits) | `rb730` |
| `integration` | patches 1, 2, 3, 4, 6, 5 | `master` |
| `notes` | this branch | |

## Directories

| Directory | What's in it |
|---|---|
| `docs/` | descriptions, handover notes and findings (Markdown), and the sound device table |
| `tools/` | scripts to run |
| `tests/` | test harnesses and test scripts |
| `patches/` | the same work as patch files, for use without git |

### docs/

| File | What it is |
|---|---|
| `SIMH_overview.md` | **read this first**: current state, patch order, settled device facts, tests passed |
| `SIMH_known_issues.md` | numbered list of known bugs and suspicious code; fixed cases stay in the list, marked DONE |
| `SIMH_Sound_design.md` | the agreed design of the sound system |
| `SIMH_Sound_handover.md` | the sound system's current state: commands, devices, timing, tests |
| `SIMH_Sound_devices.ods` | the devices prepared for sound, with rotation speed, access and spin times (generated, see `tools/`) |
| `DMV11_OpenSIMH_handover.md` | how the DMV11/DMP11 was built (uses older patch names) |
| `DMV11_similarity_report.md` | comparison with Mark Pizzolato's DMV11-Test branch |
| `DMF32_OpenSIMH_handover.md` | how the DMF32 was built (uses older unit names DMF0–DMF4), with the VAX 8200 fix |
| `DMP11_VMS37_tributary_finding.md` | finding: a VMS V3.7 DMP11 tributary that stayed in "synchronizing" |
| `DMC_v7_VMS_regression.md` | regression of the DMC/ULTRIX fixes with VMS V4.7 and V3.7 |
| `ULTRIX11_VAX780_install_log.md` | installing ULTRIX-32 V1.1 on the VAX-11/780 simulator |
| `VMS35_4MB_pool_stall.md` | finding: the VMS V3.5 stall on a 4 MB system (a hand-tuned WSMAX, not a SIMH bug) |

### tools/

| File | What it does |
|---|---|
| `rb730_layout_convert.py` | converts RB02/RB80 images between the old RB730 layout of earlier SIMH versions and the standard layout (DEC logical block order): `python3 -I tools/rb730_layout_convert.py {old2std\|std2old} {rb02\|rb80} <input> <output>` |
| `make_spindle_sounds.py` | generates the synthetic spindle sounds of the disks, with the drive characteristics as parameters |
| `make_vmb_seek_disk.py` | makes a raw MSCP disk image whose VMB boot block loads N blocks at a given LBN (drives realistic transfers from the MicroVAX console): `python3 -I tools/make_vmb_seek_disk.py <image> <blocks> <lbn> <size>` |
| `make_sound_devices_ods.py` | builds `docs/SIMH_Sound_devices.ods`; when a device gets sound support, add it there and run `python3 tools/make_sound_devices_ods.py docs/SIMH_Sound_devices.ods` |

### tests/

| File | What it is |
|---|---|
| `dmctest_kit.txt` | the two-node DECnet test kit for the DMC11/DMR11/DMP11/DMF32 (`run_pair.sh`, `run_all.sh`, `vcon.py`, `dmctest.conf`, README), all files in one text file; each starts with a `===== FILE: path =====` line |
| `dmv11_test_harness.txt` | the earlier DMV11 test harness (MicroVAX II), same format |
| `vms_pair_test_harness.txt` | the earlier VMS pair test scripts, replaced by the dmctest kit |
| `rb730/rbtest.py` | RB730 regression: VMS V3.5 on the VAX-11/730 (RB80 and RB02), then the RB02 cartridge on the VAX-11/750 RL11. The VMS disk images are not included; give their paths in `RBTEST_RB80` and `RBTEST_RP06` |
| `rb730/mkvhd.ini`, `rb730/mkvhd31.ini` | copy an RB80 image into a VHD container (for the VHD test runs) |
| `rb730/realism.ini` | realism mode on the VAX-11/730: RB80 and RB02 with RUN/STOP, mount verification and seek sounds |
| `rc25/` | RC25 realism tests on the MicroVAX 3900 console (`c1`–`c9b`); see `rc25/README.txt` |

### patches/

Base: Open SIMH commit 87eb7d5. The sources use CR LF line ends, so apply the
patches with `git am --keep-cr`, in this order:

```sh
git checkout -b mywork 87eb7d5
git am --keep-cr patches/opensimh_1_dmv11_v11.patch \
                 patches/opensimh_2_dmf32_v6.patch \
                 patches/opensimh_3_vax8200_uba_fix.patch \
                 patches/opensimh_4_dmc_ultrix_v8.patch \
                 patches/opensimh_6_rb730_v1.patch \
                 patches/opensimh_5_sound_v14.patch
```

- Patch 6 must come before patch 5.
- Patch 3 and patch 6 also apply on their own; patches 2 and 4 need patch 1.
- `opensimh_2_dmf32_v6_standalone.patch` is the DMF32 without the
  DMV11/DMP11; use it instead of patches 1 and 2, not together with them.
- Patch 5 (v14) leaves out the Visual Studio project files, so a Visual
  Studio build does not compile `sim_sound.c` yet. The makefile, CMake and
  `descrip.mms` builds are complete. The `sound` branch still has the
  Visual Studio changes.
- The "trailing whitespace" and "quoted CRLF" warnings of `git am` come from
  the CR LF line ends and are harmless.
- If `git am` stops, `git am --skip` drops the failed patch and goes on with
  the next one; `git am --abort` ends the whole session and puts the branch
  back where it was.

## License

The SIMH code is under the SIMH license (see the code branches). The new
sound recordings and synthetic sounds are under the SIMH license too.
