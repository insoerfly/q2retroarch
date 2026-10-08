# Разбор содержимого папки `res` (прошивка приставки R3X / A13)

Дата разбора: 2026-10-06
Источник: `C:\distr\soft\r3xclone\res`
Всё распаковано в: `C:\distr\soft\r3xclone\unpack`

---

## Краткий вывод

Это **ресурсно-флэш-набор китайской ретро-игровой приставки на SoC
Allwinner A13 (sun5i, ARM Cortex-A8, GPU Mali-400)**. Платформа —
Linux 3.4.104 (Allwinner BSP) + rootfs, собранный на **buildroot 2016.02**;
фронтенд — **MiniGUI + SDL**, эмуляторы — отдельные `.so`-ядра;
в приставке **~10 400 встроенных игр** (NES/SNES/Sega/GBA/PSX/аркады FBA и MAME2003+ и др.).
Внутреннее имя платы в ядре — **`A13_4C`**, конфиг платы сделан на основе
**Olimex A13-OLinuXino-MICRO V1.0**.

Дополнительно в наборе лежат посторонние x86-64 файлы (ядро Ubuntu 16.04 и
x86-микрокод/initrd). Они физически несовместимы с ARM A13 и, судя по всему,
попали в набор случайно/из другого сборочного контура (см. раздел «Аномалия»).

---

## Файлы верхнего уровня `res`

| Файл | Что это на самом деле |
|------|-----------------------|
| `DATA01` | **U-Boot uImage**, ядро Linux/ARM. `Linux-3.4.104-gee852210-dirty`, сборка `a13@ub` (Linaro GCC 4.9.4), дата 21.06.2024, load/entry `0x40008000`, размер 4 942 864 B. Внутри — сжатый (gzip) `vmlinux`. |
| `DATA02` | **Allwinner sys_config** (бинарный board-config). Ключи: `product/target/card_boot/dram_para/twi_para/uart_para/jtag_para/lcd0_para/hdmi_para/mmc0-2_para/usbc0-1/gpio_para/...`; строка `A13-olinuxino-micro-V1.0`, драйверы `ft5x_ts`, `hv_keypad`, `gc0308`, `bma222`, `ltr501als`, `pcf8563`, `spidev`; LED `green:pg09:led1`; USB gadget `Android/USB Developer`. |
| `DATA03` | **274 МБ контейнер** (см. подробный разбор ниже). Его полезная часть — **файловая система ext3 на 256 МиБ** (раздел ресурсов приставки). |
| `DATA04` | **SQLite-БД**, но **пустая**: только схема-шаблон `tbl_collect` (каталог эмуляторов). |
| `DATA06` | **Раздел подкачки (Linux swap)**, 128 МиБ, UUID `73bb6a8b-be44-4358-9d50-c16b4be49de7`. |
| `system.img` | **x86-64 bzImage**: `Linux 4.15.0-140-generic #144~16.04.1-Ubuntu` (Ubuntu 16.04). Внутри — подпись **UEFI Secure Boot** сертификатом **Canonical Secure Boot Signing (2017)** (см. `STOCK_DISPLAY_INIT.md` §1). U-Boot проверяет лишь наличие файла. |
| `settings/factory.txt`, `settings/setting.txt` | Конфиг: `lcd_save_time=60`, `Language` (0=English,1=Español), `background_music`, `button_tone`, пороги батареи 62/63. |
| `ext/DATA01`, `ext/DATA02` | Дубли ядра/sys_config, но **другие сборки** (ext/DATA01 — от 28.06.2024, 4 784 592 B). |
| `lib/libunity.so` | **Не Unity!** Это тоже **uImage того же ARM-ядра** (сборка 13.04.2023, 4 802 112 B). |
| `lib/libSDL.so` | **Не SDL!** Это **sys_config** (вариант DATA02). |

> Имена `libSDL.so` / `libunity.so` — маскировка: под ними лежат ядро и board-config.
> Всего в `res` — **3 варианта ARM-ядра** и **3 варианта sys_config**.

MD5 (различия вариантов):
```
DATA01         4c3f867bb8874db10213d722668fcbc5
ext/DATA01     4d9433bb2b202cc05d5b3a501ca07e5a
lib/libunity.so 709ca79c276c4d01c60cae08346b5e86
DATA02         8e8e1906b5686876ebdcc31f3d8e1c6c
ext/DATA02     1fee819d98a31bf31d16b4babfb39eeb
lib/libSDL.so  f4976d76c3c035476543f35094ea3ffb
DATA03         5909142d6c353108da8f6dd8a3d73446
```

---

## Подробный разбор `DATA03` (274 726 912 байт)

Внутри — три последовательные области:

1. **`0x000000 – ~0x005000`** — cpio-архив с микрокодом (AMD `AuthenticAMD.bin`).
2. **`0x007C00 – 0x4CFF2C`** — второй cpio-архив с микрокодом Intel
   (`kernel/x86/microcode/GenuineIntel.bin`, 5 013 504 B) → распакован в `x86_microcode.cpio`.
3. **`0x4D0000`** — **gzip**-поток. Распаковывается в initramfs **Ubuntu x86-64**
   (`scripts/init-premount/...`, `usr/lib/x86_64-linux-gnu/libcrypto.so.1.1`, plymouth и т.п.).
   Поток **обрывается** на ~3,6 МБ (неполный) → `x86_initrd_partial.cpio`.
4. **`0x600000` (6 МиБ) … конец** — образ **ext3, 256 МиБ** (Block size 1024,
   Inode count 65536, Block count 262144, UUID `9253e55e-fcf6-443f-8b31-3be790e06fda`,
   создан 28.06.2024, `Last mounted on /home/a13/sd-a13/tmp_mnt`).
   Это и есть **раздел ресурсов приставки** → `res_ext3_partition.img`,
   распакован в `rootfs_arm/`.

Заголовок sys_config (`DATA02`), кстати, тоже начинается с таблицы разделов,
а `DATA03` = «x86-прелюдия + ARM ext3». Это смешение архитектур — нетипично.

---

## Содержимое раздела ресурсов (`rootfs_arm/`, ext3)

Метка версии:
- `version_q2` → `V-20240628`
- `version_4c` → `Version-20240628-v2.0`

**UI-варианты для разных моделей/экранов:**
`ui_320_240_v2` (симлинк → `ui_q2_v2`), `ui_q2_v2`, `ui_q2_v3`,
`ui_800x480_v2`, `ui_m3`, `ui_x`, `ui_keypad_test`, а также `bak/ui_320_240`.
Т.е. модельный ряд: **4C, Q2, Q3, M3, X** с экранами **320×240 и 800×480**.

**Базы игр (`db/`):** sqlite с таблицей `tbl_all`
(`game_id, en_name, cn_name, cn_match, suffix, class_type, emu_type, img_name[MD5], long_en_name`):
```
game_list_a13_4c_64g.db      10 458 игр
game_list_a13_q2_64g.db      10 464 игры
game_list_a13_q3_32g_8k.db    8 389 игр
game_list_a13_q2_32g_8k.db    8 413 игр
game_list_a13_q2_64g_last.db 10 467 игр
+ варианты на 32g/3000
```
Распределение по эмуляторам (emu_type), пример для q2_64g:
`0→2703, 1→1420, 2→348, 3→3123, 4→2804, 5→49, 6→9, 7→7, 8→1`.
Примеры: Super Mario Bros, Captain Commando, Final Fight, Sangokushi II, Hook.

**Ядра эмуляторов (`lib/`), все ARM EABI5:**
- `libemulnes.so` (NES), `libfceumm.so` (FCEUmm)
- `libemulsfc.so` (SNES), `libemulmd.so` (Mega Drive/Genesis), `libemulgba.so` (GBA)
- `libemulpsx.so` (PlayStation), `libemula26.so` (Atari 2600), `libemula78.so` (Atari 7800)
- `libemulfbalpha.so` (FinalBurn Alpha, ~28 МБ), `libemulmame2003p.so` (MAME 2003 Plus, ~29 МБ)
- Плюс: `libminigui_sa-3.0.so` (MiniGUI — сам UI-тулкит), `libMali.so`/`libUMP.so`
  (GPU Mali-400), `libav*` = **ffmpeg 2.8.6**, `libasound`/ALSA, `libfreetype`, `libpng`,
  `libjpeg`, `libusb`, `libxml2`, `libc/libm/libpthread` = **eglibc 2.19 (Linaro 2014.08)**.

**`bin/`** — заводские/тестовые утилиты (не сам фронтенд):
`video_test`, `video_test_4C`, `video_png_test`, `video_aging_test`, `scaler_test(1)`,
`mali_test`, `mali_version`, `memtester`, `lima-memtester`, `audio_test`, `aplay`, `amixer`,
`ffmpeg_mp4_test`, `keypad_test_4c`, `keypad_test_q2`,
`emu_nes_test`, `emu_mame_test`, `emu_psx_test` (+`_big_lcd`).
> «Главного» exe-лаунчера в этом разделе нет — он, по-видимому, в системном
> rootfs (в `res` не входит). Здесь только ресурсы, ядра, библиотеки и тесты.

**`xml/`** — строки UI на ~25 языках (`lang_EN`, `lang_RU`, `lang_ES`, `lang_DE`,
`lang_PT`, `lang_THA`, ...), а также варианты `xml_q2`, `xml_4c`, `xml_m3`.
Пример (фрагменты меню): `list/class/history/download/favorite/search/settings/language/...`

**`wav/`** — звуки UI: `bkgnd.wav` (~9,9 МБ), `button_1..5.wav`, `confirm.wav`, `page.wav`;
`wav/org/run.sh` — скрипт перекодирования через ffmpeg (`pcm_s16le -ac 1 -ar 32040`).
**`font/`** — `Arial_Unicode_MS.ttf`.
**`ui_*/`** — PNG-ассеты: `desktop/`, `class/`, `menu/`, `setting/`, `search/`, `save/`,
`battery/`, `files_list/`, `help/` (help_cn/help_en) и т.д.

Всего в разделе — 675 файлов (497 PNG, 69 XML, 19 `.so`, 16 WAV, 7 DB и др.).

---

## Идентификация устройства

- **SoC:** Allwinner **A13 / sun5i**, CPU ARM **Cortex-A8**, GPU **Mali-400**.
- **Плата (референс):** **Olimex A13-OLinuXino-MICRO V1.0** (конфиг `DATA02`).
- **Имя в ядре:** **`A13_4C`**; ядро `Linux 3.4.104-gee852210-dirty` (Allwinner BSP),
  сборка от `a13@ub`, Linaro GCC 4.9.4; загрузочный cmdline `console=ttyS0,115200 loglevel=3`.
- **ОС/юзерспейс:** buildroot 2016.02, arm-linux-gnueabihf, eglibc 2.19, ffmpeg 2.8.6.
- **Фронтенд:** MiniGUI + SDL, эмуляторные ядра как `.so`.
- **Модельный ряд:** 4C, Q2 (320×240), Q3, M3, X, (800×480).
- **Игры:** ~10 400 встроенных, интерфейс EN/ES/RU и др.

### «Аномалия» (x86-совместимость)
В наборе присутствуют **x86-64** артефакты, не запускаемые на A13:
`system.img` (ядро Ubuntu 16.04 4.15.0-140) и x86-прелюдия в `DATA03`
(Intel+AMD microcode + обрезанный Ubuntu-инitrd). Скорее всего это
остаточные/посторонние файлы другого сборочного контура либо неаккуратная
упаковка образа. Вероятная гипотеза названия проекта: **«R3X»** — младшая
модель/клон в этом семействе (в самих бинарниках явной строки-бренда нет).

---

## Что лежит в `unpack/` после распаковки

```
unpack/
├─ REPORT.md                     (этот отчёт)
├─ STOCK_DISPLAY_INIT.md         полный разбор инициализации дисплея/железа стока + сертификат system.img
├─ FEX_DATA02_stock_dump.txt     дамп script.bin (res/DATA02)
├─ FEX_DATA02_ext_dump.txt       дамп script.bin (res/ext/DATA02)
├─ FEX_libSDL_dump.txt           дамп script.bin (res/lib/libSDL.so)
├─ res_ext3_partition.img        раздел ресурсов (ext3, 256 МиБ) из DATA03
├─ rootfs_arm/                   распакованный раздел ресурсов (675 файлов)
├─ x86_microcode.cpio            cpio с микрокодом Intel (из DATA03)
├─ x86_initrd_partial.cpio       неполный Ubuntu x86-64 initrd (из DATA03)
├─ DATA04.sqlite                 пустая SQLite-схема tbl_collect
└─ kernels/
   ├─ DATA01_kernel.uImage.body  тело uImage (ARM zImage) из DATA01
   ├─ kernel_arm_vmlinux.bin     распакованное ARM-ядро (8,3 МБ)
   └─ system_x86_vmlinux.bin     распакованное x86-64 ядро Ubuntu (31 МБ)
```

Инструменты, которыми это сделано (в WSL Ubuntu): `file`, `cpio`, `dd`, `gzip`,
`debugfs`/`dumpe2fs` (ext3), `7z`, Python (`zlib`/`sqlite3`).

---

## Обновление (bring-up на железе, 2026-10-08)

Работа с реальным устройством (см. `../lakka-a13/STATUS.md`). Существенные уточнения к
разбору выше:

- **U‑Boot стоковый и рабочий.** Вендорский загрузчик грузит ядро из `res/DATA01` +
  `res/DATA02` (sys_config) и проверяет наличие `res/system.img`. Наше BSP‑ядро
  грузится этим U‑Boot'ом. Т.е. boot0/u-boot — из образа `20240628.img` (секторы 16..2047).
- **`20240628.img` — частичный дамп (418 МБ);** таблица разделов содержит один раздел
  `start=2048, size≈50.8 ГБ`. Файл `res/DATA01` в нём совпадает с нашим
  `out/stock_DATA01.uImage` (sha256 `d32f3b47…`).
- **Кодировка портов в FEX — A=1.** Поэтому `lcd0_para` (`port=4`) = **PD** (шина LCD,
  `mul=2`), а `gpio_para` (`port=5`) = **PE**. Раньше это путалось.
- **Дисплей:** наше ядро настраивает весь тракт корректно (тайминги 800×480, PD в режиме
  LCD, `TCON` включён, кадры ~60 Гц — по дампу `LCDC0 @0x01C0C000`), но матрица не
  показывает ⇒ не хватает board‑инициализации/питания панели из вендорского ядра.
  Подсветка — активный низ (включается при `ALL-LOW`).
- **План:** сначала подтвердить вендорскую систему целиком (полный `20240628.img`), затем
  портировать RetroArch на вендорский стек.

### ДИСПЛЕЙ РЕШЁН (2026-10-08)

Устройство грузит **`res/ext/DATA01`** ⇒ панель — **GC9307, CPU‑8080, 320×240** (reset
PD2). Стоковую инициализацию (`LCD_panel_init` + `gc9307_init`, 85 команд, ветка
`lcd_type==9307`) перенесли в наше ядро (`lcd0_panel_cfg.c`) — на железе появились
цветные полосы. Подсветка — активный низ на `gpio_para` (ядро включает само).
Полностью: `STOCK_DISPLAY_INIT.md` §8 и `../lakka-a13/STATUS.md`.

### ЗАГРУЗКА LAKKA / ОБХОД systemd (2026-10-08)

Довели наше ядро с Lakka‑юзерспейсом до `systemd`: включили `CONFIG_SQUASHFS`
(стоковый `SYSTEM` был **zstd** — ядро 3.4 не умеет; пересобрали в **xz**),
`CONFIG_OVERLAY_FS`; в initramfs добавили `/bin`,`/sbin`,`/lib` и апплеты busybox;
cmdline — пути устройств. `systemd 242` падает (`Bad file descriptor` при проверке
mountpoint‑ов, `Failed to mount API filesystems`). Тест‑бинарник показал, что **syscalls
ядра исправны** ⇒ EBADF рождает сам systemd. Обошли systemd: `rdinit=/a13init` —
собственный init монтирует FAT/SYSTEM/overlay и запускает RetroArch. Детали — `../lakka-a13/STATUS.md`.
