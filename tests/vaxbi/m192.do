; open-simh-extended/tests/vaxbi/m192.do  v1 (2026-11-17, patch 14)
; 192MB of memory, the most real boards give: twelve MS820-CA.  SET CPU 128M
; puts in MCTL0-MCTL7 (nodes 15-8); MCTL8-MCTL11 get the free nodes 1, 3, 5
; and 7 (KA1, DMB32 and DEBNA are disabled; KDB50 4, KLESI-B 6 are taken
; only when enabled: TB is disabled by default), then their type.
set cpu 128m
set mctl8 node=1
set mctl9 node=3
set mctl10 node=5
set mctl11 node=7
set mctl8 type=ms820-ca
set mctl9 type=ms820-ca
set mctl10 type=ms820-ca
set mctl11 type=ms820-ca
