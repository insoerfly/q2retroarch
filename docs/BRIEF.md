# Lakka A13 (sun5i) — бриф для анализа сторонней моделью

Цель файла: дать полный контекст, чтобы вторая модель помогла разобраться с **неработой кнопок**.
Просьба: **проверять утверждения по исходникам** (они в этой папке), не додумывать. Многие
«очевидные» догадки на этом железе уже оказались ложными.

---

## 1. Устройство и задача

- Портативная консоль на **Allwinner A13 (sun5i)**, шильдик `A13_4C`, но реальная ревизия платы — **Q2**
  (в вендорском rootfs есть `version_q2`, `ui_q2_v2/v3`, `keypad_test_q2`).
- Экран **GC9307 320×240** (CPU-8080 интерфейс).
- Задача: собрать Lakka и запустить **RetroArch** на этом железе.
- Всё делается на вендорском железе с кастомным ядром (sunxi-3.4 BSP).

## 2. Архитектура загрузки (как сейчас работает)

- Ядро BSP (linux-sunxi 3.4.104), грузится вендорским u-boot с SD-FAT (`res/ext/DATA01` = наш uImage).
- Встроенный initramfs (busybox). Ядро запускает **наш init** (`rdinit=/a13init`) — без systemd.
- init монтирует `/dev /proc /sys`, затем `/flash` (FAT p1), затем `/newroot` = `SYSTEM` (squashfs xz),
  tmpfs `/run /var /tmp /storage`, devpts/shm, и делает `chroot /newroot` → запускает RetroArch от root.
- Видео: собственный драйвер RetroArch **`gfx/drivers/sunxi_gfx.c`** (софт-рендер pixman → DISP-слой).
- FB пропатчен в `dev_fb.c`: **32bpp (ARGB8888)** и **3 буфера** (драйвер sunxi пишет кадры ниже
  видимого экрана: `offset=(yres+i*src_h)*xres*4`).
- Дедлок меню пофикшен: `sunxi_set_texture_enable()` больше НЕ делает `sthread_join(vsync_thread)`.

## 3. Что уже работает

- Загрузка, экран, **меню RetroArch (rgui) рисуется**, кадры идут (`SUNXIFRAME N WxH` в логе).
- RetroArch не падает; аудио/сеть недоступны (нет CONFIG_NET, нет /dev/snd) — не критично.
- Кнопок пока НЕТ.

## 4. Кнопки: факты

### 4.1 Вендорская таблица (из `libsys_a13_a.so`, символ `g_key_info_a13_x`)

Таблица записей по 56 байт: имя пина + имя функции. Порт в FEX нумеруется с 1 (1=PA..5=PE),
поэтому FEX `(5,n)` = **PEn**, `(2,16)` = **PB16**.

| GPIO | функция |
|------|---------|
| PE00 | home |
| PE01 | A |
| PE02 | B |
| PE03 | Y |
| PE04 | X |
| PE05 | L |
| PE06 | left |
| PE07 | R |
| PE08 | right |
| PE09 | up |
| PE10 | select |
| PE11 | down |
| PB16 | start |

(Второй вариант `g_key_info_big_4c` отличается только хвостом: `PB16=v+`, `PB17=v-`, `PB18=—`.)

### 4.2 Наш GPIO-скан (userspace, `/dev/mem`, эмпирически на этом Q2)

Набор пинов **совпал**: кнопки = `{PE00..PE11, PB16}` (13 шт). Idle: `PE=0x00000fff`, `PB=0x00070410`
(button-пины = 1, **active-low**). При нажатиях PE-биты уходят в 0.

### 4.3 Наш драйвер ядра `a13keys.c`

- `ioremap(0x01C20800)`, настраивает PE0..PE11 + PB16 как вход с pull-up, таймер каждые 10 мс,
  `input_report_key` по active-low, регистрирует `input_dev` name=`a13-retro-keys` (EV_KEY, BTN_*).
- Появляются `/dev/input/event0` и `/dev/input/js0` (CONFIG_INPUT_EVDEV=y, CONFIG_INPUT_JOYDEV=y).

### 4.4 Ключевая улика (dmesg)

- При инициализации драйвер читает: `PE=00000fff PB=00070410` (всё отпущено) — корректно.
- Но в **T≈2.35 c** (момент инициализации дисплея) драйвер **разом сообщает «нажаты» ВСЕ 13 кнопок**
  (`report code=544..547,304..316 pressed=1`) — т.е. пины PE/PB16 в этот момент стали читаться как 0.
- После этого изменений нет (залипание).

### 4.5 Причина (наша гипотеза)

В **нашем** `lcd0_panel_cfg.c` функция `a13_backlight_on()` (вызывается из `LCD_panel_init`, т.е.
при инициализации панели) выставляет **PE0..PE11 как выходы LOW** и **PB16** как выход LOW:
```c
for (i = 0; i <= 11; i++)
    a13_pin_out_low(pio, 0x90, i);
a13_pin_out_low(pio, 0x24, 16); a13_pin_out_low(pio, 0x24, 10);
```
Это ровно наши кнопки → они залипают «нажатыми», события в userspace не идут.

### 4.6 Что проверяли в userspace

- `jstest`-подобная утилита: `js0 name=[a13-retro-keys] axes=0 buttons=13`; при открытии joydev шлёт
  INIT-события **все `value=1`** (это `!!test_bit(dev->key,…)` — т.е. ядро реально считает кнопки
  нажатыми), а при нажатиях — **тишина** (согласуется с 4.5).
- `libudev` в нашем окружении сломан (нет `CONFIG_NET` → нет netlink → enumerate возвращает 0,
  `EBADF`). Поэтому RetroArch использует **`input_joypad_driver="linuxraw"`** (читает `/dev/input/jsX`,
  без libudev).

### 4.7 Конфиг RetroArch (appendconfig)

```
video_driver = "sunxi"
video_threaded = "false"
input_driver = "udev"
input_joypad_driver = "linuxraw"
audio_driver = "null"
menu_driver = "rgui"
joypad_autoconfig_dir = "/flash/joypads"
```
Автоконфиг `/flash/joypads/a13-retro-keys.cfg` (драйвер linuxraw). Значения — **js-индексы кнопок**:
```
input_driver = "linuxraw"
input_device = "a13-retro-keys"
input_a_btn = "0"   ... (индексы 0..12 по возрастанию BTN_*)
```

## 5. Текущий фикс (уже прошит, ждём проверки)

В `lcd0_panel_cfg.c` из `a13_backlight_on()` **убран** драйв PE0..PE11 и PB16 (остальные пины
PG/PC/PB10/PB2/PB4/PB3/PB15 оставлены). Ожидаем: LCD продолжит работать, а кнопки перестанут
залипать.

## 6. Файлы в этой папке

- `a13init` — наш init (`rdinit`).
- `a13keys.c` — драйвер кнопок.
- `lcd0_panel_cfg.c` — конфиг LCD (в т.ч. `a13_backlight_on`).
- `dev_fb.c` — fb-драйвер (в нём наши правки 32bpp/3 буфера).
- `sunxi_gfx.c` — видеодрайвер RetroArch (есть отладочные `SUNXIDBG`).
- `kernel.config`, `a13-retro-keys.cfg`,
  `lakka-uImage`/`lakka-zImage` (ядро), `retroarch-sunxi` (бинарь),
  `FEX_DATA02_*_dump.txt` (FEX: пины/периферия),
  `analysis.md` (предыдущее стороннее ревью — **частично ошибочное**, см. ниже).

## 7. ВОПРОСЫ

1. Верна ли причина залипания (4.5) и **безопасен ли фикс** (5) для работы LCD? (PE/PB16 — точно
   кнопки по вендорской таблице, но подтверди, что панель их не использует.)
2. Верны ли значения автоконфига как **js-индексы** для драйвера `linuxraw`? (Проверь
   `input/drivers_joypad/linuxraw_joypad.c`: `linuxraw_joypad_button()` сравнивает `joykey` с
   `BIT32_GET(pad->buttons, joykey)`, а js-события ставят бит по `event.number`.)
3. Что ещё может мешать кнопкам (порядок фиксации btn, `input_sync`, joydev INIT, маппинг RetroArch)?
4. Достоверна ли раскладка `a13_x` именно для платы **Q2**? Мы подтвердили только **набор** пинов,
   не назначение. Как это перепроверить малой ценой?
5. Замечания по `a13keys.c` / `lcd0_panel_cfg.c` / `sunxi_gfx.c` (по делу, с ссылками на строки).

## 8. Важное предупреждение

`analysis.md` (рядом) — предыдущее ревью: пункты #2, #4, #5, #6 **ложные** (проверено по исходникам),
а #2 к тому же противоречит #7. Не применять их. Пример того, почему нужна верификация.
