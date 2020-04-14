/* Defines */


/* Includes */
#include <stddef.h>
#include <stdio.h>
#include "system.h"
#include "io.h"
#include <unistd.h>
#include <string.h>						// C Strings
#include "altera_up_avalon_ps2.h"		// ps2
#include "altera_up_ps2_keyboard.h"		// ps2 keyboard
#include "altera_avalon_pio_regs.h" 	// to use PIO functions
#include "alt_types.h"					// alt_u32 is a kind of alt_types
#include "sys/alt_irq.h" 				// to register interrupts

/* Scheduler includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/*	Functions  */
void createTasks();
void startInterrupts();

