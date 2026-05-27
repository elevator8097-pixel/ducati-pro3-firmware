/**
 * display.c — Керування дисплеєм MTW-7349CGW
 * Підключення: роз'єм BAT TX RX EN G
 * UART 9600bps, 8N1
 *
 * Пакет контролера → дисплей (кожні 100мс):
 * [0x55][0xAA][speed][bat_bars][mode][flags][odo_hi][odo_lo][xor]
 */

#include "display.h"
#include "gd32f10x.h"

#define DISPLAY_UART        USART1
#define DISPLAY_BAUD        9600
#define DISPLAY_SYNC1       0x55
#define DISPLAY_SYNC2       0xAA
#define DISPLAY_PACKET_LEN  9
#define DISPLAY_UPDATE_MS   100

static uint32_t odometer_km   = 0;
static uint32_t last_update   = 0;
static uint8_t  current_mode  = 1;   // 0=ECO, 1=D, 2=S+
static uint8_t  light_on      = 0;

static void uart_send_byte(uint8_t b);
static uint8_t calc_checksum(uint8_t *data, uint8_t len);
static uint8_t voltage_to_bars(uint16_t mv);

void display_init(void)
{
    rcu_periph_clock_enable(RCU_USART1);
    rcu_periph_clock_enable(RCU_GPIOA);

    gpio_init(GPIOA, GPIO_MODE_AF_PP,    GPIO_OSPEED_50MHZ, GPIO_PIN_2); // TX
    gpio_init(GPIOA, GPIO_MODE_IN_FLOAT, GPIO_OSPEED_50MHZ, GPIO_PIN_3); // RX

    usart_deinit(USART1);
    usart_baudrate_set(USART1, DISPLAY_BAUD);
    usart_word_length_set(USART1, USART_WL_8BIT);
    usart_stop_bit_set(USART1, USART_STB_1BIT);
    usart_parity_config(USART1, USART_PM_NONE);
    usart_hardware_flow_rts_config(USART1, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(USART1, USART_CTS_DISABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_enable(USART1);
}

void display_update(uint8_t speed_kmh, uint16_t bat_mv,
                    uint8_t is_running, uint32_t tick_ms)
{
    if ((tick_ms - last_update) < DISPLAY_UPDATE_MS) return;
    last_update = tick_ms;

    /* Одометр */
    static uint32_t last_odo_tick = 0;
    static uint32_t partial_m = 0;
    if (is_running && speed_kmh > 0) {
        uint32_t dt_ms = tick_ms - last_odo_tick;
        partial_m += ((uint32_t)speed_kmh * dt_ms) / 3600;
        if (partial_m >= 1000) {
            odometer_km += partial_m / 1000;
            partial_m   %= 1000;
        }
    }
    last_odo_tick = tick_ms;

    uint8_t pkt[DISPLAY_PACKET_LEN];
    pkt[0] = DISPLAY_SYNC1;
    pkt[1] = DISPLAY_SYNC2;
    pkt[2] = speed_kmh;
    pkt[3] = voltage_to_bars(bat_mv);
    pkt[4] = current_mode;
    pkt[5] = light_on ? 0x01 : 0x00;
    pkt[6] = (uint8_t)((odometer_km >> 8) & 0xFF);
    pkt[7] = (uint8_t)(odometer_km & 0xFF);
    pkt[8] = calc_checksum(&pkt[2], 6);

    for (int i = 0; i < DISPLAY_PACKET_LEN; i++) uart_send_byte(pkt[i]);
}

DisplayButton_t display_read_button(void)
{
    if (usart_flag_get(USART1, USART_FLAG_RBNE) == RESET) return BTN_NONE;
    uint8_t b = (uint8_t)usart_data_receive(USART1);
    switch (b) {
        case 0x01: return BTN_POWER_SHORT;
        case 0x02: return BTN_POWER_LONG;
        case 0x03: return BTN_PLUS;
        case 0x04: return BTN_MINUS;
        case 0x05: return BTN_LIGHT;
        default:   return BTN_NONE;
    }
}

void display_next_mode(void) { current_mode = (current_mode + 1) % 3; }
uint8_t display_get_mode(void) { return current_mode; }
void display_toggle_light(void) { light_on = !light_on; }

static uint8_t voltage_to_bars(uint16_t mv)
{
    if (mv <= 32000) return 0;
    if (mv >= 42000) return 7;
    return (uint8_t)(((uint32_t)(mv - 32000) * 7) / 10000);
}

static void uart_send_byte(uint8_t b)
{
    while (usart_flag_get(USART1, USART_FLAG_TBE) == RESET);
    usart_data_transmit(USART1, b);
    while (usart_flag_get(USART1, USART_FLAG_TC) == RESET);
}

static uint8_t calc_checksum(uint8_t *data, uint8_t len)
{
    uint8_t cs = 0;
    for (uint8_t i = 0; i < len; i++) cs ^= data[i];
    return cs;
}
