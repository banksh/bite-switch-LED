#include "anim.h"

#define ANIM(name, ...) \
    static void name(uint8_t *breakout) { \
        uint32_t index = 0; \
        uint32_t cnt = 0; \
        *breakout = 0; \
        while (1) { \
            if (*breakout) { \
                return; \
            } \
        __VA_ARGS__ \
        } \
    }


// ********** animations ********** //

ANIM(off,
        hal_set_user_leds(0, 0, 0);
        hal_set_output_leds(0, 0, 0);
        for (cnt = 0; cnt < 200; cnt++) {
            if (*breakout) {
                return;
            }
            hal_delay_ms(10);
        }
        hal_set_user_leds(1, 0, 0);
        hal_delay_ms(10);
    )

ANIM(red,
        hal_set_user_leds(1, 0, 0);
        hal_set_output_leds(1, 0, 0);
    )

ANIM(green,
        hal_set_user_leds(0, 1, 0);
        hal_set_output_leds(0, 1, 0);
    )

ANIM(blue,
        hal_set_user_leds(0, 0, 1);
        hal_set_output_leds(0, 0, 1);
    )

ANIM(rainbow,
        switch (index) {
            case 0:
                hal_set_user_leds(1, 0, 0);
                hal_set_output_leds(1, 0, 0);
                index++;
                break;
            case 1:
                hal_set_user_leds(0, 1, 0);
                hal_set_output_leds(0, 1, 0);
                index++;
                break;
            case 2:
                hal_set_user_leds(0, 0, 1);
                hal_set_output_leds(0, 0, 1);
                index = 0;
                break;
            }
        hal_delay_ms(100);
    )
                


/*
static void red(uint8_t *breakout) {
    *breakout = 0;
    while (1) {
        if (*breakout) {
            return;
        }
        hal_set_user_leds(1, 0, 0);
        hal_set_output_leds(1, 0, 0);
    }
}

static void green(uint8_t *breakout) {
    *breakout = 0;
    while (1) {
        if (*breakout) {
            return;
        }
        hal_set_user_leds(0, 1, 0);
        hal_set_output_leds(0, 1, 0);
    }
}

static void blue(uint8_t *breakout) {
    *breakout = 0;
    while (1) {
        if (*breakout) {
            return;
        }
        hal_set_user_leds(0, 0, 1);
        hal_set_output_leds(0, 0, 1);
    }
}

static void rainbow(void) {
    static uint32_t counter = 0;

    hal_set_user_leds(1, 0, 0);
}
*/



// ********** utilities ********** //

typedef void (*animation_t)(uint8_t *);

static animation_t animations[] = {
    red,
    green,
    blue,
    rainbow,
    off,
};

static const uint32_t anim_len = sizeof (animations) / sizeof (animation_t);

static volatile uint32_t anim_index = 0;

static volatile uint8_t stop = 0;

void anim_advance() {
    stop = 1;
    anim_index++;
    if (anim_index >= anim_len) {
        anim_index = 0;
    }
}

void anim_reset() {
    stop = 1;
    anim_index = 0;
}

void anim_run() {
    animations[anim_index](&stop);
}
