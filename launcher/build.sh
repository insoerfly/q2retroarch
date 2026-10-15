#!/bin/bash
# Build the custom launcher app against the (fixed) SDK tree.
set -e
REPO=https://github.com/insoerfly/atj2273sdk
BASE=/root/launcher_sdk
SRC=/mnt/c/distr/soft/mp5x7/mylauncher

if [ ! -f "$BASE/sdk/case/makefile/makefile_app.rules" ]; then
    rm -rf "$BASE"
    git clone --depth 1 "$REPO" "$BASE"
fi
SDK=$BASE/sdk
APP=$SDK/case/apps/desktop/launcher_app
mkdir -p "$APP/out"
cp -f "$SRC"/launcher_main.c "$SRC"/launcher_scene.c "$SRC"/launcher.h "$SRC"/xmb_assets.h "$SRC"/Makefile "$APP/"

echo "=== build ==="
make -C "$APP" SDKROOT="$SDK" clean >/dev/null 2>&1 || true
make -C "$APP" SDKROOT="$SDK" 2>&1 | grep -vE "warning:|note:|^ *[0-9]+ \||^ *\||^ +\|" | tail -25
echo "=== result ==="
ls -l "$APP/out/" 2>&1
file "$APP/out/launcher.app" 2>&1
mipsel-linux-gnu-readelf -h "$APP/out/launcher.app" 2>/dev/null | grep -E "Machine|Type|Entry|Flags"
