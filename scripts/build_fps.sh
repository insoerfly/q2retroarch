#!/bin/bash
B=/home/agent/r3x/lakka/build.Lakka-A13_4C.arm-3.7.5-devel
TC="$B/toolchain"
RA=$B/retroarch-ad89b0c
O=/mnt/c/distr/soft/r3xclone/lakka-a13/out
CROSS=$TC/bin/armv7a-libreelec-linux-gnueabi-
LOG=/tmp/fps.log
exec > "$LOG" 2>&1
echo "START $(date)"

echo "== add fps print to sunxi_gfx.c =="
python3 - "$RA/gfx/drivers/sunxi_gfx.c" <<'PY'
import sys
F=sys.argv[1]; t=open(F).read()
if 'SUNXIFPS' in t:
    print("already"); raise SystemExit
# ensure sys/time.h include
if '#include <sys/time.h>' not in t:
    t=t.replace('#include "../../verbosity.h"', '#include "../../verbosity.h"\n#include <sys/time.h>',1)
old='   { static int _fc=0; if(_fc<5){ fprintf(stderr,"SUNXIDBG gfx_frame #%d\\n",_fc++); } }'
new='   { static int _fc=0; static struct timeval _t0; struct timeval _t; gettimeofday(&_t,0); if(_fc==0)_t0=_t; if((_fc%60)==0){ double _ms=(_t.tv_sec-_t0.tv_sec)*1000.0+(_t.tv_usec-_t0.tv_usec)/1000.0; fprintf(stderr,"SUNXIFPS frame=%d t=%.0fms fps=%.1f\\n",_fc,_ms,(_ms>0)?(_fc*1000.0/_ms):0.0);} _fc++; }'
if old in t:
    t=t.replace(old,new,1); print("replaced old")
else:
    # insert after the menu_is_alive line
    anchor='#ifdef HAVE_MENU\n   bool menu_is_alive            = video_info->menu_is_alive;\n#endif'
    if anchor in t:
        t=t.replace(anchor, anchor+'\n   '+new.strip(),1); print("inserted")
    else:
        print("ANCHOR NOT FOUND"); sys.exit(1)
open(F,'w').write(t)
print("ok" if 'SUNXIFPS' in t else "FAIL")
PY
grep -n SUNXIFPS "$RA/gfx/drivers/sunxi_gfx.c"

cd "$RA" || exit 1
make V=1 HAVE_LAKKA=1 HAVE_ZARCH=0 HAVE_WIFI=1 HAVE_BLUETOOTH=1 HAVE_FREETYPE=1 -j"$(nproc)"
echo "RAMAKE RC=$?"

mkdir -p /mnt/e2; mountpoint -q /mnt/e2 || mount -t drvfs E: /mnt/e2 -o rw
if [ -f /mnt/e2/retroarch ]; then
  cp -f "$RA/retroarch" /mnt/e2/retroarch
  rm -f /mnt/e2/RA.LOG
  sync; echo "FLASHED"
else echo "no card"; fi
echo "DONE $(date)"
