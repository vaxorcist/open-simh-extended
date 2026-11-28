open-simh-extended/tests/nopeer/README.txt  v1

Tests of SET <unit> NOPEER (patch 30, known issue 131): before it, a PEER
once set could not be cleared (SET <unit> PEER= with no value is refused).

Each script sets a PEER, clears it with NOPEER and shows "peer=unspecified";
it also checks the refusals.  Run from this directory:

  BIN/vax780    n1_vax780.ini     DMC0 (DMC11, then TYPE=DMR), DMP1, DMP0;
                                  NOPEER=x -> "Invalid argument" (PEER kept);
                                  NOPEER while attached -> "Unit already
                                  attached"; DMF and DMF-SYNC (both spellings),
                                  refusal while DMF-SYNC is attached
  BIN/microvax2 n2_microvax2.ini  DMV0
  BIN/pdp11     n3_pdp11.ini      DMC0 and DMP0 on an 11/34, DMV0 on an 11/73;
                                  ATTACH after NOPEER only listens
  BIN/vax8200   n4_vax8200.ini    DMB-SYNC; SET DMB0 NOPEER (async unit) ->
                                  "Command not allowed"

Expected "peer=unspecified" lines: n1 7, n2 1, n3 4, n4 1.  Each script
ends with EXIT.  Example:

  timeout -s KILL 20 ../../../open-simh-extended/BIN/vax780 n1_vax780.ini
