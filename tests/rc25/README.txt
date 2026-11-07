open-simh-extended/tests/rc25/README.txt  v3 (2026-11-07; v3: realism rules c10, g1, e3; v2: rc25test.py)

RC25 sound realism tests on the MicroVAX 3900 simulator (console and VMB
only, no operating system).  Run them from the directory that holds the
disk images, e.g. with SDL_AUDIODRIVER=dummy on a host without audio:

    BIN/microvax3900 tests/rc25/c1.ini

Images for c4, c5, c6, c9, c9b (raw, LBN order, made by
tools/make_vmb_seek_disk.py; block 0 is a VMB boot block, the loaded
blocks are zero = HALT):

    python3 -I tools/make_vmb_seek_disk.py rcf.img 50902 40000 100
    python3 -I tools/make_vmb_seek_disk.py rcr.img 50902 3000 10
    python3 -I tools/make_vmb_seek_disk.py ra60.img 400176 300000 50

c1, c2 and c6 create their own empty images (ATTACH -N).  The realism
tests set SET RQ KLESI (sound patch v13: RC25 drives only on a KLESI).

Test   What                                                  Expected
-----  ----------------------------------------------------  ---------------------------------
c1     realism: pairing, Run button on either unit, no       messages as in the handover
       start without a cartridge, waits, DETACH, HELP RQ
c2     realism: BOOT DUA1 with the drive stopped             ?4D DEVOFFLINE
c4     realism: VMB transfers, SET RQ DEBUG=TRACE timing     DUA1: long seek 645 cyl, ~157 ms;
                                                             DUA0 seeks back from cyl 646
                                                             (shared actuator)
c5     RA60 regression (realism)                             long seek 1190 cyl, 80..97 ms
c6     normal mode: RCF25 without cartridge, Run button      boots; OFFLINE -> ?4D; DETACH of
       acts on both units, DETACH of the cartridge           the cartridge -> RCF25 boots
c9     ?53 self-test error after a failed VMB boot and       see known issues case 22
       SET RQ0 ONLINE (RA60, normal mode)
c9b    the same with DETACH/ATTACH instead of the button     ?53 too: not related to the sound work
e3     failed VMB boot with nothing attached, BOOT CPU       ?53 also on plain Open SIMH (case 22)
c10    vax730, commands only: realism configuration rules    messages in the echo lines
       (KLESI, RC25 pairs, unit numbers)

Set SET DEBUG only after the console self-test (the expect lines do
this): debug output during the self-test causes ?53 errors by itself.

VMS V3.5 tests (VAX-11/730; the system disk is an RCF25 image, e.g. Ulli's
VMS035.RC25; the image is copied, not changed):

    python3 -I tests/rc25/rc25test.py BIN/vax730 VMS035.RC25 <work dir> [N1] [R1]

N1 normal mode, R1 realism mode; see the header of rc25test.py and the
handover (section "Tests with VMS V3.5 on the VAX-11/730").

g1.ini (vax730) prints the geometry VMS sees (F$GETDVI) for a copy of the
system disk named g1.rcf25: expected GEOM 31/2/821/50902.
