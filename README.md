<!-- open-simh-extended/README.md  v1 (integration branch: fork introduction above the Open SIMH README) -->
# open-simh-extended

**A fork of [Open SIMH](https://github.com/open-simh/simh) with new devices and
features for the DEC simulators.**

This repository is based on Open SIMH (commit 87eb7d5). Its `master` branch is
kept identical to Open SIMH; the additions are on separate branches, one per
topic, so that each can be offered back to Open SIMH as its own pull request.
This branch, `integration`, has all of them together.

The work is by Ulli (vaxorcist), developed with Claude (Anthropic).

## What's added

- **DMV11 and DMP11** synchronous communications controllers (Qbus MicroVAXen,
  Unibus VAXen, PDP-11), with DDCMP multipoint: control station and up to
  12 (DMV11) or 32 (DMP11) tributaries. Tested with VMS V3.5–V4.7 DECnet.
- **DMF32** multi-function interface for the Unibus VAXen: 8 asynchronous
  lines, a synchronous line (DDCMP, HDLC/SDLC), a line printer port and a
  DR11-C compatible parallel interface.
- **DMC11/DMR11** fixes that make the ULTRIX-32 V1.1 `if_dmc` driver work.
- **VAX 8200**: DWBUA Unibus adapter fixes; VMS V4.7 now boots.
- **VAX-11/730 RB730**: disk images in DEC logical block order (an RB02
  cartridge is interchangeable with an RL02 on the RL11), the SIMH disk
  library (VHD, autosizing), and status, seek and range fixes.
- **Sound**: a common SDL2 sound system for all simulators, with a realism mode
  (`SET SOUND ENABLED`): power and blower sounds, RUN/STOP buttons with
  spin-up and spin-down, realistic seek and transfer times, and seek sounds
  for the RK06/07, RL01/02, RP04–RP07, RM03/05/80, RA60 and RB02/RB80.

## Branches

| Branch | Content |
|---|---|
| `integration` | everything below, together (default branch) |
| `dmv11-dmp11`, `dmf32`, `dmc-ultrix`, `vax8200-uba`, `rb730`, `sound` | one topic each |
| `notes` | documentation, tools, test scripts and the same work as patch files |
| `master` | Open SIMH, unchanged |

## Building

    git clone https://github.com/vaxorcist/open-simh-extended
    cd open-simh-extended
    make vax780        # or any other simulator; install the SDL2 development files for sound

The descriptions, known issues and test results are in the
[`notes` branch](https://github.com/vaxorcist/open-simh-extended/tree/notes).

---

*The original Open SIMH README follows.*

# Open SIMH machine simulator

This is the codebase of SIMH, a framework and collection of computer system simulators.

SIMH was created by Bob Supnik, originally at Digital Equipment Corporation, and extended by contributions of many other people.  It is now an open source project, licensed under an MIT open source license (see [LICENSE.txt](LICENSE.txt) for the specific wording).  The project gatekeepers are the members of the [SIMH Steering Group](SIMH-SG.md).  We welcome and encourage contributions from all.  Contributions will be covered by the project license.

The Open SIMH code base was taken from a code base maintained by Mark Pizzolato as of 12 May 2022.  From that point onward there is no connection between that source and the Open SIMH code base.  A detailed listing of features as of that point may be found in [SIMH-V4-status](SIMH-V4-status.md).

## PLEASE NOTE

**Do not** contribute material taken from `github.com/simh/simh` unless you are the author of the material in question.

<a href="https://scan.coverity.com/projects/open-simh-simh">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/29458/badge.svg"/>
</a>
