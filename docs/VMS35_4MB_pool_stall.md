<!-- open-simh-extended/docs/VMS35_4MB_pool_stall.md  v5 -->
# VMS 3.5 on the vax750, 4 MB: the startup stall (idle pauses)

Finding of 03-Nov-2026.
- v2: the two checks, with the crash dump.
- v3: the root cause, found by disassembling VMS's pool code and confirmed with WSMAX.
- v4: the parameters of fresh V3.5 installations (by Ulli, 04-Nov-2026) compared with the image's.
- v5: a fresh 8 MB installation added. The image's parameters come from no installation at all.

Test scripts:
- /home/claude/simh-sound/hang/*.ini (isolation runs)
- /home/claude/simh-sound/check/*.ini (crash dump, runs without sound and with more pool)
- /home/claude/simh-sound/pool/*.ini (SYS.MAP, disassembly, expansion variables, WSMAX test)

## Symptom
With SET CPU 4M, booting VAX/VMS V3.5 from the RP06 can stop after one to three
`%SYSGEN-F-INSFMEM` messages. The system then runs only the null process and does no disk
I/O. The stall is intermittent, and happens both with and without SET SOUND ENABLED.

## Result in one sentence
VMS 3.5 lets nonpaged pool grow only while the remaining pageable memory stays above
the maximum working set size (MAXWSCNT, derived from WSMAX). With WSMAX 7500 on a 4 MB
machine this leaves room for about 15 extra pool pages, so SYSGEN runs out of pool during
AUTOCONFIGURE and waits forever in MWAIT. The simulator is not at fault.

## Evidence

### 1. The stalled system (forced crash dump, SDA)
- STARTUP (running SYSGEN.EXE) is in MWAIT with event flag wait mask 3. 3 is RSN$_NPDYNMEM,
  nonpaged dynamic memory (from SYS.MAP).
- SCH$GL_RESMASK is 00000008, so resource 3 is being waited on.
- Nonpaged pool: 63,488 bytes, 48 bytes free. NPAGEDYN is 55,808, so the pool did grow by
  15 pages.
- Only DLA0 and DLA1 exist: SYSGEN was creating the next RL unit when the pool ran out.

### 2. The expansion code (EXE$ALONONPAGED, EXE$EXTENDPOOL; disassembled with SIMH, names from SYS.MAP)
- When an allocation from the variable pool fails, EXE$ALONONPAGED sets bit 0 in
  MMG$GL_NPAGNEXT ("expansion wanted") and calls EXE$EXTENDPOOL, which adds one page.
- Limit A (the documented one): MMG$GL_NPAGNEXT must stay below
  MMG$GL_NPAGEDYN + SGN$GL_NPAGEVIR.
- Limit B (not in the parameter description): a physical page is taken only if
  `PFN$GL_PHYPGCNT − (MPW$GW_LOLIM + SGN$GL_FREELIM) > SGN$GL_MAXWSCNT`.
  Each pool page then permanently lowers PFN$GL_PHYPGCNT by one.
- Waiting processes are woken only by EXE$DEANONPAGED (pool returned) or after a successful
  expansion. Both go through SCH$RAVAIL with resource 3.

### 3. The variables

| Variable | 4 MB, WSMAX 7500 (at the stall) | 8 MB, WSMAX 7500 | 4 MB, WSMAX 4000 |
|---|---|---|---|
| PFN$GL_PHYPGCNT | 7,389 | 15,206 | 7,323 |
| MPW$GW_LOLIM + SGN$GL_FREELIM | 24 + 16 = 40 | 40 | 40 |
| SGN$GL_MAXWSCNT | **7,349** | 7,500 (= WSMAX) | 4,000 (= WSMAX) |
| Room left for limit B | **7,389 − 40 − 7,349 = 0** | 7,666 pages | 3,283 pages |
| Pool size (NPAGNEXT − start) | 63,488 (bit 0 set: expansion wanted) | 111,616 | 98,304 |
| Limit A (NPAGEVIR) | 167,936, not reached | not reached | not reached |

- At 4 MB, SYSBOOT trims MAXWSCNT below WSMAX (7,349 < 7,500), close to all pageable memory.
  After 15 pages the expansion check fails exactly: 7,349 is not greater than 7,349.
- At 8 MB the startup needs about 109 KB of pool and gets it.

### 4. Confirmation runs
- WSMAX 4000, default NPAGEDYN, 4 MB, sound enabled: 3 of 3 boots finish with no INSFMEM.
  The pool grew to 96 KB.
- NPAGEDYN 120000, 4 MB, sound disabled: 3 of 3 boots finish with no INSFMEM.
- Default parameters, 4 MB, sound disabled: 2 of 5 boots stall. The timing (and so
  which allocation hits the wall first) varies with the wall-clock calibration.

## Why it looked related to sound
STARTUP's SYSGEN AUTOCONFIGURE competes for pool with the starting system processes.
- If a CONNECT fails outright, VMS prints INSFMEM and STARTUP goes on.
- If the RL units get their UCBs first, a later request waits in MWAIT.

The realism timing makes the second case more likely. It does not create it.

## Is it a VMS bug?
It is VMS 3.5 behaving as coded, with parameters that don't match the machine. WSMAX 7500 is
more than a 4 MB system can give one process, and the pool's growth is tied to that value. The
manual warns that pool depletion leaves processes in MWAIT "until sufficient memory is returned
... you may be forced to reboot". AUTOGEN for a 4 MB configuration would normally set a smaller
WSMAX and a larger NPAGEDYN. Later VMS versions are untested.

## Parameters of fresh V3.5 installations (Ulli, 04-Nov-2026)

| Memory | WSMAX | NPAGEDYN | NPAGEVIR | FREELIM | MPW_LOLIMIT |
|---|---|---|---|---|---|
| 2 MB, fresh install | 2,974 | 87,552 | 263,680 | 58 | (not given) |
| 4 MB, fresh install | 3,500 | 87,552 | 263,680 | 58 | (not given) |
| 8 MB, fresh install | 3,500 | 87,552 | 263,680 | 58 | (not given) |
| The test image | 7,500 | 55,808 | 167,936 | 16 | 24 |

- The installation gives the same pool parameters for 2, 4 and 8 MB. Only WSMAX differs:
  3,500 at 4 and 8 MB, and 2,974 at 2 MB.
- None of these matches the test image. Its values were changed by hand afterwards:
  - WSMAX was raised to 7,500;
  - NPAGEDYN and NPAGEVIR were lowered;
  - FREELIM was set to 16, the SYSGEN minimum.
- Booted at 4 MB, that WSMAX leaves pool expansion no room, which is the stall.
- A fresh 4 MB installation gives about 3,800 pages of room for limit B
  (about 7,390 − (58 + MPW_LOLIMIT) − 3,500). NPAGEVIR (263,680) becomes the real limit, as the
  manual describes.
- It also starts with a larger pool: NPAGEDYN 87,552 against 63,488 at the stall. The stall
  cannot happen with these values. This was calculated, not yet tested by booting the fresh system.
- Lesson: never trust an image with an unknown history. Check its SYSGEN parameters, or run
  AUTOGEN for the configuration it will run on.

## Remedies (guest side)
- Lower WSMAX for small memory, e.g. 4000: `RUN SYS$SYSTEM:SYSGEN`, `USE CURRENT`,
  `SET WSMAX 4000`, `WRITE CURRENT`, then reboot. Verified 3 of 3.
- Or raise NPAGEDYN, e.g. 120000. Verified 3 of 3.
- Or run AUTOGEN for the actual configuration, or use 8 MB, or SET RL DISABLE.
