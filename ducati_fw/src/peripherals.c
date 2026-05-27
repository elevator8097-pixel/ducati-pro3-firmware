/**
 * throttle.c — Читання газу через ADC (PA0)
 * Стандартний Hall-throttle: 0.8V–4.2V → 0–100%
 */

#include "throttle.h"
#include "gd32f10x.h"

/* ADC референс = 3.3V, 12-біт = 4095
 * 0.8V → ADC = 0.8/3.3 * 4095 = ~993
 * 4.2V → ADC = 4.2/3.3 * 4095 = ~5209 → обмежуємо 4095
 */
#define THROTTLE_ADC_MIN    993
#define THROTTLE_ADC_MAX    4095
#define THROTTLE_DEADBAND   50      // мертва зона на початку

void throttle_init(void)
{
    /* GPIO вже налаштовано в gpio_init як AIN на PA0 */
}

uint16_t throttle_read_raw(void)
{
    /* Вибираємо канал 0 (PA0) */
    adc_regular_channel_config(ADC0, 0, ADC_CHANNEL_0, ADC_SAMPLETIME_55POINT5);
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);

    /* Чекаємо завершення */
    while (!adc_flag_get(ADC0, ADC_FLAG_EOC));
    adc_flag_clear(ADC0, ADC_FLAG_EOC);

    return adc_regular_data_read(ADC0);
}

uint8_t throttle_to_percent(uint16_t raw)
{
    if (raw < THROTTLE_ADC_MIN + THROTTLE_DEADBAND) return 0;
    if (raw > THROTTLE_ADC_MAX) raw = THROTTLE_ADC_MAX;

    uint32_t span = THROTTLE_ADC_MAX - THROTTLE_ADC_MIN;
    uint32_t val  = raw - THROTTLE_ADC_MIN;
    uint8_t  pct  = (uint8_t)((val * 100) / span);

    if (pct > 100) pct = 100;
    return pct;
}

/* ─────────────────────────────────────────────── */

/**
 * brake.c — Читання гальма (PA2, активний LOW)
 * При натисканні гальма замикається на GND
 */

#include "brake.h"
#include "gd32f10x.h"

void brake_init(void)
{
    /* GPIO вже налаштовано як IPU (input pull-up) на PA2 */
}

uint8_t brake_is_active(void)
{
    /* Активний LOW: гальмо натиснуто = пін = 0 */
    return (gpio_input_bit_get(GPIOA, GPIO_PIN_2) == RESET) ? 1 : 0;
}

/* ─────────────────────────────────────────────── */

/**
 * battery.c — Читання напруги батареї через ADC (PA1)
 * Дільник напруги: 36V → 3.3V через резистори
 * Типовий дільник: R1=100кОм, R2=10кОм
 * V_bat = V_adc * (R1+R2)/R2 = V_adc * 11
 */

#include "battery.h"
#include "gd32f10x.h"

/* Коефіцієнт дільника напруги (підбирається по факту!) */
#define BAT_DIVIDER_MULT    11      // (100k+10k)/10k = 11
#define ADC_VREF_MV         3300    // Референс 3.3V

void battery_init(void)
{
    /* GPIO вже налаштовано як AIN на PA1 */
}

uint16_t battery_read_mv(void)
{
    /* Вибираємо канал 1 (PA1) */
    adc_regular_channel_config(ADC0, 0, ADC_CHANNEL_1, ADC_SAMPLETIME_55POINT5);
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);

    while (!adc_flag_get(ADC0, ADC_FLAG_EOC));
    adc_flag_clear(ADC0, ADC_FLAG_EOC);

    uint16_t raw = adc_regular_data_read(ADC0);

    /* V_adc в мілівольтах */
    uint32_t v_adc_mv = ((uint32_t)raw * ADC_VREF_MV) / 4095;

    /* Реальна напруга батареї */
    uint32_t v_bat_mv = v_adc_mv * BAT_DIVIDER_MULT;

    return (uint16_t)v_bat_mv;
}
