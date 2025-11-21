# rp2040-freertos-template

Шаблон прошивки на FreeRTOS для плат на RP2040 (по умолчанию Waveshare RP2040 Zero). В комплекте демо, эмулирующее ответы GPS-приёмника u‑blox по протоколу UBX, таймеры FreeRTOS, индикацию WS2812, простую SHA‑256 и сохранение режима во флеш.

## Что умеет демо
- FreeRTOS + Pico SDK 2.1.0, задачи для индикации и обновления временных меток/CRC.
- Эмуляция UBX: ответы на CFG-PRT/RATE/MSG/VALSET/CFG/GNSS/PMS и др.; генерация NAV_PVT/NAV_SVINFO/NAV_POS*/MON_RF/TIMEUTC/CLOCK и других пакетов с настраиваемой частотой.
- Накопление переданных пакетов в SHA-256 и периодическая отправка SEC_ECSIGN (шаблоны пакетов в `src/massivs.h`).
- Переключение режима кнопкой на GPIO6: аппаратный UART0 ↔ PIO-UART; выбранный режим и цвет WS2812 сохраняются во флеш в секции `.my_section`.
- Локальные статические библиотеки OpenSSL уже лежат в `src/OSSL`; mbedTLS — в `src/mbedtls` (по умолчанию не подключается).

## Требования
- Плата на RP2040; `PICO_BOARD` по умолчанию `waveshare_rp2040_zero`.
- ARM GCC toolchain, CMake ≥ 3.12, Ninja/Make, `git` (для подмодуля).
- Подмодуль `lib/FreeRTOS-Kernel` (`git submodule update --init --recursive`).
- Pico SDK 2.1.0 уже в репозитории (`pico-sdk-2.1.0`); при желании можно переопределить `PICO_SDK_PATH`.
- USB/TinyUSB не инициализирован (если нужно — подтяните submodule tinyusb в SDK).

## Структура
- `CMakeLists.txt` — верхний уровень, подключает Pico SDK и FreeRTOS.
- `src/CMakeLists.txt` — цели прошивки, генерация PIO заголовков, линковка с локальными OpenSSL (`src/OSSL/lib/libssl.a`, `libcrypto.a`).
- `src/main.c` — логика FreeRTOS: обработка UBX, таймеры выдачи пакетов, сохранение режима во флеш, индикация WS2812, SHA-256.
- `src/massivs.h` — набор готовых UBX пакетов.
- `src/sha256.c/.h` — простая реализация SHA-256.
- `src/uart_rx.pio`, `src/uart_tx.pio`, `src/ws2812.pio` и сгенерированные заголовки.
- `src/memory.x` — линкерный скрипт: секция `.my_section` по адресу `0x10040000`.
- `lib/FreeRTOS-Kernel`, `pico-sdk-2.1.0`, `src/OSSL`, `src/mbedtls` — зависимости в репозитории.

## Сборка
```bash
git submodule update --init --recursive
cmake -B build -G "Ninja" -DPICO_BOARD=waveshare_rp2040_zero
cmake --build build
```
Результаты: `build/src/podmena.uf2` (для загрузчика RP2040) и `podmena.elf/bin/hex`. Если нужен другой путь к SDK: `-DPICO_SDK_PATH=/ваш/путь/pico-sdk-2.1.0`.

## Работа прошивки
- WS2812 на GPIO16 мигает раз в 500 мс; GPIO5 — управляющий вывод. GPIO6 с IRQ переключает режим:
  - `regim=0` — режим «эмулятор»: прошивка сама отвечает на UBX-запросы через аппаратный UART0 (TX=GPIO0, RX=GPIO1), скорость по умолчанию 115200 бод.
  - `regim=1` — режим «прозрачный»: прошивка не отвечает сама, а пропускает ответы, приходящие на вход PIO-UART с GPIO3 (RX), далее выдаёт их на GPIO0 (TX) через тот же PIO. Аппаратный UART0 при этом выключен.
  Состояние (режим и цвет) сохраняется во флеш в `.my_section`.
- Таймеры FreeRTOS 1 Гц и 10 Гц рассылают NAV_PVT/NAV_SVINFO, m10_* таймеры управляют NAV_*/MON_RF и SEC_ECSIGN; `callback_m10_timer_nav` копит SHA-256, `callback_m10_timer_sign` отправляет подпись.
- `SecundaTaskCRC` раз в секунду инкрементирует время в NAV_PVT/NAV_TIMEUTC и пересчитывает CK_A/CK_B.
- Обработчик `UART0_IRQ` разбирает команды: смена скорости (CFG-PRT), частоты (CFG-RATE/VALSET), выбор пакетов (CFG-MSG/VALSET), запросы CFG-CFG/GNSS/PMS/SEC-UNIQID/MGA-DBD и т.п.; ответы из `massivs.h`.

## Настройка
- Правьте содержимое UBX пакетов в `src/massivs.h` и логику в `src/main.c` (пины: WS2812 — GPIO16, кнопка — GPIO6, UART0 — GPIO0/1, PIO-UART — GPIO3).
- Параметры FreeRTOS — `src/FreeRTOSConfig.h` (альтернатива `src/__FreeRTOSConfig.h`).
- При переносе на другую плату обновите `PICO_BOARD` и при необходимости адрес `.my_section` в `src/memory.x`.
- Если хотите линковаться с mbedTLS вместо OpenSSL — раскомментируйте соответствующие строки в `src/CMakeLists.txt` и убедитесь, что нужные архивы присутствуют в `src/mbedtls/library`.

## Примечание по предупреждениям сборки
Компилятор может ругнуться на `PICO_FLASH_ASSUME_CORE1_SAFE` (переопределение) и на `memcpy` из адреса `0x10040000` в `main.c`. Для чистой сборки можно удалить свой `#define PICO_FLASH_ASSUME_CORE1_SAFE` и привести адрес к указателю `(const void *)CUSTOM_SECTION_START` или читать флеш через API Pico.
