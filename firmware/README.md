# firmware — аппаратные клиенты «Дорога на восток»

Это PlatformIO-проект с прошивками для аппаратных клиентов игры. На старте здесь
живёт один клиент — для LilyGO TTGO T-Display v1.1, — но структура рассчитана
на то, чтобы рядом без боли появлялись клиенты для других плат.

## Структура

```
firmware/
├── platformio.ini          # одна секция [env:*] на каждое устройство
├── boards/                 # кастомные манифесты плат для PlatformIO
│   └── lilygo-t-display-v11.json
├── include/                # общие заголовки (auto-included PlatformIO)
│   ├── road_east_version.h
│   └── local_secrets.example.h
├── lib/                    # приватные библиотеки проекта (см. lib/README)
├── src/
│   ├── common/             # общий C/C++ код всех клиентов (появится по мере роста)
│   └── lilygo_tdisplay/    # точка входа и платформо-зависимый код клиента
│       └── main.cpp
└── test/                   # PlatformIO unit-tests
```

Один общий `platformio.ini` со множеством environment-ов — это идиоматичный
PlatformIO-способ держать несколько целей в одном репозитории: общий код
шарится через `src/common/` и `lib/`, а каждое устройство получает свой
`build_src_filter`, свой `board`, свои `lib_deps` и свои `build_flags`.

## Сборка и прошивка

Требуется установленный [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
(или расширение PlatformIO IDE для VS Code). Из директории `firmware/`:

```bash
# собрать прошивку для LilyGO T-Display (env по умолчанию)
pio run

# залить на подключенную плату
pio run -t upload

# подключиться к serial-консоли
pio device monitor

# собрать конкретное окружение
pio run -e lilygo-t-display
```

## Локальные секреты

Wi-Fi пароли и прочие токены кладём в `include/local_secrets.h`, который
гитом игнорируется. Шаблон лежит рядом — `include/local_secrets.example.h`,
просто скопируй его и заполни:

```bash
cp include/local_secrets.example.h include/local_secrets.h
```

## Как добавить новое устройство

1. Создай папку `src/<device_id>/` и положи туда минимум `main.cpp`.
2. Если плата нестандартная — добавь её манифест в `boards/`.
3. В `platformio.ini` добавь новую секцию environment по образцу
   `[env:lilygo-t-display]`. Ключевые поля:
   - `platform`, `board` — целевая платформа и плата.
   - `build_src_filter` — обязательно ограничить сборку нужными папками:
     ```ini
     build_src_filter =
         -<*>
         +<common/**>
         +<<device_id>/**>
     ```
   - `lib_deps`, `build_flags` — зависимости и флаги, специфичные для платы.
4. Общий код, который захочется шарить между устройствами, кладём в
   `src/common/` (или, когда модуль вырастет, выносим в `lib/`).

Договорённость: device-specific код **никогда** не дёргается из `src/common/`
напрямую. Общий слой объявляет интерфейс, а каждое устройство реализует
платформо-зависимую часть у себя — это держит шаринг кода честным и
предотвращает скрытые зависимости от одной конкретной железки.

## LilyGO TTGO T-Display v1.1 — пины

| Назначение         | GPIO |
|--------------------|------|
| Кнопка «выбор»     | 0    |
| Кнопка «переключение» | 35 |
| Подсветка дисплея  | 4    |
| TFT MOSI / SCLK    | 19 / 18 |
| TFT CS / DC / RST  | 5 / 16 / 23 |

Прошивка работает в landscape (`setRotation(1)`), экран — 240x135. Кнопки
опрашиваются по фронту (`justPressed`); кнопка на GPIO 35 не имеет внутреннего
pull-up на этой ревизии, поэтому объявлена как `INPUT`, а не `INPUT_PULLUP`.

## Конвертация ассетов в C++ ресурсы

Графика и шрифт с кириллицей собираются в заголовочные файлы из бинарных
ассетов скриптом [`scripts/build_firmware_assets.py`](../scripts/build_firmware_assets.py).
Сейчас он собирает:

- `firmware/include/generated/splash_image.h` — 16-цветный splash из
  `assets/sprites/splash.png` (палитра RGB565 + 4bpp упакованные пиксели).
- `firmware/include/generated/font_8x16.h` — растровый шрифт 8×16 с
  печатной ASCII + кириллицей (А-я, Ё/ё), рендерится из TTF.

Установка зависимостей и запуск из корня репозитория:

```bash
pip install Pillow
python scripts/build_firmware_assets.py
```

Опции:

```
--splash-png PATH   путь к 16-цветному PNG (по умолчанию assets/sprites/splash.png)
--font-ttf PATH     путь к TTF (по умолчанию C:/Windows/Fonts/consola.ttf)
--out-dir PATH      куда писать заголовки (по умолчанию firmware/include/generated/)
```

Сгенерированные заголовки **коммитятся в репозиторий** — обычная сборка
прошивки не требует ни Python, ни Pillow. Перегенерируйте их каждый раз,
когда меняется исходный спрайт или нужно подменить шрифт.

UTF-8 строки в коде декодируются и отрисовываются через
`firmware/src/lilygo_tdisplay/text_render.{h,cpp}` — поддерживаются ASCII
0x20–0x7E, диапазон U+0410..U+044F и Ё/ё. Остальные кодпойнты заменяются на
пробел, добавить их можно расширив `FONT_CODEPOINTS` в скрипте и `glyphIndex`
в `text_render.cpp` синхронно.
