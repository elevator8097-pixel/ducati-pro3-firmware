#ifndef DISPLAY_H
#define DISPLAY_H
#include <stdint.h>

typedef enum {
    BTN_NONE        = 0,
    BTN_POWER_SHORT,
    BTN_POWER_LONG,
    BTN_PLUS,
    BTN_MINUS,
    BTN_LIGHT,
} DisplayButton_t;

void            display_init(void);
void            display_update(uint8_t speed_kmh, uint16_t bat_mv,
                               uint8_t is_running, uint32_t tick_ms);
DisplayButton_t display_read_button(void);
void            display_next_mode(void);
uint8_t         display_get_mode(void);
void            display_toggle_light(void);

#endif
