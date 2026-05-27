/**
 * motor.c — Керування BLDC мотором
 * 6-step Hall-sensor commutation для 36V/350W мотора
 */

#include "motor.h"
#include "gd32f10x.h"

/* ─── Константи ──────────────────────────────── */
#define PWM_PERIOD      4499    // Відповідає timer_init period
#define MIN_DUTY_PCT    5       // Мінімальний газ %
#define MAX_DUTY_PCT    95      // Максимальний газ %
#define RAMP_STEP       2       // Крок розгону % за цикл (плавний старт)

/* ─── Поточний стан ──────────────────────────── */
static volatile uint8_t  hall_state    = 0;
static volatile uint8_t  current_duty  = 0;
static volatile uint8_t  target_duty   = 0;

/*
 * Таблиця комутації для 6-step BLDC
 * Індекс = стан датчиків Холла (біти: C B A)
 * Значення: які фази HIGH/LOW/OFF
 *
 * Фаза A = CH0, Фаза B = CH1, Фаза C = CH2
 * 1 = PWM HIGH, -1 = LOW (закритий), 0 = OFF
 */
typedef struct {
    int8_t a;   // Фаза A
    int8_t b;   // Фаза B
    int8_t c;   // Фаза C
} CommStep_t;

static const CommStep_t COMM_TABLE[8] = {
    { 0,  0,  0},   // 000 — невалідний
    { 1, -1,  0},   // 001 — A+ B-
    {-1,  0,  1},   // 010 — C+ A-
    { 1,  0, -1},   // 011 — A+ C-
    { 0,  1, -1},   // 100 — B+ C-
    { 0, -1,  1},   // 101 — C+ B-
    {-1,  1,  0},   // 110 — B+ A-
    { 0,  0,  0},   // 111 — невалідний
};

/* ─── Внутрішні функції ──────────────────────── */
static void apply_commutation(uint8_t hall, uint8_t duty_pct);
static uint32_t duty_to_ccr(uint8_t pct);
static void phase_pwm(uint32_t ch, int8_t mode, uint32_t ccr);

/* ─── Ініціалізація ──────────────────────────── */
void motor_init(void)
{
    current_duty = 0;
    target_duty  = 0;
    hall_state   = motor_read_hall();

    /* EXTI для пінів Холла PB6, PB7, PB8 */
    rcu_periph_clock_enable(RCU_AF);

    /* PB6 → EXTI6 */
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_6);
    exti_init(EXTI_6, EXTI_INTERRUPT, EXTI_TRIG_BOTH);

    /* PB7 → EXTI7 */
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_7);
    exti_init(EXTI_7, EXTI_INTERRUPT, EXTI_TRIG_BOTH);

    /* PB8 → EXTI8 */
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_8);
    exti_init(EXTI_8, EXTI_INTERRUPT, EXTI_TRIG_BOTH);

    motor_stop();
}

/* ─── Читання датчиків Холла ─────────────────── */
uint8_t motor_read_hall(void)
{
    uint8_t ha = (gpio_input_bit_get(GPIOB, GPIO_PIN_6) != RESET) ? 1 : 0;
    uint8_t hb = (gpio_input_bit_get(GPIOB, GPIO_PIN_7) != RESET) ? 1 : 0;
    uint8_t hc = (gpio_input_bit_get(GPIOB, GPIO_PIN_8) != RESET) ? 1 : 0;
    return (hc << 2) | (hb << 1) | ha;
}

/* ─── Встановити потужність мотора (0–100%) ───── */
void motor_set_duty(uint8_t pct)
{
    if (pct > MAX_DUTY_PCT) pct = MAX_DUTY_PCT;
    target_duty = pct;

    /* Плавний розгін */
    if (current_duty < target_duty) {
        current_duty += RAMP_STEP;
        if (current_duty > target_duty) current_duty = target_duty;
    } else if (current_duty > target_duty) {
        current_duty -= RAMP_STEP;
        if (current_duty < target_duty) current_duty = target_duty;
    }

    hall_state = motor_read_hall();
    apply_commutation(hall_state, current_duty);
}

/* ─── Зупинка мотора ─────────────────────────── */
void motor_stop(void)
{
    target_duty  = 0;
    current_duty = 0;

    /* Всі канали в нуль */
    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_0, 0);
    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_1, 0);
    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_2, 0);
}

/* ─── Застосувати комутацію ──────────────────── */
static void apply_commutation(uint8_t hall, uint8_t duty_pct)
{
    if (hall == 0 || hall == 7) {
        motor_stop();   // Невалідний стан
        return;
    }

    uint32_t ccr = duty_to_ccr(duty_pct);
    const CommStep_t *step = &COMM_TABLE[hall];

    phase_pwm(TIMER_CH_0, step->a, ccr);
    phase_pwm(TIMER_CH_1, step->b, ccr);
    phase_pwm(TIMER_CH_2, step->c, ccr);
}

/* ─── Конвертація % в CCR ────────────────────── */
static uint32_t duty_to_ccr(uint8_t pct)
{
    return ((uint32_t)pct * PWM_PERIOD) / 100;
}

/* ─── Керування фазою ────────────────────────── */
static void phase_pwm(uint32_t ch, int8_t mode, uint32_t ccr)
{
    switch (mode) {
        case 1:     // PWM (активна фаза high)
            timer_channel_output_pulse_value_config(TIMER1, ch, ccr);
            break;
        case -1:    // Замкнуто на землю (low side ON)
            timer_channel_output_pulse_value_config(TIMER1, ch, 0);
            break;
        case 0:     // Відключено
        default:
            timer_channel_output_pulse_value_config(TIMER1, ch, 0);
            break;
    }
}

/* ─── EXTI переривання від датчиків Холла ─────── */
void EXTI5_9_IRQHandler(void)
{
    if (exti_interrupt_flag_get(EXTI_6) ||
        exti_interrupt_flag_get(EXTI_7) ||
        exti_interrupt_flag_get(EXTI_8))
    {
        exti_interrupt_flag_clear(EXTI_6);
        exti_interrupt_flag_clear(EXTI_7);
        exti_interrupt_flag_clear(EXTI_8);

        hall_state = motor_read_hall();

        if (current_duty > 0) {
            apply_commutation(hall_state, current_duty);
        }
    }
}
