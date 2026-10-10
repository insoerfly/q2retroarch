#!/usr/bin/env python3
# Set [target] boot_clock (MHz) in a sunxi FEX (script.bin / DATA02).
# usage: fex-set-cpu.py <DATA02> <freq_mhz>
import struct, sys

F = sys.argv[1]
FR = int(sys.argv[2])
d = bytearray(open(F, 'rb').read())
count = struct.unpack_from('<I', d, 0)[0]
secs = {}
for i in range(count):
    o = 16 + i * 40
    n = d[o:o + 32].split(b'\0')[0].decode('latin1', 'replace')
    cnt, so = struct.unpack_from('<II', d, o + 32)
    secs[n] = (cnt, so)

def props(name):
    cnt, so = secs[name]
    base = so << 2
    out = []
    for j in range(cnt):
        p = base + j * 40
        pn = d[p:p + 32].split(b'\0')[0].decode('latin1', 'replace')
        po, pa = struct.unpack_from('<II', d, p + 32)
        out.append((pn, po << 2))
    return out

done = False
for pname, vp in props('target'):
    if pname == 'boot_clock':
        old = struct.unpack_from('<I', d, vp)[0]
        struct.pack_into('<I', d, vp, FR)
        print("boot_clock %d -> %d" % (old, FR))
        done = True
if not done:
    print("boot_clock NOT FOUND"); sys.exit(1)
open(F, 'wb').write(d)
print("written", F)
