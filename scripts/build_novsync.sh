#!/bin/bash
B=/home/agent/r3x/lakka/build.Lakka-A13_4C.arm-3.7.5-devel
TC="$B/toolchain"
RA=$B/retroarch-ad89b0c
CROSS=$TC/bin/armv7a-libreelec-linux-gnueabi-
LOG=/tmp/novsync.log
exec > "$LOG" 2>&1
echo "START $(date)"
python3 - "$RA/gfx/drivers/sunxi_gfx.c" <<'PY'
import re,sys
F=sys.argv[1]; t=open(F).read()
pat=re.compile(r'if \(_dispvars->menu_active\)\s*\{.*?return true;\s*\}', re.S)
m=pat.search(t)
if not m:
    print("MENU BLOCK NOT FOUND"); sys.exit(1)
print("old block len", m.end()-m.start())
t=t[:m.start()]+'if (_dispvars->menu_active)\n      return true; /* A13FIX: no vsync wait (LCD vsync ~354ms) */'+t[m.end():]
open(F,'w').write(t)
print("ok" if 'A13FIX: no vsync wait' in t else "FAIL")
PY
grep -n "no vsync wait\|SUNXITIME\|FBIO_WAITFORVSYNC" "$RA/gfx/drivers/sunxi_gfx.c" | head
cd "$RA" || exit 1
make V=1 HAVE_LAKKA=1 HAVE_ZARCH=0 HAVE_WIFI=1 HAVE_BLUETOOTH=1 HAVE_FREETYPE=1 -j"$(nproc)"
echo "RAMAKE RC=$?"
mkdir -p /mnt/e2; mountpoint -q /mnt/e2 || mount -t drvfs E: /mnt/e2 -o rw
if [ -f /mnt/e2/retroarch ]; then cp -f "$RA/retroarch" /mnt/e2/retroarch; rm -f /mnt/e2/RA.LOG; sync; echo "FLASHED"; else echo "no card"; fi
echo "DONE $(date)"
