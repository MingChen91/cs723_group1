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
	qFreq = xQueueCreate(100, sizeof(FreqQStruct)); // queue for from the freq isr to the freq violation
	modeMutex = xSemaphoreCreateMutex();			// mutex for protecting the mode
}
