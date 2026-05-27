/* motor.h */
#ifndef MOTOR_H
#define MOTOR_H
#include <stdint.h>

void    motor_init(void);
uint8_t motor_read_hall(void);
void    motor_set_duty(uint8_t pct);
void    motor_stop(void);

#endif /* MOTOR_H */


/* throttle.h */
#ifndef THROTTLE_H
#define THROTTLE_H
#include <stdint.h>

void     throttle_init(void);
uint16_t throttle_read_raw(void);
uint8_t  throttle_to_percent(uint16_t raw);

#endif /* THROTTLE_H */


/* brake.h */
#ifndef BRAKE_H
#define BRAKE_H
#include <stdint.h>

void    brake_init(void);
uint8_t brake_is_active(void);

#endif /* BRAKE_H */


/* battery.h */
#ifndef BATTERY_H
#define BATTERY_H
#include <stdint.h>

void     battery_init(void);
uint16_t battery_read_mv(void);

#endif /* BATTERY_H */
