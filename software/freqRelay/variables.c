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
	qFreq = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(QFreqStruct)); // queue for from the freq isr to the freq violation
	qLed = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(QLedStruct));
	qInformation = xQueueCreate(QUEUE_SIZE_BIG, sizeof(QInformationStruct));
	qKeyBoard = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(int));
}
