#!/usr/bin/env python3
# Set usbc0.usb_port_type = 0 (DEVICE) in a sunxi FEX (script.bin / DATA02).
# The kernel sunxi USB manager reads this key and, only for DEVICE/OTG,
# registers the UDC platform device (sw_usb_udc). Default on the Q2 stock FEX
# is 1 (HOST) which is why the gadget never bound.
#
# usage: fex-set-usb-device.py <DATA02>
import struct, sys

F = sys.argv[1]
d = bytearray(open(F, 'rb').read())
count = struct.unpack_from('<I', d, 0)[0]
secs = {}
for i in range(count):
    o = 16 + i * 40
    n = d[o:o+32].split(b'\0')[0].decode('latin1', 'replace')
    cnt, so = struct.unpack_from('<II', d, o + 32)
    secs[n] = (cnt, so)

def props(name):
    cnt, so = secs[name]
    base = so << 2
    out = []
    for j in range(cnt):
        p = base + j * 40
        pn = d[p:p+32].split(b'\0')[0].decode('latin1', 'replace')
        po, pa = struct.unpack_from('<II', d, p + 32)
        out.append((pn, po << 2))
    return out

for pname, vp in props('usbc0'):
    if pname == 'usb_port_type':
        old = struct.unpack_from('<I', d, vp)[0]
        struct.pack_into('<I', d, vp, 0)
        print("usb_port_type %d -> %d" % (old, struct.unpack_from('<I', d, vp)[0]))
open(F, 'wb').write(d)
print("written", F)
