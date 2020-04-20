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
	fclose(serialUart);
}

/*
	Polls the switchs 
*/
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
int8_t buildNumber(float *resultFloat, char inputChar, char *charBuffer)
{
	// Keeps track of which which element in char array
	static uint8_t bufferIndex = 0;

	// Checks if input is valid, needs to be 0-9 , or '.' or null terminator
	if (((inputChar < '0') || (inputChar > '9')) && (inputChar != '.') && (inputChar != '\0'))
		return 1;

	// New input gets put into the buffer
	charBuffer[bufferIndex] = inputChar;

	// Check if overflowing char buffer
	if (bufferIndex >= BUFFER_SIZE - 1) // last char needed for null terminator
	{
		charBuffer[bufferIndex] = '\0';
	}

	// End of string, convert to float
	if (charBuffer[bufferIndex] == '\0')
	{
		// Convert the string to float
		*resultFloat = strtof(charBuffer, NULL);
		// reset buffer
		for (bufferIndex = 0; bufferIndex < BUFFER_SIZE; bufferIndex++)
		{
			charBuffer[bufferIndex] = '\0';
		}
		bufferIndex = 0;
		return 0;
	}

	printf("Buffer : %s\n", charBuffer);
	bufferIndex++;
	return 1;
}

/*
	Taking inputs from the keyboard and changing thresholds 
*/
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
	char charBuffer[BUFFER_SIZE] = {'\0'};
	float resultFloat = 0;
	int8_t buildStatus;

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

				buildStatus = buildNumber(&resultFloat, receieveKeyboardQueueItem, charBuffer);
				if (buildStatus == 0)
				{

					printf("New Freq : Freq = %10.5f \n", resultFloat);
					state = IDLE;
				}
				// build temp buffer and change freq
				break;
			case BUILDING_ROC:
				buildStatus = buildNumber(&resultFloat, receieveKeyboardQueueItem, charBuffer);
				if (buildStatus == 0)
				{
					printf("New ROC : Freq = %10.5f \n", resultFloat);
					state = IDLE;
				}
				// build temp buffer and change ROC
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
