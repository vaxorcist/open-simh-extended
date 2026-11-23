<!-- open-simh-extended/docs/DEUNA_boot_design.md  v5, 2026-11-23 (v5: B9 with remwrt.tsk, an RSX task image, as LOAD FILE: loads and runs; v4: B9 with a real RSX-11S image (DECserver 500 V2.2): VMS loads it completely; v3: VMS V4.7 as the load host (B9): loaders PASS, the system load needs a real RSX-11S image; v2: implemented in patch 23 (branch deuna-boot), tests B2-B8, DEC's TERUNA runs, known issues 111-114; v1: facts from the DEUNA TM, the DELUA UG and DEC's DECnet-11M V4.2 UNA loaders; design of the BOOT port command for SIMH's XU device; known issue 109) -->
# DEUNA/DELUA BOOT port command for the SIMH PDP-11 (known issue 109)

Ulli's decision (2026-11-22): implement the BOOT port command, option 1:
SIMH does what the board's processor would do with the loaded secondary
loader ("built-in secondary loader"), so that a real load host sees the
same MOP dialogue as with a real DEUNA.

DEC material used (DEC copyright: facts only, in our own words; none of
it goes into the fork or the project):

- EK-DEUNA-TM-PRE, DEUNA Technical Manual (Dec 1982), section 4.3;
- EK-DELUA-UG-002, DELUA User's Guide, chapter 5 (remote boot and
  down-line load, frame formats);
- DECnet-11M V4.2 distribution (BB-AS90B-BC "DECKIT" tape, DOS-11
  format, Ulli's copy): [136,54]SECUNA.SYS and TERUNA.SYS, the UNA
  secondary and tertiary loaders DECnet-RSX serves to a DEUNA (also
  SECQNA/TERQNA, SECDMC/TERDMC and others). Read with SIMH's own
  disassembler (RSX task image: label blocks 0-1, image from block 2).

## 1. What happens on a real DEUNA/DELUA

1. **Host side (M9312 'XE' ROM, 23-E32A9/E33A9):** starts the port, reads
   the default physical address (ancillary function 2, PCB at 700),
   prints it, writes `BR .` (000777) to location 2 and the PCSR0 address
   to location 4, gives the BOOT port command (5) and jumps to 2. The
   CPU now loops at 2.
2. **Board, BOOT port command (DELUA UG 5.1.5, DEUNA TM 4.3.4.1):** the
   board enters the primary load state and sets DNI. It sends a MOP
   Request Program (dump/load protocol 60-01, code 08) to the load
   server address (ancillary function 25), or to the load assistant
   multicast AB-00-00-01-00-00 if none was set:
   device type (DEUNA 1; the DELUA UG gives 11), format version 1,
   program type 0 (secondary loader), software ID (ancillary function
   23, UDBB+10, bits 7:0), processor 0 (system), then the MOP version
   (3.0.0), function and hardware address fields of DELUA UG Table 5-2.
   It repeats the request after 5 s, up to 8 times; then (system
   software) with software ID 0 to the multicast address every 30 s.
3. **The answer**, a Memory Load with Transfer Address (code 00), holds
   the secondary loader (for a DEUNA: SECUNA.SYS). The board stores it
   in **its own memory** and runs it on **its own processor** (a T-11,
   PDP-11 instruction set; DELUA UG 5.1.3 steps 14-15). SECUNA is linked
   for the board's address 014000; it uses the board firmware through
   hook vectors in the board's low memory.
4. **SECUNA (facts from the program):**
   - accepts only dump/load frames (60-01) with code 00, 02 or 03 from
     the load host that answered (its address is kept; its own receive
     filter compares the source);
   - sends a Request Program with device type 01 (UNA), format 01,
     **program type 1 (tertiary loader)**, the software ID the board was
     booted with, processor 0; to the load host, or to the multicast
     address when it has none;
   - each **Memory Load (code 02)** is copied into **host memory** at
     its load address (the board's DMA: host address register pair,
     auto-incrementing data port); it answers with **Request Memory
     Load (code 0A)** carrying the next load number (and an error flag
     byte); a repeated load number is answered again without storing;
     an odd byte count or odd address is refused (the request is
     repeated); a timer repeats the last request after 10 ticks;
   - the final **Memory Load with Transfer Address (code 00)** is stored
     the same way and its transfer address kept;
   - **start of the host:** it writes 000137 and the transfer address
     to host locations 6 and 10 (`JMP @#transfer`), then 012701 to
     location 2. The CPU looping at 2 now executes `MOV #(4),R1`, i.e.
     R1 = the PCSR0 address left at 4, and falls into the JMP: the
     tertiary loader starts with R1 = CSR. Then SECUNA returns control
     to the board firmware (the board is a normal port again).
5. **TERUNA.SYS** (linked at 002000) runs on the PDP-11 and drives the
   DEUNA with an ordinary port driver to request the operating system
   (program type 2, "standard operating system").

The remote boot frame (remote console 60-02, code 06, verification
code, BOOT SEL switches; DELUA UG 5.1.2/5.1.3) takes the host over by
itself (it writes a small program at 2-34 and asserts ACLO) and then
continues as above. Not part of this step.

## 2. Design for SIMH (pdp11_xu.c)

- **BOOT port command:** if the port is attached, the controller enters
  a boot state (primary load), sets DNI, and a boot engine (a timer
  unit or the existing timer service) sends the first Request Program
  (program type 0) as in 1.2; retries every 5 s, 8 times, then every
  30 s to the multicast address with software ID 0. Not attached:
  as today's refusal, with a message.
- **Secondary load:** a Memory Load with Transfer Address (code 00)
  from any host (the first one fixes the load host) ends phase 1. Its
  image is the board's secondary loader; SIMH does not execute it
  (SIMH has no T-11 and no DEUNA firmware) but takes over its role:
  phase 2, exactly as SECUNA (1.4): Request Program type 1, Memory
  Loads into host memory with Request Memory Load answers, final load
  with transfer address, host start by the 2/4/6 words.
- **Device type:** 1 for TYPE=DEUNA, 11 for TYPE=DELUA in the first
  request (DELUA UG); the tertiary request always 01 (SECUNA's value).
- **Receive path:** while the boot engine is active, dump/load frames
  for the board's own address (or the load assistant multicast) go to
  the engine, not to the host's receive ring (the host is not running
  a driver).
- **End:** after the start the board is a normal port in the ready
  state (TERUNA initializes it again).
- **Debug:** a BOOT debug flag logs every MOP frame of the engine.

## 3. Tests (planned in v1)

- A Python MOP load host over SIMH's UDP Ethernet transport (`ATTACH XU
  udp:...`): answers the Request Program type 0 with a dummy secondary
  image, type 1 with a small tertiary in several Memory Loads (prints a
  text with R1 = CSR and halts); checks the frames' fields (device type,
  program type, software ID, load numbers). No DEC files needed.
- With Ulli's files (only in his workspace): SECUNA.SYS and TERUNA.SYS
  served by the Python host; TERUNA then asks for the system image.
- Through the M9312 'XE' ROM (`tests/m9312_roms` X1 changes from
  "BOOT refused" to "boot OK"), and by the BOOT command from a program.

## 4. Implementation (patch 23, branch `deuna-boot`, 2026-11-22)

`PDP11/pdp11_xu.c`, `pdp11_xu.h` (the XU device; also built into the VAX
simulators, which do not use BOOT):

- `xu_boot_begin()` (BOOT port command, attached port only; otherwise a
  message and PCEI): primary load state, DNI, first Request Program.
- `xu_boot_receive()` (called from the read callback): while a boot runs
  it takes every frame; dump/load frames for the board's own address go to
  the engine. Phase 1 accepts a Memory Load with Transfer Address (from
  the load server, if one was set and fewer than 8 requests went out);
  phase 2 accepts Memory Loads (02) and Memory Load with Transfer Address
  (00) from that load host only, checks the load number (load 0 starts
  over, any other unexpected number is answered with the request for the
  expected one), refuses odd addresses and lengths (request repeated with
  the error byte 1), writes the data with `Map_WriteB` and answers Request
  Memory Load with the next number; the final load starts the host.
- `xu_boot_timer()` (the 1 s timer `xu_tmrsvc`): phase 1 repeats after 5 s
  (8 tries), then every 30 s to the multicast with software ID 0; phase 2
  gives up after 10 s without a load (ready state, USCI).
- The load host that sent the secondary becomes the load server address
  (SECUNA keeps it in the board firmware's load server address; TERUNA
  reads it with function 24).
- Write System ID (function 23) keeps the software ID (UDB word 4, bits
  7:0) for the request; a reset clears it and ends a boot.
- `SET XU DEBUG=BOOT`.

Found on the way with DEC's TERUNA, fixed in the same patch: STOP/START
(known issue 111), counter list length (112), Read Load Server Address
(113), SET XU MAC (114).

## 5. Results (2026-11-22)

- `tests/deuna_boot/` B2-B8 PASS (BOOT from a program and through DEC's
  'XE' ROMs, retry after 5 s, a repeated load, the DELUA's device type,
  refusal without a network).
- B7, DEC's own loaders: SECUNA.SYS (1408 bytes) as the secondary, TERUNA.SYS
  (2048 bytes) as the tertiary in 128 Memory Loads. TERUNA relocates itself
  to the top of memory (with the 11/34's MMU), gives STOP, sets the PCBB,
  reads the load server and physical address, resets the port, writes the
  ring format, mode and physical address, STARTs, reads the counters and
  asks the load host for the system (Request Program 08 01 01 02 FF 00 with
  a buffer size parameter of 1026). It accepts Memory Loads and a
  Parameter Load with Transfer Address (code 14 hex) and starts the system
  after checking location 2 for PS 340 (the system image's vectors).
- VMS V4.7 DECnet over a DEUNA on two VAX-11/780s (`tests/dmv_regress/
  pair.sh` v8, `DEV=UNA`): PASS (3 of 3 compares, circuit off/on); the
  same with the build before patch 23: PASS with the same events (one
  "circuit down, circuit fault" while DECnet starts the line, one at the
  test's circuit off).

## 6. VMS V4.7 as the load host (B9, `tests/deuna_boot/vmshost.sh`)

- VAX-11/780 with a DEUNA, NCP: circuit UNA-0 with SERVICE ENABLED; node
  PDPNET with SERVICE CIRCUIT UNA-0, HARDWARE ADDRESS 08-00-2B-12-34-56,
  SERVICE DEVICE UNA, CPU PDP11, SECONDARY LOADER SECUNA.SYS, TERTIARY
  LOADER TERUNA.SYS, LOAD FILE.
- VMS answers the board's Request Program from the load assistant
  multicast and loads SECUNA and TERUNA ("Load, Successful" in OPCOM). It
  takes the loaders as RSX task images (label: base address; the image
  from block 2) and starts them at their base address.
- TERUNA asks VMS for the system; VMS loads the LOAD FILE, but as an
  RSX-11S system image: it reads the downline load parameters that
  DECnet-RSX's VNP stores in the system image (VMS message documentation:
  "the RSX-11 file image of the system", remedy "use VNP"). A test file
  without that structure was loaded as zeros from 0 to 173777 (with a task
  label) or refused with %RMS-E-EOF (without one). The SIMH side worked in
  every run: every Memory Load reached TERUNA through the DEUNA port and
  was acknowledged.
- Needed for a complete VMS test: a real RSX-11S system image (RSX-11S
  SYSGEN, VMR, VNP).
- With a real RSX-11S image (Ulli's DS5TSV.SYS, DECserver 500 Terminal
  Server V2.2, 352 blocks; `LOADFILE=... PDPMEM=248k vmshost.sh`): VMS
  reports all three loads successful ("... Operating system"), TERUNA
  starts the image, and it runs until a trap with no stack yet (PC 113010,
  SP 4); it needs DECserver 500 hardware. The complete net boot path VMS
  load host -> SIMH DEUNA boot engine -> TERUNA -> system image works.
- With an RSX task image as LOAD FILE (Ulli's remwrt.tsk, Lee Gleason's
  LOCALWRITE client, 66 blocks, task label plus image, made with
  tests/dmv11_vmsload/obj2tsk.py; `LOADFILE=remwrt.tsk vmshost.sh`,
  2026-11-23): all three loads successful, TERUNA starts remwrt, which
  prints its banner and "Choose disk unit to write to:" and waits. Its
  DEQNA/RQDX3 part is not usable here (Q-bus devices, the DEUNA is
  Unibus). So VMS also serves a plain task image as the operating system;
  why the task-labelled mknettest.py v4 file came out as zeros was not
  investigated (its label fields differ from obj2tsk.py's).

