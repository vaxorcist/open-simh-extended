# open-simh-extended/tools/make_sound_devices_ods.py  v7 (v7: tapes, TS11 and TSV05; v6: default sounds; v5: RC25/RCF25 and the per-track model; v4: paths of the notes branch)
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

TABLE_VERSION = "v6"
TABLE_DATE = "2026-11-07"

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
       notes="42 + 1 spare sector per track; only removable RA drive; other RQ drive types unchanged"),
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

SYSTEMS = [
  dict(sim="vax750 (VAX-11/750)", snd="vax750 (own: none) -> default_cpu (opt-in)",
       files="default_cpu: starting.wav, running.wav (4.27 s seamless loop), stopping.wav",
       throttle="300K (provisional)", src="recordings of a real VAX-11/750", notes="calibration of the THROTTLE still open"),
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
                 "published (RP, RA60); fixed 0.69 (RK06/RK07, RM05, RP07) or 0.67 (RL01/RL02) otherwise."],
  ["Rotational latency", "0 .. one revolution, random; the table shows the average (half a revolution)."],
  ["Transfer time", "One sector time per sector (RA60: 43 slots per track, 42 + 1 spare); RA60 adds a "
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
  ["Not yet", "RX01/RX02, RX50/RX33 and TU58 (no spin-up/spin-down); tapes other than the TS device (TM, TU, TQ)."],
  ["Update", "Generated by tools/make_sound_devices_ods.py (open-simh-extended, branch notes); add new devices there."],
  ["Version history", "v1 2026-11-04: RK06/RK07, RL01/RL02, RM03, RP04, RP05, RP06, RM05, RM80, RP07, RA60; VAX-11/750 system. v2 2026-11-04: RB02, RB80 (RB730). v3 2026-11-04: RB80 with 31 sectors per track (RB730 fixes). v4 2026-11-06: RC25 and RCF25 (one spindle, one actuator). v5 2026-11-07: default sounds (default_disk, default_cpu). v6 2026-11-07: sheet Tapes: TS11 and TSV05 (TS device)."],
]
add_table("Notes", ["4.0cm", "22cm"], "SIMH sound devices: notes, table %s" % TABLE_VERSION,
          ["Topic", "Explanation"], notes)

doc.save(sys.argv[1] if len(sys.argv) > 1 else "SIMH_Sound_devices.ods")
