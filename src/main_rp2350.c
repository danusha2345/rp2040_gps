/**
 * @file main_rp2350.c
 * @brief u-blox GNSS Emulator for RP2350 (Raspberry Pi Pico 2)
 * @author Daniil
 * @date 2025
 *
 * Dual-core implementation:
 * - Core0: FreeRTOS scheduler, UART handling, UBX message generation
 * - Core1: LED control, alarm pool timers, CRC calculation
 *
 * Features:
 * - UBX-NAV-PVT (10Hz configurable)
 * - UBX-NAV-SVINFO (1Hz configurable)
 * - UBX-NAV-POSLLH, UBX-NAV-POSECEF
 * - UBX-MON-HW, UBX-TIM-TP
 * - Dynamic baudrate switching
 * - Passthrough mode via PIO
 */

#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "ws2812.pio.h"
#include "uart_rx.pio.h"
#include "massivs.h"

// ============================================================================
// Configuration
// ============================================================================

#define LED_PIN         16
#define UART_TX_PIN     0
#define UART_RX_PIN     1
#define PIO_IN_PIN      3
#define PIO_OUT_PIN     0
#define MODE_BTN_PIN    6
#define MODE_BTN_PWR    5

#define Timer_10hz      pdMS_TO_TICKS(100)
#define Timer_1hz       pdMS_TO_TICKS(1000)

// ============================================================================
// Global variables
// ============================================================================

volatile int r = 0, g = 50, b = 0;
volatile uint skorost_uart_0 = 115200;
volatile int count = 0;
volatile int otvet = 0;
volatile bool flag = 0;
volatile bool flag_gpio6 = 1;

// Message enable flags
volatile bool UBX_NAV_PVT_fl = true;
volatile bool UBX_NAV_SVINFO_fl = true;
volatile bool timepulse_fl = false;
volatile bool UBX_NAV_POSLLH_fl = false;
volatile bool UBX_NAV_POSECEF_fl = false;
volatile bool UBX_MON_HW_fl = false;

// FreeRTOS handles
TimerHandle_t TTimer_10hz, TTimer_1hz;

// PIO handles (shared between cores)
PIO pio_led, pio_passthrough;
uint sm_led, sm_passthrough, offset_passthrough, offset_led;

// ============================================================================
// Function prototypes
// ============================================================================

void core1_entry(void);
void setup_uart0(void);
void off_uart0(void);
void on_uart_rx0(void);
void gpio_6_on(void);
void CRC_gen(uint8_t *adr, int razmer);
void secunda(void);
void secunda2(void);
void callback_1hz(TimerHandle_t xTimer);
void callback_10hz(TimerHandle_t xTimer);
void vOneTimeTask(void *pvParameters);

// ============================================================================
// WS2812 LED functions
// ============================================================================

static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// ============================================================================
// UBX CRC calculation (Fletcher checksum)
// ============================================================================

void CRC_gen(uint8_t *adr, int razmer) {
    uint8_t CK_A = 0, CK_B = 0;
    int N = razmer - 2;
    for (int i = 2; i < N; i++) {
        CK_A = CK_A + adr[i];
        CK_B = CK_B + CK_A;
    }
    adr[N] = CK_A;
    adr[N + 1] = CK_B;
}

// ============================================================================
// Time update functions
// ============================================================================

void secunda(void) {
    // Update iTOW (ms of week) - increment by 100ms
    uint32_t val = UBX_NAV_PVT[9] << 24 | UBX_NAV_PVT[8] << 16 |
                   UBX_NAV_PVT[7] << 8 | UBX_NAV_PVT[6];
    val = val + 100;

    // Update all messages with same iTOW
    UBX_NAV_PVT[6] = val; UBX_NAV_PVT[7] = val >> 8;
    UBX_NAV_PVT[8] = val >> 16; UBX_NAV_PVT[9] = val >> 24;

    UBX_NAV_SVINFO[6] = UBX_NAV_PVT[6]; UBX_NAV_SVINFO[7] = UBX_NAV_PVT[7];
    UBX_NAV_SVINFO[8] = UBX_NAV_PVT[8]; UBX_NAV_SVINFO[9] = UBX_NAV_PVT[9];

    Timepulse[6] = UBX_NAV_PVT[6]; Timepulse[7] = UBX_NAV_PVT[7];
    Timepulse[8] = UBX_NAV_PVT[8]; Timepulse[9] = UBX_NAV_PVT[9];

    UBX_NAV_POSECEF[6] = UBX_NAV_PVT[6]; UBX_NAV_POSECEF[7] = UBX_NAV_PVT[7];
    UBX_NAV_POSECEF[8] = UBX_NAV_PVT[8]; UBX_NAV_POSECEF[9] = UBX_NAV_PVT[9];

    UBX_NAV_POSLLH[6] = UBX_NAV_PVT[6]; UBX_NAV_POSLLH[7] = UBX_NAV_PVT[7];
    UBX_NAV_POSLLH[8] = UBX_NAV_PVT[8]; UBX_NAV_POSLLH[9] = UBX_NAV_PVT[9];

    // Recalculate CRC for all updated messages
    CRC_gen(UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
    CRC_gen(UBX_NAV_SVINFO, sizeof(UBX_NAV_SVINFO));
    CRC_gen(Timepulse, sizeof(Timepulse));
    CRC_gen(UBX_NAV_POSECEF, sizeof(UBX_NAV_POSECEF));
    CRC_gen(UBX_NAV_POSLLH, sizeof(UBX_NAV_POSLLH));
}

void secunda2(void) {
    // Update time fields (sec, min, hour)
    UBX_NAV_PVT[16] = UBX_NAV_PVT[16] + 0x01;  // seconds
    if (UBX_NAV_PVT[16] == 0x3C) {  // 60 seconds
        UBX_NAV_PVT[16] = 0x00;
        UBX_NAV_PVT[15] = UBX_NAV_PVT[15] + 0x01;  // minutes
        if (UBX_NAV_PVT[15] == 0x3C) {  // 60 minutes
            UBX_NAV_PVT[15] = 0x00;
            UBX_NAV_PVT[14] = UBX_NAV_PVT[14] + 0x01;  // hours
            if (UBX_NAV_PVT[14] == 0x18) {  // 24 hours
                UBX_NAV_PVT[14] = 0x00;
            }
        }
    }
    CRC_gen(UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
}

// ============================================================================
// UART setup
// ============================================================================

void setup_uart0(void) {
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(uart0, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(uart0, UART_RX_PIN));
    uart_init(uart0, skorost_uart_0);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    irq_set_exclusive_handler(UART0_IRQ, on_uart_rx0);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);
    uart_set_fifo_enabled(uart0, false);
}

void off_uart0(void) {
    irq_set_enabled(UART0_IRQ, false);
    irq_remove_handler(UART0_IRQ, on_uart_rx0);
    uart_deinit(uart0);
}

// ============================================================================
// One-time UART init task
// ============================================================================

void vOneTimeTask(void *pvParameters) {
    sleep_ms(10);
    setup_uart0();
    vTaskDelete(NULL);
}

// ============================================================================
// FreeRTOS Timer callbacks
// ============================================================================

void callback_1hz(TimerHandle_t xTimer) {
    flag = !flag;
    secunda2();

    if (UBX_NAV_SVINFO_fl) {
        uart_write_blocking(uart0, UBX_NAV_SVINFO, sizeof(UBX_NAV_SVINFO));
    }
    if (timepulse_fl) {
        uart_write_blocking(uart0, Timepulse, sizeof(Timepulse));
    }
    if (UBX_MON_HW_fl) {
        uart_write_blocking(uart0, UBX_MON_HW, sizeof(UBX_MON_HW));
    }
    if (UBX_NAV_POSLLH_fl) {
        uart_write_blocking(uart0, UBX_NAV_POSLLH, sizeof(UBX_NAV_POSLLH));
    }
    if (UBX_NAV_POSECEF_fl) {
        uart_write_blocking(uart0, UBX_NAV_POSECEF, sizeof(UBX_NAV_POSECEF));
    }

    flag = !flag;
}

void callback_10hz(TimerHandle_t xTimer) {
    flag = !flag;
    secunda();

    if (UBX_NAV_PVT_fl) {
        uart_write_blocking(uart0, UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
    }

    flag = !flag;
}

// ============================================================================
// LED blink timer callback (runs on Core1)
// ============================================================================

bool led_timer = 0;
bool timer_callback(__unused struct repeating_timer *t) {
    if ((led_timer = !led_timer) == 1) {
        put_pixel(pio_led, sm_led, urgb_u32(r, g, b));
    } else {
        put_pixel(pio_led, sm_led, urgb_u32(0, 0, 0));
    }
    return true;
}

// ============================================================================
// Mode button handler (GPIO6) - runs on Core1
// ============================================================================

void gpio_6_on(void) {
    gpio_put(MODE_BTN_PWR, 0);
    busy_wait_ms(100);

    if (flag_gpio6 == 1) {
        // Switch to passthrough mode
        r = 0; g = 0; b = 100;
        off_uart0();
        gpio_set_function(PIO_OUT_PIN, PIO_FUNCSEL_NUM(pio_passthrough, PIO_OUT_PIN));
        pio_sm_set_enabled(pio_passthrough, sm_passthrough, true);
        flag_gpio6 = 0;
    } else {
        // Switch to emulation mode
        r = 0; g = 100; b = 0;
        pio_sm_set_enabled(pio_passthrough, sm_passthrough, false);
        flag_gpio6 = 1;
        setup_uart0();
    }

    gpio_put(MODE_BTN_PWR, 1);
}

// ============================================================================
// Core1 entry point - LED and GPIO handling
// ============================================================================

void core1_entry(void) {
    // Initialize WS2812 LED on PIO
    pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio_led,
                                                      &sm_led, &offset_led, LED_PIN, 1, true);
    ws2812_program_init(pio_led, sm_led, offset_led, LED_PIN, 800000, false);
    put_pixel(pio_led, sm_led, urgb_u32(r, g, b));

    // Create alarm pool for LED blink timer on Core1
    struct repeating_timer timer;
    alarm_pool_t *pool = alarm_pool_create(1, 10);
    alarm_pool_add_repeating_timer_ms(pool, 500, timer_callback, NULL, &timer);

    // Calculate initial CRC for all messages
    CRC_gen(UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
    CRC_gen(UBX_NAV_SVINFO, sizeof(UBX_NAV_SVINFO));
    CRC_gen(Timepulse, sizeof(Timepulse));
    CRC_gen(UBX_NAV_POSECEF, sizeof(UBX_NAV_POSECEF));
    CRC_gen(UBX_NAV_POSLLH, sizeof(UBX_NAV_POSLLH));

    // Initialize mode button GPIO
    gpio_init(MODE_BTN_PWR);
    gpio_init(MODE_BTN_PIN);
    gpio_set_dir(MODE_BTN_PWR, GPIO_OUT);
    gpio_set_dir(MODE_BTN_PIN, GPIO_IN);
    gpio_put(MODE_BTN_PWR, 1);
    gpio_set_irq_enabled_with_callback(MODE_BTN_PIN, GPIO_IRQ_LEVEL_HIGH, true, &gpio_6_on);

    // Infinite loop on Core1
    while (1) {
        tight_loop_contents();
    }
}

// ============================================================================
// UART RX interrupt handler - UBX command processing
// ============================================================================

void on_uart_rx0(void) {
    // Read all available bytes with buffer overflow protection
    while (uart_is_readable_within_us(uart0, 300)) {
        uint8_t bait = uart_getc(uart0);
        if (count < sizeof(RxData)) {
            RxData[count] = bait;
            count++;
        }
    }

    // Minimum 4 bytes required for UBX header (sync + class + id)
    if (count < 4) {
        count = 0;
        return;
    }

    // Validate UBX sync bytes
    if (RxData[0] != 0xB5 || RxData[1] != 0x62) {
        count = 0;
        return;
    }

    // UBX-MON-VER (0x0A 0x04) - Version request
    if (flag == 0 && RxData[2] == 0x0A && RxData[3] == 0x04) {
        busy_wait_us(350);
        uart_write_blocking(uart0, monitor_ver, sizeof(monitor_ver));
        otvet++;
    }

    // UBX-CFG-PRT (0x06 0x00) - Port configuration (needs 18+ bytes)
    if (count >= 18 && RxData[2] == 0x06 && RxData[3] == 0x00) {
        // Read 32-bit baudrate from bytes 14-17 (little-endian)
        uint32_t baudrate = RxData[14] | (RxData[15] << 8) |
                           (RxData[16] << 16) | (RxData[17] << 24);

        if (baudrate > 0) {
            skorost_uart_0 = baudrate;
            uart_set_baudrate(uart0, skorost_uart_0);
            busy_wait_ms(1);
            uart_write_blocking(uart0, mes_1, sizeof(mes_1));
            otvet++;
        } else {
            // Baudrate 0 = disable messages
            timepulse_fl = false;
            UBX_NAV_PVT_fl = false;
            UBX_NAV_SVINFO_fl = false;
            otvet++;
        }
    }

    // UBX-CFG-RATE (0x06 0x08) - Navigation rate (needs 14+ bytes)
    if (count >= 14 && RxData[2] == 0x06 && RxData[3] == 0x08 && RxData[4] == 0x06) {
        int16_t period = RxData[7] << 8 | RxData[6];  // 16-bit period in ms
        xTimerChangePeriod(TTimer_10hz, pdMS_TO_TICKS(period), 0);
        busy_wait_us(350);
        uart_write_blocking(uart0, mes_2, sizeof(mes_2));
        otvet++;
    }

    // UBX-CFG-CFG (0x06 0x09) - Configuration save/load/clear
    if (RxData[2] == 0x06 && RxData[3] == 0x09) {
        busy_wait_us(350);
        uart_write_blocking(uart0, UBX_CFG_CFG, sizeof(UBX_CFG_CFG));
        otvet++;
    }

    // UBX-CFG-NAV5 (0x06 0x24) - Navigation engine settings
    if (RxData[2] == 0x06 && RxData[3] == 0x24) {
        busy_wait_us(350);
        uart_write_blocking(uart0, mes_3, sizeof(mes_3));
        otvet++;
    }

    // UBX-CFG-NAVX5 (0x06 0x23) - Navigation engine expert settings
    if (RxData[2] == 0x06 && RxData[3] == 0x23) {
        busy_wait_us(350);
        uart_write_blocking(uart0, mes_4, sizeof(mes_4));
        otvet++;
    }

    // UBX-CFG-GNSS (0x06 0x3E) - GNSS system configuration
    if (RxData[2] == 0x06 && RxData[3] == 0x3E) {
        busy_wait_us(350);
        uart_write_blocking(uart0, mes_5, sizeof(mes_5));
        otvet++;
    }

    // UBX-CFG-PMS (0x06 0x86) - Power management settings
    if (RxData[2] == 0x06 && RxData[3] == 0x86) {
        busy_wait_us(350);
        uart_write_blocking(uart0, mes_6, sizeof(mes_6));
        otvet++;
    }

    // UBX-CFG-MSG (0x06 0x01) - Message configuration (needs 11+ bytes)
    if (count >= 11 && RxData[2] == 0x06 && RxData[3] == 0x01) {
        busy_wait_us(250);
        uart_write_blocking(uart0, mes_7, sizeof(mes_7));
        otvet++;

        // NAV-PVT enable/rate
        if (RxData[6] == 0x01 && RxData[7] == 0x07 && RxData[8] > 0) {
            UBX_NAV_PVT_fl = true;
            xTimerChangePeriod(TTimer_10hz, pdMS_TO_TICKS(1000 / RxData[8]), 0);
        }
        // NAV-SVINFO enable/rate
        if (RxData[6] == 0x01 && RxData[7] == 0x30 && RxData[8] > 0) {
            UBX_NAV_SVINFO_fl = true;
            xTimerChangePeriod(TTimer_1hz, pdMS_TO_TICKS(1000 / RxData[8]), 0);
        }
        // MON-HW enable
        if (RxData[6] == 0x0A && RxData[7] == 0x09 && RxData[8] > 0) {
            UBX_MON_HW_fl = true;
        }
    }

    count = 0;
}

// ============================================================================
// Stack overflow hook for debugging
// ============================================================================

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    put_pixel(pio_led, sm_led, urgb_u32(255, 0, 0));
    configASSERT(NULL);
}

// ============================================================================
// Main entry point (Core0)
// ============================================================================

int main(void) {
    stdio_init_all();
    set_sys_clock_khz(150000, true);

    // Launch Core1 for LED and GPIO handling
    multicore_launch_core1(core1_entry);

    // Create FreeRTOS timers
    TTimer_1hz = xTimerCreate("1Hz", Timer_1hz, pdTRUE, 0, callback_1hz);
    TTimer_10hz = xTimerCreate("10Hz", Timer_10hz, pdTRUE, 0, callback_10hz);

    // Start timers
    xTimerStart(TTimer_1hz, 0);
    xTimerStart(TTimer_10hz, 10);

    // Create one-time UART init task
    xTaskCreate(vOneTimeTask, "OneTimeTask", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    // Initialize PIO passthrough program
    pio_claim_free_sm_and_add_program_for_gpio_range(&uart_rx_mini_program, &pio_passthrough,
                                                      &sm_passthrough, &offset_passthrough,
                                                      PIO_IN_PIN, PIO_OUT_PIN, true);
    uart_rx_mini_program_init(pio_passthrough, sm_passthrough, offset_passthrough,
                               PIO_IN_PIN, PIO_OUT_PIN, 3000000);

    // Start FreeRTOS scheduler
    vTaskStartScheduler();

    // Should never reach here
    put_pixel(pio_led, sm_led, urgb_u32(255, 0, 0));
    for (;;) {
        tight_loop_contents();
    }
}
