# q2retroarch — Lakka/RetroArch для консоли Allwinner A13 (ревизия «Q2» / A13_4C)

Готовая к самостоятельной сборке версия Lakka-LibreELEC + RetroArch для портативной
консоли на **Allwinner A13 (sun5i)** с экраном **GC9307 320×240 (CPU-8080)**.
Устройство продаётся как «Q2» (в rootfs вендора — `version_q2`, `ui_q2_v2/v3`).

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
- RetroArch (собственный софт-видеодрайвер `sunxi`, pixman → DISP): меню и игры выводятся.
- **Кнопки** (стик и кнопки A/B/X/Y/L/R/Start/Select/Menu) — драйвер `a13keys` + раскладка Q2.
- **Звук** (ALSA, кодек sunxi).
- **Сохранение настроек** и раздел под игры (авто‑расширение на свободное место карты).
- Меню быстрее: убран блокирующий `FBIO_WAITFORVSYNC` в пути меню (LCD даёт vsync ~354 мс).

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

Лицензии компонентов — согласно исходным проектам (Linux GPLv2, RetroArch GPLv3, Lakka/LibreELEC).
