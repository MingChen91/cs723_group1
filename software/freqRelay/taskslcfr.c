/*
 * tasks.c
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#include <stdio.h>
#include <stdlib.h>
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
		if (xQueueReceive(qInformation, &incomingInformationItem, portMAX_DELAY))
		{
			switch (incomingInformationItem.informationType)
			{
			case FREQ:
				fprintf(serialUart, "f%f\r\n", incomingInformationItem.value);
				break;
			case MODE:
				// send mode to uart
				break;
			case EXEC_TIME:
				// calculate highest, lowest , average, total
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
/*
	Driving LEDs
*/
void ledDriverTask()
{
	QLedStruct receiveLedQueueItem;
	while (1)
	{
		if (xQueueReceive(qLed, &receiveLedQueueItem, portMAX_DELAY))
		{
			IOWR(RED_LEDS_BASE, 0, receiveLedQueueItem.ledr);
			IOWR(GREEN_LEDS_BASE, 0, receiveLedQueueItem.ledg);
		}
	}
}

/*
	Building Freq and ROC from keyboard ISR individual chars
*/

#define BUFFER_SIZE 10
int8_t buildNumber(float *resultFloat, char input, char *charBuffer)
{
	// Keeps track of which which element in char array
	static int bufferCount = 0;

	// Checks if input is valid, needs to be 0-9 , or '.' or null terminator
	if (((input < '0') || (input > '9')) && (input != '.') && (input != '\0'))
		return 1;

	// New input gets put into the buffer
	charBuffer[bufferCount] = input;
	printf("Count : %d\n", bufferCount);
	bufferCount++;

	// If end of string, or buffer size reached, convert to float.
	if (input == '\0')
	{
		*resultFloat = strtof(charBuffer, charBuffer[bufferCount - 1]);
		bufferCount = 0;
		return 0;
	}
	else if (bufferCount > BUFFER_SIZE - 2)
	{
		charBuffer[bufferCount] = '\0';
		*resultFloat = strtof(charBuffer, charBuffer[bufferCount - 1]);
		bufferCount = 0;
		return 0;
	}
	else
	{
		printf("Buffer : %s\n", charBuffer);
		return 1;
	}
}

void keyboardTask()
{
	enum KeyBoardState
	{
		IDLE,
		BUILDING_FREQ,
		BUILDING_ROC
	};
	enum KeyBoardState state = IDLE;
	unsigned char receieveKeyboardQueueItem;

	char charBuffer[BUFFER_SIZE];
	float resultFloat = 0;

	int8_t status;
	while (1)
	{
		if (xQueueReceive(qKeyBoard, &receieveKeyboardQueueItem, portMAX_DELAY))
		{
			switch (state)
			{
			case IDLE:
				switch (receieveKeyboardQueueItem)
				{
				case 'F':
					state = BUILDING_FREQ;
					printf("Building Frequency\n");
					break;
				case 'R':
					printf("Building Rate of Change\n");
					state = BUILDING_ROC;
					break;
				default:
					break;
				}
				break;
			case BUILDING_FREQ:
				// build temp buffer and change freq
				status = buildNumber(&resultFloat, receieveKeyboardQueueItem, charBuffer);
				if (status == 0)
				{
					printf("New Freq : Freq = %f \n", resultFloat);
					state = IDLE;
				}
				break;
			case BUILDING_ROC:
				// build temp buffer and change freq
				break;
			default:
				break;
			}
		}
	}
}

void createTasks()
{
	xTaskCreate(frequencyViolationTask, "frequencyViolationTask", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
	xTaskCreate(informationTask, "informationTask", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
	xTaskCreate(switchPollingTask, "switchPollingTask", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
	xTaskCreate(ledDriverTask, "ledDriverTask", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
	xTaskCreate(keyboardTask, "keyboardTask", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}
