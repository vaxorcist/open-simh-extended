open-simh-extended/tests/rx50_8200/README.txt  v1

VAX 8200 console floppy (CS device, an RX50 drive with two slots) in the
sound realism mode (2026-11-11).

Files
-----
mkprog.py   writes prog.ini: a VAX program at 1000 that drives the console
            floppy controller registers (200B0000) directly: read sector
            for a list of (slot, track, sector), polling CS0 Done
prog.ini    the do-file made by mkprog.py
t1.ini      vax8200, realism mode: runs the program with SET CS DEBUG=FNC
            (one line per command with its realistic time), then SHOW CSn
            DRIVE before and after 3 s at the prompt

Commands (in a directory with prog.ini and t1.ini):

  python3 -c "open('a.rx50','wb').write(bytes(409600))"
  python3 -c "open('b.rx50','wb').write(bytes(409600))"
  BIN/vax8200 t1.ini

Results (2026-11-11)
--------------------
  slot A track 40 sector 3     620.4 ms  motor start 0.5 s, 40 steps, head load
  slot A track 40 sector 5      40.0 ms  every second sector: two sector times
  slot A track 40 sector 7      40.0 ms
  slot A track 40 sector 9      40.0 ms
  slot A track 40 sector 10    220.0 ms  the very next sector: just missed
  slot A track 40 sector 9     180.0 ms  back one sector
  slot B track 40 sector 7     101.2 ms  no steps (one carriage), A unloads,
                                         B loads (own diskette phase)
  slot B track 10 sector 1     280.0 ms  30 steps + settle
  slot A track 79 sector 10    638.8 ms  69 steps, B unloads, A loads

After the program: both slots "motor running (off in 2.00 s)"; after 3 s
at the prompt: heads unloaded, "motor off" for both (one motor).
