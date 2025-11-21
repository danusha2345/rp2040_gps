# rp2040-freertos-template

Шаблон прошивки на FreeRTOS для Waveshare RP2040 Zero: два режима работы GPS-спуфера под UBX, таймеры NAV/MON, сбор SHA-256 и подпись ECDSA secp192r1 (micro-ecc), индикация WS2812 и сохранение режима в flash.

## Стек и компоненты
- FreeRTOS + Pico SDK 2.1.0, PIO-UART (RX на GPIO3, вывод в GPIO0), PIO-WS2812 (GPIO16).
- UBX-сообщения в `src/massivs.h`: CFG-*, NAV_PVT/NAV_SVINFO/NAV_POS*/MON_RF/TIMEUTC/CLOCK и др.
- SHA-256 своя (`src/sha256.c`), подпись ECDSA secp192r1 через micro-ecc (`src/uECC.*`): первичный hash → вторичный hash с session_id → сворачивание до 192 бит → `uECC_sign_deterministic`.
- Статические OpenSSL библиотеки лежат в `src/OSSL` (используются только для линковки sha256.c), mbedTLS присутствует, но по умолчанию не подключен. TinyUSB отключён, субмодуль SDK можно не инициализировать.

## Аппаратные сигналы
- WS2812: GPIO16. Синий/зелёный/красный отражают режимы.
- Кнопка: GPIO6 (IRQ) переключает режим и пишет его в flash `.my_section` по адресу 0x10040000 вместе с цветом.
- Светодиод питания: GPIO5.
- UART0: TX=GPIO0, RX=GPIO1 (обычный режим).
- PIO-UART «сквозной»: вход с GPIO3, вывод на GPIO0 (режим passthrough).

## Собрать
```bash
git submodule update --init --recursive   # FreeRTOS-Kernel и pico-sdk, если не скачаны
cmake -B build -G "Ninja" -DPICO_BOARD=waveshare_rp2040_zero
cmake --build build
```
Артефакты: `build/src/podmena.uf2` (для прошивки), `build/src/podmena.elf/bin/hex`.

## Как работает
- Таймеры FreeRTOS 1 Гц/10 Гц шлют NAV_PVT, NAV_SVINFO и другие включённые UBX. CRC CK_A/CK_B считается в `CRC_gen`.
- `callback_m10_timer_nav` отправляет выбранные сообщения и одновременно копит SHA-256 первичного потока.
- `callback_m10_timer_sign`:
  1. Финализирует первичный SHA-256 (hash оригинальных сообщений) и записывает его в `SEC_ECSIGN`.
  2. Считает вторичный hash = SHA256(primary_hash + SEC_SESSION_ID).
  3. Сворачивает вторичный hash до 24 байт XOR'ом верхних 8 байт.
  4. Формирует буфер для подписи: folded(24) + SEC_SESSION_ID(24).
  5. Подписывает secp192r1 детерминированно (`uECC_sign_deterministic`), R и S кладёт в `SEC_ECSIGN`.
  6. Пересчитывает CRC и отправляет UBX-SEC-ECSIGN.
- Режимы:
  - `regim=0` — обычный: приём/ответ через UART0.
  - `regim=1` — сквозной: вход с GPIO3 (PIO-UART), вывод на GPIO0, UART0 отключён.

## Настройки и файлы
- `src/massivs.h`: все UBX-шаблоны, `SEC_PRIV_KEY` (24 байта, заполните свой), `SEC_SESSION_ID` (24 байта, по умолчанию нули).
- `src/main.c`: логика таймеров, подсчёт SHA, подпись, обработчики UART/PIO/кнопки.
- `src/memory.x`: закрепляет `.my_section` по адресу 0x10040000 под состояние режима и цвета.
- `src/CMakeLists.txt`: подключает `uECC.c`, `sha256.c`, статические `libssl.a/libcrypto.a` из `src/OSSL`.

## Зависимости
- ARM GCC toolchain, CMake ≥ 3.12, Ninja/Make.
- Pico SDK 2.1.0 (путь задаётся `PICO_SDK_PATH` или локальной папкой `pico-sdk-2.1.0`).
- FreeRTOS-Kernel как субмодуль в `lib/`.
- Для USB понадобится инициализировать tinyusb в SDK (необязательно).

## Примечания
- В `main.c` используется чтение из flash по `0x10040000`; при необходимости уберите `PICO_FLASH_ASSUME_CORE1_SAFE`.
- Ключ и session_id равны нулю по умолчанию — замените на реальные значения перед использованием подписи.
