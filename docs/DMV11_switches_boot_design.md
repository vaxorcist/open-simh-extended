<!-- open-simh-extended/docs/DMV11_switches_boot_design.md  v9, 2026-11-22 (v9: section 9, starting the M9312 'XM' ROM by hand: GO 173004, not BOOT CPU; v8: a master clear keeps DTR (Ulli), known issues 103 and 105, patch 21 v4; v7: section 9, DEC's M9312 DECnet boot ROM in SIMH, known issues 101-104, patch 21 v3; v6: section 8, the DMC11 (known issue 99, patch 21 v2); v5: VMS pair regressions PASS; v4: section 7, DMP11 and DMR11 switch packs and boot with one board model, DMC11 open, known issue 90 fixed, patch 21; v3: SET DMVn PROGRAM (Ulli: configurable, default secondary loader), SP 1000 for the started program, remwrt loaded and run, patch 18 v3; v2: section 6, VMS V4.7 as load host, the TM's DTR rule, patch 18 v2; v1: switch packs E101/E113 and the primary MOP boot of the PDP-11, patch 18, branch dmv-boot) -->
# DMV11 switch packs and primary MOP boot (patch 18)

Ulli received a real DMV11 and its User's Guide (EK-DMVQM-UG-003, QMA DMV11
Synchronous Controller User's Guide, 1988) and asked for two features:

- the tributary address that the board's switchpack sets, overriding the
  address the software gives;
- the boot functions (MOP), with commands to switch them on.

Decisions (Ulli, 2026-11-20):

- The switch is called `TRIBUTARY=n`, because `ADDRESS` is already the
  DMV's bus (CSR) address. SIMH's SET stops at the first modifier whose
  name matches, so a unit-level `ADDRESS` could not exist beside the
  device-level one.
- Booting works in the **PDP-11 simulator only**. Ulli checked that a
  MicroVAX cannot boot from a DMV11. In the VAX simulators the boot
  switches are refused.
- `BOOT DMVn` with the power-on boot switch off is refused, as on the real
  board.
- All switches default to the manual's defaults.
- Tests use a control station written in Python and a small PDP-11 test
  program (no load image from DEC was available).

Source: `PDP11/pdp11_dmc.c`, topic branch `dmv-boot` (on `ts-online`),
patch `patches/opensimh_18_dmv11_boot_v3.patch`, tests
`tests/dmv11_boot/`.

## 1. What the manual says

Page and section numbers are those of EK-DMVQM-UG-003. The facts below are
summarised in our own words.

### Switch packs

- **E101 (M8053), E107 (M8064 integral modem module): switch selectable
  features** (Figure 2-6, Table 2-6, installation flow Figure 2-5):
  - SW1 mode enable: OFF = the mode comes from SW6-8, ON = the software
    sets it with a mode definition command.
  - SW2 unit number for booting: ON = unit 0, OFF = unit 1. It tells the
    loaded program which of two DMV11s did the boot.
  - SW3 auto answer.
  - SW4 power-on boot (OFF = enabled).
  - SW5 remote load detect (OFF = enabled).
  - SW6-8 mode (Table 4-5). SW6 OFF = full duplex. SW7 OFF = not DMC
    compatible (point-to-point) or tributary (multipoint). SW8 OFF =
    multipoint. The resulting number is the mode code of the mode
    definition command (Table 3-4): 0 HDX point-to-point DMC compatible,
    1 FDX ditto, 2/3 HDX/FDX point-to-point, 4/5 control station, 6/7
    tributary.
  - SW9 speed (OFF = 19.2 kb/s or more; the M8064 needs OFF).
  - SW10 interface (ON = V.35, OFF = RS-232-C/RS-423-A; not used on the M8064).
  - Figure 2-5: "SWITCH OFF = 1, SWITCH ON = 0, DEFAULT IS 0". All
    switches are ON by default.
- **E113 (M8053), E119 (M8064): the "DDCMP address register",
  tributary/password**: SW1-8, OFF = 1. SW1 is the lowest bit: the
  point-to-point setting is SW1 OFF, SW2-8 ON, which is address 1.
- A switch-set mode is set at every master clear ("If the type of station
  is switch assigned, a master clear has no affect"). A mode definition
  overrides it only with the diagnostic mode bit (BSEL1 bit 3, SEL0
  bit 11) set (Table 3-1).
- A switch-assigned tributary address needs the mode from the switches too
  (3.3.2). "Any other nonzero tributary address in a control command is
  overridden by the switches" (3.3.3); the host then needs only one
  ESTABLISH.

### Boot (section 4.7)

The remote station (a multipoint tributary, or one end of a point-to-point
line) boots its host from the control station in three ways. They can be
combined.

1. **Remote load detect** (SW5): the control station sends an Enter MOP
   Mode message. The DMV11 checks the address and the password, which is
   the E113 value. It then "NPRs a tight-loop routine into main memory",
   "transfers control to the routine through the powerfail/restart
   vector", and sends a primary MOP Request Program. It stores the Memory
   Load with Transfer Address message by DMA and starts the program.
2. **Power-on boot** (SW4): the same steps, but at power-up. The first poll
   from the control station is answered with Request Program.
3. **Invoke primary MOP**: the program sets BSEL1 bit 5 together with master
   clear (bit 6); then as with power-on boot. A multipoint tributary may
   use a software-assigned address here: switches 0, the first established
   tributary. This does not work with the power-on boot switch on.

Booting needs the mode from the switches (4.7.4). Section 4.7.4.3 notes that
on a point-to-point line the E113 switches can carry a unique password,
because the DDCMP address there is always 1.

### MOP

From the MOP V3 functional specification (AA-X436A-TK, bitsavers) and the
MOP registry:

- the DMV11 is MOP device type **34**;
- Request Program (code 8) is device type, format version 1, program type
  (0 = secondary loader); the fields after the device type are optional;
- Memory Load (2) and Memory Load with Transfer Address (0) carry a load
  number (modulo 256; 0 is always valid and restarts the sequence) and a
  4-byte load address;
- Request Memory Load (10) is load number + error byte;
- Parameter Load with Transfer Address (20) has parameter entries, an end
  mark 0 and the transfer address;
- Enter MOP Mode / Boot (6) carries a 4-byte (MOP 2.1) or 8-byte
  verification.

## 2. The SIMH commands

```
SET DMVn MODE={SOFTWARE|HDXDMC|FDXDMC|HDXPOINT|FDXPOINT|HDXCONTROL|FDXCONTROL|HDXTRIBUTARY|FDXTRIBUTARY}
SET DMVn TRIBUTARY=n          ; 0-255
SET DMVn BOOTUNIT={0|1}       ; PDP-11 only
SET DMVn {NO}POWERONBOOT      ; PDP-11 only
SET DMVn {NO}REMOTELOAD       ; PDP-11 only
SET DMVn PROGRAM={SECONDARY|TERTIARY|SYSTEM}  ; PDP-11 only, simulator setting
SHOW DMVn SWITCHES            ; both switch packs as on the board, and the boot state
BOOT DMVn                     ; PDP-11 only: power-on boot
```

- The switch packs are kept per line as on the board, in the registers
  `E101` and `E113` (bit n-1 = switch n OFF). SAVE and RESTORE keep them.
  Defaults are 0 (all ON).
- `MODE=SOFTWARE` turns SW1 ON. Any other mode turns SW1 OFF and sets
  SW6-8. Mode names must be written in full (no abbreviations: `HDX` would
  be ambiguous).
- The switches are read at master clear (also at RESET and BOOT). A `SET
  MODE` after the last master clear takes effect at the next one, as on
  the real board.
- SW3, SW9 and SW10 have no effect in the simulation and cannot be set.
  `SHOW SWITCHES` shows them as ON.
- `PROGRAM` is not a switch of the board: it selects the MOP program type
  of the Request Program (0 secondary loader, 1 tertiary loader, 2 system
  image). The manuals do not give the message. Decision (Ulli,
  2026-11-21): configurable, default `SECONDARY`, the most likely
  behaviour of a primary loader that takes a single Memory Load with
  Transfer Address. `SYSTEM` makes a load host send the load file itself,
  in as many Memory Loads as needed (VMS: `LOAD FILE`). Register `PROGRAM`,
  shown by `SHOW DMVn SWITCHES`.
- `BOOT DMVn` checks, in this order: the power-on boot switch, the mode
  from the switches, not a control station, the unit attached, and a
  multipoint tributary's switch address.

## 3. Implementation

### Switches in the host interface

- `dmp_reset_host_state()` (master clear): with SW1 OFF the station type is
  the SW6-8 mode, and `dmp_station_known` is set.
- `dmp_process_command()`: for a multipoint tributary (station type 6/7)
  with E113 not 0, a non-zero BSEL3 other than the switch address is
  replaced in the register. The command, its buffers and its responses
  then all carry the switch address.
- Mode definition: another mode gives error 104, as before, unless the
  device is a DMV and SEL0 bit 11 (diagnostic mode) is set while the
  command is processed.

### Boot (PDP-11 only, `DMV_BOOT_SUPPORTED`)

- **Taking the CPU over.** The DMV writes `MOV #1000,SP / WAIT / BR .-2` at
  40 (reserved vector locations) and vector 24/26 = 40/340.
  - Remote load detect and invoke: it clears MMR0 and MMR3 (a power-up
    leaves memory management off, so the vector is read physically) and
    requests the power-fail trap (`trap_req |= TRAP_PWRFL`).
  - Power-on boot (`BOOT DMVn`): it sets the CPU's start address to 40
    (`cpu_set_boot`).

  In the WAIT the CPU fetches nothing, so the loaded program may cover
  the routine. SP is 1000 (patch 18 v3), so the started program has the
  usual stack below 1000. SP 40 (first try) stopped the simulator with
  "Trap stack push abort" (a kernel stack below 400 is a stack limit
  violation); SP 404 (v1, v2) worked for the routine itself, but remwrt,
  which sets up no stack, then began with SP 400 and stopped the same way
  at its first CALL.
- **Starting the program.** The DMV saves the loaded words at 24-26 and
  774-776. It writes vector 24 = transfer address with PSW 340, clears
  MMR0/MMR3 and requests the power-fail trap. The trap pushes PC and PSW at
  774-776, so the program starts with SP 774. One instruction later (`dmv_boot_finish()` from `dmc_svc`) the
  saved words are put back.
- **The protocol engine** (`dmv_boot_receive()`, `dmv_boot_mop()`):
  - It runs instead of the normal receive path while a boot is active, and
    reads DDCMP messages from the line itself. It checks the CRCs, and a
    multipoint tributary takes only its own address.
  - The first non-maintenance message is the poll and gets Request Program
    `08 22 01 00`.
  - Memory Loads are written with `Map_WriteB` (Qbus, 22-bit physical) and
    answered with Request Memory Load n+1. If the memory cannot take a
    load, the answer is the same n with error 1. A repeated load n-1 gets
    the last request again; other load numbers are ignored.
  - Memory Load with Transfer Address and Parameter Load with Transfer
    Address (parameters skipped) start the program.
  - An unanswered request is sent again every 5 s (`DMV_BOOT_RETRY`,
    wall clock), and also when the line was busy.
- **Host interface during the boot.** The boot begins with an internal
  master clear. That master clear keeps DTR (`dmv_keep_line`): with modem
  control passed through, DTR off disconnects the TCP line. The DMV then
  raises DTR. After the start, the DMV stays master-cleared, with the mode
  from the switches.
- **Remote load detect while the host runs.**
  - `dmv_peek()` reads one message ahead after the normal receive path,
    also when the host has no receive buffer, so a crashed system can be
    reached.
  - An Enter MOP Mode for this station with the right password starts the
    boot. The address must match: the switch address, or for a tributary
    with switches 0 the first established tributary. The password check:
    the first verification byte equals E113 and the other bytes are 0.
  - Any other message is held for the host (`dmv_get_packet()` gives it
    out first). If it is still not taken at the next poll, it is dropped;
    the peer sends it again. This happens only with REMOTELOAD on.
- **Invoke primary MOP** (`dmc_process_master_clear()`): with SEL0 bit 13,
  the first established tributary is noted before the master clear
  forgets it. DTR is kept, and `dmv_boot_invoke()` starts the boot. If
  the conditions are not met, the master clear ends as an ordinary one
  (DTR off, debug WARN).

### Multipoint maintenance messages

`dmp_mp_send_session()` now puts the tributary address into maintenance
(MOP) messages too; before, they kept address 1. The receiving multipoint
station routes every message by its address, so MOP from a control station
reached only tributary 1 (known issues 88). The DMV11 manual says the
tributary recognizes its address in the Enter MOP Mode message.

## 4. What is not modelled, assumptions

- **Unit number for booting (SW2):** the manual does not say how the
  loaded program learns it. The switch is kept and displayed only (known
  issues 89).
- The address of the waiting routine, its SP, and the restore of the
  vector words are this emulation's choices: the manual only describes
  the outline.
- No bus INIT at the takeover: other devices keep running. The CPU is at
  priority 7, so no interrupts come through.
- A transfer address above 64 KB is started at its low 16 bits, because
  memory management is off (debug WARN).
- The 5 s retransmission interval is a choice; MOP derives its timeout
  from the line speed plus 1 s.
- Multipoint selection rules are not applied to the boot messages. The
  maintenance messages have the select flag set (`ddcmp_build_maintenance_packet`),
  which returns the line to the control station.

## 5. Tests (`tests/dmv11_boot/`)

`run_tests.sh BIN/pdp11` runs T1-T6. The control station is `mopctl.py`.
The PDP-11 programs of T3, T4 and T6 are assembled by `mkini.py`.

| Test | What | Result 2026-11-20 |
|---|---|---|
| T1 | power-on boot, point-to-point; first Request Program unanswered: repeated after 5.0 s | PASS |
| T2 | power-on boot, tributary 5; poll for 6 ignored; duplicate load answered again; load 7 ignored | PASS |
| T3 | remote load detect, password 123, host running with MMU on (page 0 at physical 100000) and DDCMP started; wrong password ignored | PASS |
| T4 | invoke primary MOP, tributary, switches 0, program established 9: boot with address 9 | PASS |
| T5 | BOOT refusals (SW4 off, software mode, control station, tributary without switch address) | PASS |
| T6 | MODE=FDXTRIBUTARY, TRIBUTARY=5: ESTABLISH 9 gives 5; mode definition 3 → error 104; with the diagnostic bit accepted | PASS |

Each boot test also checks that the program printed "DMV BOOT OK", halted at
1032, and that its own words at 24, 26, 34, 36, 40, 400 and 402 are intact.

Other checks:

- In the microvax2 simulator, the boot switches are refused and `BOOT
  DMV0` fails, while MODE and TRIBUTARY are accepted.
- `tests/dmv11/mc.ini` (DMV11 master clear signature) on microvax1,
  microvax2 and microvax3900: PASS.
- The microvax2, vax780 and microvax3900 builds compile with `-Werror`.

Not tested yet: the VMS DECnet regressions with the DMV11 and DMP11
(point-to-point and multipoint) with patch 18 v2.

## 6. VMS V4.7 as the load host (2026-11-21, patch 18 v2)

Test: `tests/dmv11_vmsload/` (MicroVAX II with Ulli's VMS047.R81, PDP-11/73).

### What the technical manual adds (EK-DMV11-TM-001)

- Appendix D (modem control): with remote load detect, power-on boot or a
  boot request selected, the DMV11 asserts DTR again by itself. Patch 18 v2:
  with remote load detect on, DTR comes back right after every master clear
  and at ATTACH, so the control station can reach a halted station.
- Section 4.7 describes the same three boot sequences as the User's Guide.
  The manual names no format for the Request Program message and says
  nothing about how a loaded program learns the boot unit (known issue 89
  stays open).

### Setup

Tests: `tests/dmv11_vmsload/run.sh` runs two cases, `pdp.ini` (default,
secondary loader) and `pdp_system.ini` (`PROGRAM=SYSTEM`, load file).

PDP-11 (no PEER, it only listens; known issue 93):

```
set cpu 11/73
set cpu 256k
set dmv enable
set dmv0 mode=fdxpoint
set dmv0 tributary=123
set dmv0 remoteload
attach dmv0 21202
dep 1000 000001
dep 1002 000776
dep PSW 0
dep SP 1000
go 1000
```

MicroVAX II: `set dmv enable`, `set dmv0 peer=127.0.0.1:21202`,
`attach dmv0 21201`. NCP on VMS (password 7B hex = 123):

```
DEFINE LINE DMP-0 STATE ON PROTOCOL DDCMP POINT
DEFINE CIRCUIT DMP-0 STATE ON SERVICE ENABLED
DEFINE NODE 1.5 NAME PDP SERVICE CIRCUIT DMP-0 SERVICE PASSWORD 7B SERVICE DEVICE DMV CPU PDP11
DEFINE NODE PDP SECONDARY LOADER SYS$SYSTEM:HELLO.TSK LOAD FILE SYS$SYSTEM:HELLO.TSK
LOAD NODE PDP
```

### What happens

1. NCP puts DMP-0 into maintenance mode and sends Enter MOP Mode
   (`06 7B 00 00 00 00`, 5 verification bytes, DDCMP address 1).
2. The PDP-11's DMV11 checks address and password, parks the CPU and sends
   Request Program `08 22 01 00` (DMV, MOP format 1, secondary loader).
3. VMS answers with one Memory Load with Transfer Address: load 0,
   address 6, the task image's L$BLDZ x 64 bytes (128 for the 72-byte
   test program), transfer address 6.
4. The DMV11 writes the data and starts the program at 6. The PDP-11
   console shows "HELLO from the DMV11 secondary loader".

First this hung after step 1: the Enter MOP Mode message never came back
to VMS as transmitted (known issue 92, fixed in patch 18 v2 for all
DMC/DMR/DMP/DMV types).

### Findings

- **VMS loads a secondary loader at address 6**, whatever the task base
  (L$BSA 0) and the transfer address (0) say, and pads it to L$BLDZ x 64
  bytes. A secondary loader must therefore be position independent. The
  first non-PIC try printed rubbish.
- **After the secondary loader NCP waits** for the next Request Program
  (tertiary loader or system image). A program that is not a loader leaves
  NCP waiting until its timeout.
- **remwrt** (32548 bytes, Lee Gleason's LOCALWRITE client for DEQNA +
  RQDX3) cannot be a secondary loader. With the default `PROGRAM=SECONDARY`
  it would need a DMV11 secondary loader (a small PIC program that drives
  the DMV11 in maintenance mode and fetches the load file); none was
  written. Instead, Ulli decided to make the program type configurable.
- **remwrt with `SET DMV0 PROGRAM=SYSTEM`** (2026-11-21): the DMV11 sends
  Request Program `08 22 01 02`; VMS sends `LOAD FILE`
  SYS$SYSTEM:REMWRT.TSK in 128 Memory Loads (loads 0-127, 256 bytes each,
  the last 64), then a Parameter Load with Transfer Address
  (load 128, node name PDP and other parameters, transfer 0). remwrt
  starts and prints its banner "REMWRT V01A03 ... Control-G Consultants",
  then "Choose disk unit to write to:" and waits for console input (its
  DEQNA/RQDX3 work needs the LOCALWRITE server, not part of this test).
  The banner lines run together on the console because of its VT/CR line
  end (known issues 95). remwrt was assembled with macro11 v0.3 (0 errors)
  and converted to `remwrt.tsk` (66 blocks, transfer 0); see known issues
  95 and 96.

## 7. DMP11 and DMR11 (2026-11-22, patch 21)

Ulli asked for the same switches and boot for the DMP11, DMR11 and DMC11,
following DRY, and for the fix of known issue 90. Sources (facts only, our
words): DMP11 User Guide EK-DMPQM-UG-001, DMR11 User's Guide EK-DMR11-UG-002
and Technical Manual EK-DMR11-TM-002, M8207 Technical Manual
EK-M8207-TM-002, DMC11 IPL microprocessor maintenance manual EK-DMCMP-MM-001.

Decisions (Ulli, 2026-11-22):

- **DMC11:** wait for a line unit manual (the uploaded ones do not say what
  the line unit's switch packs 2 and 3 mean). Known issue 99.
- **DMR11:** faithful plus built-in: the remote load detect restarts the CPU
  into a boot ROM at 173000 + offset as the board does; when no ROM answers
  there, the DMR11 runs the shared MOP load itself.
- **DMP11 defaults:** boot features off, like the DMV11 (the DMP11's
  power-on boot switch enables the boot when ON, so the default is SW10 OFF).

### What the manuals say

- **DMP11** (M8207-YA + M8203 line unit): pack E121 SW1-4 unit number
  (0-15), SW5-7 mode (same code as the DMV11's SW6-8), SW8 mode enable (OFF:
  from the switches), SW9 remote load detect (OFF: enabled), SW10 power-on
  boot (ON: enabled). Pack E134 SW1-8 tributary address/password, SW9 not
  used, SW10 line speed. Boot (section 4.7): the same three functions as
  the DMV11; invoke primary MOP is BSEL1 bit 5 with master clear; the
  invoke function uses the power-on boot's switch settings; PDP-11 only.
- **DMR11** (M8207-RA + M8203): pack E121 SW1-8 bootstrap offset (boot
  address 173000 + offset; 000 when remote load detect is not used), SW9
  DMC11 line compatible, SW10 line speed. Pack E134 SW1-8 password (377:
  remote load detect off), SW9 auto answer, SW10 microdiagnostics (they run
  at master clear when SEL0 bit 13 differs from the switch: OFF and bit
  clear, or ON and bit set). Remote load detect: an Enter MOP Mode (code 6,
  four password bytes, all equal to E134) with a good CRC makes the DMR11
  DMA 173000 + offset into 24 and 0 into 26 and pull AC LO; the CPU's
  power-up recovery enters the boot ROM (M9312 DECnet ROM, M9301-YJ), and
  the ROM does the MOP load through the DMR11. The DMR11 itself sends no
  MOP message.
- **DMC11** (M8200 + M8201/M8202): the line unit's packs 2 and 3 hold the
  password and a ROM offset; the DMC11 either starts a ROM, loads itself
  (Request Program 8,12,1) or passes the message on. Which switch selects
  what is missing.

### One board model (DRY)

All boards share one implementation in `pdp11_dmc.c`:

- `DMX_BOARD` describes a board: pack names and places, switch counts, the
  mode enable switch and the first mode switch, the unit number switches,
  the power-on boot switch and its sense, the remote load detect switch (or
  none: the DMR11's password decides), the MOP device type (DMV11 34,
  DMP11 18, DMR11 40), and per pack a `DMX_SWDESC` table for SHOW SWITCHES.
- `dmx_board()` gives a line's board (NULL for a DMC11), `dmx_pack()` its two
  pack words (bit n-1 = switch n OFF), `dmx_program()` its PROGRAM setting.
- Every switch query goes through these: `dmv_mode_from_switches`,
  `dmv_switch_mode`, `dmv_switch_address`, `dmx_poweron`, `dmx_remoteload`.
  So the switch-assigned mode and tributary address of the host interface
  (mode definition, ESTABLISH) now also work on the DMP11.
- SET/SHOW: one routine per command for all boards (`DMX_UNIT_BOARD` finds
  the line and board, a DMC11 is refused). The DMP and DMV MTAB entries
  come from one macro, `DMX_SWITCH_MTAB`, which only names the switches.
- SHOW SWITCHES prints any board from its `DMX_SWDESC` tables.
- The boot engine (`dmv_boot_*`) serves all three; only the Request
  Program's device type and program type come from the board and line.
- The help texts of the DMV11 serve the DMP11 too; the board's switch names
  are scp_help strings %4s-%9s.

### SIMH commands

DMP (Unibus PDP-11): `MODE`, `TRIBUTARY`, `BOOTUNIT=0-15`, `{NO}POWERONBOOT`,
`{NO}REMOTELOAD`, `PROGRAM`, `SHOW DMPn SWITCHES`, `BOOT DMPn`; registers
`E121`, `E134`, `PROGRAM`.

DMC with `TYPE=DMR`: `PASSWORD=n` (octal, default 377), `BOOTOFFSET=n`
(octal, default 000), `PROGRAM`, `SHOW DMCn SWITCHES` (also tells whether
a boot ROM answers at the boot address); registers `E121`, `E134`,
`PROGRAM`. E134 SW10 is shown from `SET MICRODIAG` (ENABLE = switch OFF).

In the VAX and PDP-10 simulators the boot switches are refused, SHOW
SWITCHES works.

### Implementation details

- **DMR11 remote load detect** (`dmr_remote_load`): if an I/O page reader
  exists at 173000 + offset (a ROM attached with the ROM device), the DMR11
  is master-cleared with the line kept up, 24/26 get the boot address and
  0, memory management is switched off and the power-fail trap requested.
  Otherwise `dmv_boot_begin()` as for a DMV11 remote load detect (point-to-
  point, address 1, device type 40).
- **Password:** DMR11: bytes 1-4 of the Enter MOP Mode all equal E134
  SW1-8; DMV11/DMP11 unchanged (byte 1 equal to the address switches, the
  rest of the 4 or 8 verification bytes 0).
- **Enter MOP Mode while the host takes the messages:** `dmv_get_packet()`
  now checks every message it reads for the line (remote load detect
  armed): the microcode scans every message. Before, only the read-ahead
  while the host gave no receive buffer looked for it.
- **Invoke primary MOP** is checked for the DMP11 as for the DMV11 (SEL0
  bit 13 with master clear).
- **Known issue 90:** `dmc_reset()` names the shared DMP/DMV units after the
  device in use and sets their `dptr`.

### Tests (`tests/dmv11_boot/`, 2026-11-22)

- P1 DMP11 power-on boot point-to-point (BOOT DMP0, device type 18, first
  request unanswered): PASS.
- P2 DMP11 remote load detect, multipoint tributary 5, wrong password
  first, CPU running: PASS.
- P3 DMP11 invoke primary MOP, point-to-point: PASS.
- P4 BOOT DMP0 refusals, unit number range: PASS.
- R1 DMR11 remote load detect without a ROM (the DMR11 loads itself,
  device type 40), wrong password first: PASS.
- R2 DMR11 remote load detect with a ROM at 173000 (offset 020): CPU
  restarted at 173020 with 24 = 173020, 26 = 0, no MOP message: PASS.
- R3 DMC11 refusal, DMR11 value checks: PASS.
- T1-T6 (DMV11) unchanged: PASS.
- VMS V4.7 DECnet pairs (`tests/dmv_regress/pair.sh`, the receive path
  changed): microvax2 DMV11 point-to-point and multipoint, vax780 DMP11
  point-to-point and multipoint: PASS (3 of 3 compares each).
- A test "operating system" needs a stack: the takeover pushes PC and PSW
  through the power-fail trap; with SP 0 an 11/70 stops with a red stack
  trap (the 11/73 of the DMV11 tests did not mind).

## 8. DMC11 (2026-11-22, patch 21 v2, known issue 99)

Ulli uploaded the DMC11 line unit manuals EK-DMCLU-OP-001 (user's manual)
and EK-DMCLU-MM-001 (maintenance manual). Both are scans without text; read
with OCR. Facts only, our words.

- **The line unit manuals:** switch pack no. 2 is line unit register 15 (E87
  on the M8201, E90 on the M8202), no. 3 is register 16 (E88 / E91); both are
  shipped all OFF (377); they belong to the down-line loading, and their
  meaning is up to the microcode. No bit table.
- **The DMC11 diagnostics** (MAINDEC-11-DZDMG-C-D, bitsavers) read them with
  the microinstructions 320, the "line number switch", and 340, the "BM873
  address": IBUS registers 15 and 16.
- **The microprocessor manual** (EK-DMCMP-MM-001 3.4.11, both versions): the
  Enter MOP Mode message is code 6 and four equal bytes, the value of a
  switch pack (the password); the other pack is an 8-bit word offset into
  the boot ROM space; depending on the switches the DMC11 starts a boot ROM
  (BM873, M9301), loads the system itself (later microcode: INIT, tight
  loop, Request Program 8,12,1, Memory Load with Transfer Address) or passes
  the message on.

Result: pack no. 2 (R15) is the password, pack no. 3 (R16) the word offset,
as on the DMR11 (E134 = R15, E121 = R16). Which setting selects ROM, own
load or pass-on is not documented; the simulation uses the DMR11 rule
(Ulli's decision for the DMR11): password 377 passes the message on (the
shipped setting), a ROM at the boot address is started, otherwise the DMC11
loads itself.

Implementation: a fourth `DMX_BOARD` (`dmx_dmc`): packs "no. 3" (pack 0,
the offset) and "no. 2" (pack 1, the password), 8 switches each, storage
`dmc_sp3`/`dmc_sp2` (registers `SP3`, `SP2`, default 377), MOP device type
12, `offset_scale` 2 (words; the DMR11 has 1, bytes), `short_request`: a
secondary loader is requested with the three bytes 8,12,1. The DMC11 and the
DMR11 share every routine (`dmr_boot_address`, `dmr_boot_rom`,
`dmr_remote_load`, the password check). The DMC11's INIT of the Unibus is
not done (known issue 100).

Commands (TYPE=DMC): `SET DMCn PASSWORD=n`, `SET DMCn BOOTOFFSET=n` (octal),
`SET DMCn PROGRAM=...`, `SHOW DMCn SWITCHES`.

Tests (`tests/dmv11_boot/`): R3 (defaults, word offset), R4 (the DMC11 loads
itself: Request Program `08 0C 01`, wrong password first), R5 (boot ROM at
173000, offset 010 words: restart at 173020). All 15 boot tests PASS.

## 9. DEC's M9312 DECnet boot ROM (2026-11-22, patch 21 v3)

Ulli asked whether SIMH has PDP-11 boot ROMs for `BOOT CPU`, to look for
images on the internet, and which format SIMH needs.

- **SIMH has none.** `BOOT CPU` (and `BOOT` alone) starts the image attached
  to ROM0 (`cpu_boot()` calls the ROM device's boot routine); `BOOT ROMn`
  starts ROMn at its first address. The only images built into the PDP-11
  simulator are the DELQA's boot ROM (XQ) and VT11 demo programs.
- **Format:** a flat binary file of 16-bit words, low byte first (as in
  PDP-11 memory, on the usual little-endian hosts: known issue 104), the
  first word at the unit's ADDRESS, the file size is the ROM size
  (`SET ROMn ADDRESS=17773000`, `ATTACH ROMn file`). HELP ROM says so now.
- **Images found:** Don North (AK6DN) publishes the M9312 boot ROMs as
  MACRO-11 source, listing and PROM programmer files (Intel HEX of the
  4-bit PROM cells), verified against real chips and DEC's listing document
  K-SP-M9312 (https://ak6dn.github.io/PDP-11/M9312/; the repository
  github.com/ak6dn/ak6dn.github.io can be cloned from the workspace, the
  web page cannot). Among them the DECnet ROM set for the DMC11/DMR11, 'XM',
  23-862A9, 23-863A9, 23-864A9. Raw PROM dumps (.BIN, 512 bytes per chip)
  are also at pdp-11.classiccmp.org/M9312/. For the M9301, BM873 and the
  KDJ11 boards no ready images were looked at.
- **Conversion:** `tools/m9312_lst2bin.py` builds a ROM device image from
  the listings (the assembled words with their addresses), places each ROM
  at its socket (173000, 173200, 173400, 173600; the listings are all
  assembled at 173000, the code is position independent) and with `--hex`
  checks every word against the PROM programmer file. The M9312 boot PROM
  holds word i in cells 4i..4i+3: cell bits = word bits 8,1,2,3 / 4,5,6,7 /
  0,9,10,11 / 12,13,14,15, with 10, 11 and 12 stored inverted; all 64 words
  of the three XM ROMs match.
- **The XM ROM** (from its listing): entries 173004 (unit 0) and 173030
  (unit 1) without diagnostics (173006/173032 with them; they need the
  console ROM 23-248F1 at 165000); it checks that ROMs 2 and 3 sit in
  sockets 2 and 3, sets SP 17776, finds the DMC at its floating CSR by
  probing (table DJ11, DH11, DQ11, DU11, DUP11, LK11, DMC11), executes
  RESET, gives the DMC a base table at 17370, CNTL IN maintenance mode +
  half duplex, a receive buffer at 0 of 4092 bytes and sends Request Program
  10,14,1,0 octal (8,12,1,0: DMC11, secondary loader) from 17774, retrying 8
  times, then RESET and again. When a received message begins with the code
  0 and load number 0 (Memory Load with Transfer Address), it executes
  RESET and jumps to 6, with the unit number in R0. That is why VMS sends a
  secondary loader to address 6 (section 6). The pseudo vector words at
  173024/173026 of each ROM are 173000/340.
- **DMR11 and DMC11 offsets for it:** DMR11 `BOOTOFFSET=4` (bytes, 173004),
  DMC11 `BOOTOFFSET=2` (words, 173004); unit 1: 30 / 14.
- **Starting it by hand** (Ulli's question, tested 2026-11-22): `BOOT CPU`
  starts the ROM at its first word, 173000, which is the ROM's header (the
  device code "MX"), not code: the CPU runs into the middle of the ROM
  without a CSR and loops. Start it at its entry instead, after a reset
  (`GO` does not reset the devices):

  ```
  set cpu 11/70
  set dmc enable
  set dmc0 type=dmr
  set dmc0 peer=127.0.0.1:21201
  attach dmc0 21202
  set rom0 address=17773000
  attach rom0 m9312_xm.bin
  reset
  go 173004
  ```

  173004 is unit 0 and 173030 unit 1, both without diagnostics; the
  entries with diagnostics (173006, 173032) jump to the console ROM
  23-248F1 at 165000. PASSWORD and BOOTOFFSET are not needed for this
  (they serve the remote load detect). The ROM finds the board by the
  floating CSR rules and sets SP 17776 (at least 8 KB of memory), and a
  load host must answer its Request Program (8,12,1,0) with a secondary
  loader, which runs at 6. On a real machine the console ROM or the
  M9312's boot switches choose the entry.

Found on the way and fixed (patch 21 v3):

- `dmp_description()` chose DMP11/DMV11 by the bus, so HELP BOOT on a Qbus
  CPU called the DMP device a DMV11 (known issue 101, reported by Ulli).
- `dmc_reset()` restarted the 2-second connection poll at every reset; the
  ROM's RESET at each retry (about every 0.5 s) starved it, and the line,
  dropped by the master clear, was never connected again (known issue 102).

A master clear dropped DTR, so every RESET closed the TCP line; the DMC11
manual says the board keeps DTR (known issue 103). Ulli decided to keep DTR
at master clear for every DMC/DMR/DMP/DMV line (patch 21 v4): the master
clear asserts DTR on an attached line; the explicit drops (HALT, the DMP
station halt, the DMC's "drop DTR" microinstruction) stay. DTR had also
served as the DMC11's sign that the host started the line; that is now the
host's start itself, and a maintenance mode request goes to maintenance mode
at once instead of starting DDCMP first (known issue 105, which kept the
ROM's Request Program queued once the line stayed up). With both, the ROM
boots without a single line drop.

Tests: `tests/dmv11_boot/` R6 (DMR11) and R7 (DMC11) through the real ROM:
Request Program `08 0C 01 00`, the program at 6 prints "XM ROM BOOT OK" and
halts at 34. The image is not in the repository (DEC firmware); the README
of the tests says how to build it. All 17 boot tests PASS; the VMS V4.7
DECnet pairs PASS with the poll change (DMV11, DMP11, point-to-point and
multipoint) and again with DTR kept, now also over a DMC11 and a DMR11
(`pair.sh` v7, `DEV=DMC|DMR`): six of six.
