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
#include "sha256.h"
#include "uECC.h"
#include <string.h>

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

#define Timer_10hz      pdMS_TO_TICKS(200)   // 5Hz (как в реальном устройстве)
#define Timer_1hz       pdMS_TO_TICKS(1000)
#define Timer_3sec      pdMS_TO_TICKS(3000)   // First SEC-SIGN delay
#define Timer_4sec      pdMS_TO_TICKS(4000)   // Subsequent SEC-SIGN period

// ============================================================================
// SEC-SIGN ECDSA private key (SECP192R1)
// ============================================================================
static const uint8_t sec_sign_private_key[24] = {
    0xea, 0xa5, 0xc0, 0x11, 0x1e, 0x18, 0xdb, 0xd1,
    0x7a, 0xdb, 0x3d, 0xc9, 0x39, 0x4b, 0xfb, 0x45,
    0x1f, 0x9d, 0x5e, 0x83, 0xf9, 0x38, 0x22, 0xc7
};

// ============================================================================
// Global variables
// ============================================================================

volatile int r = 0, g = 50, b = 0;
volatile uint skorost_uart_0 = 115200;
volatile int count = 0;
volatile int otvet = 0;
volatile bool flag = 0;
volatile bool flag_gpio6 = 1;

// Message enable flags (all disabled by default, enabled via CFG-MSG)
// 5Hz messages (NAV-*)
volatile bool UBX_NAV_PVT_fl = false;
volatile bool UBX_NAV_POSECEF_fl = false;
volatile bool UBX_NAV_POSLLH_fl = false;
volatile bool UBX_NAV_STATUS_fl = false;
volatile bool UBX_NAV_DOP_fl = false;
volatile bool UBX_NAV_VELNED_fl = false;
volatile bool UBX_NAV_TIMEUTC_fl = false;
volatile bool UBX_NAV_CLOCK_fl = false;
volatile bool UBX_NAV_SAT_fl = false;
volatile bool UBX_NAV_AOPSTATUS_fl = false;
volatile bool UBX_RXM_RAWX_fl = false;
// New M10 messages
volatile bool UBX_NAV_EOE_fl = false;
volatile bool UBX_NAV_TIMEGPS_fl = false;
volatile bool UBX_NAV_VELECEF_fl = false;
volatile bool UBX_NAV_TIMELS_fl = false;
volatile bool UBX_NAV_COV_fl = false;
volatile bool UBX_NAV_HPPOSECEF_fl = false;
volatile bool UBX_MON_RF_fl = false;
// 1Hz messages (MON-*)
volatile bool UBX_MON_COMMS_fl = false;
// Legacy
volatile bool UBX_NAV_SVINFO_fl = false;
volatile bool UBX_MON_HW_fl = false;
volatile bool timepulse_fl = false;

// Flag to start message transmission after 1 second delay
volatile bool msg_output_started = false;

// SEC-SIGN variables (always enabled, first at 3s, then every 4s)
SHA256_CTX sec_sign_sha256_ctx;
volatile uint16_t sec_sign_packet_count = 0;

// FreeRTOS handles
TimerHandle_t TTimer_10hz, TTimer_1hz, TTimer_first_sec_sign, TTimer_4sec, TTimer_msg_start;

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
void gpio_6_on(uint gpio, uint32_t events);
void CRC_gen(uint8_t *adr, int razmer);
void secunda(void);
void secunda2(void);
void callback_1hz(TimerHandle_t xTimer);
void callback_10hz(TimerHandle_t xTimer);
void callback_first_sec_sign(TimerHandle_t xTimer);
void callback_4sec(TimerHandle_t xTimer);
void callback_msg_start(TimerHandle_t xTimer);
void vOneTimeTask(void *pvParameters);
void sec_sign_accumulate(const uint8_t *msg, size_t len);
void sec_sign_send(void);

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
    // Update iTOW (ms of week) - increment by 200ms (5Hz)
    uint32_t val = UBX_NAV_PVT[9] << 24 | UBX_NAV_PVT[8] << 16 |
                   UBX_NAV_PVT[7] << 8 | UBX_NAV_PVT[6];
    val = val + 200;

    // Update iTOW in all NAV messages
    UBX_NAV_PVT[6] = val; UBX_NAV_PVT[7] = val >> 8;
    UBX_NAV_PVT[8] = val >> 16; UBX_NAV_PVT[9] = val >> 24;

    UBX_NAV_POSECEF[6] = val; UBX_NAV_POSECEF[7] = val >> 8;
    UBX_NAV_POSECEF[8] = val >> 16; UBX_NAV_POSECEF[9] = val >> 24;

    UBX_NAV_POSLLH[6] = val; UBX_NAV_POSLLH[7] = val >> 8;
    UBX_NAV_POSLLH[8] = val >> 16; UBX_NAV_POSLLH[9] = val >> 24;

    UBX_NAV_STATUS[6] = val; UBX_NAV_STATUS[7] = val >> 8;
    UBX_NAV_STATUS[8] = val >> 16; UBX_NAV_STATUS[9] = val >> 24;

    UBX_NAV_DOP[6] = val; UBX_NAV_DOP[7] = val >> 8;
    UBX_NAV_DOP[8] = val >> 16; UBX_NAV_DOP[9] = val >> 24;

    UBX_NAV_VELNED[6] = val; UBX_NAV_VELNED[7] = val >> 8;
    UBX_NAV_VELNED[8] = val >> 16; UBX_NAV_VELNED[9] = val >> 24;

    UBX_NAV_TIMEUTC[6] = val; UBX_NAV_TIMEUTC[7] = val >> 8;
    UBX_NAV_TIMEUTC[8] = val >> 16; UBX_NAV_TIMEUTC[9] = val >> 24;

    UBX_NAV_CLOCK[6] = val; UBX_NAV_CLOCK[7] = val >> 8;
    UBX_NAV_CLOCK[8] = val >> 16; UBX_NAV_CLOCK[9] = val >> 24;

    UBX_NAV_SAT[6] = val; UBX_NAV_SAT[7] = val >> 8;
    UBX_NAV_SAT[8] = val >> 16; UBX_NAV_SAT[9] = val >> 24;

    UBX_NAV_AOPSTATUS[6] = val; UBX_NAV_AOPSTATUS[7] = val >> 8;
    UBX_NAV_AOPSTATUS[8] = val >> 16; UBX_NAV_AOPSTATUS[9] = val >> 24;

    // Legacy messages (if enabled)
    UBX_NAV_SVINFO[6] = val; UBX_NAV_SVINFO[7] = val >> 8;
    UBX_NAV_SVINFO[8] = val >> 16; UBX_NAV_SVINFO[9] = val >> 24;

    Timepulse[6] = val; Timepulse[7] = val >> 8;
    Timepulse[8] = val >> 16; Timepulse[9] = val >> 24;

    // New M10 messages - update iTOW
    UBX_NAV_EOE[6] = val; UBX_NAV_EOE[7] = val >> 8;
    UBX_NAV_EOE[8] = val >> 16; UBX_NAV_EOE[9] = val >> 24;

    UBX_NAV_TIMEGPS[6] = val; UBX_NAV_TIMEGPS[7] = val >> 8;
    UBX_NAV_TIMEGPS[8] = val >> 16; UBX_NAV_TIMEGPS[9] = val >> 24;

    UBX_NAV_VELECEF[6] = val; UBX_NAV_VELECEF[7] = val >> 8;
    UBX_NAV_VELECEF[8] = val >> 16; UBX_NAV_VELECEF[9] = val >> 24;

    UBX_NAV_TIMELS[6] = val; UBX_NAV_TIMELS[7] = val >> 8;
    UBX_NAV_TIMELS[8] = val >> 16; UBX_NAV_TIMELS[9] = val >> 24;

    UBX_NAV_COV[6] = val; UBX_NAV_COV[7] = val >> 8;
    UBX_NAV_COV[8] = val >> 16; UBX_NAV_COV[9] = val >> 24;

    // UBX_NAV_HPPOSECEF has iTOW at offset 10
    UBX_NAV_HPPOSECEF[10] = val; UBX_NAV_HPPOSECEF[11] = val >> 8;
    UBX_NAV_HPPOSECEF[12] = val >> 16; UBX_NAV_HPPOSECEF[13] = val >> 24;

    // Recalculate CRC for all messages
    CRC_gen(UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
    CRC_gen(UBX_NAV_POSECEF, sizeof(UBX_NAV_POSECEF));
    CRC_gen(UBX_NAV_POSLLH, sizeof(UBX_NAV_POSLLH));
    CRC_gen(UBX_NAV_STATUS, sizeof(UBX_NAV_STATUS));
    CRC_gen(UBX_NAV_DOP, sizeof(UBX_NAV_DOP));
    CRC_gen(UBX_NAV_VELNED, sizeof(UBX_NAV_VELNED));
    CRC_gen(UBX_NAV_TIMEUTC, sizeof(UBX_NAV_TIMEUTC));
    CRC_gen(UBX_NAV_CLOCK, sizeof(UBX_NAV_CLOCK));
    CRC_gen(UBX_NAV_SAT, sizeof(UBX_NAV_SAT));
    CRC_gen(UBX_NAV_AOPSTATUS, sizeof(UBX_NAV_AOPSTATUS));
    CRC_gen(UBX_RXM_RAWX, sizeof(UBX_RXM_RAWX));
    CRC_gen(UBX_NAV_SVINFO, sizeof(UBX_NAV_SVINFO));
    CRC_gen(Timepulse, sizeof(Timepulse));
    // New M10 messages CRC
    CRC_gen(UBX_NAV_EOE, sizeof(UBX_NAV_EOE));
    CRC_gen(UBX_NAV_TIMEGPS, sizeof(UBX_NAV_TIMEGPS));
    CRC_gen(UBX_NAV_VELECEF, sizeof(UBX_NAV_VELECEF));
    CRC_gen(UBX_NAV_TIMELS, sizeof(UBX_NAV_TIMELS));
    CRC_gen(UBX_NAV_COV, sizeof(UBX_NAV_COV));
    CRC_gen(UBX_NAV_HPPOSECEF, sizeof(UBX_NAV_HPPOSECEF));
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
// SEC-SIGN functions
// ============================================================================

void sec_sign_accumulate(const uint8_t *msg, size_t len) {
    sha256_update(&sec_sign_sha256_ctx, msg, len);
    sec_sign_packet_count++;
}

void sec_sign_send(void) {
    // Get accumulated SHA256 hash
    uint8_t sha256_field[32];
    SHA256_CTX ctx_copy = sec_sign_sha256_ctx;
    sha256_final(&ctx_copy, sha256_field);

    // SessionID is zeros for M10
    uint8_t sessionId[24] = {0};

    // Compute z = SHA256(sha256_field || sessionId), then fold to 192 bits
    uint8_t to_sign[56];
    memcpy(to_sign, sha256_field, 32);
    memcpy(to_sign + 32, sessionId, 24);

    uint8_t final_hash[32];
    sha256(to_sign, 56, final_hash);

    // Fold: XOR bytes 0-7 with bytes 24-31
    uint8_t z_bytes[24];
    memcpy(z_bytes, final_hash, 24);
    for (int i = 0; i < 8; i++) {
        z_bytes[i] ^= final_hash[24 + i];
    }

    // Sign using ECDSA SECP192R1
    uint8_t signature[48];  // R (24 bytes) + S (24 bytes)
    uECC_Curve curve = uECC_secp192r1();

    // uECC_sign expects hash to be truncated to curve size (192 bits = 24 bytes)
    if (!uECC_sign(sec_sign_private_key, z_bytes, 24, signature, curve)) {
        return;  // Signing failed
    }

    // Build SEC-SIGN message
    // Version (bytes 6-7)
    UBX_SEC_SIGN[6] = 0x01;
    UBX_SEC_SIGN[7] = 0x00;

    // Packet count (bytes 8-9, little-endian)
    UBX_SEC_SIGN[8] = sec_sign_packet_count & 0xFF;
    UBX_SEC_SIGN[9] = (sec_sign_packet_count >> 8) & 0xFF;

    // SHA256 field (bytes 10-41)
    memcpy(&UBX_SEC_SIGN[10], sha256_field, 32);

    // SessionID (bytes 42-65)
    memcpy(&UBX_SEC_SIGN[42], sessionId, 24);

    // R signature (bytes 66-89)
    memcpy(&UBX_SEC_SIGN[66], signature, 24);

    // S signature (bytes 90-113)
    memcpy(&UBX_SEC_SIGN[90], signature + 24, 24);

    // Calculate checksum
    CRC_gen(UBX_SEC_SIGN, sizeof(UBX_SEC_SIGN));

    // Send message
    uart_write_blocking(uart0, UBX_SEC_SIGN, sizeof(UBX_SEC_SIGN));

    // Reset for next interval
    sha256_init(&sec_sign_sha256_ctx);
    sec_sign_packet_count = 0;
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

    // Wait for msg_output_started before sending
    if (!msg_output_started) {
        flag = !flag;
        return;
    }

    // MON-* messages at 1Hz
    if (UBX_MON_COMMS_fl) {
        uart_write_blocking(uart0, UBX_MON_COMMS, sizeof(UBX_MON_COMMS));
        sec_sign_accumulate(UBX_MON_COMMS, sizeof(UBX_MON_COMMS));
    }
    // Legacy messages (disabled by default)
    if (UBX_NAV_SVINFO_fl) {
        uart_write_blocking(uart0, UBX_NAV_SVINFO, sizeof(UBX_NAV_SVINFO));
        sec_sign_accumulate(UBX_NAV_SVINFO, sizeof(UBX_NAV_SVINFO));
    }
    if (timepulse_fl) {
        uart_write_blocking(uart0, Timepulse, sizeof(Timepulse));
        sec_sign_accumulate(Timepulse, sizeof(Timepulse));
    }
    if (UBX_MON_HW_fl) {
        uart_write_blocking(uart0, UBX_MON_HW, sizeof(UBX_MON_HW));
        sec_sign_accumulate(UBX_MON_HW, sizeof(UBX_MON_HW));
    }
    if (UBX_MON_RF_fl) {
        uart_write_blocking(uart0, UBX_MON_RF, sizeof(UBX_MON_RF));
        sec_sign_accumulate(UBX_MON_RF, sizeof(UBX_MON_RF));
    }

    flag = !flag;
}

void callback_10hz(TimerHandle_t xTimer) {
    flag = !flag;
    secunda();

    // Wait for msg_output_started before sending
    if (!msg_output_started) {
        flag = !flag;
        return;
    }

    // All NAV-* and RXM-* messages at 5Hz
    if (UBX_NAV_PVT_fl) {
        uart_write_blocking(uart0, UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
        sec_sign_accumulate(UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
    }
    if (UBX_NAV_POSECEF_fl) {
        uart_write_blocking(uart0, UBX_NAV_POSECEF, sizeof(UBX_NAV_POSECEF));
        sec_sign_accumulate(UBX_NAV_POSECEF, sizeof(UBX_NAV_POSECEF));
    }
    if (UBX_NAV_POSLLH_fl) {
        uart_write_blocking(uart0, UBX_NAV_POSLLH, sizeof(UBX_NAV_POSLLH));
        sec_sign_accumulate(UBX_NAV_POSLLH, sizeof(UBX_NAV_POSLLH));
    }
    if (UBX_NAV_STATUS_fl) {
        uart_write_blocking(uart0, UBX_NAV_STATUS, sizeof(UBX_NAV_STATUS));
        sec_sign_accumulate(UBX_NAV_STATUS, sizeof(UBX_NAV_STATUS));
    }
    if (UBX_NAV_DOP_fl) {
        uart_write_blocking(uart0, UBX_NAV_DOP, sizeof(UBX_NAV_DOP));
        sec_sign_accumulate(UBX_NAV_DOP, sizeof(UBX_NAV_DOP));
    }
    if (UBX_NAV_VELNED_fl) {
        uart_write_blocking(uart0, UBX_NAV_VELNED, sizeof(UBX_NAV_VELNED));
        sec_sign_accumulate(UBX_NAV_VELNED, sizeof(UBX_NAV_VELNED));
    }
    if (UBX_NAV_TIMEUTC_fl) {
        uart_write_blocking(uart0, UBX_NAV_TIMEUTC, sizeof(UBX_NAV_TIMEUTC));
        sec_sign_accumulate(UBX_NAV_TIMEUTC, sizeof(UBX_NAV_TIMEUTC));
    }
    if (UBX_NAV_CLOCK_fl) {
        uart_write_blocking(uart0, UBX_NAV_CLOCK, sizeof(UBX_NAV_CLOCK));
        sec_sign_accumulate(UBX_NAV_CLOCK, sizeof(UBX_NAV_CLOCK));
    }
    if (UBX_NAV_SAT_fl) {
        uart_write_blocking(uart0, UBX_NAV_SAT, sizeof(UBX_NAV_SAT));
        sec_sign_accumulate(UBX_NAV_SAT, sizeof(UBX_NAV_SAT));
    }
    if (UBX_NAV_AOPSTATUS_fl) {
        uart_write_blocking(uart0, UBX_NAV_AOPSTATUS, sizeof(UBX_NAV_AOPSTATUS));
        sec_sign_accumulate(UBX_NAV_AOPSTATUS, sizeof(UBX_NAV_AOPSTATUS));
    }
    if (UBX_RXM_RAWX_fl) {
        uart_write_blocking(uart0, UBX_RXM_RAWX, sizeof(UBX_RXM_RAWX));
        sec_sign_accumulate(UBX_RXM_RAWX, sizeof(UBX_RXM_RAWX));
    }
    // New M10 messages
    if (UBX_NAV_TIMEGPS_fl) {
        uart_write_blocking(uart0, UBX_NAV_TIMEGPS, sizeof(UBX_NAV_TIMEGPS));
        sec_sign_accumulate(UBX_NAV_TIMEGPS, sizeof(UBX_NAV_TIMEGPS));
    }
    if (UBX_NAV_VELECEF_fl) {
        uart_write_blocking(uart0, UBX_NAV_VELECEF, sizeof(UBX_NAV_VELECEF));
        sec_sign_accumulate(UBX_NAV_VELECEF, sizeof(UBX_NAV_VELECEF));
    }
    if (UBX_NAV_TIMELS_fl) {
        uart_write_blocking(uart0, UBX_NAV_TIMELS, sizeof(UBX_NAV_TIMELS));
        sec_sign_accumulate(UBX_NAV_TIMELS, sizeof(UBX_NAV_TIMELS));
    }
    if (UBX_NAV_COV_fl) {
        uart_write_blocking(uart0, UBX_NAV_COV, sizeof(UBX_NAV_COV));
        sec_sign_accumulate(UBX_NAV_COV, sizeof(UBX_NAV_COV));
    }
    if (UBX_NAV_HPPOSECEF_fl) {
        uart_write_blocking(uart0, UBX_NAV_HPPOSECEF, sizeof(UBX_NAV_HPPOSECEF));
        sec_sign_accumulate(UBX_NAV_HPPOSECEF, sizeof(UBX_NAV_HPPOSECEF));
    }
    // NAV-EOE should be sent last in epoch (End of Epoch marker)
    if (UBX_NAV_EOE_fl) {
        uart_write_blocking(uart0, UBX_NAV_EOE, sizeof(UBX_NAV_EOE));
        sec_sign_accumulate(UBX_NAV_EOE, sizeof(UBX_NAV_EOE));
    }

    flag = !flag;
}

void callback_first_sec_sign(TimerHandle_t xTimer) {
    // First SEC-SIGN at 3 seconds after boot
    sec_sign_send();
    // Start periodic 4-second timer for subsequent SEC-SIGN messages
    xTimerStart(TTimer_4sec, 0);
}

void callback_4sec(TimerHandle_t xTimer) {
    sec_sign_send();
}

void callback_msg_start(TimerHandle_t xTimer) {
    // Called 1 second after first CFG-MSG, enables message output
    msg_output_started = true;
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

void gpio_6_on(uint gpio, uint32_t events) {
    (void)gpio;   // Unused parameter
    (void)events; // Unused parameter
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
        uart_write_blocking(uart0, monitor_ver_M10, sizeof(monitor_ver_M10));
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
        // MON-HW enable (class 0x0A, id 0x09)
        if (RxData[6] == 0x0A && RxData[7] == 0x09 && RxData[8] > 0) {
            UBX_MON_HW_fl = true;
        }
        // NAV-POSLLH enable (class 0x01, id 0x02)
        if (RxData[6] == 0x01 && RxData[7] == 0x02 && RxData[8] > 0) {
            UBX_NAV_POSLLH_fl = true;
        }
        // NAV-POSECEF enable (class 0x01, id 0x01)
        if (RxData[6] == 0x01 && RxData[7] == 0x01 && RxData[8] > 0) {
            UBX_NAV_POSECEF_fl = true;
        }
        // TIM-TP enable (class 0x0D, id 0x01)
        if (RxData[6] == 0x0D && RxData[7] == 0x01 && RxData[8] > 0) {
            timepulse_fl = true;
        }

        // Start message output after 1 second delay (if not already started)
        if (!msg_output_started) {
            xTimerStart(TTimer_msg_start, 0);
        }
    }

    // UBX-CFG-VALSET (0x06 0x8A) - M10 configuration (needs 12+ bytes minimum)
    if (count >= 12 && RxData[2] == 0x06 && RxData[3] == 0x8A) {
        uint16_t payload_len = RxData[4] | (RxData[5] << 8);
        // Parse key-value pairs starting at offset 10 (after header + version + layers + reserved)
        int i = 10;
        int end = 6 + payload_len;

        while (i + 4 <= end && i + 4 < count) {
            uint32_t key = RxData[i] | (RxData[i+1] << 8) | (RxData[i+2] << 16) | (RxData[i+3] << 24);
            uint8_t key_size = (key >> 28) & 0x07;
            // Size mapping: 1=1bit, 2=1byte, 3=2bytes, 4=4bytes, 5=8bytes
            uint8_t val_size = (key_size == 3) ? 2 : (key_size == 4) ? 4 : (key_size == 5) ? 8 : 1;
            i += 4;

            if (i + val_size <= end && i + val_size <= count) {
                uint8_t val8 = RxData[i];
                uint16_t val16 = RxData[i] | (RxData[i+1] << 8);

                switch (key) {
                    // CFG-RATE-MEAS (0x30210001) - measurement period in ms
                    case 0x30210001:
                        if (val16 >= 50 && val16 <= 10000) {
                            xTimerChangePeriod(TTimer_10hz, pdMS_TO_TICKS(val16), 0);
                        }
                        break;

                    // CFG-MSGOUT-UBX_NAV_PVT_UART1 (0x20910007)
                    case 0x20910007: UBX_NAV_PVT_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_POSECEF_UART1 (0x2091000D)
                    case 0x2091000D: UBX_NAV_POSECEF_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_POSLLH_UART1 (0x2091000F)
                    case 0x2091000F: UBX_NAV_POSLLH_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_STATUS_UART1 (0x2091001F)
                    case 0x2091001F: UBX_NAV_STATUS_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_DOP_UART1 (0x2091001B)
                    case 0x2091001B: UBX_NAV_DOP_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_VELNED_UART1 (0x20910024)
                    case 0x20910024: UBX_NAV_VELNED_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_TIMEUTC_UART1 (0x20910021)
                    case 0x20910021: UBX_NAV_TIMEUTC_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_CLOCK_UART1 (0x20910016)
                    case 0x20910016: UBX_NAV_CLOCK_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_SAT_UART1 (0x20910022)
                    case 0x20910022: UBX_NAV_SAT_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_AOPSTATUS_UART1 (0x20910039)
                    case 0x20910039: UBX_NAV_AOPSTATUS_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_RXM_RAWX_UART1 (0x2091007A)
                    case 0x2091007A: UBX_RXM_RAWX_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_TIM_TP_UART1 (0x209102A5)
                    case 0x209102A5: timepulse_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_MON_COMMS_UART1 (0x20910232)
                    case 0x20910232: UBX_MON_COMMS_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_MON_HW_UART1 (0x2091035A)
                    case 0x2091035A: UBX_MON_HW_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_MON_RF_UART1 (0x2091034B)
                    case 0x2091034B: UBX_MON_RF_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_SEC_SIG_UART1 (0x2091017E) - SEC-SIGN is always on
                    case 0x2091017E: /* SEC-SIGN always enabled */ break;

                    // New M10 messages
                    // CFG-MSGOUT-UBX_NAV_TIMEGPS_UART1 (0x20910025)
                    case 0x20910025: UBX_NAV_TIMEGPS_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_VELECEF_UART1 (0x2091002A)
                    case 0x2091002A: UBX_NAV_VELECEF_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_TIMELS_UART1 (0x2091005C)
                    case 0x2091005C: UBX_NAV_TIMELS_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_COV_UART1 (0x20910066)
                    case 0x20910066: UBX_NAV_COV_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_HPPOSECEF_UART1 (0x20910043)
                    case 0x20910043: UBX_NAV_HPPOSECEF_fl = (val8 > 0); break;
                    // CFG-MSGOUT-UBX_NAV_EOE_UART1 (0x2091003E)
                    case 0x2091003E: UBX_NAV_EOE_fl = (val8 > 0); break;

                    default:
                        break;
                }
                i += val_size;
            } else {
                break;
            }
        }

        // Send ACK for CFG-VALSET
        busy_wait_us(350);
        uart_write_blocking(uart0, UBX_CFG_VALSET, sizeof(UBX_CFG_VALSET));
        otvet++;

        // Start message output after 1 second delay (if not already started)
        if (!msg_output_started) {
            xTimerStart(TTimer_msg_start, 0);
        }
    }

    // UBX-SEC-UNIQID poll (0x27 0x03) - respond with unique ID
    if (count >= 8 && RxData[2] == 0x27 && RxData[3] == 0x03) {
        uint16_t payload_len = RxData[4] | (RxData[5] << 8);
        if (payload_len == 0) {  // Poll request (no payload)
            busy_wait_us(350);
            uart_write_blocking(uart0, UBX_SEC_UNIQID, sizeof(UBX_SEC_UNIQID));
            otvet++;
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

    // Initialize SEC-SIGN (always enabled, first at 3s, then every 4s)
    sha256_init(&sec_sign_sha256_ctx);

    // Create FreeRTOS timers
    TTimer_1hz = xTimerCreate("1Hz", Timer_1hz, pdTRUE, 0, callback_1hz);
    TTimer_10hz = xTimerCreate("10Hz", Timer_10hz, pdTRUE, 0, callback_10hz);
    // One-shot timer for first SEC-SIGN at 3 seconds
    TTimer_first_sec_sign = xTimerCreate("FirstSecSign", Timer_3sec, pdFALSE, 0, callback_first_sec_sign);
    // Periodic timer for subsequent SEC-SIGN every 4 seconds
    TTimer_4sec = xTimerCreate("4sec", Timer_4sec, pdTRUE, 0, callback_4sec);
    // One-shot timer to start message output 1 second after first CFG-MSG
    TTimer_msg_start = xTimerCreate("MsgStart", pdMS_TO_TICKS(1000), pdFALSE, 0, callback_msg_start);

    // Start timers
    xTimerStart(TTimer_1hz, 0);
    xTimerStart(TTimer_10hz, 10);
    xTimerStart(TTimer_first_sec_sign, 0);  // First SEC-SIGN at 3s, then periodic 4s

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
