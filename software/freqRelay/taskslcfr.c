/*
 * tasks.c
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#include <stdio.h>
#include <string.h>
#include "taskslcfr.h"
#include "altera_avalon_pio_regs.h" // to use PIO functions
#include "system.h"					// for IORD()

void frequencyViolationTask()
{
	QFreqStruct incomingFreqItem;
	QInformationStruct outgoingInformationItem;
	QViolationStruct outgoingViolationItem;
	uint16_t sampleCountNew, sampleCountOld;
	float frequencyNew, frequencyOld, rateOfChange;

	// first run, collect data so have 2 data points to calculate averages
	if (xQueueReceive(qFreq, &incomingFreqItem, portMAX_DELAY) == pdTRUE)
	{
		sampleCountNew = incomingFreqItem.adcSampleCount;
		frequencyNew = (SAMPLING_FREQUENCY / (float)sampleCountNew);
	}

	while (1)
	{
		// Wait for item in queue.
		if (xQueueReceive(qFreq, &incomingFreqItem, portMAX_DELAY) == pdTRUE)
		{
			// Keep track of 2 sets of frequency data.
			sampleCountOld = sampleCountNew;
			sampleCountNew = incomingFreqItem.adcSampleCount;
			frequencyOld = frequencyNew;
			frequencyNew = (SAMPLING_FREQUENCY / (float)sampleCountNew);
			rateOfChange = ((frequencyNew - frequencyOld) * 2 * SAMPLING_FREQUENCY) / (sampleCountNew + sampleCountOld);

			// Send frequency data to display task
			outgoingInformationItem.informationType = FREQ;
			outgoingInformationItem.value = frequencyNew;
			xQueueSend(qInformation, &outgoingInformationItem, pdFALSE);

#if DEBUG_FREQ
			printf("old freq %f\n", frequencyOld);
			printf("new freq %f\n", frequencyNew);
			printf("old count %d\n", sampleCountOld);
			printf("new count %d\n", sampleCountNew);
			printf("rate of change: %f\n\n", rateOfChange);
#endif /* DEBUG_FREQ */
		}
	}
}

void informationTask()
{
	// create a seperate queue for serial
	QInformationStruct incomingInformationItem;
	FILE *serialUart;
	serialUart = fopen("/dev/uart", "w");
	if (serialUart != NULL)
	{
		printf("\n***********************\n");
		printf("   UART CONNECTED");
		printf("\n***********************\n");
	}
	else
	{
		printf("\n***************************\n");
		printf("  ERROR CONNECTING TO UART");
		printf("\n***************************\n");
	}

	while (1)
	{
		if (xQueueReceive(qInformation, &incomingInformationItem, portMAX_DELAY) == pdTRUE)
		{
			switch (incomingInformationItem.informationType)
			{
			case FREQ:
				fprintf(serialUart, "f%f\r\n", incomingInformationItem.value);
				break;
			default:
				break;
			}
		}
	}
	// fclose(serialUart);
}

void switchPollingTask()
{
	QLedStruct sendLedQueueItem;
	sendLedQueueItem.ledg = 0;
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
		if (xQueueReceive(qLed, &receiveLedQueueItem, portMAX_DELAY) == pdTRUE)
		{
			IOWR(RED_LEDS_BASE, 0, receiveLedQueueItem.ledr);
			IOWR(GREEN_LEDS_BASE, 0, receiveLedQueueItem.ledg);
		}
	}
}

void createTasks()
{
	xTaskCreate(frequencyViolationTask, "frequencyViolationTask", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
	xTaskCreate(informationTask, "informationTask", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
	xTaskCreate(switchPollingTask, "switchPollingTask", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
	xTaskCreate(ledDriverTask, "ledDriverTask", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
}
