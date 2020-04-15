/*
 * variables.h
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#ifndef VARIABLES_H_
#define VARIABLES_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
/* Structs */

typedef struct
{
	int adcCount;
	TickType_t isrTickCount;
} FreqQStruct;




/*  Variables */
QueueHandle_t qFreq;
SemaphoreHandle_t modeMutex;
int currentMode;

/*Functions */
void createVariables();

#endif /* VARIABLES_H_ */
