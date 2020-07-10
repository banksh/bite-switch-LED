#include "hal.h"

#define DEBOUNCE_TIME 5 // in ms
#define SNOOZE_TIME 15 // in ms

GPIO_InitTypeDef    GPIO_InitStruct;
EXTI_InitTypeDef    EXTI_InitStruct;
NVIC_InitTypeDef    NVIC_InitStruct;

static void (*button_cb)(void);
static void (*wakeup_cb)(void);

static volatile uint32_t sleep_timer = WAKE_DURATION;

// Used to disregard inputs immediately after waking up 
static volatile uint32_t snooze_timer = 0;

static volatile uint32_t debounce_timer;
static volatile uint32_t delay_timer;
static volatile uint8_t button_flag;

void hal_init() {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Pin = V12_EN | USER_LED_RED | USER_LED_BLUE | USER_LED_GREEN | OUTPUT_LED_RED | OUTPUT_LED_GREEN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = OUTPUT_LED_BLUE;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = SWITCH | USER_BUTTON;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    // These need to change if SWITCH or USER_BUTTON change pins
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource5);

    EXTI_InitStruct.EXTI_Line = SWITCH;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);

    EXTI_InitStruct.EXTI_Line = USER_BUTTON;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);

    SysTick_Config(SystemCoreClock / 1000); // SysTick fires every 1ms
    NVIC_SetPriority(SysTick_IRQn, 0x02);

    NVIC_InitStruct.NVIC_IRQChannel = EXTI0_1_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 0x01;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    NVIC_InitStruct.NVIC_IRQChannel = EXTI4_15_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 0x01;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    GPIO_SetBits(GPIOA, V12_EN);
}

void hal_set_user_leds(uint8_t r, uint8_t g, uint8_t b) {
    GPIOA->BSRR = (r ? USER_LED_RED : 0) |
        (g ? USER_LED_GREEN : 0) |
        (b ? USER_LED_BLUE : 0);
    GPIOA->BRR = (r ? 0 : USER_LED_RED) |
        (g ? 0 : USER_LED_GREEN) |
        (b ? 0 : USER_LED_BLUE);
}

void hal_set_output_leds(uint8_t r, uint8_t g, uint8_t b) {
    // TODO: Change to using timers to dim them
    GPIOA->BSRR = (r ? OUTPUT_LED_RED : 0) |
        (g ? OUTPUT_LED_GREEN : 0);
    GPIOA->BRR = (r ? 0 : OUTPUT_LED_RED) |
        (g ? 0 : OUTPUT_LED_GREEN);
    GPIOB->BSRR = (b ? OUTPUT_LED_BLUE : 0);
    GPIOB->BRR = (b ? 0 : OUTPUT_LED_BLUE);
}

void hal_bind_user_input(void (*f)) {
    button_cb = f;
}

void hal_bind_wakeup(void (*f)) {
    wakeup_cb = f;
}

static void hal_deep_sleep() {
    // WFI is wait for interrupt, and we want to wake up with an EXTI interrupt
    hal_set_12v(DISABLE);
    hal_set_user_leds(0, 0, 0);
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
}

// hal_wakeup returns 1 if we actually were asleep, and 0 if it did nothing
static int hal_wakeup() {
    sleep_timer = WAKE_DURATION; // You know what? Let's just go back to sleep #mood

    // Might be a hack? Determine whether we just woke from stop
    // by checking whether the PLL is enabled
    // seems to work fine *shrug*
    if (!(RCC->CR & RCC_CR_PLLON)) {

        wakeup_cb();

        // Re-enable PLL which gets cleared by STOP mode (see
        // RM page 99 PLLON bit and RM page 103 SW bits)
        RCC->CR |= RCC_CR_PLLON;
        RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;
        while (RCC->CR & RCC_CR_PLLRDY);
        snooze_timer = SNOOZE_TIME;
        return 1;
    }
    return 0;
}

void hal_set_12v(uint8_t state) {
    (state == ENABLE) ? GPIO_SetBits(GPIOA, V12_EN) : GPIO_ResetBits(GPIOA, V12_EN);
}

void hal_delay_ms(uint32_t delay) {
    delay_timer = delay;
    while (delay_timer);
}



// INTERRUPTS //

void SysTick_Handler() {
    if (sleep_timer > 0) {
        sleep_timer--;
        hal_set_12v(ENABLE);
    }
    else {
        hal_deep_sleep();
    }

    if (debounce_timer > 0) {
        // If the button goes back OFF while we're debouncing
        // then we don't want to register an action
        if (GPIO_ReadInputDataBit(GPIOA, USER_BUTTON) ||
                GPIO_ReadInputDataBit(GPIOA, SWITCH)) {
            button_flag = 0;
            debounce_timer = 0;
        }
        else {
            debounce_timer--;
        }
    }
    else if (button_flag){
        button_flag = 0;
        button_cb();
    }

    if (delay_timer > 0) {
        delay_timer--;
    }

    if (snooze_timer > 0) {
        snooze_timer--;
    }
}

// Switch release
void EXTI0_1_IRQHandler() {
    EXTI_ClearITPendingBit(SWITCH);
    debounce_timer = DEBOUNCE_TIME;
    if (!hal_wakeup() && (snooze_timer == 0)) {
        button_flag = 1;
    }
}

// User button release
void EXTI4_15_IRQHandler() {
    EXTI_ClearITPendingBit(USER_BUTTON);
    debounce_timer = DEBOUNCE_TIME;
    if (!hal_wakeup() && (snooze_timer == 0)) {
        button_flag = 1;
    }
}
