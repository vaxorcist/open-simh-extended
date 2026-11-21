# open-simh-extended/tests/dmv11_vmsload/obj2tsk.py  v1
# Converts a single-module MACRO-11 object file (RT-11 formatted binary,
# as written by the macro11 cross assembler) whose code is in one
# relocatable program section into
#   - a raw memory image linked at base address 0 (<name>.bin), and
#   - an RSX-11 task image without task header (<name>.tsk), as TKB
#     /-HD with PAR=GEN:0:160000 would build it: two label blocks, the
#     image from block 2 (L$BHRB).  VMS's MOM serves such files to PDP-11
#     targets in a MOP down-line load (load address L$BSA, transfer
#     address L$BXFR).
# Only what remwrt.mac needs is supported: the blank psect and . ABS.,
# relocation types internal (1), internal displaced (3), location counter
# definition (7) and modification (010).  Global references are refused.
#
# usage: python3 -I obj2tsk.py remwrt.obj REMWRT
#        -> remwrt.bin, remwrt.tsk (lower case of the second argument)
import sys, struct

def records(data):
    """Formatted binary records: 1, 0, length (incl. the 4 header bytes),
    data, checksum (negative sum of all bytes)."""
    i = 0
    while i < len(data):
        if data[i] == 0:                       # fill between records
            i += 1
            continue
        if data[i] != 1 or data[i + 1] != 0:
            raise SystemExit("bad record lead at %d" % i)
        n = data[i + 2] | (data[i + 3] << 8)
        body = data[i + 4:i + n]
        if (sum(data[i:i + n + 1]) & 0xFF) != 0:
            raise SystemExit("checksum error at %d" % i)
        yield body
        i += n + 1

def rad50(s):
    tab = " ABCDEFGHIJKLMNOPQRSTUVWXYZ$.?0123456789"
    s = (s.upper() + "      ")[:6]
    w = []
    for k in (0, 3):
        v = 0
        for c in s[k:k + 3]:
            v = v * 40 + tab.index(c)
        w.append(v)
    return w

def main():
    obj, name = sys.argv[1], sys.argv[2]
    data = open(obj, "rb").read()
    mem = bytearray(65536)
    hi = 0
    xfer = None
    lcbase = 0                                 # current location counter base (psect base 0)
    lastaddr = 0
    reloc = []                                 # (address, value) to apply after the text
    for rec in records(data):
        rtype = rec[0] | (rec[1] << 8)
        if rtype == 1:                         # GSD
            for k in range(2, len(rec), 8):
                ent = rec[k:k + 8]
                etype = ent[5]
                val = ent[6] | (ent[7] << 8)
                if etype == 3 and ent[4] & 0o10 and val != 1:   # transfer address (1 = none)
                    xfer = val
                if etype == 4 and ent[4] & 0o10 == 0 and ent[4] & 0o100 == 0:
                    pass
                if etype == 4 and not (ent[4] & 0o10):          # global reference
                    raise SystemExit("global references are not supported")
        elif rtype == 3:                       # TXT: load address, bytes
            addr = (rec[2] | (rec[3] << 8)) + lcbase
            txt = rec[4:]
            mem[addr:addr + len(txt)] = txt
            hi = max(hi, addr + len(txt))
            lastaddr = addr
        elif rtype == 4:                       # RLD
            k = 2
            while k < len(rec):
                cmd = rec[k] & 0o177
                disp = rec[k + 1]
                if cmd == 1:                   # internal: value = constant + base (0)
                    con = rec[k + 2] | (rec[k + 3] << 8)
                    reloc.append((lastaddr + disp - 4, con))
                    k += 4
                elif cmd == 3:                 # internal displaced: constant - (loc + 2)
                    con = rec[k + 2] | (rec[k + 3] << 8)
                    loc = lastaddr + disp - 4
                    reloc.append((loc, (con - (loc + 2)) & 0xFFFF))
                    k += 4
                elif cmd == 7:                 # location counter definition: psect name, constant
                    k += 8
                elif cmd == 0o10:              # location counter modification: constant
                    k += 4
                else:
                    raise SystemExit("RLD type %o not supported" % cmd)
        elif rtype == 6:                       # ENDMOD
            break
    for loc, val in reloc:
        old = mem[loc] | (mem[loc + 1] << 8)
        if old != val:
            print("relocation at %06o: text %06o, relocated %06o" % (loc, old, val))
        mem[loc:loc + 2] = struct.pack("<H", val)
    if xfer is None:
        raise SystemExit("no transfer address")
    image = bytes(mem[:hi + (hi & 1)])
    base = name.lower()
    open(base + ".bin", "wb").write(image)
    # task image without header: label block 0, label block 1 (zero),
    # then the image from address 0 (L$BSA) in 512-byte blocks
    lbl = bytearray(1024)
    def w(off, v):
        lbl[off:off + 2] = struct.pack("<H", v & 0xFFFF)
    t = rad50(name)
    w(0o0, t[0]); w(0o2, t[1])                 # L$BTSK task name
    p = rad50("GEN")
    w(0o4, p[0]); w(0o6, p[1])                 # L$BPAR partition name
    w(0o10, 0)                                 # L$BSA  base address
    w(0o12, len(image) - 1)                    # L$BHGV highest window 0 address
    w(0o14, len(image) - 1)                    # L$BMXV highest task address
    ldz = (len(image) + 63) // 64
    w(0o16, ldz)                               # L$BLDZ load size, 32-word blocks
    w(0o20, ldz)                               # L$BMXZ maximum size
    w(0o30, 0o40000)                           # L$BFLG TS$NHD: no task header
    w(0o350, xfer)                             # L$BXFR transfer address
    w(0o356, 2)                                # L$BHRB image relative block
    w(0o360, 2)                                # L$BBLK blocks in the label
    pad = (-len(image)) % 512
    open(base + ".tsk", "wb").write(bytes(lbl) + image + bytes(pad))
    print("%s.bin: %d bytes (0-%06o), transfer address %06o; %s.tsk: %d blocks"
          % (base, len(image), len(image) - 1, xfer, base, (1024 + len(image) + pad) // 512))

main()
