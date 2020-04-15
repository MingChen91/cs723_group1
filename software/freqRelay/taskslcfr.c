/*
 * tasks.c
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#include <stdio.h>
#include "taskslcfr.h"
#include "altera_avalon_pio_regs.h" // to use PIO functions
#include "system.h"					// for IORD()

void frequencyViolationTask()
{
	while (1)
	{
		QFreqStruct temp;
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

void switchPollingTask()
{
	QLedStruct sendLedQueueItem;
	sendLedQueueItem.ledg = 255;
	while (1)
	{
		sendLedQueueItem.ledr = (IORD(SLIDE_SWITCH_BASE, 0) & 0xff); // bit masking to 8 bits
		xQueueSend(qLed, &sendLedQueueItem, pdFALSE);
		vTaskDelay(100);
	}
}

void ledDriverTask()
{
	QLedStruct receiveLedQueueItem;
	while (1)
	{
		if (xQueueReceive(qLed, &receiveLedQueueItem, pdFALSE))
		{
			IOWR(RED_LEDS_BASE, 0, receiveLedQueueItem.ledr);
			IOWR(GREEN_LEDS_BASE, 0, receiveLedQueueItem.ledg);
		}
	}
}

void createTasks()
{
	xTaskCreate(frequencyViolationTask, "frequencyViolationTask", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
	// xTaskCreate(task2, "task2", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
	xTaskCreate(switchPollingTask, "switchPollingTask", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
	xTaskCreate(ledDriverTask, "ledDriverTask", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
}
