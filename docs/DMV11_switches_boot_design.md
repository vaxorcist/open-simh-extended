<!-- open-simh-extended/docs/DMV11_switches_boot_design.md  v1, 2026-11-20 (v1: switch packs E101/E113 and the primary MOP boot of the PDP-11, patch 18, branch dmv-boot) -->
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
patch `patches/opensimh_18_dmv11_boot_v1.patch`, tests
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

- **Taking the CPU over.** The DMV writes `MOV #404,SP / WAIT / BR .-2` at
  40 (reserved vector locations) and vector 24/26 = 40/340.
  - Remote load detect and invoke: it clears MMR0 and MMR3 (a power-up
    leaves memory management off, so the vector is read physically) and
    requests the power-fail trap (`trap_req |= TRAP_PWRFL`).
  - Power-on boot (`BOOT DMVn`): it sets the CPU's start address to 40
    (`cpu_set_boot`).

  In the WAIT the CPU fetches nothing, so the loaded program may cover
  the routine. SP is 404 because a kernel stack below 400 is a stack limit
  violation: the first try with SP 40 stopped the simulator with "Trap
  stack push abort".
- **Starting the program.** The DMV saves the loaded words at 24-26 and
  400-402. It writes vector 24 = transfer address with PSW 340, clears
  MMR0/MMR3 and requests the power-fail trap. The trap pushes PC and PSW at
  400-402. One instruction later (`dmv_boot_finish()` from `dmc_svc`) the
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

Not tested yet (needs Ulli's VMS images):

- the VMS DECnet regressions with the DMV11 and DMP11 (point-to-point and
  multipoint);
- a boot with a VMS control station (`NCP TRIGGER NODE ... SERVICE
  PASSWORD`, `LOAD NODE`), which needs a PDP-11 load file that VMS can
  serve.
