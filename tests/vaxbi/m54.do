; open-simh-extended/tests/vaxbi/m54.do  v1 (2026-11-17, patch 14)
; A mixed configuration of 54MB (3 x 16 + 4 + 2): SET CPU 48M (MCTL0-MCTL2, three MS820-CA on
; nodes 15-13), then an MS820-BA as MCTL3 on node 12 and an MS820-AA as
; MCTL4 on node 11 (their default nodes).
set cpu 48m
set mctl3 type=ms820-ba
set mctl4 type=ms820-aa
