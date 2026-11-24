# open-simh-extended/tests/xq_boot/watch.py  v1
# Prints every Ethernet frame the simulator sends over SIMH's UDP transport
# (ATTACH XQ udp:SIMPORT:127.0.0.1:PORT) for S seconds: destination, source,
# type, and for MOP dump/load (60-01) the code and the first bytes.
# usage: python3 -I watch.py PORT SECONDS
import socket, sys, time
port, secs = int(sys.argv[1]), float(sys.argv[2])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('127.0.0.1', port)); s.settimeout(0.5)
end = time.time() + secs
while time.time() < end:
    try:
        d, a = s.recvfrom(2000)
    except socket.timeout:
        continue
    dst, src, typ = d[0:6].hex('-'), d[6:12].hex('-'), d[12:14].hex()
    print('%.1f %s <- %s type %s len %d %s' % (time.time() % 1000, dst, src, typ, len(d), d[14:40].hex()), flush=True)
