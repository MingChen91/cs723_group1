#ifndef VARIABLES_H_
#define VARIABLES_H_

#define QUEUE_SIZE_SMALL 20
#define QUEUE_SIZE_BIG   100
#define FREQ_INIT        45
#define ROC_INIT         30

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* Enums */
typedef enum
{
    FREQ = 1,
    ROC = 2,
    MODE = 3,
    EXEC_TIME = 4,
    ROC_THRESH = 5,
    FREQ_TRHESH = 6
} InformationType;

typedef enum
{
    STABLE = 1,
    LOAD_MANAGEMENT = 2,
    MAINTANENCE = 3
} OpMode;

/* Structs for queue items */
typedef struct
{
    uint16_t adcSampleCount;
    TickType_t isrTickCount;
} QFreqStruct;

typedef struct
{
    uint8_t ledR;
    uint8_t ledG;
    uint32_t isrTickCount;
} QLedStruct;

typedef struct
{
    uint8_t violationOccured;
    TickType_t isrTickCount;
} QViolationStruct;

typedef struct
{
    InformationType informationType;
    float value;
} QInformationStruct;

/* Handles */
QueueHandle_t qFreq;
QueueHandle_t qLed;
QueueHandle_t qInformation;
QueueHandle_t qKeyBoard;
QueueHandle_t qViolation;

TaskHandle_t loadManagementTaskHandle;
TaskHandle_t switchPollingTaskHandle;
TaskHandle_t initVariablesTaskHandle;

SemaphoreHandle_t mutexRoc;
SemaphoreHandle_t mutexFreq;
SemaphoreHandle_t mutexMode;
SemaphoreHandle_t semaphoreButton;

/* Globals */
OpMode currentMode;
float frequencyThreshold, rocThreshold;

/*Functions */
void createVariables();

#endif /* VARIABLES_H_ */
