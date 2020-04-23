/*
 * variables.h
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#ifndef VARIABLES_H_
#define VARIABLES_H_

#define QUEUE_SIZE_SMALL 20
#define QUEUE_SIZE_BIG 100

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
/* Structs */
// From frequency isr to violation task
typedef struct
{
    uint16_t adcSampleCount;
    TickType_t isrTickCount;
} QFreqStruct;

// queue going into leddriver
typedef struct
{
    uint8_t ledr; // 8 bit of leds
    uint8_t ledg;
} QLedStruct;

typedef struct
{
    uint8_t violationOccured;
    TickType_t isrTickCount;
} QViolationStruct;

typedef enum
{
    FREQ = 1,
    ROC = 2,
    MODE = 3,
    EXEC_TIME = 4,
    ROC_THRESH = 5,
    FREQ_TRHESH = 6
} InformationType;

typedef struct
{
    InformationType informationType;
    float value;
} QInformationStruct;

typedef enum
{
    STABLE = 1,
    LOAD_MANAGEMENT = 2,
    MAINTANENCE = 3
} OpMode;

/******** Variables ********/
QueueHandle_t qFreq;
QueueHandle_t qLed;
QueueHandle_t qInformation;
QueueHandle_t qKeyBoard;
QueueHandle_t qViolation;

TaskHandle_t vSwitchPollingTaskHandle;

SemaphoreHandle_t xMutexRoc;
SemaphoreHandle_t xMutexFreq;
SemaphoreHandle_t xMutexMode;
SemaphoreHandle_t xButtonSemaphore;
OpMode currentMode;
float frequencyThreshold, rocThreshold;

/*Functions */
void createVariables();

#endif /* VARIABLES_H_ */
