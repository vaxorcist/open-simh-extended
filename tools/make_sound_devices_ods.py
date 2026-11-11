# open-simh-extended/tools/make_sound_devices_ods.py  v14 (v14: VAX 8200 console RX50; v13: RX50 (RQ); v12: RX01: RX device and the VAX-11/780 console floppy CS; v11: sheet Floppies: RX02; v10: RA81 start 20 s, RA80 40 s from the User Guides; v9: the fixed RA drives RA70-RA92; v8: sheet Streamers: TK50; MicroVAX II THROTTLE; v7: tapes, TS11 and TSV05; v6: default sounds; v5: RC25/RCF25 and the per-track model; v4: paths of the notes branch)
#
# Builds SIMH_Sound_devices.ods: the devices prepared for SIMH sound (realism
# mode, SET SOUND ENABLED) with rotation speed, access times and spin times.
#
# The table is generated from the data below, which mirrors the constants in
# the device sources (pdp11_hk.c, pdp11_rl.c, pdp11_rp.c, pdp11_rq.c,
# vax730_rb.c, pdp11_ts.c) and
# vax750_syslist.c.  When a device gets sound support, add a row here, bump
# TABLE_VERSION and run:
#     python3 tools/make_sound_devices_ods.py docs/SIMH_Sound_devices.ods
#   (from the top directory of the notes branch)
#
# Derived columns (revolution, latency, sector time, seek curve values,
# average access) are computed with the same seek model as the simulator:
# t(d) = a + b * d^e, d in cylinders, t(1) = track-to-track; the exponent e
# is either fixed (HK 0.69, RL 0.67, RP types without a published maximum
# 0.69) or fitted by bisection so the curve passes through the published
# average (at a third of the stroke) and the maximum (RP, RA60).

import sys
from odf.opendocument import OpenDocumentSpreadsheet
from odf.style import Style, TableColumnProperties, TextProperties, TableCellProperties, ParagraphProperties
from odf.table import Table, TableColumn, TableRow, TableCell
from odf.text import P
from odf.dc import Title

TABLE_VERSION = "v13"
TABLE_DATE = "2026-11-11"

# ---------------------------------------------------------------- data
# fit: "fixed" (exponent given, curve through t2t and max), "fit" (exponent
# fitted to t2t/avg/max), "avg" (exponent 0.69 given, curve through t2t and
# avg; no published maximum)
DISKS = [
  # dev, drive, pack, cyl, sect, surf, rpm, t2t, avg_pub, max_pub, fit, exp, slot_div,
  # spin_up_s, spin_down_s, spin_src, s_lim, m_lim, regs, snd_dir, timing_src, notes
  dict(dev="HK (RK611)", drive="RK06", pack="removable", cyl=411, sect=22, surf=3, rpm=2400,
       t2t=8.0, avg=None, mx=75.0, fit="fixed", e=0.69, slots=22,
       up=30, down=30, spin_src="provisional (chosen for the pilot)", s=25, m=150,
       regs="SPINUP, SPINDOWN, SEEK6S, SEEK6M", snd="rk07 (shared with the RK07)",
       src="DEC RK06/RK07 specifications", notes="pilot device"),
  dict(dev="HK (RK611)", drive="RK07", pack="removable", cyl=815, sect=22, surf=3, rpm=2400,
       t2t=6.5, avg=None, mx=71.0, fit="fixed", e=0.69, slots=22,
       up=30, down=30, spin_src="provisional (chosen for the pilot)", s=50, m=300,
       regs="SPINUP, SPINDOWN, SEEK7S, SEEK7M", snd="rk07",
       src="DEC RK06/RK07 specifications", notes="synthetic spindle sounds available separately"),
  dict(dev="RL (RL11/RLV12)", drive="RL01", pack="removable", cyl=256, sect=40, surf=2, rpm=2400,
       t2t=15.0, avg=55.0, mx=100.0, fit="fixed", e=0.67, slots=40,
       up=40, down=30, spin_src="published (start 40 s, stop 30 s)", s=15, m=95,
       regs="SPINUP, SPINDOWN, SEEK1S, SEEK1M", snd="rl02 (shared with the RL02)",
       src="DEC RL01/RL02 specifications", notes="head switch only: 7 ms; ONLINE/LOAD = LOAD button"),
  dict(dev="RL (RL11/RLV12)", drive="RL02", pack="removable", cyl=512, sect=40, surf=2, rpm=2400,
       t2t=15.0, avg=55.0, mx=100.0, fit="fixed", e=0.67, slots=40,
       up=40, down=30, spin_src="published (start 40 s, stop 30 s)", s=30, m=190,
       regs="SPINUP, SPINDOWN, SEEK2S, SEEK2M", snd="rl02",
       src="DEC RL01/RL02 specifications", notes="head switch only: 7 ms; ONLINE/LOAD = LOAD button"),
  dict(dev="RB (RB730, VAX-11/730)", drive="RB02 (an RL02)", pack="removable", cyl=512, sect=40, surf=2, rpm=2400,
       t2t=15.0, avg=55.0, mx=100.0, fit="fixed", e=0.67, slots=40,
       up=40, down=30, spin_src="as the RL02 (published start 40 s, stop 30 s)", s=30, m=190,
       regs="SPINUP, SPINDOWN, SEEK02S, SEEK02M", snd="rl02 (the RB02 is an RL02)",
       src="DEC RL01/RL02 specifications", notes="LOAD button = ONLINE/LOAD, OFFLINE/UNLOAD; head switch only: 7 ms"),
  dict(dev="RB (RB730, VAX-11/730)", drive="RB80 (an R80)", pack="fixed", cyl=559, sect=31, surf=14, rpm=3600,
       t2t=6.0, avg=25.0, mx=50.0, fit="fit", e=None, slots=32,
       up=None, down=None, spin_src="fixed disk: no LOAD button", s=35, m=205,
       regs="SEEK80S, SEEK80M", snd="rm80 (the RB80 is the RM80's drive)",
       src="Computer History Wiki (RM80/R80)", notes="ready as soon as attached; 31 of 32 sector slots used (242,606 blocks, as the RM80)"),
  dict(dev="RP (Massbus)", drive="RM03 (RM02)", pack="removable", cyl=823, sect=32, surf=5, rpm=3600,
       t2t=6.0, avg=30.0, mx=55.0, fit="fit", e=None, slots=32,
       up=30, down=30, spin_src="provisional (not published)", s=50, m=300,
       regs="SPINUP, SPINDOWN, SEEKRM03S, SEEKRM03M", snd="rm03",
       src="Computer History Wiki", notes="the real RM02 turns at 2400 rpm; SIMH has one type for both"),
  dict(dev="RP (Massbus)", drive="RP04", pack="removable", cyl=411, sect=22, surf=19, rpm=3600,
       t2t=7.0, avg=28.0, mx=50.0, fit="fit", e=None, slots=22,
       up=30, down=30, spin_src="provisional (not published)", s=25, m=150,
       regs="SPINUP, SPINDOWN, SEEKRP04S, SEEKRP04M", snd="rp04",
       src="Computer History Wiki", notes=""),
  dict(dev="RP (Massbus)", drive="RP05", pack="removable", cyl=411, sect=22, surf=19, rpm=3600,
       t2t=7.0, avg=28.0, mx=50.0, fit="fit", e=None, slots=22,
       up=30, down=30, spin_src="provisional (not published)", s=25, m=150,
       regs="SPINUP, SPINDOWN, SEEKRP05S, SEEKRP05M", snd="rp05",
       src="Computer History Wiki", notes="RP04 work-alike"),
  dict(dev="RP (Massbus)", drive="RP06", pack="removable", cyl=815, sect=22, surf=19, rpm=3600,
       t2t=10.0, avg=30.0, mx=55.0, fit="fit", e=None, slots=22,
       up=30, down=30, spin_src="provisional (not published)", s=50, m=300,
       regs="SPINUP, SPINDOWN, SEEKRP06S, SEEKRP06M", snd="rp06",
       src="Computer History Wiki; Columbia University (maximum seek)", notes=""),
  dict(dev="RP (Massbus)", drive="RM05", pack="removable", cyl=823, sect=32, surf=19, rpm=3600,
       t2t=6.0, avg=23.0, mx=None, fit="avg", e=0.69, slots=32,
       up=30, down=30, spin_src="provisional (not published)", s=50, m=300,
       regs="SPINUP, SPINDOWN, SEEKRM05S, SEEKRM05M", snd="rm05",
       src="Computer History Wiki", notes="maximum seek not published"),
  dict(dev="RP (Massbus)", drive="RM80", pack="fixed", cyl=559, sect=31, surf=14, rpm=3600,
       t2t=6.0, avg=25.0, mx=50.0, fit="fit", e=None, slots=31,
       up=None, down=None, spin_src="fixed disk: no RUN/STOP button (published start 20 s, stop 9 s, not used)", s=35, m=205,
       regs="SEEKRM80S, SEEKRM80M", snd="rm80",
       src="Computer History Wiki", notes="ready as soon as attached"),
  dict(dev="RP (Massbus)", drive="RP07", pack="fixed", cyl=630, sect=50, surf=32, rpm=3633,
       t2t=5.0, avg=23.0, mx=None, fit="avg", e=0.69, slots=50,
       up=None, down=None, spin_src="fixed disk: no RUN/STOP button", s=40, m=230,
       regs="SEEKRP07S, SEEKRP07M", snd="rp07",
       src="Computer History Wiki", notes="maximum seek not published; ready as soon as attached"),
  dict(dev="RQ (MSCP: UDA50, KDA50, ...)", drive="RA60", pack="removable", cyl=1600, sect=42, surf=6, rpm=3600,
       t2t=7.0, avg=42.0, mx=70.0, fit="fit", e=None, slots=43,
       up=35, down=35, spin_src="published (DEC brochure, Aug 1982)", s=100, m=600,
       regs="RQ: SPINUP, SPINDOWN, SEEKRA60S, SEEKRA60M", snd="ra60",
       src="DEC RA60/RA80/RA81 brochure, Aug 1982 (Computer History Wiki: 6.7/41.7/75 ms, start 60 s, stop 40 s max.)",
       notes="42 + 1 spare sector per track; the only removable RA drive"),
  dict(dev="RQ (MSCP: KLESI, ...)", drive="RC25 (cartridge)", pack="removable", cyl=821, sect=31, surf=2, rpm=2850,
       t2t=10.0, avg=35.0, mx=55.0, fit="fit", e=None, slots=31,
       up=60, down=30, spin_src="published (RC25 User Guide: start 60 s incl. purge and self-test, stop 30 s)", s=50, m=300,
       regs="RQ: SPINUPRC25, SPINDOWNRC25, SEEKRC25S, SEEKRC25M", snd="rc25",
       src="RC25 Disk Subsystem User Guide EK-ORC25-UG-002 (Sep 1983); geometry: Computer History Wiki",
       notes="one spindle and one actuator with the RCF25 (next unit); Run button on either unit; "
             "31 ms per track (spiral read): + 9.95 ms per track crossed; the drive does not start without a cartridge"),
  dict(dev="RQ (MSCP: KLESI, ...)", drive="RCF25 (fixed platter)", pack="fixed (on the RC25 spindle)", cyl=821, sect=31, surf=2, rpm=2850,
       t2t=10.0, avg=35.0, mx=55.0, fit="fit", e=None, slots=31,
       up=60, down=30, spin_src="the RC25 drive's spindle (an RCF25 without an RC25 before it: fixed disk, no button)", s=50, m=300,
       regs="as the RC25", snd="rc25",
       src="RC25 Disk Subsystem User Guide EK-ORC25-UG-002 (Sep 1983)",
       notes="realism mode: offline while no cartridge is loaded in the RC25 unit; normal mode: usable without one"),
  dict(dev="RQ (MSCP: UDA50, KDA50, ...)", drive="RA70", pack="fixed (RUN/STOP button)", cyl=1507, sect=33, surf=11, rpm=4000,
       t2t=4.0, avg=19.5, mx=35.0, fit="fit", e=None, slots=34,
       up=59, down=15, spin_src="Computer History Wiki (start 59 s avg., stop 15 s avg.)", s=90, m=550,
       regs="RQ: SPINUPRA70, SPINDOWNRA70, SEEKRA70S, SEEKRA70M", snd="ra70",
       src="RA7x/SA7x Pocket Reference Guide EK-RSA7X-PG-002 (4000 rpm); Computer History Wiki (average seek 19.5 ms)",
       notes="5.25-inch HDA; track-to-track 4 ms and maximum 35 ms provisional; spindle at speed within 15 s"),
  dict(dev="RQ (MSCP: UDA50, KDA50, ...)", drive="RA71", pack="fixed (RUN/STOP button)", cyl=1921, sect=51, surf=14, rpm=3600,
       t2t=3.0, avg=13.3, mx=26.0, fit="fit", e=None, slots=52,
       up=60, down=15, spin_src="provisional (RF72, same family: start < 60 s, stop < 15 s)", s=120, m=700,
       regs="RQ: SPINUPRA71, SPINDOWNRA71, SEEKRA71S, SEEKRA71M", snd="ra71",
       src="RA7x/SA7x Pocket Reference Guide EK-RSA7X-PG-002 (3600 rpm); seeks from the RF72 (Computer History Wiki: average 13.3 ms)",
       notes="5.25-inch HDA; all seek times provisional"),
  dict(dev="RQ (MSCP: UDA50, KDA50, ...)", drive="RA72", pack="fixed (RUN/STOP button)", cyl=1921, sect=51, surf=20, rpm=3600,
       t2t=3.0, avg=13.3, mx=26.0, fit="fit", e=None, slots=52,
       up=60, down=15, spin_src="provisional (RF72: start < 60 s, stop < 15 s)", s=120, m=700,
       regs="RQ: SPINUPRA72, SPINDOWNRA72, SEEKRA72S, SEEKRA72M", snd="ra72",
       src="RA7x/SA7x Pocket Reference Guide EK-RSA7X-PG-002; seeks from the RF72 (Computer History Wiki)",
       notes="5.25-inch HDA; rpm not stated in the guide (3600 as RA71/RA73); seek times provisional"),
  dict(dev="RQ (MSCP: UDA50, KDA50, ...)", drive="RA73", pack="fixed (RUN/STOP button)", cyl=2667, sect=70, surf=21, rpm=3600,
       t2t=3.0, avg=13.3, mx=26.0, fit="fit", e=None, slots=71,
       up=60, down=15, spin_src="provisional (RF72: start < 60 s, stop < 15 s)", s=160, m=980,
       regs="RQ: SPINUPRA73, SPINDOWNRA73, SEEKRA73S, SEEKRA73M", snd="ra73",
       src="RA7x/SA7x Pocket Reference Guide EK-RSA7X-PG-002 (3600 rpm); seeks from the RF72 (Computer History Wiki)",
       notes="5.25-inch HDA; seek times provisional"),
  dict(dev="RQ (MSCP: UDA50, KDA50, ...)", drive="RA80", pack="fixed (RUN/STOP button)", cyl=546, sect=31, surf=14, rpm=3600,
       t2t=6.0, avg=25.0, mx=50.0, fit="fit", e=None, slots=32,
       up=40, down=20, spin_src="published (User Guide: start 40 s max., 15 s typical; stop 20 s from the brochure, which says start 50 s)", s=35, m=200,
       regs="RQ: SPINUPRA80, SPINDOWNRA80, SEEKRA80S, SEEKRA80M", snd="ra80",
       src="RA80 User Guide EK-0RA80-UG-001 (Jan 1982); DEC RA60/RA80/RA81 brochure, Aug 1982",
       notes="14-inch HDA; 31 + 1 spare sectors; SIMH's RA80 has 237212 LBNs, the User Guide 236,964 (known issues)"),
  dict(dev="RQ (MSCP: UDA50, KDA50, ...)", drive="RA81", pack="fixed (RUN/STOP button)", cyl=1258, sect=51, surf=14, rpm=3600,
       t2t=7.0, avg=28.0, mx=50.0, fit="fit", e=None, slots=52,
       up=20, down=20, spin_src="published (User Guide: start 20 s max., stop 20 s max.; the brochure says start 50 s)", s=75, m=460,
       regs="RQ: SPINUPRA81, SPINDOWNRA81, SEEKRA81S, SEEKRA81M", snd="ra81",
       src="RA81 User Guide EK-0RA81-UG-001 (1982; maxima 7/28/50 ms); brochure 6/28/52 ms",
       notes="14-inch HDA; 51 + 1 spare sectors"),
  dict(dev="RQ (MSCP: UDA50, KDA50, ...)", drive="RA82", pack="fixed (RUN/STOP button)", cyl=1435, sect=57, surf=15, rpm=3600,
       t2t=6.0, avg=24.0, mx=38.0, fit="fit", e=None, slots=58,
       up=50, down=20, spin_src="Computer History Wiki (start 50 s max., stop 20 s max.)", s=90, m=530,
       regs="RQ: SPINUPRA82, SPINDOWNRA82, SEEKRA82S, SEEKRA82M", snd="ra82",
       src="Computer History Wiki (RA82)",
       notes="14-inch HDA; 57 + 1 spare sectors"),
  dict(dev="RQ (MSCP: UDA50, KDA50, ...)", drive="RA90", pack="fixed (RUN/STOP button)", cyl=2656, sect=69, surf=13, rpm=3600,
       t2t=5.5, avg=18.5, mx=31.5, fit="fit", e=None, slots=70,
       up=40, down=30, spin_src="published start 40 s max. (RA90/RA92 User Guide); stop 30 s provisional", s=160, m=980,
       regs="RQ: SPINUPRA90, SPINDOWNRA90, SEEKRA90S, SEEKRA90M", snd="ra90",
       src="RA90/RA92 Disk Drive User Guide EK-ORA90-UG-002",
       notes="9-inch HDA; 69 + 1 spare sectors"),
  dict(dev="RQ (MSCP: UDA50, KDA50, ...)", drive="RA92", pack="fixed (RUN/STOP button)", cyl=3101, sect=73, surf=13, rpm=3405,
       t2t=3.0, avg=16.0, mx=29.0, fit="fit", e=None, slots=74,
       up=40, down=30, spin_src="published start 40 s max. (RA90/RA92 User Guide); stop 30 s provisional", s=190, m=1150,
       regs="RQ: SPINUPRA92, SPINDOWNRA92, SEEKRA92S, SEEKRA92M", snd="ra92",
       src="RA90/RA92 Disk Drive User Guide EK-ORA90-UG-002",
       notes="9-inch HDA; 73 + 1 spare sectors; 3405 rpm"),
]

# Tape transports (pdp11_ts.c ts_type_ts11 / ts_type_tsv05, sim_sound.c SND_REEL)
TAPES = [
  dict(dev="TS (TS11, Unibus)", drive="TS11 (TS04 transport)", motion="start/stop for every record",
       ips=45.0, rew=(150.0, 150.0, 150.0), bpi=1600, gap=0.6, tm=0.7, start=8.0, stop=8.0, ramp=0.342,
       access=0.0, rew_extra=1.5, erase=3.0, clean="10 in forward and back",
       load=8, unload=8, load_src="provisional (manual pages 4-4/4-5 missing in the scan)",
       regs="LOADTIME, UNLOADTIME, REEL (SET TS REEL=600|1200|2400), TPOS",
       snd="ts11 (own: none) -> default_tape (all eight files)",
       src="TS11 Subsystem Technical Manual EK-OTS11-TM-003 (Jan 1984); data sheet: 45 in/s, rewind 150 in/s "
           "(3 min for a 10.5 in reel), start/stop 8 ms, gap 0.5 in minimum",
       notes="gap 0.6 in (ANSI nominal; the manual gives 0.5 minimum); a full-reel skip takes 15-20 min "
             "(manual), the model gives about 15 min for 512-byte records"),
  dict(dev="TS (TSV05, Qbus)", drive="TSV05 (TS05 transport)", motion="streaming; 40 ms access from rest",
       ips=25.0, rew=(130.0, 150.0, 180.0), bpi=1600, gap=0.65, tm=0.75, start=0.0, stop=0.0, ramp=0.0,
       access=40.0, rew_extra=2.0, erase=3.0, clean="no-op",
       load=30, unload=15, load_src="published (load 30 s typical, unload 15 s max from BOT)",
       regs="LOADTIME, UNLOADTIME, REEL, TPOS",
       snd="tsv05 (own: load, unload, motion, start, stop) -> default_tape (rewind)",
       src="TSV05 Tape Transport Subsystem User's Guide EK-TSV05-UG-003: 25 in/s (TS11 software), "
           "rewind 130-180 in/s by reel size (max 0.9/1.6/2.8 min), access 40 ms from rest, 140 ms worst, "
           "gap 0.65 in typical, 40 KB/s",
       notes="100 in/s mode (special software) not simulated; the tape coasts through one gap after a "
             "command, a command in that time continues streaming"),
]

# Cartridge streamers (pdp11_tq.c tq_tk50, sim_sound.c SND_STREAMER)
STREAMERS = [
  dict(dev="TQ (TQK50 Qbus, TUK50 Unibus)", drive="TK50 (also TZ30)", ips=75.0, rate=45.0, tracks=22,
       track_in=7092.0, tape_ft=600, cap="94.5 MB formatted (131 MB unformatted)", buffer=32768, backup=60,
       repos=1.42, start="5 in (0.133 s) / 3.5 in (0.093 s)", rew_ips=80.0, load="10-15 (random per load)",
       cal=120, unload=9,
       regs="BUFFER, BACKUP, REPOS, LOADMIN, LOADMAX, CALTIME, UNLOADTIME; SET TQn LOAD/UNLOAD, SHOW TQn TRANSPORT",
       snd="tk50 (own: load, calibrate, unload, start, forward, stop, rstart, reverse, rstop)",
       src="TK50 Tape Drive Subsystem Technical Manual EK-OTK50-TM-002, TUK50 Owner's Manual EK-TUK50-OM-001, "
           "TK50 User's Guide; reposition cycle and sounds measured on a TZ30 recording (about 3 s per cycle)",
       notes="buffer 32 KB and backup 60 in provisional (the manuals do not give them); TK70, TU81 and TKUSER "
             "keep the old behaviour; EOT at the end of the 22nd track"),
]

SYSTEMS = [
  dict(sim="vax750 (VAX-11/750)", snd="vax750 (own: none) -> default_cpu (opt-in)",
       files="default_cpu: starting.wav, running.wav (4.27 s seamless loop), stopping.wav",
       throttle="300K (provisional)", src="recordings of a real VAX-11/750", notes="calibration of the THROTTLE still open"),
  dict(sim="microvax2 (MicroVAX II)", snd="none (no system sounds yet)", files="-",
       throttle="450K (provisional)", src="0.9 times an 11/780 (about 0.5 MIPS at THROTTLE 500K)",
       notes="calibration by a CPU benchmark still open (the boot time is no measure); rtVAX1000 unchanged"),
]

# ---------------------------------------------------------------- model
def model(d):
    """returns (e, a, b, avg_model, max_model) of t(dist) = a + b * dist^e in ms"""
    dmax = float(d["cyl"] - 1)
    t2t = d["t2t"]
    if d["fit"] == "fit":
        want = (d["avg"] - t2t) / (d["mx"] - t2t)
        lo, hi = 0.2, 1.5
        for _ in range(60):
            e = (lo + hi) / 2.0
            if ((dmax / 3.0) ** e - 1.0) / (dmax ** e - 1.0) > want:
                lo = e
            else:
                hi = e
        b = (d["mx"] - t2t) / (dmax ** e - 1.0)
    elif d["fit"] == "fixed":
        e = d["e"]
        b = (d["mx"] - t2t) / (dmax ** e - 1.0)
    else:  # through t2t and avg
        e = d["e"]
        b = (d["avg"] - t2t) / ((dmax / 3.0) ** e - 1.0)
    a = t2t - b
    t = lambda x: a + b * x ** e
    return e, a, b, t(dmax / 3.0), t(dmax)

# ---------------------------------------------------------------- ods helpers
doc = OpenDocumentSpreadsheet()
doc.meta.addElement(Title(text="SIMH sound devices %s" % TABLE_VERSION))

def style(name, family, **kw):
    s = Style(name=name, family=family)
    for k, v in kw.items():
        s.addElement(v)
    doc.automaticstyles.addElement(s)
    return s

st_title = style("title", "table-cell", t=TextProperties(fontweight="bold", fontsize="13pt"))
st_head = style("head", "table-cell",
                t=TextProperties(fontweight="bold"),
                c=TableCellProperties(backgroundcolor="#dde4ee", wrapoption="wrap", verticalalign="middle",
                                      border="0.5pt solid #808080"))
st_cell = style("cell", "table-cell", c=TableCellProperties(wrapoption="wrap", verticalalign="top",
                                                             border="0.5pt solid #c0c0c0"))
st_num = style("num", "table-cell", c=TableCellProperties(verticalalign="top", border="0.5pt solid #c0c0c0"),
               p=ParagraphProperties(textalign="end"))

def colstyle(name, width):
    return style(name, "table-column", c=TableColumnProperties(columnwidth=width))

def cell(v, st=None):
    if v is None or v == "":
        c = TableCell(stylename=st or st_cell)
        return c
    if isinstance(v, (int, float)) and not isinstance(v, bool):
        c = TableCell(valuetype="float", value=v, stylename=st or st_num)
        c.addElement(P(text=("%g" % v) if isinstance(v, int) else ("%.2f" % v).rstrip("0").rstrip(".")))
        return c
    c = TableCell(valuetype="string", stylename=st or st_cell)
    c.addElement(P(text=str(v)))
    return c

def add_row(table, values, st=None):
    r = TableRow()
    for v in values:
        r.addElement(cell(v, st))
    table.addElement(r)

def add_table(name, widths, title, header, rows):
    t = Table(name=name)
    for i, w in enumerate(widths):
        t.addElement(TableColumn(stylename=colstyle("%s_c%d" % (name, i), w)))
    add_row(t, [title], st_title)
    add_row(t, [])
    add_row(t, header, st_head)
    for r in rows:
        add_row(t, r)
    doc.spreadsheet.addElement(t)

# ---------------------------------------------------------------- sheet: disks
hdr = ["SIMH device", "Drive", "Pack", "Cylinders", "Sectors/track", "Surfaces", "Rotation (rpm)",
       "Revolution (ms)", "Avg rotational latency (ms)", "Sector time (µs)",
       "Seek track-to-track (ms)", "Seek average, published (ms)", "Seek maximum, published (ms)",
       "Seek curve exponent e", "Model seek at 1/3 stroke (ms)", "Model full-stroke seek (ms)",
       "Avg access = model avg seek + avg latency (ms)",
       "Spin-up (s)", "Spin-down (s)", "Spin times: origin", "Seek sound short up to (cyl)",
       "Seek sound medium up to (cyl)", "Registers", "Sound directory", "Timing source", "Notes"]
rows = []
for d in DISKS:
    e, a, b, avg_m, max_m = model(d)
    rev = 60000.0 / d["rpm"]
    rows.append([d["dev"], d["drive"], d["pack"], d["cyl"], d["sect"], d["surf"], d["rpm"],
                 round(rev, 2), round(rev / 2, 2), round(rev * 1000.0 / d["slots"], 1),
                 d["t2t"], d["avg"], d["mx"], round(e, 3), round(avg_m, 1), round(max_m, 1),
                 round(avg_m + rev / 2, 1),
                 d["up"], d["down"], d["spin_src"], d["s"], d["m"], d["regs"], "sound/" + d["snd"] + " (seek sounds: default_disk unless own files exist)",
                 d["src"], d["notes"]])
add_table("Disks", ["3.6cm", "2.4cm", "2.0cm", "1.8cm", "1.8cm", "1.6cm", "1.8cm", "1.9cm", "2.2cm", "1.9cm",
                    "2.1cm", "2.2cm", "2.2cm", "1.9cm", "2.2cm", "2.2cm", "2.6cm", "1.6cm", "1.6cm", "5.0cm",
                    "2.0cm", "2.0cm", "5.2cm", "3.6cm", "6.0cm", "6.5cm"],
          "SIMH sound: disk drives with the realism mode (SET SOUND ENABLED), table %s, %s" % (TABLE_VERSION, TABLE_DATE),
          hdr, rows)

# ---------------------------------------------------------------- sheet: tapes
def tape_record_ms(t, nbytes):
    dist = nbytes / float(t["bpi"]) + t["gap"]
    if t["access"] > 0:
        return 1000.0 * dist / t["ips"]
    run = max(0.0, dist - t["ramp"])
    return t["start"] + t["stop"] + 1000.0 * run / t["ips"]

thdr = ["SIMH device", "Transport", "Motion", "Read/write speed (in/s)", "Density (bpi)", "Gap (in)",
        "Tape mark (in)", "Start / stop (ms)", "Access from rest (ms)",
        "Record 512 B (ms)", "Record 2048 B (ms)", "Record 8192 B (ms)",
        "Throughput 8 KB records (KB/s)", "BOT to EOT, 2400 ft (MB in 8 KB records)",
        "Skip a full 2400 ft reel of 512 B records (min)",
        "Rewind speed 600/1200/2400 ft (in/s)", "Full rewind 2400 ft (s)", "Erase (in)", "Clean",
        "Load (s)", "Unload from BOT (s)", "Load/unload: origin", "Registers", "Sound directory",
        "Timing source", "Notes"]
trows = []
for t in TAPES:
    eot_in = (2400 - 16 - 25) * 12.0
    r8 = tape_record_ms(t, 8192)
    n8 = eot_in / (8192.0 / t["bpi"] + t["gap"])
    n512 = eot_in / (512.0 / t["bpi"] + t["gap"])
    trows.append([t["dev"], t["drive"], t["motion"], t["ips"], t["bpi"], t["gap"], t["tm"],
                  "%g / %g" % (t["start"], t["stop"]) if t["access"] == 0 else "-",
                  t["access"] if t["access"] else "-",
                  round(tape_record_ms(t, 512), 1), round(tape_record_ms(t, 2048), 1), round(r8, 1),
                  round(8.0 / (r8 / 1000.0), 1), round(n8 * 8192 / 1e6, 1),
                  round(n512 * tape_record_ms(t, 512) / 60000.0, 1),
                  "%g / %g / %g" % t["rew"], round(eot_in / t["rew"][2] + t["rew_extra"], 0),
                  t["erase"], t["clean"], t["load"], t["unload"], t["load_src"], t["regs"],
                  "sound/" + t["snd"], t["src"], t["notes"]])
add_table("Tapes", ["3.2cm", "3.2cm", "3.2cm", "1.8cm", "1.6cm", "1.4cm", "1.6cm", "1.8cm", "1.8cm",
                    "1.8cm", "1.8cm", "1.8cm", "2.2cm", "2.4cm", "2.6cm", "2.6cm", "2.0cm", "1.4cm", "2.6cm",
                    "1.4cm", "1.6cm", "5.0cm", "5.0cm", "5.0cm", "7.0cm", "7.0cm"],
          "SIMH sound: tape transports with the realism mode (SET SOUND ENABLED), table %s, %s" % (TABLE_VERSION, TABLE_DATE),
          thdr, trows)

# ---------------------------------------------------------------- sheet: streamers
srows = []
for t in STREAMERS:
    pass_s = t["track_in"] / t["ips"]
    srows.append([t["dev"], t["drive"], t["ips"], t["rate"], t["tracks"], round(t["track_in"] / 12.0),
                  t["tape_ft"], t["cap"], t["buffer"], t["backup"], t["repos"],
                  round(t["buffer"] / (t["rate"] * 1000.0) + t["repos"] + 0.13, 2), t["start"],
                  round(pass_s, 1), round(t["tracks"] * pass_s / 60.0, 1), t["rew_ips"],
                  round(t["track_in"] / t["rew_ips"], 0), t["load"], t["cal"], t["unload"], t["regs"],
                  "sound/" + t["snd"], t["src"], t["notes"]])
add_table("Streamers", ["3.4cm", "2.6cm", "1.6cm", "1.8cm", "1.4cm", "1.8cm", "1.6cm", "3.4cm", "1.8cm",
                        "1.8cm", "1.8cm", "2.4cm", "3.4cm", "1.8cm", "2.2cm", "1.8cm", "2.0cm", "2.4cm", "2.0cm",
                        "1.6cm", "6.0cm", "6.0cm", "7.0cm", "7.0cm"],
          "SIMH sound: cartridge streamers with the realism mode (SET SOUND ENABLED), table %s, %s" % (TABLE_VERSION, TABLE_DATE),
          ["Device", "Drive", "Speed (in/s)", "Data rate (KB/s)", "Tracks", "Track (ft)", "Tape (ft)", "Capacity",
           "Buffer (bytes)", "Backup (in)", "Reposition (s)", "Cycle of a slow host (s)", "Start / stop distance",
           "One track pass (s)", "Worst access (min)", "Rewind (in/s)", "Longest rewind (s)", "Load (s)",
           "Calibration of a new cartridge (s)", "Unload (s)", "Registers / commands", "Sound directory",
           "Source", "Notes"], srows)

# ---------------------------------------------------------------- sheet: floppies
# Mirrors sim_flp_rx01/sim_flp_rx02 in sim_sound.c and the RX, CS and RY
# registers (pdp11_rx.c, vax780_stddev.c, pdp11_ry.c).
FLOPPIES = [
  dict(dev="RY (RX211/RXV21)", drive="RX02", media='8", 1 side, FM 128 B / MFM 256 B', tracks=77, sect=26,
       rpm=360, step=6.0, settle=25.0, hdload=25.0, unload=200, motor="AC, on the power line (runs while the power is on)",
       up=2.0, avg_pub=262, regs="STEP, SETTLE, HDLOAD, HDUNLOAD, MOTORUP; SHOW RYn DRIVE",
       snd="rx02 (all ten files)", src="RX02 Technical Manual EK-0RX02-TM-001 1.4, 5.3.4; print set MP00629 (H771)",
       notes="Initialize <= 1.8 s; Set Media Density about 15 s; Read Status about 250 ms; buffer transfer 1.2 us/bit"),
  dict(dev="RX (RX11/RXV11), PDP-11", drive="RX01", media='8", 1 side, FM 128 B', tracks=77, sect=26,
       rpm=360, step=10.0, settle=20.0, hdload=20.0, unload=46, motor="AC, on the power line (runs while the power is on)",
       up=2.0, avg_pub=488, regs="STEP, SETTLE, HDLOAD, HDUNLOAD, MOTORUP; SHOW RXn DRIVE",
       snd="rx01, else rx02 (same drive mechanics), else default_floppy",
       src="RX8/RX11 Maintenance Manual EK-RX01-MM-002 (Dec 1976) 1.6, 2.4.4, microcode STEPHD (head load after the steps, 20 ms)",
       notes="Head unloaded when no new command within 45.87 ms; Initialize <= 1.8 s; Read Status 1-2 revolutions (about 250 ms); 18 us per byte; power up: ten tracks in and back. Published average 488 ms (the manual's own sum: 493 ms) uses half the stroke"),
  dict(dev="CS (VAX-11/780 console floppy)", drive="RX01", media='8", 1 side, FM 128 B', tracks=77, sect=26,
       rpm=360, step=10.0, settle=20.0, hdload=20.0, unload=46, motor="AC, on the power line (runs while the power is on)",
       up=2.0, avg_pub=488, regs="STEP, SETTLE, HDLOAD, HDUNLOAD, MOTORUP; SHOW CS DRIVE",
       snd="rx01, else rx02 (same drive mechanics), else default_floppy",
       src="EK-RX01-MM-002 as the RX; console floppy protocol: VAX Architecture Reference Manual",
       notes="One drive on the console LSI-11's RXV11; the 128 bytes between drive and RXV11 at 18 us each, the bytes to/from the VAX at XTIME (unchanged, real speed unknown); VMS: CSA1 (SYSGEN CONNECT CONSOLE)"),
  dict(dev="RQ (RQDX1/3/4, RUX50)", drive="RX50", media='5.25", 1 side, MFM 512 B; two diskettes per drive', tracks=80, sect=10,
       rpm=300, step=6.0, settle=30.0, hdload=30.0, unload=2000,
       motor="DC, switched by the controller: at speed in 0.5 s, off after 2 s idle (provisional); one motor for both diskettes",
       up=0.5, avg_pub=164, regs="RX50STEP, RX50SETTLE, RX50HDLOAD, RX50HDUNLOAD, RX50MOTORUP, RX50MOTOROFF; SHOW RQn DRIVE",
       snd="rx50 (all ten files)",
       src="Rainbow 100 Technical Manual EK-PC100-TM-001 ch. 6; Computer History Wiki (164 ms average access); RQDX block mapping from the VMS and RT-11 drivers (TUHS / classiccmp)",
       notes="Two RX50 units with consecutive unit numbers = one drive (one motor, one carriage); block mapping: logical track + 1 (last 10 blocks on track 0), 2:1 interleave, 2-sector skew per track; head unload 2 s provisional; the published 164 ms average access is below the model's (a third of the stroke + settle + half a revolution)"),
  dict(dev="CS (VAX 8200 console floppy)", drive="RX50", media='5.25", 1 side, MFM 512 B; two slots CS0/CS1', tracks=80, sect=10,
       rpm=300, step=6.0, settle=30.0, hdload=30.0, unload=2000,
       motor="DC, switched by the controller: at speed in 0.5 s, off after 2 s idle (provisional); one motor for both slots",
       up=0.5, avg_pub=164, regs="STEP, SETTLE, HDLOAD, HDUNLOAD, MOTORUP, MOTOROFF; SHOW CSn DRIVE",
       snd="rx50 (all ten files)", src="as the RX50 on the RQ",
       notes="Registers at 200B0000 (CS0-CS5, EB/FB, GO); the guest gives physical tracks and sectors; the CPU moves the 512 bytes itself"),
]
frows = []
for f in FLOPPIES:
    rev = 60000.0 / f["rpm"]
    model_avg = f["tracks"] / 3.0 * f["step"] + max(f["settle"], f["hdload"]) + rev / 2.0
    frows.append([f["dev"], f["drive"], f["media"], f["tracks"], f["sect"], f["rpm"], round(rev, 1),
                  round(rev / f["sect"], 2), f["step"], f["settle"], f["hdload"], f["unload"],
                  round(model_avg, 0), f["avg_pub"], f["motor"], f["up"], f["regs"], "sound/" + f["snd"],
                  f["src"], f["notes"]])
add_table("Floppies", ["3.4cm", "1.6cm", "5.0cm", "1.4cm", "1.4cm", "1.4cm", "1.8cm", "1.8cm", "1.8cm",
                       "1.8cm", "1.8cm", "2.0cm", "2.2cm", "2.2cm", "5.5cm", "1.8cm", "6.0cm", "4.0cm",
                       "7.0cm", "7.0cm"],
          "SIMH sound: floppy drives with the realism mode (SET SOUND ENABLED), table %s, %s" % (TABLE_VERSION, TABLE_DATE),
          ["Device", "Drive", "Medium", "Tracks", "Sectors", "rpm", "Revolution (ms)", "Sector (ms)",
           "Step per track (ms)", "Settle (ms)", "Head load (ms)", "Head unload after (ms)",
           "Model average access (ms)", "Published average access (ms)", "Motor", "Motor run-up (s)",
           "Registers / commands", "Sound directory", "Source", "Notes"], frows)

# ---------------------------------------------------------------- sheet: systems
add_table("Systems", ["4.2cm", "2.6cm", "9.0cm", "3.2cm", "5.0cm", "6.0cm"],
          "SIMH sound: system (CPU cabinet) sounds, table %s, %s" % (TABLE_VERSION, TABLE_DATE),
          ["Simulator", "Sound directory", "Starting / running / stopping", "Default THROTTLE (realism mode)",
           "Source", "Notes"],
          [[s["sim"], "sound/" + s["snd"], s["files"], s["throttle"], s["src"], s["notes"]] for s in SYSTEMS])

# ---------------------------------------------------------------- sheet: notes
notes = [
  ["Seek model", "t(d) = a + b * d^e (d cylinders, ms), t(1) = track-to-track. Exponent fitted to the published "
                 "track-to-track, average (taken at a third of the stroke) and maximum seek where all three are "
                 "published (RP, RA, RC25); fixed 0.69 (RK06/RK07, RM05, RP07) or 0.67 (RL01/RL02) otherwise."],
  ["Rotational latency", "0 .. one revolution, random; the table shows the average (half a revolution)."],
  ["Transfer time", "One sector time per sector (RA: the sectors of a track + 1 spare slot, e.g. RA60 43); RA adds a "
                    "track-to-track seek per cylinder crossed; RC25 adds 9.95 ms per track crossed (published "
                    "spiral read 31 ms per track minus one revolution of 21.05 ms)."],
  ["Avg access", "Model seek at a third of the stroke plus average latency (a typical random access)."],
  ["Spin-up / spin-down", "Registers of the device (ms); simulated time while the CPU runs, wall time at the "
                          "sim> prompt; SET <unit> ONLINE/OFFLINE wait at the prompt unless SET -N."],
  ["Seek sounds", "<prefix>-s/m/l.wav in the sound directory, chosen by the distance and the two limit registers; "
                  "if a file is missing, seek-s/m/l.wav from sound/default_disk (the former synthetic RK07 seek "
                  "sounds; since sound patch v14 no drive has own seek sounds yet)."],
  ["Default sounds", "Per file, an own file wins: default_disk (seek sounds, no spindle sounds), default_cpu "
                     "(system sounds, recordings of a real VAX-11/750, for simulators that opt in), default_tape "
                     "(synthetic TS11 transport sounds, since sound patch v16). SHOW SOUND lists the defaults in use."],
  ["Tape model", "Physical position in inches from the BOT marker, counted from the records moved over "
                 "(bytes / bpi + gap; tape marks; erase). BOT marker 16 ft after the tape start, EOT marker 25 ft "
                 "before its end: a reel of L ft has L - 41 ft between the markers. Start/stop transport (TS11): "
                 "start + (length - start and stop distances) / speed + stop per record. Streaming transport "
                 "(TSV05): length / speed, plus the access time when the tape was at rest. Rewind: position / "
                 "rewind speed + ramps (BOT overshoot and creep back)."],
  ["Tape sounds", "load, unload, rewind_start, rewind (reel motor loop at 8 rev/s, played per reel with a pitch "
                  "glide that follows the reel radius), rewind_stop, start, motion (loop), stop; "
                  "<dir>/<prefix>_<name>.wav, else default_tape/<name>.wav. Made by tools/make_tape_sounds.py."],
  ["Streamer model", "TK50: the host's data goes through the drive's buffer. Reading, the drive reads ahead until "
                     "the buffer is full, stops and repositions (stop, back up, stop: REPOS), and starts again "
                     "over the backed-up tape when the host has taken half of the buffer; writing, the drive writes "
                     "the buffer out and repositions when it runs empty. A host slower than 45 KB/s gives the cycle "
                     "stream - stop - reverse - start (a slow host: buffer / rate + reposition + start). Serpentine "
                     "tracks: positions and spacing follow the tape (track turns cost a pass), rewind along the tape. "
                     "Load: a random time between LOADMIN and LOADMAX, plus CALTIME for a new (empty) cartridge."],
  ["Floppy model", "Step per track, then the longer of settle and head load; the sector is read when it comes "
                   "round under the head (rotational position from the realtime clock, random phase per diskette), "
                   "so software interleave has its real effect. Model average access: a third of the tracks "
                   "stepped + settle + half a revolution (the manual's formula). The head unloads after an idle "
                   "time. Sounds: motor_start, motor, motor_stop, disk, head, head_load, head_unload, step "
                   "(repeated at the step rate), door_close, door_open; <dir>/<prefix>_<name>.wav, else "
                   "default_floppy/<name>.wav. Made by tools/make_floppy_sounds.py."],
  ["Not yet", "RX33 (RQ and VAXstation 3100), RX18; TU58; tapes other than "
              "the TS device and the TK50 (TM, TU, TK70, TU81). PDP-8 RX8E/RX28: postponed."],
  ["Update", "Generated by tools/make_sound_devices_ods.py (open-simh-extended, branch notes); add new devices there."],
  ["Version history", "v1 2026-11-04: RK06/RK07, RL01/RL02, RM03, RP04, RP05, RP06, RM05, RM80, RP07, RA60; VAX-11/750 system. v2 2026-11-04: RB02, RB80 (RB730). v3 2026-11-04: RB80 with 31 sectors per track (RB730 fixes). v4 2026-11-06: RC25 and RCF25 (one spindle, one actuator). v5 2026-11-07: default sounds (default_disk, default_cpu). v6 2026-11-07: sheet Tapes: TS11 and TSV05 (TS device). v7 2026-11-08: sheet Streamers: TK50 (TQ device); MicroVAX II THROTTLE 450K. v8 2026-11-10: the fixed RA drives RA70, RA71, RA72, RA73, RA80, RA81, RA82, RA90, RA92 (RUN/STOP button, timing, seek sounds). v9 2026-11-10: RA81 start 20 s, RA80 40 s (User Guides win over the brochure). v10 2026-11-11: sheet Floppies: RX02 (RY device). v11 2026-11-11: RX01 (RX device, VAX-11/780 console floppy CS). v12 2026-11-11: RX50 (RQ device). v13 2026-11-11: VAX 8200 console RX50 (CS)."],
]
add_table("Notes", ["4.0cm", "22cm"], "SIMH sound devices: notes, table %s" % TABLE_VERSION,
          ["Topic", "Explanation"], notes)

doc.save(sys.argv[1] if len(sys.argv) > 1 else "SIMH_Sound_devices.ods")
