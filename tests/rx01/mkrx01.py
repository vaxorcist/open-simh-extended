# open-simh-extended/tests/rx01/mkrx01.py  v1
#
# Makes a test image for the RX01 tests (2026-11-11): an RX01 diskette of
# 77 tracks x 26 sectors x 128 bytes (256,256 bytes), image order track 0
# sector 1 first (the SIMH RX layout).  Every sector holds its own address:
# byte 0 = track, byte 1 = sector, bytes 2..127 = (track * 26 + sector) & 255,
# so a test program can check that the right sector arrived.  Track 1
# sector 1 (the sector the RX boot ROM loads to address 0) holds a tiny
# PDP-11 program instead: MOV #123,R0; HALT, so BOOT RX0 can be checked.
#
# Usage: python3 -I mkrx01.py <image file>

import sys

TRACKS, SECTORS, BYTES = 77, 26, 128

def main(path):
    img = bytearray()
    for t in range(TRACKS):
        for s in range(1, SECTORS + 1):
            sec = bytearray([(t * SECTORS + s) & 0xFF] * BYTES)
            sec[0], sec[1] = t, s
            if (t, s) == (1, 1):                     # boot sector
                prog = [0o012700, 0o000123, 0o000000]
                sec = bytearray(BYTES)
                for i, w in enumerate(prog):
                    sec[2 * i] = w & 0xFF
                    sec[2 * i + 1] = w >> 8
            img += sec
    with open(path, 'wb') as f:
        f.write(img)
    print('%s: %d bytes' % (path, len(img)))

if __name__ == '__main__':
    main(sys.argv[1])
