#!/bin/bash
R=/home/agent/r3x/lakka/build.Lakka-A13_4C.arm-3.7.5-devel/retroarch-ad89b0c
S=$R/gfx/drivers/sunxi_gfx.c
O=/mnt/c/distr/soft/r3xclone/lakka-a13/out
LOG=/tmp/ra_menufix.log
exec > "$LOG" 2>&1
echo "START $(date)"
python3 - "$S" <<'PY'
import sys
S=sys.argv[1]; t=open(S).read()

old_ste='''static void sunxi_set_texture_enable(void *data, bool state, bool full_screen)
{
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;

   /* If it wasn't active and starts being active... */
   if (!_dispvars->menu_active && state)
   {
      /* Stop the vsync thread. */
      _dispvars->keep_vsync = false;
      sthread_join(_dispvars->vsync_thread);
   }

   /* If it was active but now it isn't active anymore... */
   if (_dispvars->menu_active && !state)
   {
      _dispvars->keep_vsync = true;
      _dispvars->vsync_thread = sthread_create(sunxi_vsync_thread_func, _dispvars);
   }
   _dispvars->menu_active = state;
}'''
new_ste='''static void sunxi_set_texture_enable(void *data, bool state, bool full_screen)
{
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;
   fprintf(stderr, "SUNXIDBG set_texture_enable state=%d menu_active=%d\\n", (int)state, (int)_dispvars->menu_active);
   /* A13FIX: do NOT join/create the vsync thread here; it deadlocks when the
    * menu activates. Keep the thread alive for the driver's lifetime. */
   _dispvars->menu_active = state;
}'''
assert old_ste in t, "set_texture_enable not found"
t=t.replace(old_ste,new_ste)

old_free='''   /* Stop the vsync thread and wait for it to join. */
   /* When menu is active, vsync thread has already been stopped. */
   if (!_dispvars->menu_active)
   {
      _dispvars->keep_vsync = false;
      sthread_join(_dispvars->vsync_thread);
   }'''
new_free='''   /* A13FIX: always stop and join the vsync thread we keep alive. */
   fprintf(stderr, "SUNXIDBG gfx_free joining vsync\\n");
   _dispvars->keep_vsync = false;
   sthread_join(_dispvars->vsync_thread);'''
assert old_free in t, "gfx_free not found"
t=t.replace(old_free,new_free)

# make keep_vsync volatile so the thread reliably observes stop
t=t.replace("   bool keep_vsync;", "   volatile bool keep_vsync;")
open(S,'w').write(t)
print("patched set_texture_enable, gfx_free, keep_vsync")
PY
grep -n "A13FIX\|volatile bool keep_vsync\|SUNXIDBG set_texture_enable" "$S"
cd "$R" || exit 1
make V=1 HAVE_LAKKA=1 HAVE_ZARCH=0 HAVE_WIFI=1 HAVE_BLUETOOTH=1 HAVE_FREETYPE=1 -j"$(nproc)"
echo "MAKE RC=$?"
ls -la "$R/retroarch" 2>&1
cp -f "$R/retroarch" "$O/retroarch-sunxi" 2>&1
mkdir -p /mnt/e2
mountpoint -q /mnt/e2 || mount -t drvfs E: /mnt/e2 -o rw 2>/dev/null
if mountpoint -q /mnt/e2; then cp -f "$R/retroarch" /mnt/e2/retroarch; printf 'menu' > /mnt/e2/MODE; sync; echo "FLASHED-TO-FAT"; else echo "NO CARD"; fi
echo "DONE $(date)"
