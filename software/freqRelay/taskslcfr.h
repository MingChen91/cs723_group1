#ifndef TASKSLCFR_H_
#define TASKSLCFR_H_

#define SAMPLING_FREQUENCY       16000
#define SWITCH_POLLING_DELAY     100
#define CHAR_BUFFER_SIZE         10
#define CONSTRUCT_FLOAT_DONE     0
#define CONSTRUCT_FLOAT_NOT_DONE 1
#define EXEC_TIME_LIMIT          200
#define LOAD_MANAGEMENT_DELAY    500
#define FROM_SWITCH_POLLING      0
#define NO_OFF_SET               0
#define RED_LEDS_CURRENT         (0xff & IORD(RED_LEDS_BASE, NO_OFF_SET))
#define GREEN_LEDS_CURRENT       (0xff & IORD(GREEN_LEDS_BASE, NO_OFF_SET))
#define SLIDE_SWITCH_CURRENT     (0xff & IORD(SLIDE_SWITCH_BASE, NO_OFF_SET))
#define SHIFT_REG_LEN            5
#define MS_TO_SEC                1000
#define SEC_TO_MIN               60
/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "variables.h"

/* Functions */
void createTasks();
void loadManagementTask();

#endif /* TASKS_H_ */
