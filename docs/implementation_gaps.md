# Анализ пробелов реализации: Лог M10 vs Код эмулятора

Сравнение лога `air3s_white_start.csv` (реальный M10 чип) с реализацией эмулятора.

## Сводная таблица

| Функция | Лог | Код | Статус |
|---------|-----|-----|--------|
| MON-VER | ✅ | ✅ | Работает |
| CFG-VALSET MSGOUT | ✅ | ⚠️ | Частично (только RATE-MEAS) |
| CFG-CFG | ✅ | ✅ | Работает |
| CFG-RST | ✅ | ❌ | **Не реализовано** |
| SEC-SIGN poll | ✅ | ❌ | **Не реализовано** |

## Критичные пробелы

### 1. CFG-VALSET не парсит MSGOUT ключи

**Проблема:** Код парсит только `CFG-RATE-MEAS` (0x30210001), но игнорирует все ключи включения сообщений.

**Ключи из лога, которые нужно добавить:**

```c
// Message Output Keys (CFG-MSGOUT-*)
0x20910007  CFG-MSGOUT-UBX_NAV_PVT_UART1
0x2091001B  CFG-MSGOUT-UBX_NAV_STATUS_UART1
0x20910039  CFG-MSGOUT-UBX_NAV_DOP_UART1
0x2091007A  CFG-MSGOUT-UBX_NAV_AOPSTATUS_UART1
0x20910016  CFG-MSGOUT-UBX_NAV_SAT_UART1
0x2091017E  CFG-MSGOUT-UBX_TIM_TP_UART1
0x20910232  CFG-MSGOUT-UBX_RXM_SFRBX_UART1
0x2091035A  CFG-MSGOUT-UBX_MON_RF_UART1
0x209102A5  CFG-MSGOUT-UBX_RXM_RAWX_UART1
0x2091001F  CFG-MSGOUT-UBX_NAV_ATT_I2C
```

**Решение:** Расширить парсер CFG-VALSET для установки флагов сообщений.

### 2. CFG-RST (0x06 0x04) не реализован

**Из лога:**
```
Raw: B5 62 06 04 04 00 00 00 01 00 0F 66
navBbrMask: 0x0000
resetMode: 1 (controlled software reset)
```

**Решение:** Добавить обработчик CFG-RST с вызовом `watchdog_reboot()` или soft reset.

### 3. SEC-SIGN poll (0x27 0x03) не обрабатывается

**Из лога:** Хост отправляет poll запрос, M10 возвращает SEC-UNIQID.

**Текущий код:** Нет обработки класса 0x27.

**Решение:** При получении `0x27 0x03` с len=0 отправить `UBX_SEC_UNIQID`.

## Отсутствующие сообщения

Эти сообщения запрашиваются хостом, но не реализованы в эмуляторе:

| Сообщение | Class | ID | Описание |
|-----------|-------|-----|----------|
| NAV-TIMEGPS | 0x01 | 0x20 | GPS Time Solution |
| NAV-VELECEF | 0x01 | 0x11 | Velocity in ECEF |
| NAV-TIMELS | 0x01 | 0x26 | Leap Second Info |
| NAV-COV | 0x01 | 0x36 | Covariance Matrix |
| NAV-HPPOSECEF | 0x01 | 0x13 | High Precision ECEF |
| NAV-EOE | 0x01 | 0x61 | End of Epoch |
| MON-RF | 0x0A | 0x38 | RF Information |

## План исправлений

### Приоритет 1 (Критично для совместимости)
1. [ ] Расширить CFG-VALSET парсер для MSGOUT ключей
2. [ ] Добавить обработку SEC-SIGN poll → SEC-UNIQID

### Приоритет 2 (Функциональность)
3. [ ] Реализовать CFG-RST (software reset)
4. [ ] Добавить NAV-EOE (End of Epoch marker)

### Приоритет 3 (Полнота)
5. [ ] Добавить NAV-TIMEGPS, NAV-VELECEF
6. [ ] Добавить MON-RF
7. [ ] Добавить NAV-HPPOSECEF, NAV-COV, NAV-TIMELS

## Рекомендуемые изменения кода

### Пример расширения CFG-VALSET парсера

```c
// В on_uart_rx0(), секция CFG-VALSET:
while (i + 4 <= end && i + 4 < count) {
    uint32_t key = RxData[i] | (RxData[i+1] << 8) |
                   (RxData[i+2] << 16) | (RxData[i+3] << 24);
    // ... определение val_size ...
    i += 4;

    if (i + val_size <= end && i + val_size <= count) {
        uint8_t val = RxData[i];  // Для 1-byte values

        switch (key) {
            case 0x30210001:  // CFG-RATE-MEAS
                // существующий код
                break;
            case 0x20910007:  // NAV-PVT
                UBX_NAV_PVT_fl = (val > 0);
                break;
            case 0x2091001B:  // NAV-STATUS (UART1)
                UBX_NAV_STATUS_fl = (val > 0);
                break;
            // ... остальные ключи ...
        }
        i += val_size;
    }
}

// Запуск вывода сообщений
if (!msg_output_started) {
    xTimerStart(TTimer_msg_start, 0);
}
```

### Пример обработки SEC-SIGN poll

```c
// В on_uart_rx0():
// UBX-SEC-UNIQID poll (0x27 0x03)
if (RxData[2] == 0x27 && RxData[3] == 0x03 && count >= 8) {
    uint16_t payload_len = RxData[4] | (RxData[5] << 8);
    if (payload_len == 0) {  // Poll request
        busy_wait_us(350);
        uart_write_blocking(uart0, UBX_SEC_UNIQID, sizeof(UBX_SEC_UNIQID));
        otvet++;
    }
}
```
