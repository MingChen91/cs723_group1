/*
 * variables.h
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#ifndef VARIABLES_H_
#define VARIABLES_H_

#define QUEUE_SIZE 20

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
/* Structs */

typedef struct
{
	int adcCount;
	TickType_t isrTickCount;
} QFreqStruct;

typedef struct
{
	uint8_t ledr; // 8 bit of leds
	uint8_t ledg;
} QLedStruct;

/*  Variables */
QueueHandle_t qFreq;
QueueHandle_t qLed;
SemaphoreHandle_t modeMutex;
int currentMode;

/*Functions */
void createVariables();

#endif /* VARIABLES_H_ */
