#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <system.h>

#include "altera_avalon_pio_regs.h" // to use PIO functions
#include "sys/alt_irq.h"

#define RED_LEDS_CURRENT 0xff & IORD(RED_LEDS_BASE, 0)
#define GREEN_LEDS_CURRENT 0xff & IORD(GREEN_LEDS_BASE, 0)
#define SLIDE_SWITCH_CURRENT 0xff & IORD(SLIDE_SWITCH_BASE, 0)

int main()
{
    printf("Test Nios Build\n");

    uint8_t swPosMax, swPosCurrent, swPosPrev;
    uint8_t redState, greenState;

    swPosMax = SLIDE_SWITCH_CURRENT;
    swPosPrev = SLIDE_SWITCH_CURRENT;
    swPosCurrent = SLIDE_SWITCH_CURRENT;

    IOWR(GREEN_LEDS_BASE, 0, 0);
    IOWR(RED_LEDS_BASE, 0, swPosMax);
    int mode;

    while (1)
    {
        // Keep track of switch positions
        swPosPrev = swPosCurrent;
        swPosCurrent = SLIDE_SWITCH_CURRENT;
        // Detect if switchs havve
        if (swPosCurrent != swPosPrev)
        {
            // Can turn things off but not on.
            swPosMax = swPosMax & swPosCurrent;
            if (mode == 0)
                IOWR(RED_LEDS_BASE, 0, (swPosMax & RED_LEDS_CURRENT));
        }

        else if (mode == 0) // turn off
        {
            redState = RED_LEDS_CURRENT;
            IOWR(RED_LEDS_BASE, 0, redState & (redState - 1));
            greenState = GREEN_LEDS_CURRENT;
            IOWR(GREEN_LEDS_BASE, 0, (greenState | (redState & ~(RED_LEDS_CURRENT))));
        }
        else if (mode == 1) // turn on
        {
            //turn reds on
            redState = RED_LEDS_CURRENT;

            // 11111111
            int temp = ~(SLIDE_SWITCH_CURRENT);
            int number = (redState | temp);

            // printf("temp3 %x \n", temp3);

            IOWR(RED_LEDS_BASE, 0, ((number | (number + 1)) & SLIDE_SWITCH_CURRENT));

            //     //turn greens off
            greenState = GREEN_LEDS_CURRENT;
            IOWR(GREEN_LEDS_BASE, 0, (greenState & (redState & ~(RED_LEDS_CURRENT))));
        }
    }
    return 0;
}
