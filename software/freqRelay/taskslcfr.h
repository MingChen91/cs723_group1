/*
 * tasks.h
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#ifndef TASKSLCFR_H_
#define TASKSLCFR_H_

#define SAMPLING_FREQUENCY 16000

// DEBUGS
#define DEBUG_FREQ 0

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "variables.h"

/* Functions*/
void createTasks();
void loadManagementTask();

#endif /* TASKS_H_ */
