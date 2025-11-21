#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include <inttypes.h>
#include "pico/multicore.h"
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "timers.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/flash.h"
#include "pico/flash.h"
#include "uart_rx.pio.h"
#include "uart_tx.pio.h"
#include "hardware/clocks.h"
#include "generated/ws2812.pio.h"
#include <stdlib.h>
#include "pico/util/queue.h"
#include <string.h>
#include "massivs.h"
#include "sha256.h"
// Для подписи secp192r1 можно подключить mbedtls, но локальные библиотеки под ARM отсутствуют
//#include "mbedtls/sha256.h"
//#include <openssl/evp.h>


// Адрес начала пользовательской секции
#define CUSTOM_SECTION_START 0x10040000
#define FLASH_TARGET_OFFSET (256 * 1024)

// Переменные, размещённые в .my_section
uint32_t my_variable1 __attribute__((section(".my_section"))) = 0x00006400;
volatile uint8_t regim;
const uint32_t addr_offset = (CUSTOM_SECTION_START - XIP_BASE);
volatile uint8_t data[FLASH_PAGE_SIZE];

#define Timer_10hz pdMS_TO_TICKS( 100 )
#define Timer_1hz pdMS_TO_TICKS( 1000 )
#define timer_sign pdMS_TO_TICKS( 1000 )
#define Timer_15_sec pdMS_TO_TICKS( 15000 )
TimerHandle_t NavRateTimer, MeasRateTimer, m10_timer_meas, m10_timer_nav, m10_timer_sign, timer_15sec;
TaskHandle_t uartna;
SemaphoreHandle_t gpio_button_sem;
SemaphoreHandle_t uart_mutex;
 
// Контекст SHA-256
//SHA256_CTX ctx;
//EVP_MD_CTX *ctx;
//uint8_t hash[32];
// ------------------------------
void on_uart_rx0();
void setup_uart0();
void off_uart0();
void on_uart_rx0_for_sem();
//uintptr_t params[2];
PIO pio, pio2, pio3;
uint sm, sm2, sm3, offset_sk, offset2, offset3 ;
uint skorost_uart_0 = 115200;
volatile uint8_t r , g, b ;
uint16_t MeasRate = 1000, NavRate;

// Функция записи во флэш ----------------------------------------------------
static void program_erase(void *param) {
        uint32_t adroffset = ((uintptr_t*)param)[0];
        flash_range_erase(adroffset, FLASH_SECTOR_SIZE);
}
static void program_flash(void *param) {
        uint32_t offset = ((uintptr_t*)param)[0];
        const uint8_t *datas = (const uint8_t *)((uintptr_t*)param)[1];
        flash_range_program(offset, datas, FLASH_PAGE_SIZE);
}


// ----------------------------------------------------------------------------
// светодиодная хрень
static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

volatile int count = 0;

void on_uart_rx1() {}

void CRC_gen(uint8_t *, int); //вычисление контрольной суммы юблокс сообщений
void CRC_gen(uint8_t *adr, int razmer)
{
        uint8_t CK_A, CK_B;
        CK_A = 0x0;
        CK_B = 0x0;
        int N = razmer - 2;
        int i;
        for(i=2; i<N; i++)
        {
            CK_A = CK_A + adr[i];
            CK_B = CK_B + CK_A;
        }
        adr[N] = CK_A;
        adr[N+1] = CK_B;
}

void secunda(){
    uint32_t val, val2;
    val = UBX_NAV_PVT[9] << 24 | UBX_NAV_PVT[8] << 16 | UBX_NAV_PVT[7] << 8 | UBX_NAV_PVT[6];
    val = val + MeasRate;
    val2 = val + 20;
    UBX_NAV_SVINFO[9] = UBX_NAV_PVT[9] = (val>>24); UBX_NAV_SVINFO[8] = UBX_NAV_PVT[8] = (val>>16);
    UBX_NAV_SVINFO[7] = UBX_NAV_PVT[7] = (val>>8); UBX_NAV_SVINFO[6] = UBX_NAV_PVT[6] = val;
    Timepulse[6] = UBX_NAV_PVT[6]; Timepulse[7] = UBX_NAV_PVT[7]; Timepulse[8] = UBX_NAV_PVT[8]; Timepulse[9] = UBX_NAV_PVT[9];
    UBX_NAV_POSECEF[6] = UBX_NAV_PVT[6]; UBX_NAV_POSECEF[7] = UBX_NAV_PVT[7]; UBX_NAV_POSECEF[8] = UBX_NAV_PVT[8]; UBX_NAV_POSECEF[9] = UBX_NAV_PVT[9];
    UBX_NAV_POSLLH[6] = UBX_NAV_PVT[6]; UBX_NAV_POSLLH[7] = UBX_NAV_PVT[7]; UBX_NAV_POSLLH[8] = UBX_NAV_PVT[8]; UBX_NAV_POSLLH[9] = UBX_NAV_PVT[9];
    UBX_NAV_TIMEUTC[6] = UBX_NAV_PVT[6]; UBX_NAV_TIMEUTC[7] = UBX_NAV_PVT[7]; UBX_NAV_TIMEUTC[8] = UBX_NAV_PVT[8]; UBX_NAV_TIMEUTC[9] = UBX_NAV_PVT[9];
    UBX_NAV_VELNED[6] = UBX_NAV_PVT[6]; UBX_NAV_VELNED[7] = UBX_NAV_PVT[7]; UBX_NAV_VELNED[8] = UBX_NAV_PVT[8]; UBX_NAV_VELNED[9] = UBX_NAV_PVT[9];
    UBX_NAV_SAT[6] = UBX_NAV_PVT[6]; UBX_NAV_SAT[7] = UBX_NAV_PVT[7]; UBX_NAV_SAT[8] = UBX_NAV_PVT[8]; UBX_NAV_SAT[9] = UBX_NAV_PVT[9];
    UBX_NAV_DOP[6] = UBX_NAV_PVT[6]; UBX_NAV_DOP[7] = UBX_NAV_PVT[7]; UBX_NAV_DOP[8] = UBX_NAV_PVT[8]; UBX_NAV_DOP[9] = UBX_NAV_PVT[9];
    UBX_NAV_STATUS[6] = UBX_NAV_PVT[6]; UBX_NAV_STATUS[7] = UBX_NAV_PVT[7]; UBX_NAV_STATUS[8] = UBX_NAV_PVT[8]; UBX_NAV_STATUS[9] = UBX_NAV_PVT[9];
    UBX_NAV_CLOCK[6] = UBX_NAV_PVT[6]; UBX_NAV_CLOCK[7] = UBX_NAV_PVT[7]; UBX_NAV_CLOCK[8] = UBX_NAV_PVT[8]; UBX_NAV_CLOCK[9] = UBX_NAV_PVT[9];
    UBX_NAV_AOPSTATUS[6] = UBX_NAV_PVT[6]; UBX_NAV_AOPSTATUS[7] = UBX_NAV_PVT[7]; UBX_NAV_AOPSTATUS[8] = UBX_NAV_PVT[8]; UBX_NAV_AOPSTATUS[9] = UBX_NAV_PVT[9];
    UBX_NAV_STATUS[21] = (val2>>24); UBX_NAV_STATUS[20] = (val2>>16); UBX_NAV_STATUS[19] = (val2>>8); UBX_NAV_STATUS[18] = val2; 
    CRC_gen(UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
    CRC_gen(Timepulse, sizeof(Timepulse));
    CRC_gen(UBX_NAV_SVINFO, sizeof(UBX_NAV_SVINFO));
    CRC_gen(UBX_NAV_POSECEF, sizeof(UBX_NAV_POSECEF));
    CRC_gen(UBX_NAV_POSLLH, sizeof(UBX_NAV_POSLLH));
    CRC_gen(UBX_NAV_TIMEUTC, sizeof(UBX_NAV_TIMEUTC));
    CRC_gen(UBX_NAV_VELNED, sizeof(UBX_NAV_VELNED));
    CRC_gen(UBX_NAV_SAT, sizeof(UBX_NAV_SAT));
    CRC_gen(UBX_NAV_DOP, sizeof(UBX_NAV_DOP));
    CRC_gen(UBX_NAV_STATUS, sizeof(UBX_NAV_STATUS));
    CRC_gen(UBX_NAV_CLOCK, sizeof(UBX_NAV_CLOCK));
    CRC_gen(UBX_NAV_AOPSTATUS, sizeof(UBX_NAV_AOPSTATUS));
    }

void secunda2(){
    UBX_NAV_PVT[16] = UBX_NAV_PVT[16] + 0x01;
    if (UBX_NAV_PVT[16] == 0x3C)
    {   UBX_NAV_PVT[16] = 0x00;
        UBX_NAV_PVT[15] = UBX_NAV_PVT[15] + 0x01;
    if (UBX_NAV_PVT[15] == 0x3C)
    {   UBX_NAV_PVT[15] = 0x00;
        UBX_NAV_PVT[14] = UBX_NAV_PVT[14] + 0x01;
    }
    CRC_gen(UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
    }    
}


volatile int8_t flag = 0;
volatile bool s0107 = 0;
volatile bool s0130 = 0;
volatile bool s0d01 = 0, s0a09 = 0, s0101 = 0, s0102 = 0, s0121 = 0, s0112 = 0, s0104 = 0, s0103 = 0, s0122 = 0, s0135 = 0, s0a38 = 0, s0160 = 0;

void callback_1hz( TimerHandle_t xTimer ){
        flag++;
        secunda2();
        if (s0130) {
                if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        uart_write_blocking(uart0, UBX_NAV_SVINFO, sizeof(UBX_NAV_SVINFO));
                        xSemaphoreGive(uart_mutex);
                }
        }
        flag--;
}

void callback_10hz( TimerHandle_t xTimer ){
        flag++;
        secunda();
        if (s0107) {
                if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        uart_write_blocking(uart0, UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
                        xSemaphoreGive(uart_mutex);
                }
        }
        flag--;
}

void callback_m10_timer_meas( TimerHandle_t xTimer ){
        
}

int8_t count_uart_mes_for_sha256 = 0;
void callback_m10_timer_sign( TimerHandle_t xTimer ){
        // 1) Завершаем текущий SHA-256 и сохраняем его
        sha256_final();
        uint8_t primary_hash[32];
        memcpy(primary_hash, hash, sizeof(primary_hash));

        // 2) Хешируем (primary_hash + session_id) -> вторичный SHA-256
        sha256_cleanup();
        sha256_init();
        sha256_update(primary_hash, sizeof(primary_hash));
        sha256_update(SEC_SESSION_ID, sizeof(SEC_SESSION_ID));
        sha256_final(); // теперь hash содержит вторичное значение

        // 3) Сворачиваем 32 байта в 24 байта
        uint8_t folded[24];
        memcpy(folded, hash, 24);
        for (int i = 0; i < 8; i++) {
                folded[i] ^= hash[24 + i];
        }

        // 4) Формируем сообщение для подписи: folded (24) + session_id (24)
        uint8_t sign_input[48];
        memcpy(sign_input, folded, 24);
        memcpy(sign_input + 24, SEC_SESSION_ID, 24);

        // 5) Подписываем secp192r1 — пока заглушка (R,S будут нулевые, заполните SEC_PRIV_KEY и подключите mbedtls)
        uint8_t r_sig[24] = {0};
        uint8_t s_sig[24] = {0};
        // TODO: подключить mbedtls и вычислять ECDSA по folded+session с ключом SEC_PRIV_KEY

        // 6) Собираем SEC_ECSIGN: первичный hash, session_id, R,S
        memcpy(&SEC_ECSIGN[10], primary_hash, 32);
        memcpy(&SEC_ECSIGN[42], SEC_SESSION_ID, 24);
        memcpy(&SEC_ECSIGN[66], r_sig, 24);
        memcpy(&SEC_ECSIGN[90], s_sig, 24);
        SEC_ECSIGN[8] = count_uart_mes_for_sha256;
        count_uart_mes_for_sha256 = 0;

        CRC_gen(SEC_ECSIGN, sizeof(SEC_ECSIGN));
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                uart_write_blocking(uart0, SEC_ECSIGN, sizeof(SEC_ECSIGN));
                xSemaphoreGive(uart_mutex);
        }

        // 7) Готовимся к следующему циклу
        sha256_cleanup();
        sha256_init();
}
void callback_m10_timer_nav( TimerHandle_t xTimer ){
        flag++;
        secunda();

        // Take mutex for UART access
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (s0a38) {uart_write_blocking(uart0, UBX_MON_RF, sizeof(UBX_MON_RF)); count_uart_mes_for_sha256++;
                        sha256_update(UBX_MON_RF, sizeof(UBX_MON_RF));};
                if (s0107) {uart_write_blocking(uart0, UBX_NAV_PVT, sizeof(UBX_NAV_PVT)); count_uart_mes_for_sha256++;
                        sha256_update(UBX_NAV_PVT, sizeof(UBX_NAV_PVT));};
                if (s0101) {uart_write_blocking(uart0, UBX_NAV_POSECEF, sizeof(UBX_NAV_POSECEF)); count_uart_mes_for_sha256++;
                        sha256_update(UBX_NAV_POSECEF, sizeof(UBX_NAV_POSECEF));};
                if (s0102) {uart_write_blocking(uart0, UBX_NAV_POSLLH, sizeof(UBX_NAV_POSLLH)); count_uart_mes_for_sha256++;
                        sha256_update(UBX_NAV_POSLLH, sizeof(UBX_NAV_POSLLH));};
                if (s0112) {uart_write_blocking(uart0, UBX_NAV_VELNED, sizeof(UBX_NAV_VELNED)); count_uart_mes_for_sha256++;
                        sha256_update(UBX_NAV_VELNED, sizeof(UBX_NAV_VELNED));};
                if (s0135) {uart_write_blocking(uart0, UBX_NAV_SAT, sizeof(UBX_NAV_SAT)); count_uart_mes_for_sha256++;
                        sha256_update(UBX_NAV_SAT, sizeof(UBX_NAV_SAT));};
                if (s0103) {uart_write_blocking(uart0, UBX_NAV_STATUS, sizeof(UBX_NAV_STATUS)); count_uart_mes_for_sha256++;
                        sha256_update(UBX_NAV_STATUS, sizeof(UBX_NAV_STATUS));};
                if (s0104) {uart_write_blocking(uart0, UBX_NAV_DOP, sizeof(UBX_NAV_DOP)); count_uart_mes_for_sha256++;
                        sha256_update(UBX_NAV_DOP, sizeof(UBX_NAV_DOP));};
                if (s0121) {uart_write_blocking(uart0, UBX_NAV_TIMEUTC, sizeof(UBX_NAV_TIMEUTC)); count_uart_mes_for_sha256++;
                        sha256_update(UBX_NAV_TIMEUTC, sizeof(UBX_NAV_TIMEUTC));};
                if (s0122) {uart_write_blocking(uart0, UBX_NAV_CLOCK, sizeof(UBX_NAV_CLOCK)); count_uart_mes_for_sha256++;
                        sha256_update(UBX_NAV_CLOCK, sizeof(UBX_NAV_CLOCK));};
                if (s0160) {uart_write_blocking(uart0, UBX_NAV_AOPSTATUS, sizeof(UBX_NAV_AOPSTATUS)); count_uart_mes_for_sha256++;
                        sha256_update(UBX_NAV_AOPSTATUS, sizeof(UBX_NAV_AOPSTATUS));};
                if (s0d01) {uart_write_blocking(uart0, Timepulse, sizeof(Timepulse)); count_uart_mes_for_sha256++;
                        sha256_update(Timepulse, sizeof(Timepulse));};
                xSemaphoreGive(uart_mutex);
        }
        flag--;
}
/* Обработка нажатия кнопки здесь*/
volatile bool flag_gpio6 = 1;
uintptr_t params[] = { FLASH_TARGET_OFFSET, (uintptr_t)data};
/*
// Этот код написан нейросеткой для обработки дребезга кнопки( надо попробовать)
#define DEBOUNCE_DELAY_MS 100
volatile uint32_t last_button_time = 0;
void gpio_6_on(void) {
    uint32_t current_time = time_us_32();
    
    // Проверка на дребезг
    if ((current_time - last_button_time) < (DEBOUNCE_DELAY_MS * 1000)) {
        return;  // Выход, если прошло меньше времени чем задержка дебаунса
    }
    last_button_time = current_time;

    // Основная логика обработки
    PIO pio_sk = pio2;
    uint sm_sk = sm2;

    if (flag_gpio6 == 1) {
        r = 0;
        g = 0;
        b = 100;
        
        off_uart0();
        gpio_set_function(0, PIO_FUNCSEL_NUM(pio2, 0));
        pio_sm_set_enabled(pio_sk, sm_sk, true);
        flag_gpio6 = 0;
        
        program_erase(params);
        data[3] = r;
        data[2] = g;
        data[1] = b;
        data[0] = regim = 1;
        program_flash(params);
    } else {
        r = 0;
        g = 100;
        b = 0;
        
        pio_sm_set_enabled(pio_sk, sm_sk, false);
        flag_gpio6 = 1;
        setup_uart0();
        
        program_erase(params);
        data[3] = r;
        data[2] = g;
        data[1] = b;
        data[0] = regim = 0;
        program_flash(params);
    }
}
*/

// ISR for button press - just signals the task
void gpio_6_on() {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(gpio_button_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Task to handle button press with flash operations
void gpio_button_task(void *pvParameters) {
        PIO pio_sk = pio2;
        uint sm_sk = sm2;

        for(;;) {
                // Wait for button press signal
                if (xSemaphoreTake(gpio_button_sem, portMAX_DELAY) == pdTRUE) {
                        // Debounce delay
                        vTaskDelay(pdMS_TO_TICKS(200));

                        gpio_put(5, 0);

                        if (flag_gpio6 == 1) {
                                r = 0; g = 0; b = 100;
                                off_uart0();
                                gpio_set_function(0, PIO_FUNCSEL_NUM(pio2,0));
                                pio_sm_set_enabled(pio_sk, sm_sk, true);
                                flag_gpio6 = 0;

                                program_erase(params);
                                data[3] = r; data[2] = g; data[1] = b; data[0] = regim = 1;
                                program_flash(params);
                        } else {
                                r = 0; g = 100; b = 0;
                                pio_sm_set_enabled(pio_sk, sm_sk, false);
                                flag_gpio6 = 1;
                                setup_uart0();

                                program_erase(params);
                                data[3] = r; data[2] = g; data[1] = b; data[0] = regim = 0;
                                program_flash(params);
                        }

                        gpio_put(5, 1);
                }
        }
}

/* Светодиод моргает здесь*/
void Led_green_blink(void *pvParameters) {
    TickType_t xLastWakeTime; 
    xLastWakeTime = xTaskGetTickCount();
    for( ;; ){
        put_pixel(pio, sm, urgb_u32(r, g, b));
        vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( 500 ) );
        put_pixel(pio, sm, urgb_u32(0, 0, 0));
        vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( 500 ) );
        }
}
// Задача выполняется каждую секунду и прибавляет 1 секунду в счётчик времени сообщения -------------------
void SecundaTaskCRC(void *pvParameters) {
    TickType_t xLastWakeTime2; 
    xLastWakeTime2 = xTaskGetTickCount();
    for( ;; ){
        UBX_NAV_PVT[16] = UBX_NAV_PVT[16] + 0x01;
        UBX_NAV_TIMEUTC[24] = UBX_NAV_PVT[16];
    if (UBX_NAV_PVT[16] == 0x3C)
    {   UBX_NAV_PVT[16] = 0x00;
        UBX_NAV_PVT[15] = UBX_NAV_PVT[15] + 0x01;
        UBX_NAV_TIMEUTC[23] = UBX_NAV_PVT[15];
    if (UBX_NAV_PVT[15] == 0x3C)
    {   UBX_NAV_PVT[15] = 0x00;
        UBX_NAV_PVT[14] = UBX_NAV_PVT[14] + 0x01;
        UBX_NAV_TIMEUTC[22] = UBX_NAV_PVT[14];
    }
       
    CRC_gen(UBX_NAV_TIMEUTC, sizeof(UBX_NAV_TIMEUTC));
    CRC_gen(UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
    } 
    vTaskDelayUntil( &xLastWakeTime2, pdMS_TO_TICKS( 1000 ) );   
        }
}
//------------------------------------------------------------------------------------------------------------
void setup_uart0(){
        gpio_set_function(0, UART_FUNCSEL_NUM(uart0, 0));
        gpio_set_function(1, UART_FUNCSEL_NUM(uart0, 1));
        uart_init(uart0, skorost_uart_0);
        uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
        irq_set_exclusive_handler(UART0_IRQ, on_uart_rx0);
        irq_set_enabled(UART0_IRQ, true);
        uart_set_irq_enables(uart0, true, false);
        uart_set_fifo_enabled(uart0, false);
}
// Выключение передачи через 15 секунд после включения контроллера
void disable_15sec(){
        xTimerStop(NavRateTimer,0); xTimerStop(MeasRateTimer,0);
        xTimerStop(m10_timer_sign,0); xTimerStop(m10_timer_nav,0);xTimerStop(m10_timer_meas,0);
}
// ------------------------------------------------------------------------------------------------------------
void off_uart0(){
        irq_set_enabled(UART0_IRQ, false);
        irq_remove_handler(UART0_IRQ, on_uart_rx0);
        uart_deinit(uart0);
}

int main() {
     //   stdio_init_all();
        set_sys_clock_khz(133000, true);
        
        // Светодиодная программа на пин 16
        uint offset;
        pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, 16, 1, true);
        ws2812_program_init(pio, sm, offset, 16, 800000, false);
        // -------------------------------------------------------------------------------------------------
        // Программа на пины для сквозного uart
        pio_claim_free_sm_and_add_program_for_gpio_range(&uart_rx_mini_program, &pio2, &sm2, &offset2, 3, 0, true); 
        uart_rx_mini_program_init(pio2, sm2, offset2, 3, 0, 3000000);

        gpio_init(5); gpio_init(6);
        gpio_set_dir(5, GPIO_OUT); gpio_set_dir(6, GPIO_IN); 
        gpio_put(5, 1);
        gpio_set_irq_enabled_with_callback(6, GPIO_IRQ_LEVEL_HIGH , true, &gpio_6_on);
        gpio_get_slew_rate(6); 
        // Вычисление контрольной суммы для массивов
        CRC_gen(UBX_NAV_PVT, sizeof(UBX_NAV_PVT));
        CRC_gen(UBX_NAV_SVINFO, sizeof(UBX_NAV_SVINFO));
        CRC_gen(Timepulse, sizeof(Timepulse));
        CRC_gen(UBX_NAV_POSECEF, sizeof(UBX_NAV_POSECEF));
        CRC_gen(UBX_NAV_POSLLH, sizeof(UBX_NAV_POSLLH));
        // -----------------------------------------
        // считываем из области памяти записанные значения в переменные --------------------------------------
        uint8_t regim_iz_pamati[4];
        memcpy(regim_iz_pamati, (const void *)CUSTOM_SECTION_START, sizeof(regim_iz_pamati));
        r = regim_iz_pamati[3];
        g = regim_iz_pamati[2];
        b = regim_iz_pamati[1];
        regim = regim_iz_pamati[0];
        // ---------------------------------------------------------------------------------------------------
        if(regim==0){
                setup_uart0();
                }
        else{
                off_uart0();
                gpio_set_function(0, PIO_FUNCSEL_NUM(pio2,0));
                pio_sm_set_enabled(pio2, sm2, true);
                flag_gpio6 = 0;
        }
        
        TaskHandle_t Led_handle, sec_task, button_task;

        // Create semaphore for button handler
        gpio_button_sem = xSemaphoreCreateBinary();
        configASSERT(gpio_button_sem != NULL);

        // Create mutex for UART access protection
        uart_mutex = xSemaphoreCreateMutex();
        configASSERT(uart_mutex != NULL);

        MeasRateTimer = xTimerCreate("MeasRateTimer", Timer_1hz, pdTRUE, 0, callback_1hz);
        NavRateTimer = xTimerCreate("NavRateTimer", Timer_10hz, pdTRUE, 0, callback_10hz);
        m10_timer_meas = xTimerCreate("m10_timer_meas", Timer_10hz, pdTRUE, 0, callback_m10_timer_meas);
        m10_timer_nav = xTimerCreate("m10_timer_nav", Timer_10hz, pdTRUE, 0, callback_m10_timer_nav);
        m10_timer_sign = xTimerCreate("Таймер для подписи", timer_sign, pdTRUE, 0, callback_m10_timer_sign);
        timer_15sec = xTimerCreate("Таймер подмены на 15 сек", Timer_15_sec, pdFALSE, 0, disable_15sec);
        xTaskCreate( Led_green_blink, "Мигание", 256, NULL, 1, &Led_handle);
        xTaskCreate( SecundaTaskCRC, "Считаем секунду", 512, NULL, 5, &sec_task);
        xTaskCreate( gpio_button_task, "Обработка кнопки", 512, NULL, 3, &button_task);
        
        sha256_init();              // Инициализация контекста
        //ctx = EVP_MD_CTX_new();                                        
        //mbedtls_sha256_starts(&ctx, 0);     // 0 означает использование SHA-256

        xTimerStart( timer_15sec, 0 );
       // xTimerStart( NavRateTimer, 10 );
        if (xTimerIsTimerActive(NavRateTimer) != pdFALSE) {
        r = 255; g = 0; b = 0; put_pixel(pio, sm, urgb_u32(r, g, b)); }
        
        vTaskStartScheduler();
        /* As always, this line should not be reached. */
        put_pixel(pio, sm, urgb_u32(255, 100, 100));
    for( ;; );
}
volatile int otvet = 0;

void on_uart_rx0() {

        while(uart_is_readable_within_us(uart0, 300) == true) {
        uint8_t bait = uart_getc(uart0);
        if (count < sizeof(RxData)) {
                RxData[count] = bait;
                count++;
        } else {
                // Buffer overflow - reset counter
                count = 0;
                break;
        }
        }
        
        if (flag == 0 && count != 0 && RxData[2] == 0x0A && RxData[3] == 0x04) {     //monitor_ver
                busy_wait_us(100);
                uart_write_blocking(uart0, monitor_ver_M10, sizeof(monitor_ver_M10));
                otvet++;
        }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x00 && RxData[8] == 0x00) {
                //xTimerStop( MeasRateTimer, 10); xTimerStop( NavRate, 20);
                s0107 = false; s0130 = false;
               //uart_write_blocking(uart0, mes_1, sizeof(mes_1));
                otvet++;
        }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x00 && RxData[15] == 0x25) {       //9600
            skorost_uart_0 = 9600;
            uart_set_baudrate(uart0, skorost_uart_0);
            s0107 = false; s0130 = false; //иначе зависнет. длинные сообщения на такой скорости не успевают отправляться
            busy_wait_ms(1);
            uart_write_blocking(uart0, mes_1, sizeof(mes_1));
            otvet++;
        }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x00 && RxData[15] == 0xC2) {       //115200
            skorost_uart_0 = 115200;
            uart_write_blocking(uart0, mes_1, sizeof(mes_1));
            uart_set_baudrate(uart0, skorost_uart_0);
            busy_wait_ms(1);
            uart_write_blocking(uart0, mes_1, sizeof(mes_1));
            otvet++;
        }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x00 && RxData[15] == 0x08) {       //460800
            skorost_uart_0 = 460800;
            uart_write_blocking(uart0, mes_1, sizeof(mes_1));
            uart_set_baudrate(uart0, skorost_uart_0);
            busy_wait_ms(1);
            uart_write_blocking(uart0, mes_1, sizeof(mes_1));
            otvet++;
        }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x00 && RxData[16] == 0x0e) {       //921600
            skorost_uart_0 = 921600;
            uart_write_blocking(uart0, mes_1, sizeof(mes_1));
            uart_set_baudrate(uart0, skorost_uart_0);
            busy_wait_ms(1);
            uart_write_blocking(uart0, mes_1, sizeof(mes_1));
            otvet++;
        }

        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x08 && RxData[4] == 0x06) {        //rate
                        int16_t period = RxData[7]<<8 | RxData[6];
                        int16_t nav_ratio = RxData[9]<<8 | RxData[8];
                        xTimerChangePeriod(m10_timer_nav, pdMS_TO_TICKS( period*nav_ratio ), 0 );
                        xTimerChangePeriod(m10_timer_meas, pdMS_TO_TICKS( period ), 0 );;
                       // xTimerChangePeriod(NavRateTimer, pdMS_TO_TICKS( period ), 0 );
                        busy_wait_us(100);
                        uart_write_blocking(uart0, mes_2, sizeof(mes_2));
                        otvet++;
        }

        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x09) {       /*UBX-CFG-CFG*/
                busy_wait_us(100);
                uart_write_blocking(uart0, UBX_CFG_CFG, sizeof(UBX_CFG_CFG));
                otvet++;
                }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x24) {       /*UBX-CFG-NAV5*/
                busy_wait_us(350);
                uart_write_blocking(uart0, mes_3, sizeof(mes_3));
                otvet++;
                }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x23) {       /*UBX-CFG-NAVX5*/
                busy_wait_us(350);
                uart_write_blocking(uart0, mes_4, sizeof(mes_4));
                otvet++;
                }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x3E) {       /*UBX-CFG-GNSS*/
                busy_wait_us(350);
                uart_write_blocking(uart0, mes_5, sizeof(mes_5));
                otvet++;
                }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x86) {       /*UBX-CFG-PMS*/
                busy_wait_us(350);
                uart_write_blocking(uart0, mes_6, sizeof(mes_6));
                otvet++;
                }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x01) {       /*UBX-CFG-MSG*/
                busy_wait_us(100);
                uart_write_blocking(uart0, mes_7, sizeof(mes_7));
            otvet++;
                if (RxData[6] == 0x01 && RxData[7] == 0x07 && RxData[8] > 0) { s0107 = true; 
                       // xTimerChangePeriod(NavRateTimer, pdMS_TO_TICKS( 1000/RxData[8] ), 0 );
                }
                if (RxData[6] == 0x01 && RxData[7] == 0x30 && RxData[8] > 0) { s0130 = true; 
                       // xTimerChangePeriod(MeasRateTimer, pdMS_TO_TICKS( 1000/RxData[8] ), 0 );
                }
        }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x8A) {       /* Ответы на все сообщения настроек 06 8A */
                busy_wait_us(50);
                uart_write_blocking(uart0, UBX_CFG_VALSET, sizeof(UBX_CFG_VALSET));
            otvet++;
            // настройка периода решений и измерений ---------------------------------------------------
                uint32_t Key_ID_1, Key_ID_2;
                Key_ID_1 = RxData[10] | (RxData[11]<<8) | (RxData[12]<<16) | (RxData[13]<<24);
                Key_ID_2 = RxData[16] | (RxData[17]<<8) | (RxData[18]<<16) | (RxData[19]<<24);
                
                MeasRate = RxData[14] | (RxData[15]<<8);
                NavRate = RxData[20] | (RxData[21]<<8);
                if (Key_ID_1 == 0x30210001) {
                        xTimerChangePeriod(m10_timer_nav, pdMS_TO_TICKS( MeasRate*NavRate ), 0 );
                        //xTimerStop(m10_timer_nav,0);
                }
                if (Key_ID_2 == 0x30210002) { 
                        xTimerChangePeriod(m10_timer_meas, pdMS_TO_TICKS( MeasRate ), 0 );
                        //xTimerStop(m10_timer_meas,0);
                }
            // ----------------------------------------------------------------------------------------   
                if (Key_ID_1 == 0x2091017e && RxData[14] == 1) { s0d01 = 1; } //timepulse
                if (Key_ID_1 == 0x20910025 && RxData[14] == 1) { s0101 = 1; } //POSECEF
                if (Key_ID_1 == 0x2091002a && RxData[14] == 1) { s0102 = 1; } //POSLLH
                if (Key_ID_1 == 0x2091001b && RxData[14] == 1) { s0103 = 1; } //STATUS
                if (Key_ID_1 == 0x20910039 && RxData[14] == 1) { s0104 = 1; } //DOP
                if (Key_ID_1 == 0x20910007 && RxData[14] == 1) { s0107 = 1; } //PVT
                if (Key_ID_1 == 0x20910043 && RxData[14] == 1) { s0112 = 1; } //VELNED
                if (Key_ID_1 == 0x2091005c && RxData[14] == 1) { s0121 = 1; } //TIMEUTC
                if (Key_ID_1 == 0x20910066 && RxData[14] == 1) { s0122 = 1; } //CLOCK
                if (Key_ID_1 == 0x2091035a && RxData[14] == 1) { s0a38 = 1; } //MON_RF
                if (Key_ID_1 == 0x20910016 && RxData[14] == 1) { s0135 = 1; } //NAV_SAT
                if (Key_ID_1 == 0x2091007a && RxData[14] == 1) { s0160 = 1; } //AOPSTATUS
                

        }
        if (count != 0 && RxData[2] == 0x27 && RxData[3] == 0x03) {                               /* UBX-SEC-UNIQID (0x27 0x03) отсылаем чип айди*/
                busy_wait_us(100);
                uart_write_blocking(uart0, UBX_SEC_UNIQID_M10, sizeof(UBX_SEC_UNIQID_M10));
                count_uart_mes_for_sha256++;
        }
        if (count != 0 && RxData[2] == 0x06 && RxData[3] == 0x04) {                               /* Reset. запускаем все сообщения */
                count_uart_mes_for_sha256 = 0;
                xTimerStart( m10_timer_sign, pdMS_TO_TICKS( 50 ) );
                xTimerStop(NavRateTimer,0); xTimerStop(MeasRateTimer,0);
                xTimerStart( m10_timer_nav, 0 );xTimerStart( m10_timer_meas, 0 );
        }
        if (count != 0 && count != 0 && RxData[2] == 0x13 && RxData[3] == 0x80) {                 /*UBX-MGA-DBD*/
                busy_wait_us(50);
                UBX_MGA_ACK[9] = RxData[3];
                UBX_MGA_ACK[10] = RxData[6];
                UBX_MGA_ACK[11] = RxData[7];
                UBX_MGA_ACK[12] = RxData[8];
                UBX_MGA_ACK[13] = RxData[9];
                CRC_gen(UBX_MGA_ACK, sizeof(UBX_MGA_ACK));
                uart_write_blocking(uart0, UBX_MGA_ACK, sizeof(UBX_MGA_ACK));
                otvet++;
                }

        count = 0;

}
void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
    configASSERT( NULL );
}
