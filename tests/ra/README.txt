open-simh-extended/tests/ra/README.txt  v2 (2026-11-10; v2: ra5, SDI rules)

Tests of the fixed RA drives (RA70, RA71, RA72, RA73, RA80, RA81, RA82,
RA90, RA92) in the sound realism mode of the RQ device (sound patch v23).
Qbus tests run on the MicroVAX II (microvax2, SET RQ KDA50), Unibus tests
on the VAX-11/750 (vax750, UDA50A).

Console tests (no operating system; run them from this directory, e.g.
with SDL_AUDIODRIVER=dummy on a host without audio):

    python3 -I ../../tools/make_vmb_seek_disk.py ra81.img 891072 800000 50
    BIN/microvax2 ra1.ini
    BIN/microvax2 ra2.ini
    BIN/microvax2 ra3.ini
    BIN/vax750 ra4.ini
    BIN/microvax2 ra5.ini

Test   What                                                  Expected
-----  ----------------------------------------------------  ---------------------------------
ra1    realism, MicroVAX II, KDA50: RA81, RA92, RA70 offline "offline" x3, RD54 "fixed disk";
       after ATTACH; RD54 ONLINE refused; RQ0 ONLINE/OFFLINE ONLINE waits 2 s, OFFLINE 1.5 s;
       wait; SET -N; registers; VMB reads 50 blocks at     RA81 long seek 1120 cylinders,
       LBN 800000 (SET RQ DEBUG=TRACE)                     about 69 ms; ?06 HLT INST
ra2    realism: BOOT DUA0 with the RA81 stopped              ?4D DEVOFFLINE, DUA0
ra3    normal mode: RA81 ready at ATTACH, OFFLINE/ONLINE     online/offline/online; boots
       at once
ra4    realism, VAX-11/750 (UDA50A): RA80, RA82 ONLINE,      waits; SHOW SOUND: ra80, ra82
       RA82 OFFLINE; SHOW RQ, SHOW SOUND                     (seek) -> default_disk
ra5    realism rules (MicroVAX II): RA81 on the RQDX3        refused (SET SOUND ENABLED, SET
       (before/after SET SOUND ENABLED); KDA50 with RA81 +   RQ0 RA81); accepted; DRIVES=8
       RA92; DRIVES=8; back to RQDX3; RQDX3 with an RA81     refused; RA units -> RD54; refused
       attached; 8 units enabled then SET SOUND ENABLED;     ("would have to change"); refused
       RC25 on the KDA50                                     (8 units); refused (KLESI)

VMS V4.7 tests with Ulli's image VMS047-1.R81 (RA81, VHD; SYSTEM/MANAGER):

    python3 -I ratest.py <BIN directory> <VMS047-1.R81> <work dir> [N1|R1|U1|T1 ...]

N1 normal mode (MicroVAX II, KDA50, RA81), R1 realism mode (RA81 system
disk with RUN/STOP, blank RA92 data disk: INITIALIZE, MOUNT, DISMOUNT spins
it down, MOUNT of the stopped drive asks the operator, mount verification
of the system disk after SET -N RQ0 OFFLINE / ONLINE), U1 realism mode on
the VAX-11/750 (the system as an RA82, flat copy with NOAUTOSIZE), T1
realism mode on the MicroVAX II (the system as an RA72, then as an RA71;
VMS V4.7 cannot mount its system disk on an RA90 or RA92, also without
sound, so the RA92 is tested as a data disk in R1).
vhd2raw.py makes the flat copies (VHD to raw).  The RQ trace of each
realism run (<test>.trc) gives the transfer times and the seek sound
classes; ratest.py prints a summary.

Note: SIMH's RAW format is a host device, not a flat file; ATTACH -N with
SET RQn FORMAT=RAW cannot create a file ("Cannot create").  Use the
default SIMH format for new containers.
