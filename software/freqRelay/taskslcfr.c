/*
 * tasks.c
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#include <stdio.h>
#include "taskslcfr.h"

void frequencyViolationTask()
{
	while (1)
	{
		FreqQStruct temp;
		if (xQueueReceive(qFreq, &temp, portMAX_DELAY) == pdTRUE)
		{
			// printf("%f\n", 16000 / ((double)temp.adcCount));
			// printf("%u\n\n", temp.isrTickCount);
		}
	}
}

void task2()
{
	while (1)
	{
		printf("hello\n");
		vTaskDelay(1000);
	}
}


void createTasks()
{
	xTaskCreate(frequencyViolationTask, "frequencyViolationTask", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
	xTaskCreate(task2, "task2", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
}
