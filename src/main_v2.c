/**
 * Ducati PRO-III Firmware v0.2.0
 * + Підтримка дисплею MTW-7349CGW
 * + Розрахунок швидкості з датчиків Холла
 * + Режими ECO / D / S+
 * + Зміна режиму кнопкою на дисплеї
 */

#include "gd32f10x.h"
#include "motor.h"
#include "throttle.h"
#include "brake.h"
#include "battery.h"
#include "display.h"

/* ─── Ліміти швидкості по режимах (%) ────────── */
#define SPEED_LIMIT_ECO     40      // ECO: 40% потужності (~15 км/год)
#define SPEED_LIMIT_D       70      // D:   70% потужності (~20 км/год)
#define SPEED_LIMIT_SPLUS   95      // S+:  95% потужності (~25 км/год)

/* ─── Порог батареї ──────────────────────────── */
#define BAT_LOW_MV          33000   // 33V — попередження
#define BAT_CUTOFF_MV       31500   // 31.5V — відключення
#define BAT_MAX_MV          42000   // 42V — перезаряд

/* ─── Стани системи ──────────────────────────── */
typedef enum {
    STATE_INIT = 0,
    STATE_IDLE,
    STATE_RUNNING,
    STATE_BRAKING,
    STATE_FAULT,
    STATE_OVERVOLT,
    STATE_UNDERVOLT,
} SystemState_t;

static volatile SystemState_t sysState  = STATE_INIT;
static volatile DisplayMode_t rideMode  = DISP_MODE_D;  // Старт в режимі D
static volatile uint8_t       light_on  = 0;

/* ─── Прототипи ──────────────────────────────── */
static void system_init(void);
static void clock_init(void);
static void gpio_init_all(void);
static void adc_init(void);
static void timer_init(void);
static void nvic_init(void);
static uint8_t apply_speed_limit(uint8_t pct, DisplayMode_t mode);
static uint8_t battery_to_percent(uint16_t mv);

/* ─────────────────────────────────────────────── */
int main(void)
{
    system_init();
    sysState = STATE_IDLE;

    /* Початковий показ на дисплеї */
    display_update(0, 100, rideMode, light_on, 0);

    while (1)
    {
        /* ── 1. Читаємо батарею ────────────────── */
        uint16_t bat_mv  = battery_read_mv();
        uint8_t  bat_pct = battery_to_percent(bat_mv);

        if (bat_mv > BAT_MAX_MV) {
            sysState = STATE_OVERVOLT;
            motor_stop();
            display_update(0, bat_pct, rideMode, light_on, 1);
            continue;
        }

        if (bat_mv < BAT_CUTOFF_MV) {
            sysState = STATE_UNDERVOLT;
            motor_stop();
            display_update(0, bat_pct, rideMode, light_on, 1);
            continue;
        }

        /* ── 2. Команди від дисплею (кнопка) ───── */
        DisplayCmd_t cmd = display_get_cmd();
        if (cmd == DISP_CMD_SHORT_PRESS) {
            /* Перемикаємо режим: ECO → D → S+ → ECO */
            switch (rideMode) {
                case DISP_MODE_ECO:   rideMode = DISP_MODE_D;     break;
                case DISP_MODE_D:     rideMode = DISP_MODE_SPLUS;  break;
                case DISP_MODE_SPLUS: rideMode = DISP_MODE_ECO;   break;
            }
        }

        /* ── 3. Гальмо ─────────────────────────── */
        if (brake_is_active()) {
            sysState = STATE_BRAKING;
            motor_stop();
            uint8_t spd = display_calc_speed();
            display_update(spd, bat_pct, rideMode, light_on, 0);
            continue;
        }

        /* ── 4. Газ ─────────────────────────────── */
        uint16_t raw = throttle_read_raw();
        uint8_t  pct = throttle_to_percent(raw);

        /* Попередження про низький заряд — обмежуємо потужність */
        if (bat_mv < BAT_LOW_MV && pct > 50) {
            pct = 50;
        }

        if (pct < 5) {
            sysState = STATE_IDLE;
            motor_stop();
        } else {
            sysState = STATE_RUNNING;
            uint8_t limited = apply_speed_limit(pct, rideMode);
            motor_set_duty(limited);
        }

        /* ── 5. Оновлення дисплею ───────────────── */
        uint8_t spd   = display_calc_speed();
        uint8_t fault = (sysState == STATE_FAULT) ? 1 : 0;
        display_update(spd, bat_pct, rideMode, light_on, fault);

        /* Невелика затримка ~1мс */
        for (volatile uint32_t i = 0; i < 7200; i++);
    }
}

/* ─── Обмеження швидкості по режиму ─────────── */
static uint8_t apply_speed_limit(uint8_t pct, DisplayMode_t mode)
{
    uint8_t limit;
    switch (mode) {
        case DISP_MODE_ECO:   limit = SPEED_LIMIT_ECO;   break;
        case DISP_MODE_D:     limit = SPEED_LIMIT_D;     break;
        case DISP_MODE_SPLUS: limit = SPEED_LIMIT_SPLUS; break;
        default:              limit = SPEED_LIMIT_D;
    }
    return (pct > limit) ? limit : pct;
}

/* ─── Батарея mV → % ──────────────────────────
 * 36V акум: 42V = 100%, 32V = 0%
 * Діапазон: 10000mV
 */
static uint8_t battery_to_percent(uint16_t mv)
{
    if (mv >= BAT_MAX_MV)    return 100;
    if (mv <= BAT_CUTOFF_MV) return 0;

    uint32_t range = BAT_MAX_MV - BAT_CUTOFF_MV;  // 10500
    uint32_t val   = mv - BAT_CUTOFF_MV;
    return (uint8_t)((val * 100) / range);
}

/* ─── Ініціалізація ──────────────────────────── */
static void system_init(void)
{
    clock_init();
    nvic_init();
    gpio_init_all();
    adc_init();
    timer_init();
    motor_init();
    throttle_init();
    brake_init();
    battery_init();
    display_init();
}

/* Тактування, GPIO, ADC, Timer — без змін з v0.1 */
static void clock_init(void)
{
    RCU_CTL |= RCU_CTL_HXTALEN;
    while (!(RCU_CTL & RCU_CTL_HXTALSTB));
    FMC_WS = (FMC_WS & ~FMC_WS_WSCNT) | WS_WSCNT_2;
    RCU_CFG0 &= ~(RCU_CFG0_PLLSEL | RCU_CFG0_PLLMF);
    RCU_CFG0 |= RCU_CFG0_PLLSEL | (0x07 << 18);
    RCU_CFG0 &= ~(RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC);
    RCU_CFG0 |= RCU_AHBPSC_DIV1 | RCU_APB1PSC_DIV2 | RCU_APB2PSC_DIV1;
    RCU_CTL |= RCU_CTL_PLLEN;
    while (!(RCU_CTL & RCU_CTL_PLLSTB));
    RCU_CFG0 = (RCU_CFG0 & ~RCU_CFG0_SCS) | RCU_CKSYSSRC_PLL;
    while ((RCU_CFG0 & RCU_CFG0_SCSS) != RCU_SCSS_PLL);
}

static void gpio_init_all(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_AFIO);

    /* ADC: PA0 (газ), PA1 (батарея) */
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, GPIO_PIN_0 | GPIO_PIN_1);

    /* Гальмо: PA2 */
    gpio_init(GPIOA, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_2);

    /* PWM мотор: PA8, PA9, PA10 (high), PB13, PB14, PB15 (low) */
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ,
              GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10);
    gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ,
              GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);

    /* Датчики Холла: PB6, PB7, PB8 */
    gpio_init(GPIOB, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
              GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8);
}

static void adc_init(void)
{
    rcu_periph_clock_enable(RCU_ADC0);
    adc_deinit(ADC0);
    adc_mode_config(ADC_MODE_FREE);
    adc_special_function_config(ADC0, ADC_SCAN_MODE, DISABLE);
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, DISABLE);
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 1);
    adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);
    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL,
                                       ADC0_1_EXTTRIG_REGULAR_NONE);
    adc_enable(ADC0);
    adc_calibration_enable(ADC0);
}

static void timer_init(void)
{
    rcu_periph_clock_enable(RCU_TIMER1);
    timer_deinit(TIMER1);

    timer_parameter_struct tp = {
        .prescaler        = 0,
        .alignedmode      = TIMER_COUNTER_CENTER_DOWN,
        .counterdirection = TIMER_COUNTER_UP,
        .period           = 4499,
        .clockdivision    = TIMER_CKDIV_DIV1,
        .repetitioncounter = 0,
    };
    timer_init(TIMER1, &tp);

    timer_break_parameter_struct bp = {
        .runoffstate     = TIMER_ROS_STATE_ENABLE,
        .ideloffstate    = TIMER_IOS_STATE_ENABLE,
        .deadtime        = 36,
        .breakpolarity   = TIMER_BREAK_POLARITY_HIGH,
        .outputautostate = TIMER_OUTAUTO_ENABLE,
        .protectmode     = TIMER_CCHP_PROT_OFF,
        .breakstate      = TIMER_BREAK_ENABLE,
    };
    timer_break_config(TIMER1, &bp);

    timer_oc_parameter_struct op = {
        .outputstate  = TIMER_CCX_ENABLE,
        .outputnstate = TIMER_CCXN_ENABLE,
        .ocpolarity   = TIMER_OC_POLARITY_HIGH,
        .ocnpolarity  = TIMER_OCN_POLARITY_HIGH,
        .ocidlestate  = TIMER_OC_IDLE_STATE_LOW,
        .ocnidlestate = TIMER_OCN_IDLE_STATE_LOW,
    };
    timer_channel_output_config(TIMER1, TIMER_CH_0, &op);
    timer_channel_output_config(TIMER1, TIMER_CH_1, &op);
    timer_channel_output_config(TIMER1, TIMER_CH_2, &op);
    timer_channel_output_mode_config(TIMER1, TIMER_CH_0, TIMER_OC_MODE_PWM1);
    timer_channel_output_mode_config(TIMER1, TIMER_CH_1, TIMER_OC_MODE_PWM1);
    timer_channel_output_mode_config(TIMER1, TIMER_CH_2, TIMER_OC_MODE_PWM1);
    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_0, 0);
    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_1, 0);
    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_2, 0);
    timer_enable(TIMER1);
    timer_primary_output_config(TIMER1, ENABLE);
}

static void nvic_init(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
    nvic_irq_enable(EXTI5_9_IRQn, 0, 0);
}

void HardFault_Handler(void)
{
    motor_stop();
    while (1);
}
