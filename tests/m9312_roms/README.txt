open-simh-extended/tests/m9312_roms/README.txt  v2 (2026-11-22; v2: patch 22 fixed the known issues 106-108 and 110: C4/C5/C7 run through, new tests D1 and L1)

DEC's M9312 console/diagnostic and boot ROMs in the SIMH PDP-11
================================================================

These tests run DEC's original ROM code in the SIMH PDP-11 through its ROM
device: ROM0 holds a console/diagnostic ROM at 165000-165777 (SET ROM0
ADDRESS=17765000), ROM1 one or more boot ROMs from 173000 (SET ROM1
ADDRESS=17773000), as the M9312 board puts them in the I/O page.

run_tests.sh BIN/pdp11 [C1 ... X1]   runs all tests (about 1 minute); prints
                                     PASS, FAIL or SKIP per test
mkbstape.py OUT TEXT [--sim-send]    makes a bootstrap-format tape (P1-P3)

The ROM images
--------------
The images are DEC firmware and not part of the repository (.gitignore).
They are built from the MACRO-11 listings and checked against the PROM
programmer files that Don North (AK6DN) published
(https://ak6dn.github.io/PDP-11/M9312/, sources: github.com/ak6dn/
ak6dn.github.io, directory PDP-11/M9312).  With that directory as M9312
and the notes as NOTES, in this directory:

    T=NOTES/tools/m9312_lst2bin.py; M=M9312
    for r in 248F1 446F1 616F1 774F1; do
        python3 -I $T m9312_$r.bin $M/23-$r/23-$r.lst \
            --hex $M/23-$r/23-$r.lst=$(ls $M/23-$r/23-$r-*.hex)
    done
    python3 -I $T m9312_pr_tt.bin $M/23-760A9/23-760A9.lst \
        --hex $M/23-760A9/23-760A9.lst=$M/23-760A9/23-760A9-0681.hex
    python3 -I $T m9312_xe.bin $M/23-E32A9/23-E32A9.lst@173000 \
        $M/23-E33A9/23-E33A9.lst@173200 \
        --hex $M/23-E32A9/23-E32A9.lst=$M/23-E32A9/23-E32A9-05D6.hex \
        --hex $M/23-E33A9/23-E33A9.lst=$M/23-E33A9/23-E33A9-0755.hex
    python3 -I $T m9312_zz.bin $M/23-ZZZA9/23-ZZZA9.lst \
        --hex $M/23-ZZZA9/23-ZZZA9.lst=$M/23-ZZZA9/23-ZZZA9-05FE.hex
    python3 -I $T m9312_xm.bin $M/23-862A9/23-862A9.lst@173000 \
        $M/23-863A9/23-863A9.lst@173200 $M/23-864A9/23-864A9.lst@173400 \
        --hex $M/23-862A9/23-862A9.lst=$M/23-862A9/23-862A9-0625.hex \
        --hex $M/23-863A9/23-863A9.lst=$M/23-863A9/23-863A9-063F.hex \
        --hex $M/23-864A9/23-864A9.lst=$M/23-864A9/23-864A9-0551.hex

Every image is checked word for word against its PROM file (2026-11-22:
all 256 words of each console ROM, all 64 of each boot ROM match).

    image             ROM(s)                    where          for
    m9312_248F1.bin   23-248F1                  165000-165777  11/04, 11/34 console + diagnostics
    m9312_446F1.bin   23-446F1                  165000-165777  11/44 diagnostics
    m9312_616F1.bin   23-616F1                  165000-165777  11/60, 11/70 diagnostics + SR boot
    m9312_774F1.bin   23-774F1                  165000-165777  11/24 diagnostics
    m9312_pr_tt.bin   23-760A9 'PR/TT'          173000-173177  PC11 reader / console DL11
    m9312_xe.bin      23-E32A9, 23-E33A9 'XE'   173000-173377  DEUNA/DELUA network boot
    m9312_zz.bin      23-ZZZA9 'ZZ'             173000-173177  continuous diagnostics (with 248F1)
    m9312_xm.bin      23-862/863/864A9 'XM'     173000-173577  DECnet boot on a DMC11/DMR11

How the ROMs are used
---------------------
23-248F1 (console): DIAG entry 165020 runs the CPU and memory tests, then
the console (NODIAG entry 165144): it prints R0 R4 SP R5 and the prompt @.
Commands (upper case): "L addr<CR>" load address; "E " examine (again:
next word); "D data<CR>" deposit (again: next word); "S<CR>" reset and
start at the load address; two-letter device code + unit + <CR> (e.g.
XM0, PR0, TT0, XE0, ZZ0) boots: the console finds the boot ROM by its
header (code word, then offset to the next header) from 173000, resets
and starts its "standard CSR, unit in R0" entry.  The many CRs in the
output are the ROM's own fill characters.

Boot ROM entries (from 173000 for the first device of a ROM): +4 unit 0
without diagnostics, +6 unit 0 with diagnostics, +12 standard CSR with
the unit in R0, +16 CSR in R1 and unit in R0.  "With diagnostics" calls
the console ROM's secondary diagnostics at 165564 with the return address
in R4; they return to 2(R4).  So every console ROM can be tested through
a boot ROM's +6 entry.

23-616F1, 23-446F1 (11/60-70, 11/44: these CPUs have their own console):
diagnostics only; 616F1 also boots by the switch register (165744:
SR<8:0> = offset into the boot ROMs, SR<15:9> = unit).
23-774F1 (11/24): diagnostics incl. EIS, MMU, all memory, a console
loopback test, then the memory size in 8 octal digits.

PR/TT (23-760A9) writes DEC's bootstrap loader to the top of memory (up to
28K words: 157744-157776) and starts it on the PC11 (177550) or the
console DL11 (177560); the tape must be in bootstrap format, e.g. DEC's
Absolute Loader tape.  mkbstape.py makes a small one (see its comments).

ZZ (23-ZZZA9) uses the 248F1's internal routines at fixed addresses, so it
works only with the 248F1 (as in SIMH here).

XE (23-E32A9 + 23-E33A9, two sockets) starts the DEUNA/DELUA at 174510,
reads and prints its Ethernet address and gives the port command BOOT:
the board itself then loads the system by MOP while the CPU waits at 2.

The tests
---------
C1  23-248F1 console on an 11/34: diagnostics, @, L/D/E/S (auto-increment)
C2  the same on an 11/04
C3  23-248F1 console command XM0: the XM ROM boots over a DMR11 (load host
    ../dmv11_boot/mopctl.py --rom, port 21141)
C4  23-616F1 on an 11/70: SR = 000006, start 165744: XM entry 173006, the
    diagnostics, boot.  Before patch 22 the cache test halted at 165554
    (known issue 106, SIMH's hit/miss register read 0); now no halt
C5  the same on an 11/60
C6  23-446F1 on an 11/44 through the XM entry 173006: diagnostics, boot
C7  23-774F1 on an 11/24 through the XM entry 173006: all tests incl.
    the console loopback test (known issue 107, fixed in patch 22), the
    memory size 01000000 with all 8 digits (known issue 108, fixed)
P1  PR/TT, PC11: the console's PR0, pr.tap prints PR BOOT OK
P2  PR/TT, PC11: entry 173006 (with the 248F1's diagnostics)
P3  PR/TT, console DL11: TT0, then the tape bytes typed by SEND
    (SET TTI 8B; the SEND must not be an EXPECT action, whose escapes are
    decoded once, so a NUL byte would end it)
Z1  ZZ with the 248F1: three passes ("End Pass 000003")
X1  XE on a DEUNA (UDP transport, no peer needed): prints ETHERNET
    PHYSICAL ADDRESS = 08-00-2B-12-34-56; the BOOT port command is not
    implemented in SIMH (known issue 109), so the CPU stays at 2
D1  (no image) SET TTI DEBUG=DAT and a NUL byte typed in: no abort
    (known issue 110, fixed in patch 22)
L1  (no image) console DL11 maintenance loopback: 'A' looped back, not
    on the terminal; 'Z' sent right before RESET still appears (known
    issues 107, 108, fixed in patch 22)

Results 2026-11-22 (integration 0fb87ac2): C1-C7, P1-P3, Z1, X1 PASS
(test v1, with the known issues as expected results).
Results 2026-11-22 (patch 22, m9312-roms 152182d5): C1-C7, P1-P3, Z1, X1,
D1, L1 PASS; D1 and L1 fail on the build before patch 22.
