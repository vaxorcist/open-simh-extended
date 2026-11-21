# open-simh-extended/tests/dmv11_vmsload/mktap.py  v1
# Writes a SIMH tape image (.tap) that holds one file as 512-byte records
# followed by two tape marks, for VMS:
#   MOUNT/FOREIGN/BLOCKSIZE=512/RECORDSIZE=512 MSA0:
#   COPY MSA0: SYS$SYSTEM:REMWRT.TSK
# SIMH format: each record is a 32-bit little-endian length, the data
# (padded to an even length), the length again; a tape mark is a 0 length.
#
# usage: python3 -I mktap.py INPUT OUTPUT.tap
import sys, struct

data = open(sys.argv[1], "rb").read()
data += bytes((-len(data)) % 512)
with open(sys.argv[2], "wb") as f:
    for i in range(0, len(data), 512):
        rec = data[i:i + 512]
        f.write(struct.pack("<I", 512) + rec + struct.pack("<I", 512))
    f.write(struct.pack("<I", 0) * 2)
print("%s: %d records of 512 bytes" % (sys.argv[2], len(data) // 512))
