<!-- open-simh-extended/docs/DMV11_similarity_report.md  v3 (v5 multipoint and v6 TYPE=DMP results added) -->
# DMV11 for Open SIMH: literal similarity check against Mark Pizzolato's DMV11-Test code

Date: 2026-10-26. Prepared with Claude for Ulli (vaxorcist).

**This is not legal advice.** It is a factual, reproducible comparison of source text.

## Background and disclosure

The Open SIMH DMV11 implementation (local commits 487aece, 962dc54 and the v4 rename commit, built on Open SIMH 87eb7d5) was written by Claude in the same working session in which Claude had earlier read and modified Mark Pizzolato's `markpizz/simh` branch DMV11-Test (commit f30f963, 25 Oct 2026). On that branch Claude fixed a segfault and a SET HOST hang and added multipoint support.

The Open SIMH code was written fresh against Open SIMH's own `PDP11/pdp11_dmc.c` and checked against the DEC manual EK-DMV11-TM-001. It is **not** clean-room work: knowledge of Mark's implementation, and of how VMS reacted to it, influenced the design. Every commit carries a `Co-Authored-By: Claude` line.

## Method

The only substantial change is to `PDP11/pdp11_dmc.c`. The other changed files are one-line build and device-list entries.

1. Files compared:
   - **base**: Open SIMH 87eb7d5 `PDP11/pdp11_dmc.c`. Its history is shared with Mark's file up to the 2022 split.
   - **Mark**: markpizz/simh f30f963 `PDP11/pdp11_dmc.c`, Mark's own DMV11 work.
   - **mine-in-Mark**: markpizz/simh da0297d, i.e. Claude's two commits on top of f30f963.
   - **ours**: the Open SIMH `PDP11/pdp11_dmc.c` after each version.
2. Every line was normalised: leading and trailing whitespace removed, runs of whitespace collapsed. Trivial lines were ignored: blank lines, braces, `break;`, `else` and lines shorter than 14 characters.
3. Counted were (a) added lines of ours that are identical to a line Mark added after the split, (b) added lines identical only to Claude's own commits in Mark's branch, and (c) runs of three or more consecutive identical lines (`difflib.SequenceMatcher`) that are not base code.

The script is `simcheck.py`, reproduced at the end of this report.

## Results

| | v3 (before renaming) | v4 (after renaming) | v5 (with multipoint) |
|---|---|---|---|
| Non-trivial lines added to the base | 653 | 659 | 1091 |
| Non-trivial lines Mark added to the base | 722 | 722 | 722 |
| Added lines identical to a line Mark added | 28 (4.3 %) | 20 (3.0 %) | 20 (1.8 %) |
| Lines identical only to Claude's commits in Mark's branch | 3 | 3 | 16 |
| Copied blocks (≥ 3 consecutive non-base lines) | none | none | none |

**v6 (`SET DMC TYPE=DMP`, 27 Oct 2026).** 1146 non-trivial added lines. The lines identical to Mark's are the same 20 generic idioms as in v5. 15 lines match only Claude's own earlier multipoint commit; that is one fewer than in v5, because `hub->lines = (int32)dmp_drops[dmc];` changed. There are still no copied blocks.

**v5 (multipoint, 27 Oct 2026).** None of the roughly 430 new multipoint lines is identical to a line of Mark's. The 20 matches are the same generic idioms as in v4.

The 16 lines that match only Claude's own earlier multipoint commit in Mark's branch (da0297d) are:
- separator comment lines;
- `t_bool ans = FALSE;` and `if (controller->link.state == Run)`;
- the standard receive call `ddcmp_tmxr_get_packet_ln (lp, &pkt, &size, *controller->corruption_factor);` and its statistics line;
- `controller->link.rcv_pkt = (uint8 *)pkt;` and `rcv_pkt_size = size;`;
- the tmxr setup statements any listening multiplexer needs: `memset (hub, …)`, `hub->lines = …`, `hub->buffered = 16384;`, `hub->dptr = dptr;`, `tmxr_open_master (hub, cptr);`;
- one help-text line.

The one "block" the tool reports is base code that both versions inherited from the shared history. Inside it, both made the same forced one-word edit: `DMC_C_TYPE_RBACC` became `dmc_is_dmc(controller) ? DMC_C_TYPE_RBACC : DMP_C_TYPE_RBACC`, and the same for XBACC.

### The 20 remaining identical lines (v4)

All are short statements that follow from the existing Open SIMH code or from the hardware:

- `if (!dmc_is_dmc(controller)) {` (2×)
- the RBACC and XBACC selection lines above (2×)
- `uint16 sel2 = *controller->csrs->sel2;` (2×)
- `if (station) {` (2×), `if (!station) {` (2×), `switch (key) {`
- `dmc_set_modem_dtr(controller);`
- `tmxr_set_get_modem_bits(controller->line, set_bits, clear_bits, NULL);`
- `buffer = dmc_buffer_queue_add(controller->xmt_queue, addr, count, TransmitData);`
- `memset(buffer->transfer_buffer, 0, DDCMP_HEADER_SIZE);`
- `buffer = (BUFFER *)remqueue(&buffer->hdr);`, `free(buffer->transfer_buffer);`
- `ASSURE(insqueue(&buffer->hdr, controller->free_queue->hdr.prev));`
- `remaining = Map_WriteB(buffer->address, buffer->actual_bytes_transferred, controller->link.rcv_pkt + DDCMP_HEADER_SIZE);` The base already calls `Map_WriteB(...)` with these arguments; both versions only capture its return value.
- `buffer->address + buffer->actual_bytes_transferred - remaining,` (the address of the failing byte)

The 3 lines that match only Claude's own commits in Mark's branch: two `/* ---…--- */` separator lines and `if (controller->link.state == Run)`.

### Removed in v4

v4 is a rename-only commit that removes the literal overlaps that showed influence:

- Controller fields `dmp_mode` and `dmp_mode_set` (the same names as Mark's) are now `dmp_station_type` and `dmp_station_known`.
- The response record's added fields `sel2`, `sel4` and `sel10` (Mark's version adds the same fields) are now one array `image[4]`, indexed by `DMP_IMG_SEL2/4/6/10`.
- `controller->link.nak_reason = 16;   /* buffer too small */`, identical to Mark's line including the comment, now uses a named DDCMP reason code, `DDCMP_NAK_MSG_TOO_LONG`.

### Design-level similarities (not literal)

These are ideas, not text, and are listed here for transparency:

- Buffers remember the tributary and the Q22 format they were given with, so their completion reports the right BSEL3. This was learned from VMS behaviour on Mark's branch.
- A receive buffer that is too small is kept, error 300 is reported, and a NAK with reason 16 is sent. This is the behaviour described in manual Section 4.6.2.2.
- A halt must not trigger a new start-up, and VMS master-clears the DMV on unknown response codes. Both are observed VMS behaviour.

## Assessment (not legal advice)

No function, code block or non-trivial comment of Mark's post-2022 code appears in the Open SIMH implementation. After v4, the remaining literal overlap is limited to generic one-line idioms that follow from the existing Open SIMH code and from DEC's hardware interface. Copyright protects expression, not ideas, functionality or interfaces (for example § 69a(2) UrhG). Mark's LICENSE.txt contains a 2022 clause about Open SIMH, so the recommendation stays: disclose how the code was made, and talk to Mark and the Open SIMH maintainers before submitting it.

## simcheck.py

```python
# simcheck.py v1 - literal similarity check between the Open SIMH DMV11
# additions and Mark Pizzolato's DMV11-Test changes.
# Usage: python3 simcheck.py DIR   (DIR holds base.c, v3.c (ours), mark.c, markmine.c)
import re, sys, difflib
d = sys.argv[1]
def load(n):
    return [l.rstrip('\r\n') for l in open(f'{d}/{n}', encoding='latin-1')]
def norm(l):
    return re.sub(r'\s+', ' ', l.strip())
def trivial(n):
    return len(n) < 14 or re.fullmatch(r'[{}();,\s]*|else|break;|return;|return SCPE_OK;|default:|\*/|/\*', n) is not None
base, v3, mark, mm = (load(x) for x in ('base.c', 'v3.c', 'mark.c', 'markmine.c'))
B = set(map(norm, base)); Mset = set(map(norm, mark))
mark_only = Mset - B
mine_in_mark = set(map(norm, mm)) - Mset
added = [(i+1, l) for i, l in enumerate(v3) if norm(l) not in B and not trivial(norm(l))]
print(f'non-trivial lines not in base: {len(added)}')
hit_mark = [(i, l) for i, l in added if norm(l) in mark_only]
hit_mine = [(i, l) for i, l in added if norm(l) in mine_in_mark and norm(l) not in mark_only]
print(f'=== {len(hit_mark)} identical to a Mark post-split line'); [print(f'{i:5}: {l.strip()}') for i, l in hit_mark]
print(f'=== {len(hit_mine)} identical only to Claude commits in Mark\'s branch'); [print(f'{i:5}: {l.strip()}') for i, l in hit_mine]
nv = [norm(l) for l in v3]; nm = [norm(l) for l in mark]
for a, b, size in difflib.SequenceMatcher(None, nv, nm, autojunk=False).get_matching_blocks():
    new = [x for x in nv[a:a+size] if x not in B and not trivial(x)]
    if size >= 3 and len(new) >= 2:
        print(f'block ours {a+1}-{a+size} = mark {b+1}-{b+size}: {len(new)} non-base lines')
```
