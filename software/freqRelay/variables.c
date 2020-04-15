/*
 * variables.c
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */
#include "variables.h"

void createVariables()
{
	currentMode = 0;
	qFreq = xQueueCreate(QUEUE_SIZE, sizeof(QFreqStruct)); // queue for from the freq isr to the freq violation
	qLed = xQueueCreate(QUEUE_SIZE, sizeof(QLedStruct));
	modeMutex = xSemaphoreCreateMutex(); // mutex for protecting the mode
}
