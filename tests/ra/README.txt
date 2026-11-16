open-simh-extended/tests/ra/README.txt  v4 (2026-11-16; v4: SDI rule, ra1/ra5 adapted; v3: ra6, ra7, bus rule; v2: ra5, SDI rules)

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
    BIN/microvax2 ra6.ini
    BIN/vax750 ra7.ini

Test   What                                                  Expected
-----  ----------------------------------------------------  ---------------------------------
ra1    realism, MicroVAX II, KDA50 (RQ0-RQ2 enabled again "offline" x3;
       one at a time): RA81, RA92, RA70 offline after       ONLINE waits 2 s, OFFLINE 1.5 s;
       ATTACH; RQ0 ONLINE/OFFLINE
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
       RC25 on the KDA50                                     (8 units: RD54 units 4-7 disabled,
                                                             SDI rule); refused (KLESI)
ra6    bus rule (MicroVAX II): UDA50A without sound, then    SET SOUND ENABLED refused; KDA50
       SET SOUND ENABLED; KDA50; UDA50A, UDA50 with sound    accepted; UDA50A/UDA50 refused
ra7    bus rule (VAX-11/750): KDA50 without sound, then      SET SOUND ENABLED refused; UDA50A
       SET SOUND ENABLED; UDA50A; KDA50 with sound           accepted; KDA50 refused

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

SDI rule (2026-11-16, known issues 68): in the realism mode an SDI controller
(UDA50, UDA50A, KDA50, KDB50) only connects SDI drives.  SET SOUND ENABLED and
SET RQ <SDI type> disable unattached other units (the default RD54 units) with
a message; the tests enable a unit again (SET RQn ENABLED) right before they
give it an RA type, one unit at a time, because the rules check all enabled
units.

Note: SIMH's RAW format is a host device, not a flat file; ATTACH -N with
SET RQn FORMAT=RAW cannot create a file ("Cannot create").  Use the
default SIMH format for new containers.
