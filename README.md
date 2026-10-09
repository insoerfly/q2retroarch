# q2retroarch — Lakka/RetroArch для консоли Allwinner A13 (ревизия «Q2» / A13_4C)

Готовая к самостоятельной сборке версия Lakka-LibreELEC + RetroArch для портативной
консоли на **Allwinner A13 (sun5i)** с экраном **GC9307 320×240 (CPU-8080)**.
Устройство продаётся как «Q2» (в rootfs вендора — `version_q2`, `ui_q2_v2/v3`).

## ⚠️ КРИТИЧНО: как получить ЗАГРУЖАЮЩИЙСЯ образ

Проверенный на железе рабочий рецепт (после нескольких неудач):

- **Разметка:** FAT‑раздел `p1` начинается с **сектора 8192 (offset 4 МиБ)**, не с 2048.
  Первые 4 МиБ отведены под загрузчик Allwinner (`eGON.BT0` + U‑Boot SPL 2019.04).
- **Загрузчик:** берётся файл **`boot_patch_4M.img`** — это первые 4 МиБ рабочего образа
  (MBR + boot0 + U‑Boot SPL). Его boot‑область **отличается** от `20240628.img` (другой md5!),
  поэтому `20240628.img` как источник загрузчика **не подходит**.
- **Рабочий базовый образ:** `lakka-a13.img` (собран `make image`) + `boot_patch_4M.img` поверх
  первых 4 МиБ. Именно так получалась рабочая карта; загрузка ядра — вендорским u‑boot из
  **`res/ext/DATA01`** (uImage) и `res/ext/DATA02` (script.bin).

Наш ранний `scripts/build_sdimage.sh` клал `p1` на 2048 и копировал загрузчик из `20240628.img` —
**такой образ не грузится**. Исправленный путь — см. `BUILD.md` (раздел «SD‑образ») и
`scripts/build_sdimage.sh`: base = `lakka-a13.img`, затем `dd boot_patch_4M.img` на offset 0,
затем заменить файлы на p1 (`res/ext/DATA01`, `SYSTEM`, `retroarch`, `joypads/`).

## ⚔️ С чем боролись и как решили

Ниже — все ключевые проблемы и их решения (это же повторено в комментариях исходников).

| # | Проблема (симптом) | Причина | Решение |
|---|--------------------|---------|---------|
| 1 | Заводской init/меню не читается | init/menu вендора зашифрованы внутри встроенного initramfs ядра | Плюнули на вендор: свой init — `rdinit=/a13init` (systemd не используется) |
| 2 | RetroArch не выводит картинку | в сборке нет видеодрайвера `sunxi` (`HAVE_SUNXI=0`) | пересобрали: `HAVE_SUNXI=1` **и** `-DHAVE_SUNXI` в `config.mk` |
| 3 | Драйвер `sunxi` падает/мусор | FB был 16bpp и 2 буфера, а драйверу нужен 32bpp и ≥3 (пишет кадры ниже экрана) | патч `dev_fb.c`: `format=ARGB8888`, `buffer_num=3` |
| 4 | Меню без контента вешает систему (futex) | `sunxi_set_texture_enable()` делал `sthread_join(vsync_thread)` | убрали join/создание потока — держим его живым всё время |
| 5 | `/dev/input` пуст, кнопки не видны | в ядре нет драйвера клавиатуры; вендор читает кнопки из userspace прямо из GPIO | свой драйвер `a13keys.c` (опроc PIO, active-low, evdev/joydev) |
| 6 | Кнопки «залипают нажатыми» | **наша** `a13_backlight_on()` гнала PE0..PE11 и PB16 (а это кнопки!) как выходы LOW при инициализации LCD | убрали эти строки из `lcd0_panel_cfg.c` |
| 7 | RetroArch не видит устройство ввода | libudev сломан: нет `CONFIG_NET` → нет netlink → enumerate даёт 0/`EBADF` | ушли с `udev`: `input_joypad_driver="linuxraw"` + `CONFIG_INPUT_JOYDEV=y` + `/dev/input/js0` + автоконфиг |
| 8 | Панель обновляется ~1 кадр/5 сек (≈2–3 fps) | путь меню блокировался на `ioctl(FBIO_WAITFORVSYNC)`; LCD даёт vsync ~354 мс | убрали ожидание vsync в пути меню (кадр не обязателен к синхро) |
| 9 | Раскладка кнопок не та | вендорская таблица `g_key_info_a13_x` **не подходит ревизии Q2** | сняли раскладку эмпирически (опроc/dmesg) и зашили верную (см. ниже) |
| 10 | Настройки не сохраняются | RetroArch грузился с `-c /etc/retroarch.cfg` (read-only squashfs) и туда же писал | конфиг перенесли в `/storage/.config/retroarch/retroarch.cfg` (на FAT) |
| 11 | На карте 32 ГБ места нет | образ был на ~1 ГБ, остальное не размечено | init авто‑расширяет: создаёт большой FAT‑раздел из свободного места |
| 12 | **Карта перестала грузиться (нет экрана/подсветки)** | ранняя версия авто‑расширения создала раздел в **секторе 16** — там `eGON.BT0`; форматирование затёрло загрузчик | авто‑расширение теперь создаёт p3 **строго после p2** (явные секторы) + даём полный образ для восстановления |
| 13 | Звука нет | драйвер был `null`, кодек sunxi не задействован | `audio_driver="alsa"` (кодек `sunxi-CODEC` уже есть в ядре) |
| 14 | Заход в Bluetooth вешает меню | нет `bluetoothctl`/сервиса | `bluetooth_driver="null"` |

**Раскладка Q2 (эмпирически подтверждена):**
`↑=PE11, ↓=PE10, ←=PE9, →=PE8, A=PE2, B=PE4, X=PE3, Y=PE5, L=PE7, R=PE6, Start=PE0, Select=PB16, Menu=PE1`
(вендорская таблица давала другой набор назначений — доверять только проверенному).

## Что уже работает
- Загрузка кастомного ядра (BSP linux-sunxi 3.4), свой init (`rdinit=/a13init`, без systemd).
- RetroArch (собственный софт-видеодрайвер `sunxi`, pixman → DISP): **меню выводится**.
- **Кнопки** (стик и кнопки A/B/X/Y/L/R/Start/Select/Menu) — драйвер `a13keys` + раскладка Q2.
- **Звук** (ALSA, кодек sunxi).
- **zram-swap 128 МБ** — драйвер `zram`/`zsmalloc` портирован на ARM (см. `patches/`).
- **Сохранение настроек** и раздел под игры (авто‑расширение на свободное место карты).
- Меню быстрее: убран блокирующий `FBIO_WAITFORVSYNC` в пути меню (LCD даёт vsync ~354 мс).
- ✅ **Игры запускаются из меню** — фикс `pthread_join` в `sunxi_gfx_free` (см. «Обновление 7»).
- ✅ **Кнопка Menu/Home открывает меню RetroArch** — hotkey+menu_toggle на js-кнопку 8.
- ⚠️ **Звука нет** (вывод идёт, но усилитель/PA-пин — см. «Известные ограничения»).

## Железо
- SoC: Allwinner A13 (sun5i), 256 МБ RAM.
- Экран: GC9307, 320×240, интерфейс CPU-8080 (TCON `lcd_cpu_if=4`).
- Карта: SD, загрузчик Allwinner `eGON.BT0` (boot0) с offset 8 КБ + u-boot.
- Загрузка ядра: вендорский u-boot читает `res/DATA01` (uImage) и `res/DATA02` (script.bin/FEX) с FAT‑раздела.

## Состав репозитория
```
bootloader/   boot0-boot1_8k-1mb.bin (секторы 16..2047), script.bin (FEX)
kernel/       a13keys.c, lcd0_panel_cfg.c, dev_fb.c, kernel.config
retroarch/    sunxi_gfx.c, config.mk
initramfs/    a13init (+ пересобранный busybox с mkfs.vfat — см. BUILD.md)
config/       a13-retro-keys.cfg (автоконфиг кнопок)
scripts/      сборка ядра/RetroArch/образа + apply_a13.sh (интеграция в дерево Lakka)
device/       интеграция проекта Allwinner/A13_4C (options, linux.conf, bootloader)
patches/      патчи ядра (zram ARM-порт, sunxi USB gadget) + fex-set-usb-device.py (см. patches/README.md)
docs/         STATUS.md, REPORT.md, BRIEF.md — история/анализ
stock/        информация о референсном образе 20240628.img (нужен для сборки, см. BUILD.md)
```

## Быстрый старт
1. Прочитай `BUILD.md`.
2. Собери ядро, RetroArch и SD‑образ (`scripts/build_sdimage.sh`).
3. Запиши `lakka-full.img` на SD‑карту (Win32 Disk Imager).
4. При **первом** включении init создаст раздел под игры из свободного места и перезагрузится.

## Ключевые отличия от апстрима Lakka
- **Видеодрайвер `sunxi`** (`gfx/drivers/sunxi_gfx.c`) — не собирался в стоке; требует `HAVE_SUNXI=1`
  и `-DHAVE_SUNXI` в `config.mk`.
- **FB 32bpp + 3 буфера** — патч `drivers/video/sunxi/disp/dev_fb.c` (драйвер `sunxi` пишет кадры
  ниже видимого экрана; формат ARGB8888).
- **Панель GC9307** — `drivers/video/sunxi/lcd/lcd0_panel_cfg.c`; в `a13_backlight_on()` **нельзя**
  гонять PE0..PE11 и PB16 (это кнопки!), иначе кнопки «залипают».
- **Драйвер кнопок** `drivers/input/a13keys.c` (опроc PIO, active-low, evdev/joydev).
- **Init** `initramfs/a13init`: монтирует SYSTEM (squashfs xz), `/storage` на FAT, запускает RetroArch.

## Известные ограничения
- Аудио/сеть: сеть в ядре выключена (`CONFIG_NET` нет) — libudev/udev недоступны, поэтому
  RetroArch использует `input_joypad_driver="linuxraw"` (читает `/dev/input/jsX`).
- Bluetooth/аудио‑выход зависят от наличия железа; Bluetooth отключён (`bluetooth_driver="null"`).
- Меню‑только старт без контента: дедлок устранён патчем `sunxi_set_texture_enable()`.
- **XMB/Ozone не работают**: встроены в RetroArch, но требуют menu-framebuffer/текстур, которых
  софтверный `sunxi`-драйвер не даёт; RetroArch откатывается на `rgui`.
- **USB-консоль**: гейджет поднимается (в Windows появляется COM-порт), но открыть его нельзя —
  устройство переконфигурируется каждые ~20 с, а LUN mass-storage «no medium».
- **Аудио**: ALSA открывается, но звука нет — пин усилителя (PA). Пока отложено.
- **USB-консоль/карта не работают**: UDC (`sw_usb_udc`) регистрируется, но композит `g_acm_ms`
  не встаёт (`unable to autoconfigure all endpoints`) — ограничение sunxi `sw_udc`
  (см. `patches/README.md`).

Лицензии компонентов — согласно исходным проектам (Linux GPLv2, RetroArch GPLv3, Lakka/LibreELEC).
