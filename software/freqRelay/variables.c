#include "variables.h"

void createVariables()
{
    // Queues and Semaphores
    qFreq = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(QFreqStruct));
    qLed = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(QLedStruct));
    qInformation = xQueueCreate(QUEUE_SIZE_BIG, sizeof(QInformationStruct));
    qViolation = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(QViolationStruct));
    qKeyBoard = xQueueCreate(QUEUE_SIZE_SMALL, sizeof(unsigned char));
    SemaphoreButton = xSemaphoreCreateCounting(5, 0);

    // Mutex lock for Global Vars
    MutexFreq = xSemaphoreCreateMutex();
    MutexRoc = xSemaphoreCreateMutex();
    MutexMode = xSemaphoreCreateMutex();
}
