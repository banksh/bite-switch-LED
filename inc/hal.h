#include "stm32f0xx.h"

#define WAKE_DURATION 10000 // in ms

#define USER_LED_RED        GPIO_Pin_2
#define USER_LED_GREEN      GPIO_Pin_3
#define USER_LED_BLUE       GPIO_Pin_4
#define OUTPUT_LED_RED      GPIO_Pin_7
#define OUTPUT_LED_GREEN    GPIO_Pin_6
#define OUTPUT_LED_BLUE     GPIO_Pin_1
#define SWITCH              GPIO_Pin_0
#define V12_EN              GPIO_Pin_1
#define USER_BUTTON         GPIO_Pin_5

void hal_init(void);

void hal_set_user_leds(uint8_t r, uint8_t g, uint8_t b);

void hal_set_output_leds(uint8_t r, uint8_t g, uint8_t b);

void hal_set_12v(uint8_t state);

void hal_bind_user_input(void (*f));

void hal_bind_wakeup(void (*f));

void hal_delay_ms(uint32_t delay);
