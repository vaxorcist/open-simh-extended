<!-- open-simh-extended/docs/DMV11_OpenSIMH_handover.md  v5 (v6 TYPE=DMP status added; use opensimh_dmv11_v6.patch) -->
# DMV11 for Open SIMH: handover for the next session

Written 2026-10-26 to continue the work in a fresh session. The cloud workspace of this session (`/home/claude/...`) does **not** survive, so everything needed is in this project:

- `DMV11_OpenSIMH_handover.md`: this file, with status, manual findings, next steps and the multipoint design.
- `opensimh_dmv11_v2.patch`: the current Open SIMH DMV11 work. Base commit 87eb7d5; it contains local commit 487aece.
- `opensimh_dmv11_v3.patch`: if present, item 1 below has been done; use it instead of v2. Check the "v3 status" section at the end.
- `dmv11_test_harness.txt`: all test scripts and .ini files, bundled. Recreate each file from its `===== FILE: path =====` marker.

The user (Ulli) can re-attach:
- the VMS V4.7 disks `VMS047-1.R81` and `VMS047-2.R81` (account SYSTEM, no password; nodes MV1 = 1.1, MV2 = 1.2);
- the manual `EK-DMV11-TM-001_DMV11_Synchronous_Controller_Technical_Manual_Sep81_t.pdf` (202 pages; extract it with `pdftotext -layout`).

## Ground rules from the user

- **Never push anything** to markpizz/simh (Mark Pizzolato) or to any remote. Commit locally only, and hand over patch files.
- The Open SIMH code is written fresh against Open SIMH's own `PDP11/pdp11_dmc.c`. Mark's DMV11-Test branch code is not copied. This is not strict clean-room work, because Claude had read Mark's code.
- File conventions: the first line of each new or changed file carries its complete path and a version number as a comment. Add detailed comments. Always give complete paths when naming files.
- The first line of `pdp11_dmc.c` is a local marker that must be dropped before submitting upstream: `/* /home/claude/open-simh/PDP11/pdp11_dmc.c  local Open SIMH DMV11 work vN (drop this line before submitting) */`.

## Recreating the environment

```
git clone https://github.com/open-simh/simh /home/claude/open-simh
cd /home/claude/open-simh && git checkout -b dmv11 87eb7d5
git apply /path/to/opensimh_dmv11_v2.patch      # or: git am --keep-cr  (the files use CRLF)
make WARNINGS=ALLOWED microvax2 pdp11 vax       # WARNINGS=ALLOWED: unrelated sim_fio.c -Werror issue
cp BIN/microvax2 /home/claude/dmvtest/microvax2.osimh
```

Test harness (rebuild it from `dmv11_test_harness.txt` into `/home/claude/dmvtest`):
- Copy the disks to `/mnt/user-data/uploads/` (boot.sh copies them from there).
- `SIM=/home/claude/dmvtest/microvax2.osimh ./boot.sh` runs two nodes in tmux sessions `node1` and `node2`. It sets the date, logs in, runs STARTNET, and needs about 10–15 minutes.
- `python3 sethost.py 1 MV2` tests SET HOST. The VMS login timer often expires on the overloaded host, so retry once or twice.
- `python3 con.py N 'command' 'regex' timeout` sends a command to node N and waits for its output.
- `boot_mp.sh` and `mp_test.sh` are the 3-node multipoint test. They are written for Mark's build (`microvax2.dbg`, `SET DMV0 DROPS=2`), so adjust the binary name.
- SIMH treats SIGTERM as "stop", so stop simulators with `pkill -9 -x microvax2.osimh`. Wait until ports 1111/2222/3333 are free.
- To flush a debug trace, press Ctrl-E in the tmux pane, then type `CONT`.

## What v2 contains and what has been tested

The work is in `PDP11/pdp11_dmc.c` plus build wiring: makefile, VAX/CMakeLists.txt, both vcproj files, the VAX syslists, `pdp11_sys.c` and `pdp11_io_lib.c` (`{ "DMV" }, 1, 2, 16, 8`).

- The DMV is wired into the VAX and MicroVAX II builds. DMP and DMV share `dmp_dib`, whose `lnt`/`ulnt` is set in `dmc_reset`: 8 bytes per DMP line, 16 per DMV line.
- Bug fixed in `dmc_get_controller_from_address`: it now uses the unit offset `>> (UNIBUS ? 3 : 4)`.
- DMP/DMV host interface: RQI/RDI/RDO handshake, mode command, control command (keys 001–005, 020 read modem, 021 write modem, status read/read-clear/write), RBACC/XBACC buffers in 18-bit and Q22 formats, and the master-clear signature (BSEL4 = 033 for a DMV, 077 for a DMP; BSEL6 = 0305).
- Output codes per Table 3-7:
  - 004 transmit threshold; 010/012/014 protocol events; 024 run.
  - Procedural errors keep the user's other fields. Implemented: 100, 102, 104, 106, 110, 112, 114, 116, 120, 122, 124, 126, 130, 132.
  - 300 buffer too small and 302 NXM, with address bits 16–21 in BSEL7 bits 0–5 (18-bit format) or in BSEL6 with the code in BSEL10 (Q22 format).
  - 304 disconnect (global); 306 overflow (global).
- Tested point-to-point between two VMS V4.7 MicroVAX IIs:
  - DECnet comes up (024).
  - SET HOST works.
  - COPY of HELPLIB.HLB (2235 blocks) gives DIFFERENCES = 0.
  - Turning MV2's circuit off and on gives 304 on MV1; VMS master-clears and recovers.
  - Pausing MV2 for 2 minutes gives 004 on MV1; VMS halts and restarts the tributary without a master clear and recovers.
- Not exercised yet: 010, 012 and 014.

## Facts from the full manual (EK-DMV11-TM-001)

Line numbers refer to `pdftotext -layout` output.

- **Response type codes (BSEL2 bits 0–2):**
  - 0 = receive buffer complete
  - 1 = control response
  - 2 = information response
  - 3 = receive buffer unused (returned)
  - 4 = transmit buffer complete
  - 6 = transmit buffer sent but not acknowledged
  - 7 = transmit buffer not sent
- **Buffer disposition format (Figure 3-8):**
  - 18-bit: SEL4 = address bits 0–15; SEL6 bits 14–15 = address bits 16–17; SEL6 bits 0–13 = count.
  - Q22: BSEL6 bits 0–5 = address bits 16–21; SEL10 bits 0–13 = count.
  - BSEL3 = tributary. In point-to-point, responses always carry 1 in BSEL3.
- **Control response in Q22 mode:** SEL6 holds address bits 16–21 and BSEL10 holds the code. This confirms the v2 guess.
- **Table 3-8, return keys for the information response:**
  - 010 = modem status.
  - 020 = buffer return complete. It is sent after the DMV has returned the buffers, in four cases: STRT received while running (010), maintenance message received while running (012), a halt request for a tributary (private buffers), and a global halt with BSEL3 = 0 (all unused common-pool buffers). BSEL4/BSEL5 are 0.
- **Halt (key 005):** "All outstanding buffers are returned":
  - private receive buffers as type 3;
  - transmit buffers sent but not acknowledged (ack-wait queue) as type 6;
  - transmit buffers still queued as type 7;
  - then information response 020.
- **Global halt (BSEL3 = 0)** only returns the common-pool buffers; it does **not** halt the tributaries. If any tributary is using the common pool, the DMV reports procedural error 136. (v2 currently halts every tributary on a global halt; this is wrong.)
- **Control command BSEL7 (Figure 3-6, Table 3-5):**
  - bit 8 (SEL6 bit 8) = enable common pool: quota += BSEL4. 377 means unlimited. A sum of 377 or more gives error 140 and sets the quota to 376.
  - bit 9 = disable common pool (quota = 0).
  - bits 10–11 = reserved.
  - bit 12 = unlatch polling state.
  - bit 13 = latch polling state (state in BSEL4 bits 0–1).
  - bits 14–15 = reserved (no label in the figure).
  - Using a reserved bit gives 134.
  - BSEL7 controls may accompany keys 000–004.
- **Status access:**
  - Read: any TSS or GSS word.
  - Read and clear: only TSS 07–17 and GSS 15–17; otherwise error 132. (v2 currently allows any location below 030; this is wrong.)
  - Write: TSS 030–037 and GSS 033–037. (v2 allows 030–037 for both.)
- **TSS layout (Figure 5-11, word addresses; low byte listed first):**
  - 0: tributary status flags / NAK reason and tributary address
  - 2: poll flags
  - 3: poll rate / priority
  - 4: reserved / maximum message counter
  - 5: common pool quota / receive threshold errors
  - 6: transmit threshold errors / selection threshold errors
  - 7: data messages transmitted (16-bit)
  - 10: data messages received (16-bit)
  - 11: selection intervals
  - 12: data errors outbound (count / flags OHBCC, ODBCC, OREP)
  - 13: data errors inbound
  - 14: local buffer errors (LBTU, LBTS)
  - 15: remote buffer errors (RBTU, RBTS)
  - 16: selection timeouts
  - 17: local reply timeouts / remote reply timeouts
  - 20–27: DDCMP variables
  - 30: transmit delay preset (default 0)
  - 31: Q/R active (default Q = 377, R = 0)
  - 32: Q/R inactive (0 / 100)
  - 33: Q/R unresponsive (0 / 20)
  - 34: NDM-INACT / TO-UNRSP (10 / 2)
  - 35: TO-DEAD / MXMC (20 / 4)
  - 36: selection timer (default 5670, i.e. 3 s in ms)
  - 37: babbling timer (default 13560, i.e. 6 s)
  - 8-bit counters latch at 255 and 16-bit counters at 65535. Counters are cleared by master clear, by establish, and by read-and-clear.
- **GSS (Figure 5-10):**
  - 15 = remote station errors; 16 = local station errors; 17 = global header BCC errors / maintenance BCC errors.
  - 33 = NUMSYNC (default 10); 34 = STREAM TRIB (13560); 35 = DELTA T (310); 36 = DEAD T (17500); 37 = POLL DELAY (0).
- **Threshold counters:**
  - Transmit: +1 for each STRT sent in ISTRT, each STACK sent in ASTRT, and in run for each REP sent or each NAK received with a reason other than 3. Cleared on entering ISTRT, ASTRT or run; in run also when the error is reported or when a message is acknowledged. While the link stays in ISTRT/ASTRT the counter is not cleared, so the error is reported again on each further error.
  - Receive: +1 for each NAK sent with reason 1, 2, 3, 8, 9 or 16. Cleared by a good message, on entering a start state, or after the report in run.
- **Receive buffer choice (Section 4.6.2.2):**
  1. If the common pool is enabled for the tributary, the quota is not 0, and a common buffer is available, use it.
  2. If that buffer is too small, try a private buffer; if none, NAK 16 (buffer too small).
  3. Otherwise use a private buffer; if it is too small, NAK 16; if there is none, NAK 8 (buffer temporarily unavailable).
  Buffers are taken in order; the DMV never searches for one of the right size.
- **Queue overflow (306):** a fatal global error; the DMV shuts down after posting its responses.
- **Manual typo:** Table 3-6 swaps the descriptions of keys 020 and 021. 020 = read modem and 021 = write modem, as VMS uses them.

## Item 1: corrections from the manual (point-to-point)

1. Status access rules: read-and-clear only TSS 07–17 and GSS 15–17; writes only TSS 030–037 and GSS 033–037; otherwise 132.
2. TSS defaults (030–037) on establish; GSS defaults (033–037) on master clear.
3. Counters: TSS 7 (new data messages sent; retransmissions not counted), 10 (data messages received), 17 low byte (REPs sent), 6 low byte (transmit threshold count), 5 low byte (pool quota).
4. Clear the transmit threshold counter on entering ISTRT and ASTRT as well. v2 clears it only on run and on an ACK.
5. BSEL7 controls: enable/disable common pool with quota; errors 134 and 140; latch/unlatch accepted and ignored.
6. Halt returns buffers, using the existing `dmp_queue_response`/CONTROL_OUT path so that the order is kept: types 3, 6 and 7, then 020. Do the same after 010 and 012.
7. Global halt (BSEL3 = 0): return the common-pool receive buffers (station 0) as type 3 with BSEL3 = 0, then 020 with BSEL3 = 0. Error 136 if a running tributary has the pool enabled. Do not halt the tributaries.

Risk: VMS behaviour after these changes has not been tested. Run the point-to-point regression: boot, SET HOST, copy + DIFFERENCES, a circuit off/on cycle, and a second copy + DIFFERENCES afterwards to catch duplicate buffers.

## Item 2: multipoint design (not started)

The goal is a DMV in mode 4/5 (control station) or 6/7 (tributary station) with up to 12 tributaries. Point-to-point (modes 0–3) keeps the current, tested single-session path.

### Engine facts in Open SIMH `pdp11_dmc.c`

- Engine state is per controller: `controller->link` (DDCMP struct: R, N, A, T, X, flags, rcv_pkt, xmt_buffer, state, timer, Scanning…), `controller->xmt_queue`, `ack_wait_queue` and `rcv_queue` (pointers), `controller->state` (Running/Halted, used by `ddcmp_UserHalt` and `ddcmp_UserStartup`), `controller->line` (TMLN), `controller->modem` and `dmp_xmt_errors`.
- The free queue, completion queue and control_out queue stay shared per controller.
- The `ddcmp_build_*` functions in `PDP11/pdp11_ddcmp.h` always write address 1 into `buf[5]`. The CRC is computed at send time (`ddcmp_tmxr_put_packet_crc_ln`), so `buf[5]` can be set to the tributary address in `dmc_ddcmp_start_transmitter` just before sending. Leave maintenance (DLE) frames at 1. Nothing checks the address on receive.
- In `ddcmp_dispatch`, actions run before the state changes. At the end, a recursive scan calls `dmc_svc(controller->unit)` directly.

### Plan: per-tributary sessions with context loading

- Extend `DMP_TRIB` with: `DDCMP link; BUFFER_QUEUE xmt_queue, ack_wait_queue, rcv_queue (private); ControllerState state; uint8 xmt_errors; TMLN *line (learned drop line, hub only); t_bool pool_enabled; uint8 pool_quota`.
- Add to `CTLR`: `int dmp_cur` (−1 = no session loaded), saved base pointers and line, `TMXR *dmp_hub`, and a once-per-outage flag for 304.
- `dmp_ctx_load(c, slot)` / `dmp_ctx_store(c)`: copy the link in and out; point the queue pointers at the session queues (`rcv_queue` = common or private, chosen per received frame); set `c->state`, `c->dmp_xmt_errors` and `c->line`. Load only when `dmp_cur < 0`, never nested.
- Entry points that must loop over the sessions in multipoint:
  - `dmc_svc`: if a session is loaded (recursive call from dispatch), coschedule and return. Otherwise, per session: start the transmitter and, if the xmt queue is not empty, dispatch(0). The host side is unchanged.
  - `dmc_buffer_fill_receive_buffers`: in multipoint, a new `dmp_mp_receive`. Read frames from the hub lines or the base line, route them by `pkt[5]` to the session (unknown address: drop), learn the hub line, pick the common or private receive queue, dispatch PKT_RCVD, decrement the quota if a common buffer was used, and clear `link.rcv_pkt` afterwards. Read a line only when its tributary has a usable receive buffer, or when the line is not learned yet. This copies the point-to-point back-pressure; otherwise NAK-8 loops spin.
  - `dmc_timer_svc`: per-session timers.
  - `dmc_poll_svc`: new connection or carrier change: dispatch all sessions. In multipoint, always call `dmc_svc` while attached. Also poll the hub mux: `tmxr_poll_conn`, set `rcve = 1` on new lines, `tmxr_poll_rx`/`tx`; when a drop line disconnects, unlearn the sessions on it. Count active lines.
  - `dmc_ddcmp_start_transmitter`: if nothing is loaded, loop over the sessions. If a session is loaded, set `buf[5]` and send on the session line. On the hub, a session without a learned line sends a copy to every connected drop line not claimed by another tributary, then completes the buffer. Ignore SPEED limiting in multipoint.
  - `ddcmp_LineConnected`/`Disconnected`: on the hub, the line counts as up while attached. `ddcmp_UserStartup` in multipoint: session `state == Running`.
  - `dmp_link_station`: if `dmp_cur >= 0`, return that session's address.
  - `ddcmp_GiveBufferToUser`: if `buffer->station == 0` (common pool), set it to `dmp_link_station(c)` so the completion carries the receiving tributary.
- Host commands in multipoint:
  - START and MAINT: load the session, set run, set `state = Running`; raise DTR on the base line only when not a hub; dispatch; store.
  - HALT: load the session, set `state = Halted`, dispatch, return buffers (types 3/6/7, then 020), store.
  - ESTABLISH: initialise the session queues with `dmc_buffer_queue_init(c, …)`, link state Halt, TSS defaults.
  - RBACC: station 0 goes to the common queue (the controller's own `rcv_queue`), otherwise to the tributary's private queue.
  - XBACC: goes to the tributary's session xmt queue.
- Master clear: `memset` of `dmp_trib` drops the sessions; `dmc_buffer_queue_init_all` rebuilds the pool. Set `dmp_cur = -1`. In `dmc_reset`, move the session buffers back to the free queue first.
- Buffer pool: for DMP, allocate more buffers in `dmc_buffer_queue_init_all` (multiplier 8 instead of 2). Do **not** change `DMP_QUEUE_SIZE`, which is also the DDCMP sequence-compare window.

### Transport ("hub", multidrop line model)

- New `SET DMVn DROPS=n` (0–12) and `SHOW DMVn DROPS`. Add it to both `dmp_mod` and `dmv_mod`, with help text, and remove the "Multi-Point mode is not currently emulated" restriction from the help.
- `ATTACH DMVn port` with DROPS > 0 (not SYNC) opens a separate TMXR per controller: `dmp_hub_desc[n]` with `dmp_hub_ldsc[n][12]`, `lines = drops`, `packet = TRUE`, `buffered = 16384`, `notelnet = TRUE`, no modem control, via `tmxr_open_master(hub, port)`. Set `UNIT_ATT` and the filename as `dmc_attach` does. `DETACH` uses `tmxr_close_master`.
- Each tributary simulator uses the normal point-to-point transport: `SET DMV0 PEER=host:hubport`, `ATTACH DMV0 ownport`. All of a tributary station's sessions share its base line.
- A dropped tributary simulator must **not** produce a 304 at the control station: VMS master-clears the whole DMV on 304, which would kill every tributary. The control station simply times out (004), and VMS halts and restarts that tributary.

### What VMS did with multipoint on Mark's branch

- NCP setup on the control station:
  - `DEFINE LINE DMP-0 PROTOCOL DDCMP CONTROL`
  - `DEFINE CIRCUIT DMP-0.2 TRIBUTARY 2 STATE ON`, and likewise `DMP-0.3`
  On the tributary stations the line protocol is presumably `DDCMP TRIBUTARY` (see `n2/mpcfg.txt` in the harness).
- VMS posts multipoint receive buffers to tributary 0 (the common pool). Completions must carry the real tributary in BSEL3. Check which BSEL7 pool bits VMS sets; if it never enables the pool, the quota gating has to be relaxed.
- VMS master-clears the DMV on 304 and on unknown codes. A halt must not trigger a new start-up.
- Test: 3 nodes (MV1 control station hub `DROPS=2` on port 1111; MV2 and MV3 tributaries 2 and 3), routing, SET HOST, `NCP TELL MV3` via MV1, copy + DIFFERENCES, tributary circuit off/on, detach/re-attach of a tributary simulator. See `mp_test.sh`.

## v3 status: item 1 is done

Item 1 is **done**: local commit 962dc54 on top of 487aece. `opensimh_dmv11_v3.patch` is cumulative from 87eb7d5, contains both commits, and replaces the v2 patch. `pdp11_dmc.c` marker is v3.

- All seven points of item 1 are implemented as listed above. The helpers are `dmp_tss_defaults`, `dmp_count_event`/`dmp_session_tss`, `dmp_pool_controls`, `dmp_return_buffer`/`dmp_return_buffers`, and the `DMP_DISP_*`, `DMP_CTL_*` and `DMP_TSS_*` constants.
- **VMS start-up sequence (seen in the trace):**
  1. TSS parameter writes 0230–0237 for tributary 1, *before* it is established; these get error 106, as on real hardware.
  2. DELETE (002).
  3. ESTABLISH plus "enable pool" (SEL6 = 0x0101, BSEL4 = 0160).
  4. START plus "enable pool, unlimited" (SEL6 = 0x0103, BSEL4 = 0377).

  This is why the BSEL7 pool controls are applied **after** the request key. VMS uses common-pool receive buffers (tributary 0) even in point-to-point mode, so a halt returns no receive buffers there.
- **Tested point-to-point with VMS V4.7 (MV1/MV2):**
  - start-up 024 with no procedural errors after the TSS writes;
  - SET HOST;
  - COPY + DIFFERENCES = 0 before and after an MV2 circuit off/on (4 × 020 on MV2, 304 and a master clear on MV1, recovery);
  - a 2-minute MV2 pause with traffic pending: MV1 returned 4 transmit buffers as type 6, then 020, restarted without a master clear, and the copy afterwards gave 0 differences.
- Still not exercised: 010/012/014, the global halt with error 136, and error 140.

## v4 status: renames only

Local commit f09a147. `opensimh_dmv11_v4.patch` is cumulative from 87eb7d5 with all three commits and replaces v2 and v3. `pdp11_dmc.c` marker is v4.

v4 renames identifiers that matched Mark's DMV11-Test code literally; there is no functional change:
- `dmp_mode`/`dmp_mode_set` became `dmp_station_type`/`dmp_station_known`;
- the `CONTROL_OUT` fields `sel2`, `sel4` and `sel10` became one array `image[4]` with the indices `DMP_IMG_SEL2/4/6/10`;
- NAK reason 16 is the named constant `DDCMP_NAK_MSG_TOO_LONG`.

Retested with VMS: DECnet up, copy + DIFFERENCES = 0. See `DMV11_similarity_report.md`. When writing multipoint, keep using fresh names and structure, and run `simcheck.py` from that report again before handing over.

## v5 status: multipoint is done

Local commit 22b9fc1. `opensimh_dmv11_v5.patch` is cumulative from 87eb7d5 with all four commits and replaces every earlier patch. `pdp11_dmc.c` marker is v5.

What is implemented, following the design above:
- `DMP_TRIB.ses` holds the per-tributary DDCMP session.
- `dmp_session_load`/`dmp_session_store` swap the session in and out, with `controller->dmp_loaded` naming the loaded slot (−1 = none).
- `dmp_dispatch`, `dmp_mp_receive`, `dmp_mp_transmit`, `dmp_mp_timers`, `dmp_hub_poll` cover the engine entry points.
- `SET/SHOW DMVn DROPS`; the hub multiplexer uses its own device-less unit (`dmp_hub_unit`), because tmxr's first poll would otherwise cancel and take over the DMV poll unit.
- Per-tributary `SHOW DDCMP`, and help text.

One extra lesson: `ddcmp_compare` used `free_queue->size` as the DDCMP sequence-number window. Enlarging the buffer pool for multipoint (multiplier 8) made that window 512, so acknowledgements were no longer recognised after the message numbers wrapped, and VMS restarted the circuit periodically. The window is now a fixed `2 * <device>_QUEUE_SIZE` (128 for DMP), as before.

Tests with VMS V4.7:
- Point-to-point: two copies + DIFFERENCES = 0; one start-up (024) per side, no halts.
- 3 nodes: MV1 control station (`SET DMV0 DROPS=2`, `ATTACH DMV0 1111`), MV2 and MV3 tributaries 2 and 3 (`PEER=LOCALHOST:1111`):
  - both tributaries reachable over DMP-0.2 and DMP-0.3;
  - SET HOST from MV1 to MV2 and to MV3;
  - `NCP TELL MV3` from MV2, routed via MV1;
  - COPY MV3 → MV1 + DIFFERENCES = 0;
  - MV3 circuit off for 3 minutes, then MV3's DMV detached for 3 minutes: MV2 stayed reachable, MV1 had no master clear, and MV3 came back each time.
  - No procedural errors on any node.
  - SET HOST still needs a retry at times, because the remote login timer expires on this overloaded host.
- Start-up sequence VMS uses as a control station: mode 5 (full-duplex control station); ESTABLISH with "enable pool" for tributaries 2 and 3; START with an unlimited quota; BSEL7 bit 4 (unlatch polling, ignored).

User report (27 Oct): with Mark's build, `ATTACH DMV0 localhost:1111` on the control station plus `PEER=localhost:1111` on the tributary gave "0 connected". The likely cause: the listener bound to one address family while the peer resolved `localhost` to the other. A bare port on ATTACH fixed it. The help text now says so.

Similarity check (`DMV11_similarity_report.md`, v5 column):
- 1091 non-trivial added lines;
- 20 identical to Mark's post-2022 lines, the same generic idioms as in v4;
- no copied blocks;
- 16 lines identical only to Claude's own earlier multipoint commit in Mark's branch: separators, the standard receive call, the tmxr setup fields.

## v6 status: `SET DMCn TYPE=DMP` (27 Oct 2026)

Local commit d78c545. `opensimh_dmv11_v6.patch` is cumulative from 87eb7d5 with all five commits and replaces every earlier patch. The `pdp11_dmc.c` marker is v6, and `pdp11_io_lib.c` now also carries a v6 marker.

Ulli asked for the DMR11 and DMP11 as variants of the DMC11 device, selected with `TYPE=`. What was done:
- **DMR11:** `TYPE=DMR` already existed. The DMR11 technical manual EK-DMR11-TM-002 (Jan 1981, OCR'd, Figure 3-8 and Table 3-5) shows **no tributary mode**: Control In BSEL7 has only bit 0 DDCMP maintenance, bit 2 half duplex and bit 3 long start timer; bits 1 and 4–7 are reserved. So the DMR11 stays point-to-point.
- **DMP11:** `SET DMCn TYPE=DMP` turns **every** line of the DMC device into a DMP11, with the DMP/DMV command/response interface including multipoint, and resets the device. This was Ulli's choice ("whole device"), because the DMP11 has another floating address and vector rank than the DMC11/DMR11. Setting DMC or DMR on any line returns all lines. DMC and DMR can still be mixed per line.
- **Rank:** `auto_config()` in `pdp11_io_lib.c` got an optional hook, `auto_config_rank_check(dptr, rank_name)` (NULL by default), where `rank_name` is the table entry's **first** name. The DMP11 entry is now `{ "DMP", "DMC" }`, and `dmc_rank_check` places the DMC device at the DMC entry or at the DMP entry according to its type. A first version passed the name the device was found under, which is "DMC" at both entries, and placed it twice; that is fixed.
  - PDP-11/70: DMC at 17760070. With TYPE=DMP it moves to 17760300, and the DZ moves down one slot. With the separate DMP device also enabled, DMP is at 17760300 and DMC-as-DMP at 17760310.
- **DMC device lines** got a SEL10 image (`dmc_sel10`), `dmc_drops` and DROPS. The hub tables `dmp_hub_desc`/`ldsc`/`unit` are now indexed by `controller->index`.
- **Help:** TYPE={DMR|DMC|DMP}. Note that TYPE is a per-line (unit) setting, so use `SET DMC0 TYPE=DMP`; `SET DMC TYPE=DMP` without a unit number does not change it.

Tests:
- **VAX-11/780** with the same VMS V4.7 disk. VMS boots there without a date prompt. New files: `n1/node1_780.ini`, `n2/node2_780.ini`, `boot780.sh`, `n*/mp*_780.ini`, `boot_mp780.sh`, `mp_test780.sh`.
  - VMS finds `XDA0:` (the DMP11 driver) at the DMP rank.
  - Point-to-point: DMP-0 on, two copies + DIFFERENCES = 0, a single 024 per side.
  - Three nodes multipoint: everything as on the MicroVAX — routing, `NCP TELL`, copy = 0 differences, MV3 circuit off/on and detach/re-attach without disturbing MV2, no master clear of MV1, no procedural errors. SET HOST works on retry.
- **MicroVAX II DMV regression:** copy + DIFFERENCES = 0.
- **Similarity:** unchanged from v5 (no new matches with Mark's code).

Possible next steps:
- polling and the half-duplex modes (not emulated, as TCP is full duplex);
- more TSS counters;
- a regression test `.ini` for the Open SIMH test suite;
- tests of the never-exercised codes (010, 012, 014, 136, 140);
- before any submission upstream: drop the first-line marker, talk to Mark and the Open SIMH maintainers.
