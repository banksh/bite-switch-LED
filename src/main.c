#include "hal.h"
#include "anim.h"

void main() {
    hal_bind_user_input(anim_advance);
    hal_bind_wakeup(anim_reset);

    hal_init();

    for (;;) {
        anim_run();
    }
}
