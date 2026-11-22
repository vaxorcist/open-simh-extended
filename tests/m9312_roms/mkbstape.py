# open-simh-extended/tests/m9312_roms/mkbstape.py  v1
#
# Makes a bootstrap-format paper tape (or console byte stream) for DEC's
# bootstrap loader, as built by the M9312 'PR/TT' boot ROM (23-760A9):
# the ROM writes the classic 14-word loader to the top of memory (memory
# sized up to 28K words: with 28K words or more the loader is at
# 157744-157776) and starts it.  The loader reads one byte at a time from
# the reader (PC11 at 177550 or the console DL11 at 177560) and stores it at
# 157400 + offset, where offset is the low byte of its own word 157752
# (initially 352) plus one after each byte; storing a byte at 157752 itself
# therefore sets the next offset.  The tape:
#
#   1. leader: bytes 351 (stored at 157752: the next offset is 352 again),
#   2. the byte (start offset - 1), stored at 157752: the next byte goes to
#      157400 + start offset,
#   3. the program bytes, 157600-157743 (offset 200-343),
#   4. the loader's own bytes 157744-157773 written back unchanged (the
#      offset word 157752 gets 352, so the following byte goes to 157753),
#   5. a new low byte for the loader's BR at 157774: it branches to 157600,
#      the program, right after this byte is stored.
#
# The program (absolute, at 157600) prints its text on the console and
# halts at 157634 (PC 157636 in SIMH's HALT message):
#
#   157600  012701 157640   mov  #text,r1
#   157604  112100       L: movb (r1)+,r0
#   157606  001406          beq  D
#   157610  105737 177564 W: tstb @#177564
#   157614  100375          bpl  W
#   157616  110037 177566   movb r0,@#177566
#   157622  000770          br   L
#   157624  105737 177564 D: tstb @#177564   ; let the last character go out
#   157630  100375          bpl  D
#   157632  000240          nop
#   157634  000000          halt
#   157636  000000          (pad)
#   157640  text, NUL-terminated
#
# usage: python3 -I mkbstape.py OUT TEXT [--sim-send]
#   OUT         the tape image (bytes; ATTACH PTR OUT)
#   TEXT        the text the program prints (CR LF are appended)
#   --sim-send  also writes OUT.send: a SIMH SEND command line that types
#               the same bytes into the console (for the 'TT' entry), as
#               \xHH escapes; SET TTI 8B must be in effect

import struct
import sys

BASE = 0o157400                     # store base of the loader (top of 28K words)
LOADER = [0o016701, 0o000026, 0o012702, 0o000352, 0o005211, 0o105711,
          0o100376, 0o116162, 0o000002, BASE, 0o005267, 0o177756, 0o000765]
LOADER_ADDR = 0o157744              # its first word (the CSR word follows at 157776)
PROG_ADDR = 0o157600


def program(text):
    words = [0o012701, 0o157640,
             0o112100, 0o001406,
             0o105737, 0o177564, 0o100375,
             0o110037, 0o177566, 0o000770,
             0o105737, 0o177564, 0o100375,
             0o000240, 0o000000, 0o000000]
    code = struct.pack('<%dH' % len(words), *words)
    assert PROG_ADDR + len(code) == 0o157640
    code += text.encode('ascii') + b'\r\n\0'
    if len(code) % 2:
        code += b'\0'
    if PROG_ADDR + len(code) > LOADER_ADDR:
        raise SystemExit("text too long: the program must end below %06o" % LOADER_ADDR)
    return code


def tape(text):
    out = bytearray([0o351] * 40)                       # 1. leader
    start = PROG_ADDR - BASE
    out.append(start - 1)                               # 2. next offset = start
    code = program(text)
    code += bytes(LOADER_ADDR - (PROG_ADDR + len(code)))  # fill up to the loader
    out += code                                         # 3. the program
    loader = bytearray(struct.pack('<%dH' % len(LOADER), *LOADER))
    out += loader[:-2]                                  # 4. 157744-157773 unchanged
    br = (PROG_ADDR - (0o157774 + 2)) // 2              # 5. BR 157600
    assert -128 <= br < 0
    out.append(br & 0o377)
    out += bytes(20)                                    # trailer (not read)
    return bytes(out)


def main(argv):
    if len(argv) < 3:
        raise SystemExit("usage: mkbstape.py OUT TEXT [--sim-send]")
    data = tape(argv[2])
    with open(argv[1], 'wb') as f:
        f.write(data)
    print("%s: %d bytes" % (argv[1], len(data)))
    if '--sim-send' in argv[3:]:
        used = data[:-20]                               # no trailer: the program runs
        with open(argv[1] + '.send', 'w') as f:
            f.write('send "' + ''.join('\\x%02X' % b for b in used) + '"\n')
        print("%s.send: SEND of %d bytes" % (argv[1], len(used)))


if __name__ == '__main__':
    main(sys.argv)
