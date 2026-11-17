open-simh-extended/tests/vaxbi/README.txt  v1 (2026-11-17, patch 14)

Tests of the VAXBI configuration of the VAX 8200/8300 (patch 14): VAXBI
nodes, the twelve MS820 memory boards, SHOW BI, the rules.  Design and
results: docs/VAXBI_config_VAX8200_design.md.

Files
  c1.ini        console test (no disk): defaults, SET CPU nM, boards by
                hand, nodes, refusals, twelve boards (warning), SIMH-128MB
                boards, the realism rules; the expected output is in the
                file (nine %SIM-ERROR refusals)
  mem47.ini     VMS V4.7 (VMS047_1.R81 copy, on KDB0) with a memory
                configuration given as the first argument; SHOW
                MEMORY/PHYSICAL_PAGES, SYSGEN SHOW/ADAPTER
  mem55.ini     the same with VMS V5.5 (v55.r82, a copy of VMS055.R82:
                confidential, workspace only); also SHOW CPU
  m54.do        3 x MS820-CA, 1 x MS820-BA, 1 x MS820-AA (54MB)
  m192.do       12 x MS820-CA (192MB): VMS uses only 8 boards (known
                issues 80)

Running (from a directory with the disk copy and the files)

    BIN/vax8200 c1.ini
    SDL_AUDIODRIVER=dummy BIN/vax8200 mem47.ini "set cpu 48m" > m48.out < /dev/null
    SDL_AUDIODRIVER=dummy BIN/vax8200 mem47.ini "do m54.do" > m54.out < /dev/null
    SDL_AUDIODRIVER=dummy BIN/vax8200 mem55.ini "set cpu 512m" > m512.out < /dev/null

Results 2026-11-17
  c1.ini                         as expected
  mem47 "do m54.do"              Main Memory (54.00Mb), BI Memory at 11-15
  mem47 "set cpu 128m"           Main Memory (128.00Mb), BI Memory at 8-15
  mem55 "set cpu 256m"           Main Memory (256.00Mb), 2 x BI Memory (128Mb)
  mem55 "set cpu 512m"           Main Memory (512.00Mb), 4 x BI Memory (128Mb)
  mem55 "do m192.do"             boots, but VMS counts 8 boards: Main Memory
                                 (128.00Mb), SHOW MEMORY fields overflow
