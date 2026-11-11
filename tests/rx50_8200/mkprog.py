# open-simh-extended/tests/rx50_8200/mkprog.py  v1
#
# Writes prog.ini (a SIMH do-file) for the VAX 8200 console floppy tests
# (2026-11-11): a VAX program at 1000 that drives the RX50 console floppy
# controller (registers at 200B0000 on the BI node) directly: for every
# (slot, track, sector) of the list it writes CS1 (track), CS2 (sector),
# CS0 (function 4 = read sector, disk select = slot B), GO, and polls CS0
# Done (0x08); then HALT.  The sector data are not taken from the buffer
# (EB), so the next command starts at once.
#
# Usage: python3 -I mkprog.py > prog.ini

CMDS = [
    (0, 40, 3),                              # A: motor start, 40 steps, head load
    (0, 40, 5), (0, 40, 7), (0, 40, 9),      # A: every second sector (2:1 interleave):
                                             #    two sector times each
    (0, 40, 10),                             # A: the very next sector: just missed,
                                             #    a revolution plus a sector
    (0, 40, 9),                              # A: back one sector: almost a revolution
    (1, 40, 7),                              # B: A unloads, B loads, no steps (one carriage)
    (1, 10, 1),                              # B: 30 steps back
    (0, 79, 10),                             # A: B unloads, 69 steps, A loads
]

REG = 0x200B0000
def absaddr(off):
    a = REG + off
    return [0x9F] + [(a >> (8 * i)) & 0xFF for i in range(4)]

def movb(val, off):                          # MOVB #val, @#reg
    src = [val] if val < 64 else [0x8F, val]
    return [0x90] + src + absaddr(off)

code = []
for slot, trk, sec in CMDS:
    code += movb(trk, 0x06)                  # CS1 track
    code += movb(sec, 0x08)                  # CS2 sector
    code += movb(0x40 | (slot << 1), 0x04)   # CS0: read sector, disk select
    code += movb(0, 0x14)                    # GO
    code += [0x93, 0x08] + absaddr(0x04)     # BITB #8, @#CS0
    code += [0x13, 0xF7]                     # BEQL back to the BITB
code += [0x00]                               # HALT

print('; open-simh-extended/tests/rx50_8200/prog.ini  v1 (made by mkprog.py)')
print('; VAX program at 1000: read sector on the console RX50 for %d commands' % len(CMDS))
for i, b in enumerate(code):
    print('deposit -b %X %02X' % (0x1000 + i, b))
