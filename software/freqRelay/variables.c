#include "variables.h"

void createVariables()
{
    // Queues and Semaphores
    qFreq = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(QFreqStruct));
    qLed = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(QLedStruct));
    qInformation = xQueueCreate(QUEUE_SIZE_BIG, sizeof(QInformationStruct));
    qViolation = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(QViolationStruct));
    qKeyBoard = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(unsigned char));
    xButtonSemaphore = xSemaphoreCreateCounting(5, 0);

    // Mutex lock for Global Vars
    xMutexFreq = xSemaphoreCreateMutex();
    xMutexRoc = xSemaphoreCreateMutex();
    xMutexMode = xSemaphoreCreateMutex();

    // Global variables
    currentMode = STABLE;
    frequencyThreshold = 40;
    rocThreshold = 30;
}
