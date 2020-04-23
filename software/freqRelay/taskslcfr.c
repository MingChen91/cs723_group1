/*
 * tasks.c
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alt_types.h"
#include "altera_avalon_pio_regs.h" // to use PIO functions
#include "sys/alt_timestamp.h"

#include "system.h" // for IORD()
#include "taskslcfr.h"

void frequencyViolationTask()
{
    QFreqStruct incomingFreqItem;
    QInformationStruct outgoingInformationItem;
    QViolationStruct outgoingViolationItem;
    uint16_t sampleCountNew, sampleCountOld;
    uint8_t violationOccured;
    float frequencyNew, frequencyOld, rateOfChange;

    // first run, collect data so have 2 data points to calculate averages
    if (xQueueReceive(qFreq, &incomingFreqItem, portMAX_DELAY) == pdTRUE)
    {
        sampleCountNew = incomingFreqItem.adcSampleCount;
        frequencyNew = (SAMPLING_FREQUENCY / (float)sampleCountNew);
    }

    while (1)
    {
        // Wait for item in queue.
        if (xQueueReceive(qFreq, &incomingFreqItem, portMAX_DELAY) == pdTRUE)
        {
            // Keep track of 2 sets of frequency data.
            sampleCountOld = sampleCountNew;
            sampleCountNew = incomingFreqItem.adcSampleCount;
            frequencyOld = frequencyNew;
            frequencyNew = (SAMPLING_FREQUENCY / (float)sampleCountNew);
            rateOfChange = ((frequencyNew - frequencyOld) * 2 * SAMPLING_FREQUENCY) / (sampleCountNew + sampleCountOld);

            // Send frequency data to display task
            outgoingInformationItem.informationType = FREQ;
            outgoingInformationItem.value = frequencyNew;
            xQueueSend(qInformation, &outgoingInformationItem, pdFALSE);

            outgoingInformationItem.informationType = ROC;
            outgoingInformationItem.value = rateOfChange;
            xQueueSend(qInformation, &outgoingInformationItem, pdFALSE);
            violationOccured = ((frequencyNew < frequencyThreshold) || (abs(rateOfChange) > rocThreshold));

            switch (currentMode)
            {
                case STABLE:
                    if (violationOccured)
                    {
                        if (xSemaphoreTake(xMutexMode, portMAX_DELAY))
                        {
                            currentMode = LOAD_MANAGEMENT;
                            xSemaphoreGive(xMutexMode);
                        }
                        outgoingInformationItem.informationType = MODE;
                        outgoingInformationItem.value = currentMode;
                        xQueueSend(qInformation, &outgoingInformationItem, pdFALSE);

                        vTaskSuspend(vSwitchPollingTaskHandle); // sleep polling
                        // Start task management
                        // TODO if state == not created.
                        xTaskCreate(loadManagementTask, "loadManagementTask", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
                        outgoingViolationItem.isrTickCount = incomingFreqItem.isrTickCount;
                        outgoingViolationItem.violationOccured = violationOccured;
                        xQueueSend(qViolation, &outgoingViolationItem, pdFALSE); // send to load management
                    }
                    break;

                case LOAD_MANAGEMENT:
                    outgoingViolationItem.violationOccured = violationOccured;
                    outgoingViolationItem.isrTickCount = incomingFreqItem.isrTickCount;
#if TIMESTAMPS
                    alt_32 tick = alt_timestamp();
                    if (tick == 0)
                    {
                        alt_timestamp_start();
                    }
                    printf("current tick is %lu\n", tick);
#endif
                    xQueueSend(qViolation, &outgoingViolationItem, pdFALSE); // send to load management
                    break;

                default:
                    break;
            }

#if DEBUG_FREQ
            printf("old freq %f\n", frequencyOld);
            printf("new freq %f\n", frequencyNew);
            printf("old count %d\n", sampleCountOld);
            printf("new count %d\n", sampleCountNew);
            printf("rate of change: %f\n\n", rateOfChange);
#endif /* DEBUG_FREQ */
        }
    }
}

/*
        displaying information to UART
*/
void informationTask()
{
    // create a seperate queue for serial
    QInformationStruct incomingInformationItem;
    FILE *serialUart;
    serialUart = fopen("/dev/uart", "w");
    if (serialUart != NULL)
    {
        printf("\n***********************\n");
        printf("   UART CONNECTED");
        printf("\n***********************\n");
    }
    else
    {
        printf("\n***************************\n");
        printf("  ERROR CONNECTING TO UART");
        printf("\n***************************\n");
    }

    while (1)
    {
        if (xQueueReceive(qInformation, &incomingInformationItem,
                          portMAX_DELAY))
        {
            switch (incomingInformationItem.informationType)
            {
                case FREQ:
                    fprintf(serialUart, "f%f\n", incomingInformationItem.value);
                    break;
                case ROC:
                    fprintf(serialUart, "r%f\n", incomingInformationItem.value);
                    break;
                case MODE:
                    fprintf(serialUart, "M%d\n",
                            (int)incomingInformationItem.value);
                    break;
                case EXEC_TIME:
                    // calculate highest, lowest , average, total
                    break;
                case ROC_THRESH:
                    // Rate of Change Threshold sent to UART
                    fprintf(serialUart, "R%f\n", incomingInformationItem.value);
                    break;
                case FREQ_TRHESH:
                    // Frequenecy Threshold sent to UART.
                    fprintf(serialUart, "F%f\n", incomingInformationItem.value);
                    break;
                default:
                    break;
            }
        }
    }
    fclose(serialUart);
}

/*
        Load Management Task
*/
void loadManagementTask()
{
    uint8_t switchConfig;
    uint8_t litLeds = IORD(SLIDE_SWITCH_BASE, 0);
    TickType_t startTick;
    QViolationStruct incomingViolationItem;
    QLedStruct outgoingLedItem;
    typedef enum
    {
        INIT,
        SHED,
        UNSHED,
        FINISH
    } State;
    State currentState = INIT;

    while (1)
    {
        if (xQueueReceive(qViolation, &incomingViolationItem, portMAX_DELAY))
        {
            switch (currentState)
            {
                case INIT:
                    // switchConfig = IORD(SLIDE_SWITCH_BASE, 0);
                    litLeds = litLeds & litLeds - 1;
                    IOWR(SLIDE_SWITCH_BASE, 0, litLeds);
                    // TickType_t a = xTaskGetTickCount();
                    // printf("tick count from isr: %u \n", incomingViolationItem.isrTickCount);
                    // printf("tick count after shed: %u\n", a);
                    // uint32_t b = a - incomingViolationItem.isrTickCount;
                    break;
                case SHED:
                    break;
                case UNSHED:
                    break;
                case FINISH:
                    break;
                default:
                    break;
            }
            outgoingLedItem.ledr = litLeds;
            xQueueSend(qLed, &outgoingLedItem, pdFALSE);
        }
    }
}

/*
        Polls the switchs
*/
void switchPollingTask()
{
    QLedStruct sendLedQueueItem;
    sendLedQueueItem.ledg = 0;
    while (1)
    {
        sendLedQueueItem.ledr = (IORD(SLIDE_SWITCH_BASE, 0) & 0xff); // bit masking to 8 bits
        xQueueSend(qLed, &sendLedQueueItem, pdFALSE);
        vTaskDelay(100);
    }
}

/*
        Driving LEDs
*/
void ledDriverTask()
{
    QLedStruct receiveLedQueueItem;
    while (1)
    {
        if (xQueueReceive(qLed, &receiveLedQueueItem, portMAX_DELAY))
        {
            IOWR(RED_LEDS_BASE, 0, receiveLedQueueItem.ledr);
            IOWR(GREEN_LEDS_BASE, 0, receiveLedQueueItem.ledg);
        }
    }
}

/*
        Building Freq and ROC from keyboard ISR individual chars
*/
#define BUFFER_SIZE 10
int8_t buildNumber(float *resultFloat, char inputChar, char *charBuffer)
{
    // Keeps track of which which element in char array
    static uint8_t bufferIndex = 0;

    // Checks if input is valid, needs to be 0-9 , or '.' or null terminator
    if (((inputChar < '0') || (inputChar > '9')) && (inputChar != '.') && (inputChar != '\0'))
        return 1;

    // New input gets put into the buffer
    charBuffer[bufferIndex] = inputChar;

    // Check if overflowing char buffer
    if (bufferIndex >= BUFFER_SIZE - 1) // last char needed for null terminator
    {
        charBuffer[bufferIndex] = '\0';
    }

    // End of string, convert to float
    if (charBuffer[bufferIndex] == '\0')
    {
        // Convert the string to float
        *resultFloat = strtof(charBuffer, NULL);
        // reset buffer
        for (bufferIndex = 0; bufferIndex < BUFFER_SIZE; bufferIndex++)
        {
            charBuffer[bufferIndex] = '\0';
        }
        bufferIndex = 0;
        return 0;
    }

    printf("Buffer : %s\n", charBuffer);
    bufferIndex++;
    return 1;
}

/*
        Taking inputs from the keyboard and changing thresholds
*/
void keyboardTask()
{
    enum KeyBoardState
    {
        IDLE,
        BUILDING_FREQ,
        BUILDING_ROC
    };
    enum KeyBoardState state = IDLE;
    unsigned char receieveKeyboardQueueItem;
    char charBuffer[BUFFER_SIZE] = {'\0'};
    float resultFloat = 0;
    int8_t buildStatus;
    QInformationStruct outGoingInformation;
    while (1)
    {
        if (xQueueReceive(qKeyBoard, &receieveKeyboardQueueItem, portMAX_DELAY))
        {
            switch (state)
            {
                case IDLE:
                    switch (receieveKeyboardQueueItem)
                    {
                        case 'F':
                            printf("Building Frequency\n");
                            state = BUILDING_FREQ;
                            break;
                        case 'R':
                            printf("Building Rate of Change\n");
                            state = BUILDING_ROC;
                            break;
                        default:
                            break;
                    }
                    break;
                case BUILDING_FREQ:
                    buildStatus = buildNumber(&resultFloat, receieveKeyboardQueueItem, charBuffer);
                    if (buildStatus == 0)
                    {
                        if (xMutexFreq != NULL)
                        {
                            if (xSemaphoreTake(xMutexFreq, portMAX_DELAY))
                            {
                                frequencyThreshold = resultFloat;
                                xSemaphoreGive(xMutexFreq);
                            }
                        }
                        // send to information queue to read.
                        outGoingInformation.informationType = FREQ_TRHESH;
                        outGoingInformation.value = frequencyThreshold;
                        xQueueSend(qInformation, &outGoingInformation, pdFALSE);
                        state = IDLE;
                    }
                    // build temp buffer and change freq
                    break;
                case BUILDING_ROC:
                    buildStatus = buildNumber(&resultFloat, receieveKeyboardQueueItem, charBuffer);
                    if (buildStatus == 0)
                    {
                        if (xMutexRoc != NULL)
                        {
                            if (xSemaphoreTake(xMutexRoc, portMAX_DELAY))
                            {
                                rocThreshold = abs(resultFloat);
                                xSemaphoreGive(xMutexRoc);
                            }
                        }
                        outGoingInformation.informationType = ROC_THRESH;
                        outGoingInformation.value = rocThreshold;
                        xQueueSend(qInformation, &outGoingInformation, pdFALSE);
                        state = IDLE;
                    }
                    // build temp buffer and change ROC
                    break;
                default:
                    break;
            }
        }
    }
}

/*
        Button to change to maintence mode.
*/
void buttonTask()
{
    QInformationStruct outgoingInfoItem;
    while (1)
    {
        if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY))
        {
            if (xSemaphoreTake(xMutexMode, portMAX_DELAY))
            {
                if (currentMode == STABLE)
                {
                    currentMode = MAINTANENCE;
                }
                else if (currentMode == MAINTANENCE)
                {
                    currentMode = STABLE;
                }
                outgoingInfoItem.value = currentMode;
                xSemaphoreGive(xMutexMode);
            }
            outgoingInfoItem.informationType = MODE;
            xQueueSend(qInformation, &outgoingInfoItem, pdFALSE);
        }
    }
}

void createTasks()
{
    xTaskCreate(informationTask, "informationTask", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
#if TIMESTAMPS
    if (alt_timestamp_start() < 0)
    {
        printf("no time stamp device available\n");
    }
    else
    {
        printf("the frequency is %lu\n", alt_timestamp_freq());
    }
#endif
    xTaskCreate(frequencyViolationTask, "frequencyViolationTask", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
    xTaskCreate(switchPollingTask, "switchPollingTask", configMINIMAL_STACK_SIZE, NULL, 4, &vSwitchPollingTaskHandle);
    xTaskCreate(ledDriverTask, "ledDriverTask", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(keyboardTask, "keyboardTask", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(buttonTask, "buttonTask", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
}
