/**
 * Ducati PRO-III Firmware v0.2.0
 * + Підтримка дисплею MTW-7349CGW
 * + Швидкомір (через датчики Холла)
 * + Режими ECO/D/S+
 */

#include "gd32f10x.h"
#include "motor.h"
#include "throttle.h"
#include "brake.h"
#include "battery.h"
#include "display.h"

/* ─── Константи ──────────────────────────────── */
#define WHEEL_CIRCUMFERENCE_MM  1696    // 8.5" колесо = ~216мм діаметр * π
#define HALL_POLES              15      // Кількість полюсів мотора (підібрати!)
#define SPEED_UPDATE_MS         200     // Оновлення швидкості

/* Ліміти швидкості по режимах (км/год) */
#define SPEED_MAX_ECO   15
#define SPEED_MAX_D     20
#define SPEED_MAX_S     25

/* Duty cycle по режимах (%) */
#define DUTY_MAX_ECO    45
#define DUTY_MAX_D      75
#define DUTY_MAX_S      95

typedef enum {
    STATE_IDLE    = 0,
    STATE_RUNNING,
    STATE_BRAKING,
    STATE_FAULT,
    STATE_OVERVOLT,
} SystemState_t;

/* ─── Глобальні змінні ───────────────────────── */
static volatile uint32_t g_tick_ms    = 0;  // Системний тік
static volatile uint32_t g_hall_count = 0;  // Лічильник подій Холла
static volatile SystemState_t g_state = STATE_IDLE;

/* ─── SysTick ────────────────────────────────── */
void SysTick_Handler(void)
{
    g_tick_ms++;
}

/* ─── Розрахунок швидкості ───────────────────── */
static uint8_t calc_speed_kmh(void)
{
    static uint32_t last_tick  = 0;
    static uint32_t last_count = 0;

    uint32_t now   = g_tick_ms;
    uint32_t dt_ms = now - last_tick;

    if (dt_ms < SPEED_UPDATE_MS) return 0xFF; // Ще рано

    uint32_t pulses = g_hall_count - last_count;
    last_tick  = now;
    last_count = g_hall_count;

    if (pulses == 0) return 0;

    /* v (мм/мс) = pulses * circumference / (poles * dt_ms) */
    /* v (км/год) = v(мм/мс) * 3600 */
    uint32_t speed_mmps = (pulses * WHEEL_CIRCUMFERENCE_MM * 1000)
                          / (HALL_POLES * dt_ms);
    uint32_t speed_kmh  = (speed_mmps * 36) / 10000;

    if (speed_kmh > 99) speed_kmh = 99;
    return (uint8_t)speed_kmh;
}

/* ─── Ліміт duty по режиму ───────────────────── */
static uint8_t get_max_duty(uint8_t mode)
{
    switch (mode) {
        case 0:  return DUTY_MAX_ECO;
        case 1:  return DUTY_MAX_D;
        case 2:  return DUTY_MAX_S;
        default: return DUTY_MAX_D;
    }
}

/* ─── Ініціалізація ──────────────────────────── */
static void system_init(void)
{
    /* SysTick: переривання кожну 1мс */
    systick_clksource_set(SYSTICK_CLKSOURCE_HCLK_DIV8);
    SysTick_Config(SystemCoreClock / 1000);

    motor_init();
    throttle_init();
    brake_init();
    battery_init();
    display_init();
}

/* ─── MAIN ───────────────────────────────────── */
int main(void)
{
    system_init();

    static uint8_t  speed_kmh  = 0;
    static uint16_t bat_mv     = 0;
    static uint32_t last_speed_tick = 0;

    while (1)
    {
        uint32_t now = g_tick_ms;

        /* --- Читаємо батарею кожні 500мс --- */
        static uint32_t last_bat = 0;
        if ((now - last_bat) >= 500) {
            bat_mv   = battery_read_mv();
            last_bat = now;
        }

        /* --- Захист батареї --- */
        if (bat_mv > 42000) {
            g_state = STATE_OVERVOLT;
            motor_stop();
            display_update(0, bat_mv, 0, now);
            continue;
        }
        if (bat_mv < 32000) {
            g_state = STATE_FAULT;
            motor_stop();
            display_update(0, bat_mv, 0, now);
            continue;
        }

        /* --- Читаємо кнопки дисплею --- */
        DisplayButton_t btn = display_read_button();
        if (btn == BTN_PLUS || btn == BTN_MINUS) {
            display_next_mode();
        }
        if (btn == BTN_LIGHT) {
            display_toggle_light();
        }

        /* --- Гальмо --- */
        if (brake_is_active()) {
            g_state = STATE_BRAKING;
            motor_stop();
            speed_kmh = 0;
            display_update(0, bat_mv, 0, now);
            continue;
        }

        /* --- Швидкість --- */
        uint8_t s = calc_speed_kmh();
        if (s != 0xFF) speed_kmh = s;

        /* --- Газ --- */
        uint16_t raw_throttle = throttle_read_raw();
        uint8_t  pct_throttle = throttle_to_percent(raw_throttle);
        uint8_t  mode         = display_get_mode();
        uint8_t  max_duty     = get_max_duty(mode);

        if (pct_throttle < 5) {
            g_state = STATE_IDLE;
            motor_stop();
        } else {
            /* Масштабуємо по режиму */
            uint8_t duty = (uint8_t)((uint32_t)pct_throttle * max_duty / 100);
            g_state = STATE_RUNNING;
            motor_set_duty(duty);
        }

        /* --- Дисплей --- */
        display_update(speed_kmh, bat_mv,
                       (g_state == STATE_RUNNING), now);
    }
}

/* ─── Лічильник подій Холла (із motor.c) ─────── */
void hall_pulse_callback(void)
{
    g_hall_count++;
}
