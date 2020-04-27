#ifndef VARIABLES_H_
#define VARIABLES_H_

#define QUEUE_SIZE_SMALL 20
#define QUEUE_SIZE_BIG   100

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* Structs */
typedef struct
{
    uint16_t adcSampleCount;
    TickType_t isrTickCount;
} QFreqStruct;

typedef struct
{
    uint8_t ledR; // 8 bit of leds
    uint8_t ledG;
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

TaskHandle_t LoadManagementTaskHandle;
TaskHandle_t SwitchPollingTaskHandle;

SemaphoreHandle_t MutexRoc;
SemaphoreHandle_t MutexFreq;
SemaphoreHandle_t MutexMode;
SemaphoreHandle_t SemaphoreButton;

// Globals
OpMode currentMode;
float frequencyThreshold, rocThreshold;

/*Functions */
void createVariables();

#endif /* VARIABLES_H_ */
