<!-- open-simh-extended/docs/DMF32_OpenSIMH_handover.md  v5 (DWBUA self-test resets the Unibus; v4: VAX 8200 fix; v3: parallel interface; v2: synchronous interface) -->
# DMF32 for Open SIMH: status and handover

Date: 2026-10-27, updated 2026-10-28. Prepared with Claude for Ulli (vaxorcist). Nothing has been pushed to any remote.

## Where things are

- Repository `/home/claude/open-simh`, branch `dmf32`, local commit 0b3c6ed. It sits on top of the DMV11 work (branch `dmv11`, d78c545 = v6).
- Patch file `opensimh_dmf32_v1.patch`. Apply it after `opensimh_dmv11_v6.patch`; it changes the `pdp11_io_lib.c` line that v6 introduced. Use `git am --keep-cr` or `git apply`.
- New file `VAX/vax_dmf.c`. The other changed files are:
  - `VAX/vax{730,750,780,820,860}_defs.h`: interrupt bits and `VEC_DEVMAX` 8;
  - the five `_syslist.c` files;
  - `makefile`, `VAX/CMakeLists.txt` and five `.vcproj` files;
  - `PDP11/pdp11_io_lib.c`: the auto_tab rank entry "DMF".
- Every changed file has a first-line `local ... (drop this line before submitting)` marker. Remove those lines before submitting.
- Sources: the DMF32 User's Guide EK-DMF32-UG-003 (chapter 4, the register model) and EK-DMF32-TD-001 (the CSR0 device code table in TD Table 1-2 is wrong; UG Table 4-1 and 4.3BSD agree with each other).

## Design

- One DIB with 16 registers, 8 vectors and a floating rank (`{ "DMF" }, 1, 8, 32, 4`). The ack routines return the vector base loaded in CSR0 (7:0) plus 4*n. Before that is loaded, they return the auto-configured vector.
- Interrupts are BR5, int_req bits 24–31. These bits are free on all five models.
- Units:
  - DMF0: the multiplexer poll (TMXR, 8 lines);
  - DMF1: the printer file;
  - DMF2: the silo alarm timer (hidden).
- CSR0 = 0xA000 (async + LP). CSR1 = 0xAA00 after reset (self test passed). Writing 0x2A to the CSR1 high byte returns microcode revision 28 (BCD).
- **Async:** TRDY reports are queued per line and delivered one at a time; reading the CSR clears TRDY and posts the next report. DMA refills the silo when it holds fewer than 2 characters. NXM is reported with the TRDY event.
- **Receive silo:** interrupts when it holds more than 16 entries or when the RSP timeout expires. RSP=1 means at once, 0 means never.
- **Modem control** (`ATTACH -M`): only on lines 0 and 1. Lines 2–7 accept a connection without DTR. Without -M, lines 0 and 1 show DSR+CD+CTS while connected.
- **LP:** one DMA print cycle per print-enable, then DONE and an interrupt.
  - Formatting follows UG Table 4-34. Auto-CR (ind. 6 bit 8) is active when the bit is clear: a CR is inserted before LF/FF and a data CR right before LF/FF is dropped. When the bit is set, the data passes unchanged. "New line" is always CR LF.
  - Without a file the printer is offline. Going offline clears print enable and interrupts; VMS restarts the cycle when the printer is back.

## Tests (VAX/VMS V4.7, VAX-11/780, `/home/claude/dmvtest`)

Files: `boot_dmf.sh` v1, `n9/node9_780.ini` v1, `txtest.py` v2, `xontest.py` v1.

| Test | Result |
|---|---|
| Autoconfigure: TXA0–TXA7, LCA0 (device type DMF32) | ok |
| Telnet login, SHOW TERMINAL, DIRECTORY (DMA output), TYPE | ok |
| 4 lines at the same time (TXA0–3), 15 KB output each | ok, identical output |
| 220-character type-ahead line into the 48-entry silo | ok |
| ^S stops output (0 bytes in 3 s), ^Q resumes (179 KB listing) | ok |
| `ATTACH -M`: line 2 without modem; TXA0 /MODEM/HANGUP gets DTR, connects, hangs up | ok |
| COPY to LCA0, SET PRINTER /CR /LOWERCASE, PRINT/QUEUE=LCA0 with flag pages | ok |
| Printer DETACH (offline) during a COPY, then ATTACH: the job completes correctly | ok |
| Diagnostic supervisor tests of vax730/750/780/8200/8600 (`VAX/tests/vax-diag_test.ini`) | all passed |

Bug fixed during testing: a DMA transfer never ended (the transmit-ready report was missing), so SHOW TERMINAL hit device timeouts. The earlier offline handling also left print enable set, which printed a stale buffer.

## Phase 2 (2026-10-28): synchronous interface

### Where things are

- Branch `dmf32`, local commit 511d5e3 (on top of 0b3c6ed). Branch `dmf32-standalone` has the same change as c810078.
- Patches, each a single cumulative commit:
  - `opensimh_dmf32_v2.patch` applies after `opensimh_dmv11_v6.patch`;
  - `opensimh_dmf32_v2_standalone.patch` applies to plain Open SIMH 87eb7d5.
  - Both replace the v1 patches. Only `VAX/vax_dmf.c` changed in phase 2.
- CSR0 now reports async + sync + LP (device code 1110). VMS configures XGA0 (driver XGDRIVER) and the DECnet line DMF-0.

### Design

- Unit DMF3 is the line. Set it up with `SET DMF PEER=host:port` and `ATTACH DMF3 port`; without a PEER the line only listens. It is a TMXR line in packet mode, raw TCP, the same as the DMC device.
- **DDCMP mode:** the transmit buffer holds the header without CRCs plus the data. The device inserts both CRCs and sends a standard DDCMP frame with the `pdp11_ddcmp.h` helpers. On receive it checks the CRCs, checks the tributary address when bit 13 of the receive CSR is set, and stores the header and data without CRCs. The frame format is the DMC device's, so the peer can be a DMF32 or a DMC/DMR/DMP/DMV11.
- **Buffers:**
  - A buffer is free when its done bit is clear; loading its count register (ind. 7/9/11/13) clears that bit. A Master Reset sets all four done bits.
  - When a message ends, the primary/secondary bit toggles, the done bit is set and an interrupt is posted.
  - The count register is left holding the remaining bytes; the address registers are not changed. VMS reloads both and selects the buffer itself by writing the P/S bit.
- **Errors:**
  - Transmit: an NXM, or a DDCMP message shorter than its header count, sets the error bits in ind. 2, sets TERR and the done bit, and clears TE.
  - Receive: an overflow, NXM or data CRC error sets ind. 1 and RERR, clears RE and toggles P/S, without setting a done bit. Reading ind. 1 or ind. 2 clears the errors.
- **Waiting messages:** a received message waits (one is held) until a buffer is free. Messages that arrive while the receiver is disabled are dropped.
- **Modem:** a connected line, or the internal loop, shows DSR, CD and CTS. A change sets the data set change bit and the change flags (misc high byte), and interrupts to the transmit vector if DSIE is set.
- **Other modes:**
  - HDLC/SDLC: frames are sent as 0176, a 16-bit length, then the frame with an FCS (CRC-CCITT) when error control is on. The secondary address is checked on receive.
  - GEN BYTE: N sync characters, then raw data. The receiver waits for two syncs, strips syncs when asked, and posts match-character interrupts. Each buffer is filled until its count reaches 0.
  - These two modes only work between two DMF32s and are **not tested** by any operating system.
- **Internal loop** (receive CSR bit 8): transmitted messages go straight to the receiver.

### Tests (VMS V4.7, `/home/claude/dmvtest`)

Files: `boot_dmfsync.sh` v3, `dmfsync_test.sh` v1, `boot_model.sh` v3, `n1/dmf1_780.ini`, `n2/dmf2_780.ini`.

| Test | Result |
|---|---|
| Autoconfigure XGA0 (780, 730, 750, 8600) | ok |
| Circuit DMF-0 between two VAX-11/780s, each with a DMF32 | up at the first try |
| HELPLIB.HLB (2235 blocks) copied each way at the same time | 29 s; checksums identical to the originals; no data errors or timeouts |
| BACKUP.EXE copy | identical to a local DECnet loopback copy (both differ from the original in the bytes after end-of-file, which FAL does not copy) |
| DMF32 ↔ DMP11 (`SET DMC0 TYPE=DMP` on node 2): circuits DMF-0 / DMP-0 up, 2235-block copy | identical checksum |
| Line dropped (DETACH DMF3) and re-attached | adjacency down, circuit comes back up by itself |
| `SET LINE DMF-0 CONTROLLER LOOPBACK` + `LOOP CIRCUIT DMF-0` (internal loop, DDCMP maintenance messages) | ok; the circuit even comes up with itself as the adjacent node |
| SET HOST over DMF-0 | works; repeated attempts sometimes end at once with `REM-S-END`, as already seen with the DMP |
| Async lines and printer on 730, 750, 8600 | ok |
| VAX 8200 | VMS V4.7 stops after the banner **with or without** the DMF32, so not a DMF issue |
| Diagnostic supervisor tests on all five simulators | passed |

## Phase 3 (2026-10-28): parallel interface and CONFIG

### Where things are

- Branch `dmf32`, local commit 0a9b393. Branch `dmf32-standalone` has the same change as 7c35777.
- Patches, each a single cumulative commit that replaces v1 and v2:
  - `opensimh_dmf32_v3.patch` applies after `opensimh_dmv11_v6.patch`;
  - `opensimh_dmf32_v3_standalone.patch` applies to plain Open SIMH 87eb7d5.
- `opensimh_vax8200_uba_fix.patch`: see the VAX 8200 section below.
- With v3 all four DMF32 functions are emulated.

### CONFIG = switch pack 3

The printer and the parallel interface share connector J3 and their hardware (TD Table 1-3; the drawings label the J3 lines "DR/LP"). `SET DMF CONFIG=` sets the device code at power-up:

| CONFIG | Functions | Device code |
|---|---|---|
| PRINTER (default) | async + sync + LP | 1110 |
| PARALLEL | async + sync + parallel | 1101 |
| ASYNCLP | async + LP | 1010 |
| ASYNC | async | 1000 |

Functions that are not configured do not respond. A program can switch to another valid configuration by writing CSR0, as the TD describes for diagnostics; `SHOW DMF` then reports "now ...". VMS V4.7 has no driver for the parallel interface: with CONFIG=PARALLEL it configures TXA0–7 and XGA0, and LCA0 is gone.

### Parallel interface

- **Registers:** CSR base+30, output buffer +32, input buffer (read) / miscellaneous register (write) +34, indirect registers +36 selected by CSR bits 4:3:
  - ind. 0 and 2: primary and secondary word address, bits 16:1; bit 17 comes from misc bits 15/14;
  - ind. 1 and 3: word counts, or the number of silo entries in silo mode.
- **DR11-C mode (0000):**
  - A write to the output buffer drives the lines and pulses New Data Ready. A byte write changes one half and pulses NDR High or Low.
  - Reading the input buffer samples the lines and pulses Data Transmitted.
  - A 0→1 change of request A or B interrupts to vector A or B if enabled.
- **Silo modes (01xx):** a 32-word silo. Every edge of the selected request line moves one word. Flush is CSR bit 11.
- **DMA modes (10xx):** primary and secondary buffers. The count goes down and the done bit is set when it reaches 0, then P/S toggles and an interrupt is posted. NXM sets DMAERR, which only a master reset clears. In transmit mode a word written by the program goes out at the next request.
- A transfer that cannot be done yet gets no handshake pulse until the program makes room, as the manual describes: silo full or empty, or both DMA buffers done.
- **User device (unit DMF4):** `ATTACH DMF4 port` (listen), `ATTACH DMF4 Connect=host:port`, or `ATTACH DMF4 Connect=/dev/ttyACM0` (serial; untested). The signals travel as text lines, hexadecimal values ending in LF:
  - to the device: `D hhhh` (output lines), `N hhhh HL|H|L` (NDR pulse), `T` (DT pulse), `C n` (CONTROL ZERO/ONE), `I` (INIT);
  - from the device: `D hhhh` (input lines), `A 0|1`, `B 0|1` (request lines).
  - The same protocol over USB serial is meant for a microcontroller board (Raspberry Pi Pico plus 5 V level shifters) driving real TTL lines; that board firmware is not written yet.

### Tests

There is no operating system driver, so `drtest/` (`run_drtest.sh` v1) tests the interface directly:
- `vaxasm.py` v3 is a tiny assembler;
- `drprog.py` v2 builds a stand-alone VAX program that runs on the 780 with physical addressing at IPL 31 and uses UBA map registers 0–1 for DMA;
- `drdevice.py` v1 plays the user device over TCP.

All 14 checks pass:
- DR11-C word and byte writes, the input handshake, interrupt A on request A;
- silo receive of 5 words, silo transmit of 3 words, flush;
- DMA receive into the primary and secondary buffers;
- DMA transmit of 3 words plus a word written by the program, with the 4th request held until that write.

The VMS regression (terminal lines, printer, DECnet over DMF-0 with checksums) was rerun with the v3 code. The diagnostic supervisor tests pass on all five simulators.

## VAX 8200 simulator fix (2026-10-28)

VMS V4.7 hung after the banner on the VAX 8200 simulator, with or without the DMF32. This is a simulator bug in `VAX/vax820_uba.c` (the DWBUA Unibus adapter), not a problem of the VMS image.

- **What happens:**
  1. During start-up, VMS resets the adapter (a BI node reset), reloads the Unibus map, and sets the error interrupt control register EICR to 000801D0 (IPL 17, vector 1D0).
  2. The UDA50 is still polling the command rings VMB set up. Its next access hits an invalid map register, so the adapter requests its error interrupt, and the UDA50 reports a fatal error.
  3. VMS then re-initializes the UDA50. The UDA50 never gets past step 1, because the step interrupt never arrives. VMS retries every 15 s for ever.
- **Why the interrupt never arrives:**
  - `uba_eval_int()` looped over `IPL_HMAX - IPL_HMIN` = 3 levels, so it never raised IPL 17. The adapter interrupt stayed pending for ever.
  - While an adapter interrupt was pending, the function did not pass on any Unibus device request (`int_req`). The examined state showed `IPL15 = 00000010` (the UDA50 request), `INT = 1`, and all `nexus_req` = 0.
- **The fix:** evaluate the adapter interrupt (at the EICR levels) and the Unibus device interrupts independently, over all `IPL_HLVL` levels.
  - Branch `vax8200-uba-fix`, commit on 87eb7d5, file `opensimh_vax8200_uba_fix.patch`. It is independent of the DMV11 and DMF32 patches and was also cherry-picked onto `dmf32` (f0a5ad3) and `dmf32-standalone`.
  - `vax820_uba.c` carries the usual first-line marker.
- **Tested:**
  - VMS V4.7 boots on the VAX 8200 (CPU type 5, date prompt) and the DMF32 works: TXA0–7, LCA0 and XGA0 online, Telnet login and directory listing, printing.
  - The diagnostic supervisor test passes.
- **Second fix (after the DWBUA manual EK-DWBUA-TM-001):**
  - The bit VMS sets in the BIIC's BICSR is bit 10, Start Self-Test (in the simulator `BICSR_RST`, "node reset"). The manual says in section 2.3.1 that it runs the DWBUA self-test, which re-initializes the adapter.
  - Section 3.3.2, item 4: "The DWBUA asserts UNIBUS AC LO when a processor sets the SST bit in the BICSR. This mechanism for initializing the DWBUA also initializes the UNIBUS."
  - The simulator reset only the adapter. It now also initializes the Unibus (`uba_ubpdn`: Unibus devices reset, UBPUP cleared for the initialization time).
  - Result: the UDA50 is reset with the adapter, the invalid map error no longer occurs, and VMS boots without any adapter error interrupt.
  - The SST fix alone also lets VMS boot. The interrupt level fix is still needed for any later DWBUA error interrupt at IPL 17.
  - Second commit on branch `vax8200-uba-fix`; `opensimh_vax8200_uba_fix.patch` now contains both commits. Both are also on `dmf32` and `dmf32-standalone`.
  - Retested: VMS V4.7 on the 8200 with the DMF32 (TXA, LCA, XGA, login, printing), and the diagnostic supervisor test.
- `boot_model.sh` v4 answers the 8200's date prompt.

## Not done yet

- Firmware for a USB microcontroller bridge that drives real parallel lines.
- A real serial port for the synchronous line, and a line speed limit (the internal baud rate generator setting is stored but not applied).
- HDLC/SDLC and GEN BYTE modes are untested.
- Break detection on receive, and split receive speed on serial ports (only the transmit speed is applied).
- The parity-error test command in CSR1.
- DAVFU loading (the codes are passed through when ind. 6 bit 11 is set).
- Not tested yet: 4.3BSD (dmf.c, dmfreg.h).
