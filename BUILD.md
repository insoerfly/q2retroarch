# BUILD.md — пошаговая сборка

Предполагается дерево Lakka-LibreELEC (`PROJECT=Allwinner DEVICE=A13_4C ARCH=arm`).
Здесь описаны ручные шаги и что именно пропатчено.

## 0. Патчи поверх апстрима

### 0.1 Ядро (`linux-sunxi` 3.4, commit `d47d367036be38c5180632ec8a3ad169a4593a88`)
- `drivers/video/sunxi/disp/dev_fb.c` → **A13FORCE**: `init_para->format[0]=DISP_FORMAT_ARGB8888` (32bpp),
  `init_para->buffer_num[0]=3` (3 буфера).
- `drivers/video/sunxi/lcd/lcd0_panel_cfg.c` → панель GC9307; в `a13_backlight_on()` **убраны** строки,
  гоняющие `PE0..PE11` и `PB16` (это кнопки): оставлены только пины подсветки (PC/PG/PB10/2/4/3/15).
- `drivers/input/a13keys.c` → новый драйвер кнопок (см. `kernel/`). Собирается как встроенный:
  добавить `obj-y += a13keys.o` в `drivers/input/Makefile`.
- `.config` (`kernel/kernel.config`): `CONFIG_INPUT_EVDEV=y`, `CONFIG_INPUT_JOYDEV=y`,
  `CONFIG_STRICT_DEVMEM` **выключен**, `CONFIG_FB_SUNXI=y`, squashfs/overlay/zram, FRAMEBUFFER_CONSOLE,
  `CONFIG_CMDLINE="console=ttyS0,115200 console=tty0 loglevel=7 rdinit=/a13init"`.

### 0.2 RetroArch (`retroarch-ad89b0c`, v1.14.0)
- `gfx/drivers/sunxi_gfx.c` (`retroarch/`) — наш видеодрайвер софт-рендера.
- `config.mk` (`retroarch/`): `HAVE_SUNXI = 1` и `-DHAVE_SUNXI` в `CFLAGS`/`CXXFLAGS`.
  `Makefile.common` добавляет `sunxi_gfx.o`, но НЕ определяет `HAVE_SUNXI` — поэтому обязательно в config.mk.
- Патч `sunxi_set_texture_enable()`: **не** делать `sthread_join(vsync_thread)` (дедлок меню).
- Путь меню: убран `ioctl(FBIO_WAITFORVSYNC)` (LCD даёт vsync ~354 мс → меню тормозит).

## 1. Ядро + initramfs

Тулчейн: `build.Lakka-A13_4C.arm-3.7.5-devel/toolchain/bin/armv7a-libreelec-linux-gnueabi-`.

```sh
cd <linux-src>
cp kernel/a13keys.c drivers/input/
grep -q a13keys drivers/input/Makefile || echo "obj-y += a13keys.o" >> drivers/input/Makefile
cp kernel/acm_ms.c drivers/usb/gadget/acm_ms.c      # ACM-only (use_ms=0), см. patches/README.md
cp kernel/a13init <initramfs-dir>/a13init         # наш init (см. initramfs/a13init)
rm -f usr/initramfs_data.cpio.gz usr/initramfs_data.o
make ARCH=arm CROSS_COMPILE=<tc>/bin/armv7a-libreelec-linux-gnueabi- \
     HOSTCC=<tc>/bin/host-gcc HOSTCXX=<tc>/bin/host-g++ \
     HOSTCFLAGS="<...>" KCFLAGS="-fgnu89-inline" -j$(nproc) zImage
<u-boot>/tools/mkimage -A arm -O linux -T kernel -C none -a 0x40008000 -e 0x40008000 \
     -n "A13_4C Lakka" -d arch/arm/boot/zImage out/lakka-uImage
```

GCC-9 фиксы (если тулчейн GCC9): `include/linux/compiler-gcc9.h`, `__asmeq(x,y) ""`.

### 1.1 Пересборка busybox initramfs с `mkfs.vfat`
init использует `mkfs.vfat` при первом расширении раздела. В init‑busybox включить:
`CONFIG_MKFS_VFAT=y` (и, при желании, `CONFIG_MKDOSFS=y`) в
`busybox-1.31.0/.<target>-init/.config`, затем пересобрать (`make busybox:init` через основной билд системы
или вручную в каталоге сборки busybox) и установить бинарь в initramfs.

## 2. RetroArch

```sh
cd <retroarch-src>
cp ../retroarch/sunxi_gfx.c gfx/drivers/
# config.mk: HAVE_SUNXI=1, -DHAVE_SUNXI в CFLAGS/CXXFLAGS
make V=1 HAVE_LAKKA=1 HAVE_ZARCH=0 HAVE_WIFI=1 HAVE_BLUETOOTH=1 HAVE_FREETYPE=1 -j$(nproc)
cp retroarch out/retroarch-sunxi
```

## 3. SD-образ

> ⚠️ Загрузочный образ делается **не** «с нуля через sfdisk», а от **рабочего базового образа**.
> Проверено: `lakka-a13.img` + `boot_patch_4M.img` (первые 4 МиБ с загрузчиком) → карта грузится.

Правильный путь (`scripts/build_sdimage.sh`):
1. base = `lakka-a13.img` (собранный `make image`), p1 FAT начинается с **сектора 8192 (offset 4 МиБ)**;
2. `dd if=boot_patch_4M.img of=<img> bs=512 count=8192 conv=notrunc` — записать загрузчик в первые 4 МиБ;
3. смонитровать p1 и заменить/положить файлы:
   - `res/ext/DATA01` (**наше ядро uImage**) и `res/ext/DATA02` (script.bin) — именно тут ищет вендорский u-boot;
   - продублировать в `res/DATA01`/`res/DATA02` (на случай boot.scr);
   - `SYSTEM` (squashfs), `retroarch`, `joypads/a13-retro-keys.cfg`, `boot.scr`.

Что было не так раньше:
- `p1` ставили на сектор 2048 (offset 1 МиБ) — загрузчику нужно 4 МиБ;
- загрузчик брали из `20240628.img` (её boot‑область — другой u‑boot, md5 не тот) → не грузилось.

Файл загрузчика `boot_patch_4M.img` (4 МиБ) должен присутствовать в репо/`out` — он получен из рабочего
образа и содержит MBR + `eGON.BT0` + `U-Boot SPL 2019.04`.

## 4. Первый запуск
Init (`initramfs/a13init`) при первом старте: если нет большого FAT‑раздела — создаёт `p3` строго
**после p2** (секторы `p2_start+p2_size+4096..end`), перезагружается, форматирует `p3` (`mkfs.vfat`)
и монтирует как `/storage`. Дальше — RetroArch; настройки пишутся в
`/storage/.config/retroarch/retroarch.cfg`.

## 5. Полезное
- Логи на FAT: `RA.LOG` (RetroArch), `RETRO.LOG` (init), `DMESG.txt`.
- Кнопки: автоконфиг `joypads/a13-retro-keys.cfg` (js-индексы) + прямые биндинги в init.
- Раскладка Q2 (проверена эмпирически): стик PE11/PE10/PE9/PE8 = ↑/↓/←/→,
  A=PE2, B=PE4, X=PE3, Y=PE5, L=PE7, R=PE6, Start=PE0, Select=PB16, Menu=PE1.

## 6. Доп. правки ядра/прошивки (zram, USB, FEX, low-mem pcsx)

См. `patches/README.md`. Кратко:

- **zram** (`patches/0001..0003`): в дереве `zram`/`zsmalloc` помечены `depends on X86` —
  уберите `X86` и портируйте `zsmalloc-main.c` (`set_pte`→`set_pte_ext`,
  `__flush_tlb_one`→`flush_tlb_kernel_page`). В `.config`: `CONFIG_STAGING=y`,
  `CONFIG_ZRAM=y`, `CONFIG_ZSMALLOC=y`. `a13init` делает swap **128 МБ** на `/dev/zram0`.
- **USB-гейджет** (`patches/0004..0006`, `kernel/acm_ms.c`, FEX): USB0 → **DEVICE** (конфиг
  ядра и FEX `usbc0.usb_port_type=0`), `CONFIG_USB_GADGET=y`, `CONFIG_USB_G_ACM_MS=m`,
  `CONFIG_USB_SW_SUNXI_UDC0=y`; модуль `g_acm_ms.ko` кладётся в initramfs и на FAT.
  **Статус: работает** — композит поднимается в режиме **только CDC-ACM** (масса-сторадж
  выключен, `use_ms=0`), доступен root-shell по COM.
- **FEX Q2** — `bootloader/script_q2.bin` (29096 б): `target.boot_clock=912`,
  `usbc0.usb_port_type=0` (правится `patches/fex-set-usb-device.py`).
- **pcsx_rearmed low-mem**: в `libpcsxcore/new_dynarec/assem_arm.h` `TARGET_SIZE_2=23`
  (кэш dynarec 8 МБ вместо 16), сборка
  `make -f Makefile.libretro HAVE_NEON_ASM=1 DYNAREC=ari64 ARCH=arm BUILTIN_GPU=neon`,
  затем `.so` → `SYSTEM/usr/lib/libretro`.
- **Отладка игр**: файл `MODE` на FAT со словом `test` — `a13init` запустит ядро+ROM
  **напрямую** (мимо меню) и запишет подробный `RA.LOG`.

## 7. Запуск игр, кнопка меню, USB-консоль (2026-10-09)

- **Запуск игры из меню** (был фриз): при загрузке контента RetroArch пересоздаёт видеодрайвер
  (`MAIN_DEINIT`→`MAIN_INIT`), и `sunxi_gfx_free` висел в `pthread_join(vsync_thread)`.
  В `retroarch/sunxi_gfx.c` join заменён на ожидание флага `vsync_exited` (таймаут 3 с);
  при таймауте память не освобождается (без use-after-free), но система не виснет.
- **Кнопка Home/Menu → меню RetroArch**: в `retroarch.cfg` (и шаблоне `a13init`)
  `input_enable_hotkey_btn="8"` + `input_menu_toggle_btn="8"` (js-кнопка 8 = `BTN_MODE`).
- **USB-консоль — работает**: `patches/0006-sw_udc-int-ep-first.patch` (в `sw_udc` `ep5-int`
  первым в `ep_list`) + `kernel/acm_ms.c` (масса-сторадж выключен, `use_ms=0`; без него
  хост каждые ~20 с переинициализировал композит из-за LUN «no medium», и порт не
  открывался). В Windows — COM-порт, в `ttyGS0` — root-shell (`a13init` поднимает
  `chroot /newroot /bin/sh -i`).
- **Логи RetroArch**: `log_to_file=true`, `log_dir=/storage/logs`; в `sunxi_gfx.c` — отладочные
  крошки `SUNXI_DBG`/`SUNXI_DBGS` (пишут в stderr с `fflush`/`fsync`, чтобы пережили фриз).
- **Сборка RetroArch**: `make V=1 HAVE_LAKKA=1 HAVE_ZARCH=0 HAVE_WIFI=1 HAVE_BLUETOOTH=1
  HAVE_FREETYPE=1 -j$(nproc)` в `retroarch-ad89b0c`; бинарь → `out/retroarch-sunxi` и в образ.

## 8. PSX: downscale кадра до панели (2026-10-09)

- `sunxi`-драйвер блитил кадр **1:1** в fb (320×720×32). Кадры крупнее панели
  (PSX hi-res 640×478 / 512×240) переполняли fb → SIGSEGV в NEON-блите pixman. В
  `retroarch/sunxi_gfx.c` добавлен целочисленный downscale до ≤320×240 с раздельными
  коэффициентами X/Y и усреднением блока, плюс guard на NULL-кадр. SNES/мелкие ядра идут
  как раньше (их масштабирует слой DISP).
- Отладочный `crashtrace.so` (`LD_PRELOAD`, кладётся в `/flash/crashtrace.so`) печатает
  бэктрейс при SIGSEGV; адреса в модуле считать с учётом загрузки non-PIE EXEC по `0x8000`.
