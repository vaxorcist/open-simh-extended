open-simh-extended/tests/klesib/README.txt  v1

KLESI-B (TB) tests on the VAX 8200/8300 (2026-11-16, patch 11), see
docs/KLESIB_VAX8200_design.md.  VMS V4.7 with Ulli's image VMS047_1.R81
(RA81 on the KDB50, SYSTEM/MANAGER; not part of the repository).

mkini.py v1   generates the scripts (one DCL command per expect step):
                b1.ini  DWBUA + KLESI-B, new tape on TB0 = MUB0: SYSGEN
                        SHOW/ADAPTER, SHOW/CONFIGURATION, INITIALIZE, BACKUP,
                        BACKUP/LIST, restore, DIFFERENCES
                b2.ini  VAXBI-only (SET UBA DISABLE): TB0 = MUA0, the b1
                        tape: BACKUP/LIST, a second save set, BACKUP/LIST
                b3.ini  VAX 8300 (SET KA1 ENABLE), VAXBI-only: as b2
                b4.ini  regression: the b1 tape on TQ0 (TUK50 on the DWBUA),
                        TB disabled: BACKUP/LIST
              Usage: python3 mkini.py <dir>

Run (in <dir>, with a copy of VMS047_1.R81):

    SDL_AUDIODRIVER=dummy vax8200 b1.ini > b1.log < /dev/null
    cp tb0.tap tb0_b2.tap
    cp tb0.tap tb0_b3.tap
    cp tb0.tap tb0_b4.tap
    SDL_AUDIODRIVER=dummy vax8200 b2.ini > b2.log < /dev/null
    SDL_AUDIODRIVER=dummy vax8200 b3.ini > b3.log < /dev/null
    SDL_AUDIODRIVER=dummy vax8200 b4.ini > b4.log < /dev/null

Each script ends with "*** bN.ini done".  Expected: b1 "Total of 24 files,
4324 blocks" and "Number of difference sections found: 0"; b2-b4 the same
total, b2/b3 also the listing of S.BCK (SYSGEN.EXE, 129 blocks).

Results 2026-11-16: b1-b4 pass (about 8 minutes each).
