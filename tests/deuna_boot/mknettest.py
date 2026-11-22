# open-simh-extended/tests/deuna_boot/mknettest.py  v4 (v4: block 0 is both an RSX task label
# and the start of memory: VMS V4.7 reads the load file's label (base, high
# address, load size) and loads the file from block 0 at address 0; v3 and v2
# (no label) gave "File open error, Load file, %RMS-E-EOF", v1 (label, program
# at 2000) loaded but started at 0 in the label)
#
# Makes NETTEST.SYS, a small PDP-11 "operating system" load file for a VMS
# load host (NCP SET NODE ... LOAD FILE), shaped like a saved RSX-11S system
# image: an RSX task label in block 0 with base 0, and the file loaded from
# block 0 at address 0 (seen with VMS V4.7), started at transfer address 0.
# DEC's tertiary loader TERUNA asks for it ("standard operating system"),
# writes the load host's parameters (node name and address, host) into a
# block at 3366 when location 2 holds PS 340, and jumps to the transfer
# address with R0 = the DEUNA's CSR.
#
# Memory (= file) layout:
#   000000  000437          BR 100 (over the label words)
#   000002  000340          PS 340 for TERUNA's check (label: task name word 2)
#   000004-000041           label words (partition name, base 0, high address,
#                           load size, ...), as in DEC's TERUNA.SYS label
#   000100  012702 000134   MOV #text,R2; print the text on the console
#                           (DL11 177560), wait for the last character,
#                           HALT at 000132 (SIMH: "HALT instruction, PC: 000134")
#   000134  the text, CR LF, NUL
#   ...     zero up to 4 blocks (the label's load size)
#
# usage: python3 -I mknettest.py OUT TEXT
import struct
import sys

SIZE = 4 * 512                           # file = loaded memory 0-3777


def build(text):
    d = bytearray(SIZE)
    high = SIZE - 1
    label = [0o000437, 0o000340,         # BR 100; PS 340 (task name words)
             0o007701, 0o062072,         # partition name (as TERUNA's label)
             0o000000,                   # base address 0
             high, high, high,           # high addresses
             SIZE // 64, SIZE // 64,     # load size, maximum size (64-byte units)
             0o000000, 0o000401, 0o000014, 0o040101, 0o000125, 0o000007, 0o000001]
    struct.pack_into('<%dH' % len(label), d, 0, *label)
    prog = [0o012702, 0o000134,          # 100: MOV #134,R2
            0o112200,                    # 104: MOVB (R2)+,R0
            0o001406,                    # 106: BEQ 124
            0o105737, 0o177564,          # 110: TSTB @#177564
            0o100375,                    # 114: BPL 110
            0o110037, 0o177566,          # 116: MOVB R0,@#177566
            0o000770,                    # 122: BR 104
            0o105737, 0o177564,          # 124: TSTB @#177564
            0o100375,                    # 130: BPL 124
            0o000000]                    # 132: HALT
    struct.pack_into('<%dH' % len(prog), d, 0o100, *prog)
    t = text.encode('ascii') + b'\r\n\0'
    assert 0o134 + len(t) < 0o340, "text too long"
    d[0o134:0o134 + len(t)] = t
    return bytes(d)


if __name__ == '__main__':
    out, text = sys.argv[1], sys.argv[2]
    open(out, 'wb').write(build(text))
    print("%s: %d bytes (label and memory image from 000000, program at 000100)" % (out, SIZE))
