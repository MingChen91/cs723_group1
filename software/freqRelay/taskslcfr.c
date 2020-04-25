#include "taskslcfr.h"
#include "alt_types.h"
#include "altera_avalon_pio_regs.h" // to use PIO functions
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system.h>
/*
    Use:
    Gets information from frequency relay ISR
    Calculates rate of change, frequency

    Starts Load Management task and sleeps switch polling if a violation has
   occured.
*/
void frequencyViolationTask()
{
    // Queues
    QFreqStruct inFreqItem;
    QInformationStruct outInformationItem;
    QViolationStruct outViolationItem;
    // Local Vars
    uint16_t sampleCountNew, sampleCountOld;
    uint8_t violationOccured;
    float frequencyNew, frequencyOld, rateOfChange;
    // First Run, Collect data so have 2 data points to calculate averages
    if (xQueueReceive(qFreq, &inFreqItem, portMAX_DELAY))
    {
        sampleCountNew = inFreqItem.adcSampleCount;
        frequencyNew = (SAMPLING_FREQUENCY / (float)sampleCountNew);
    }

    // Loop
    while (1)
    {
        if (xQueueReceive(qFreq, &inFreqItem, portMAX_DELAY) == pdTRUE)
        {
            // Keep track of new and old sets of frequency / ROC.
            sampleCountOld = sampleCountNew;
            sampleCountNew = inFreqItem.adcSampleCount;
            frequencyOld = frequencyNew;
            frequencyNew = (SAMPLING_FREQUENCY / (float)sampleCountNew);
            rateOfChange = ((frequencyNew - frequencyOld) * 2 * SAMPLING_FREQUENCY) /
                           (sampleCountNew + sampleCountOld);

            // Send frequency/ROC data to information task
            outInformationItem.informationType = FREQ;
            outInformationItem.value = frequencyNew;
            xQueueSend(qInformation, &outInformationItem, pdFALSE);
            outInformationItem.informationType = ROC;
            outInformationItem.value = rateOfChange;
            xQueueSend(qInformation, &outInformationItem, pdFALSE);

            // Calculated if there's a violation.
            violationOccured = ((frequencyNew < frequencyThreshold) ||
                                (abs(rateOfChange) > rocThreshold));

            switch (currentMode)
            {
                // Stable Mode, Check if need to start load management
                case STABLE:
                    if (violationOccured)
                    {
                        // Change the mode and send to information task to display
                        if (xSemaphoreTake(xMutexMode, portMAX_DELAY))
                        {
                            currentMode = LOAD_MANAGEMENT;
                            xSemaphoreGive(xMutexMode);
                        }
                        outInformationItem.informationType = MODE;
                        outInformationItem.value = currentMode;
                        xQueueSend(qInformation, &outInformationItem, pdFALSE);
                        // Sleep switchPollingTask and start load management task
                        vTaskSuspend(vSwitchPollingTaskHandle);
                        xTaskCreate(loadManagementTask, "loadManagementTask",
                                    configMINIMAL_STACK_SIZE, NULL, 4,
                                    vLoadManagementTaskHandle);
                        // Pass the tickcount from the frequency ISR to
                        // loadManagementTask
                        outViolationItem.isrTickCount = inFreqItem.isrTickCount;
                        outViolationItem.violationOccured = violationOccured;
                        xQueueSend(qViolation, &outViolationItem, pdFALSE);
                    }
                    break;
                // Load Management just check if violation has occured and pass on
                // isr tick count
                case LOAD_MANAGEMENT:
                    outViolationItem.violationOccured = violationOccured;
                    outViolationItem.isrTickCount = inFreqItem.isrTickCount;
                    xQueueSend(qViolation, &outViolationItem, pdFALSE);
                    break;

                default:
                    break;
            }
        }
    }
}

/*
    Use:
    Retrieves information that needs to be displayed from different tasks
    Keeps track of min, max, and past execution times from isr trigger to load
   shed. Encodes the information and sends to serial port.

*/
void informationTask()
{
    // Queues
    QInformationStruct incomingInformationItem;
    // Serial UART init
    FILE *serialUart;
    serialUart = fopen("/dev/uart", "w");
    if (serialUart != NULL)
    {
        printf("   UART CONNECTED\n");
    }
    else
    {
        printf("  ERROR CONNECTING TO UART\n");
        return;
    }

    while (1)
    {
        if (xQueueReceive(qInformation, &incomingInformationItem, portMAX_DELAY))
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
                    fprintf(serialUart, "M%d\n", (int)incomingInformationItem.value);
                    break;
                case EXEC_TIME:
                    // calculate highest, lowest , average, total
                    break;
                case ROC_THRESH:
                    fprintf(serialUart, "R%f\n", incomingInformationItem.value);
                    break;
                case FREQ_TRHESH:
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
#define RED_LEDS_CURRENT     0xff & IORD(RED_LEDS_BASE, 0)
#define GREEN_LEDS_CURRENT   0xff & IORD(GREEN_LEDS_BASE, 0)
#define SLIDE_SWITCH_CURRENT 0xff & IORD(SLIDE_SWITCH_BASE, 0)

/* Helper Function to set left most unset bit*/
int setLeftMostUnsetBit(int n)
{
    // if all number is 1
    if ((n & (n + 1)) == 0)
        return n;

    // Number of digits in binary
    int digits = 7;
    int i;

    // Loop through every binary digit
    for (i = 0; i <= digits; i++)
    {
        if (((n >> (digits - i)) & 1) == 0) // Count from the left, if the digit is 0
        {
            return (n | (1 << (digits - i))); // set that digit to 1
        }
    }
    return -1; // error out
}

void loadManagementTask()
{
    // State
    typedef enum
    {
        INIT,
        DISCONNECT_LOAD,
        CONNECT_LOAD,
        FINISH
    } State;
    State currentState = INIT;

    // Vars
    uint8_t swPosMax, swPosCurrent, swPosPrev;
    uint8_t loadState, loadNew, loadChange;
    TickType_t loadManagementDelayStartTick;

    // Queues
    QViolationStruct inViolationItem;
    QLedStruct outLedItem;
    QInformationStruct outInformationItem;
    uint32_t executionTime;

    // Initialise Switch Position record
    swPosMax = SLIDE_SWITCH_CURRENT;
    swPosPrev = SLIDE_SWITCH_CURRENT;
    swPosCurrent = SLIDE_SWITCH_CURRENT;
    outLedItem.ledg = 0;

    while (1)
    {
        if (xQueueReceive(qViolation, &inViolationItem, portMAX_DELAY))
        {
            // Check if any switches have changed since last time
            swPosPrev = swPosCurrent;
            swPosCurrent = SLIDE_SWITCH_CURRENT;
            loadState = RED_LEDS_CURRENT;

            if (swPosCurrent != swPosPrev)
            {
                if (currentState == DISCONNECT_LOAD)
                {
                    loadChange = swPosMax ^ swPosCurrent;               // Which one got turned changed.
                    outLedItem.ledg = GREEN_LEDS_CURRENT & ~loadChange; // Turn off any matching green.
                    swPosMax = swPosMax & swPosCurrent;                 // Mask switch with the max allowed switch positions, allows turn load off but not on.
                }
                else if (currentState == CONNECT_LOAD)
                {
                    swPosMax = swPosCurrent; // In reconnect mode allow more to go up.
                }
                outLedItem.ledr = (swPosMax & loadState);
                xQueueSend(qLed, &outLedItem, portMAX_DELAY);

                // Reset the 500 ms count because conditions have changed
                loadManagementDelayStartTick = xTaskGetTickCount();
            }

            // State machine, for disconnecting and reconnecting loads
            switch (currentState)
            {
                // Immediately shed one load and go to Shed load state where loads will continue to shed every 500ms if unstable
                case INIT:
                    // Check execution time
                    executionTime = xTaskGetTickCount() - inViolationItem.isrTickCount;
                    if (executionTime > EXEC_TIME_LIMIT)
                        printf("WARNING : Load shed took longer than 200ms\n");
                    // Shed one load immediately
                    loadState = RED_LEDS_CURRENT;                          // Grab current load status
                    loadNew = (loadState & (loadState - 1));               // Turns off right most set bit (load)
                    outLedItem.ledr = loadNew;                             // Add to queue
                    loadChange = loadState - (loadNew);                    // Which load got turned off
                    outLedItem.ledg = ((GREEN_LEDS_CURRENT) | loadChange); // Turn on the correponding green led
                    xQueueSend(qLed, &outLedItem, portMAX_DELAY);          // Send to queue
                    currentState = DISCONNECT_LOAD;
                    loadManagementDelayStartTick = xTaskGetTickCount();
                    break;

                // Shed a load every 500 if unstable
                case DISCONNECT_LOAD:
                    if (inViolationItem.violationOccured) // Still unstable
                    {
                        if (xTaskGetTickCount() - loadManagementDelayStartTick >= LOAD_MANAGEMENT_DELAY)
                        {
                            loadState = RED_LEDS_CURRENT;                          // Grab current load status
                            loadNew = (loadState & (loadState - 1));               // Turns off right most set bit (load)
                            outLedItem.ledr = loadNew;                             // Add to queue
                            loadChange = loadState ^ loadNew;                      // Which load got turned off
                            outLedItem.ledg = ((GREEN_LEDS_CURRENT) | loadChange); // Turn on the corresponding green led
                            xQueueSend(qLed, &outLedItem, portMAX_DELAY);
                            loadManagementDelayStartTick = xTaskGetTickCount();
                        }
                    }
                    else // Changed to stable
                    {
                        currentState = CONNECT_LOAD;
                        loadManagementDelayStartTick = xTaskGetTickCount();
                    }
                    break;

                // Reconnect loads, including any switches which have been turned on during load management
                case CONNECT_LOAD:
                    if (!inViolationItem.violationOccured) // Still stable
                    {
                        if (xTaskGetTickCount() - loadManagementDelayStartTick >= LOAD_MANAGEMENT_DELAY)
                        {
                            loadState = RED_LEDS_CURRENT;
                            if (swPosMax == loadState) // All loads have been turned back on for 500 ms
                            {
                                currentState = FINISH;
                            }
                            else
                            {
                                uint8_t skip = (loadState | ~(SLIDE_SWITCH_CURRENT));         // Skip loads that are already on and if the switch is off.
                                loadNew = (setLeftMostUnsetBit(skip) & SLIDE_SWITCH_CURRENT); // Turn on the highest priority, and mask with switches
                                outLedItem.ledr = loadNew;                                    // Add to queue
                                loadChange = loadNew ^ loadState;                             // Which load was turned on
                                outLedItem.ledg = (GREEN_LEDS_CURRENT & ~(loadChange));       // Turn off the corresponding green led
                                xQueueSend(qLed, &outLedItem, portMAX_DELAY);
                                loadManagementDelayStartTick = xTaskGetTickCount();
                            }
                        }
                    }
                    else // Changed to unstable
                    {
                        currentState = DISCONNECT_LOAD;
                        loadManagementDelayStartTick = xTaskGetTickCount();
                    }
                    break;

                case FINISH:
                    if (xSemaphoreTake(xMutexMode, portMAX_DELAY))
                    {
                        currentMode = STABLE;
                        outInformationItem.informationType = MODE;
                        outInformationItem.value = currentMode;
                        xSemaphoreGive(xMutexMode);
                    }
                    xQueueSend(qInformation, &outInformationItem, portMAX_DELAY);
                    vTaskResume(vSwitchPollingTaskHandle);
                    vTaskDelete(NULL);
                    break;

                default:
                    break;
            }
        }
    }
}

/*
    Polls the switchs every SWITCH_POLLING_DELAY
    Sends the data to ledDriver task.
*/
void switchPollingTask()
{
    QLedStruct outgoingLedQueueItem;
    outgoingLedQueueItem.ledg = 0;
    while (1)
    {
        outgoingLedQueueItem.ledr = SLIDE_SWITCH_CURRENT; // bit masking to 8 bits
        xQueueSend(qLed, &outgoingLedQueueItem, pdFALSE);
        vTaskDelay(SWITCH_POLLING_DELAY);
    }
}

/*
    Gatekeeper task for writing to the LEDs.
    Writes to the read and green leds ("connect and disconnect loads")
*/
void ledDriverTask()
{
    QLedStruct inLedQueueItem;
    while (1)
    {
        if (xQueueReceive(qLed, &inLedQueueItem, portMAX_DELAY))
        {
            IOWR(RED_LEDS_BASE, 0, inLedQueueItem.ledr);
            IOWR(GREEN_LEDS_BASE, 0, inLedQueueItem.ledg);
        }
    }
}

/*
    Constructs a Float from a stream of chars.
    Only accept '0'-'9', '.' and null terminator.
*/
int8_t constructFloat(float *resultFloat, char inputChar, char *charBuffer)
{
    // Static, persist through calls.
    static uint8_t bufferIndex = 0;

    if (((inputChar < '0') || (inputChar > '9')) && (inputChar != '.') &&
        (inputChar != '\0'))
        return CONSTRUCT_FLOAT_NOT_DONE;

    charBuffer[bufferIndex] = inputChar;

    // Check if overflowing char buffer
    if (bufferIndex >= CHAR_BUFFER_SIZE - 1)
    {
        charBuffer[bufferIndex] = '\0';
    }

    // End of string, convert to float
    if (charBuffer[bufferIndex] == '\0')
    {
        *resultFloat = strtof(charBuffer, NULL);
        // Reset buffer
        for (bufferIndex = 0; bufferIndex < CHAR_BUFFER_SIZE; bufferIndex++)
        {
            charBuffer[bufferIndex] = '\0';
        }
        bufferIndex = 0;
        return CONSTRUCT_FLOAT_DONE;
    }

    printf("Buffer : %s\n", charBuffer);
    bufferIndex++;
    return CONSTRUCT_FLOAT_NOT_DONE;
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
    unsigned char inKeyboardQueueItem;
    QInformationStruct outInformationItem;
    char charBuffer[CHAR_BUFFER_SIZE] = {'\0'};
    float resultFloat = 0;
    int8_t buildStatus;

    while (1)
    {
        if (xQueueReceive(qKeyBoard, &inKeyboardQueueItem, portMAX_DELAY))
        {
            switch (state)
            {
                // Getting a F or a R starts the float construction from chars
                case IDLE:
                    switch (inKeyboardQueueItem)
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

                // Keeps building freq until done, then send to info task and go
                // back to idle
                case BUILDING_FREQ:
                    buildStatus =
                        constructFloat(&resultFloat, inKeyboardQueueItem, charBuffer);
                    if (buildStatus == CONSTRUCT_FLOAT_DONE)
                    {
                        if (xMutexFreq != NULL)
                        {
                            if (xSemaphoreTake(xMutexFreq, portMAX_DELAY))
                            {
                                frequencyThreshold = resultFloat;
                                xSemaphoreGive(xMutexFreq);
                            }
                        }
                        outInformationItem.informationType = FREQ_TRHESH;
                        outInformationItem.value = frequencyThreshold;
                        xQueueSend(qInformation, &outInformationItem, pdFALSE);
                        state = IDLE;
                    }
                    break;

                // Keeps building freq until done, then send to info task and go
                // back to idle
                case BUILDING_ROC:
                    buildStatus =
                        constructFloat(&resultFloat, inKeyboardQueueItem, charBuffer);
                    if (buildStatus == CONSTRUCT_FLOAT_DONE)
                    {
                        if (xMutexRoc != NULL)
                        {
                            if (xSemaphoreTake(xMutexRoc, portMAX_DELAY))
                            {
                                rocThreshold = abs(resultFloat);
                                xSemaphoreGive(xMutexRoc);
                            }
                        }
                        outInformationItem.informationType = ROC_THRESH;
                        outInformationItem.value = rocThreshold;
                        xQueueSend(qInformation, &outInformationItem, pdFALSE);
                        state = IDLE;
                    }
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
    xTaskCreate(informationTask, "informationTask", configMINIMAL_STACK_SIZE,
                NULL, 3, NULL);
    xTaskCreate(frequencyViolationTask, "frequencyViolationTask",
                configMINIMAL_STACK_SIZE, NULL, 5, NULL);
    xTaskCreate(switchPollingTask, "switchPollingTask", configMINIMAL_STACK_SIZE,
                NULL, 4, &vSwitchPollingTaskHandle);
    xTaskCreate(ledDriverTask, "ledDriverTask", configMINIMAL_STACK_SIZE, NULL, 2,
                NULL);
    xTaskCreate(keyboardTask, "keyboardTask", configMINIMAL_STACK_SIZE, NULL, 1,
                NULL);
    xTaskCreate(buttonTask, "buttonTask", configMINIMAL_STACK_SIZE, NULL, 2,
                NULL);
}
