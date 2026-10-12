#!/usr/bin/env python3
# Render a boot splash "A13 Lakka" -> 320x240 ARGB8888 raw (fb little-endian BGRA).
# Uses the RA rgui 6x10 english bitmap (rzip -> raw 2048) as the font.
import zlib, os
T = "/mnt/c/Users/INSOER~1/AppData/Local/Temp/opencode/"
ENG = "/home/agent/tmp/sysroot/usr/share/retroarch-assets/rgui/font/bitmap6x10_eng.bin"

# decompress rzip (zlib chunks)
b = open(ENG, "rb").read()
assert b[:6] == b"#RZIPv"
o = 20; raw = bytearray()
while o < len(b):
    n = int.from_bytes(b[o:o+4], "little"); o += 4
    raw += zlib.decompress(b[o:o+n]); o += n
assert len(raw) == 2048

FW, FH = 6, 10
def glyph(ci):
    off = ci*8; rows = []
    for j in range(FH):
        s = ""
        for i in range(FW):
            bit = i + j*FW
            s += "#" if (raw[off+(bit>>3)] >> (bit & 7)) & 1 else "."
        rows.append(s)
    return rows

W, H = 320, 240
BG = (0x10, 0x14, 0x1c)     # dark blue-grey
FG = (0xF0, 0xF0, 0xF0)     # near white
ACC = (0x5C, 0xC8, 0xFF)    # accent (for "A13")
buf = bytearray(W*H*4)
def put(x, y, r, g, b):
    if 0 <= x < W and 0 <= y < H:
        i = (y*W + x) * 4
        buf[i] = b; buf[i+1] = g; buf[i+2] = r; buf[i+3] = 0xFF
def fill(r, g, b):
    for y in range(H):
        for x in range(W):
            put(x, y, r, g, b)
def text(s, x0, y0, scale, color):
    r, g, b = color
    cx = x0
    for ch in s:
        ci = ord(ch)
        if ch != " ":
            gl = glyph(ci)
            for j in range(FH):
                for i in range(FW):
                    if gl[j][i] == "#":
                        for sy in range(scale):
                            for sx in range(scale):
                                put(cx + i*scale + sx, y0 + j*scale + sy, r, g, b)
        cx += FW*scale

def tw(s, scale): return len(s)*FW*scale

fill(*BG)
# "A13" (accent), "Lakka" (white) - two lines, centered
s1, s2 = "A13", "Lakka"
sc1, sc2 = 9, 9
y1 = 70
text(s1, (W - tw(s1, sc1))//2, y1, sc1, ACC)
text(s2, (W - tw(s2, sc2))//2, y1 + FH*sc1 + 14, sc2, FG)

open(T+"logo.raw", "wb").write(bytes(buf))
print("logo.raw", len(buf), "bytes")
