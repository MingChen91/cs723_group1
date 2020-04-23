/*
 * variables.c
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */
#include "variables.h"

void createVariables()
{
    // global variables
    currentMode = STABLE;
    frequencyThreshold = 60;
    rocThreshold = 10;
    // queues and semaphres
    // queue for from the freq isr to the freq violation
    qFreq = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(QFreqStruct));
    qLed = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(QLedStruct));
    qInformation = xQueueCreate(QUEUE_SIZE_BIG, sizeof(QInformationStruct));
    qViolation = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(QViolationStruct));
    qKeyBoard = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(int));

    xMutexFreq = xSemaphoreCreateMutex();
    xMutexRoc = xSemaphoreCreateMutex();
    xMutexMode = xSemaphoreCreateMutex();

    xButtonSemaphore = xSemaphoreCreateCounting(5, 0);
}
