#include "taskslcfr.h"
#include "alt_types.h"
#include "altera_avalon_pio_regs.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <system.h>

/*
    Use:
    Gets information from frequency relay ISR
    Calculates rate of change, frequency
    Starts Load Management task and sleeps switch polling if a violation has occured.
*/
void frequencyViolationTask()
{
    // Queues
    QFreqStruct inFreqItem;
    QInformationStruct outInformationItem;
    QViolationStruct outViolationItem;
    // Local Variabless
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
            rateOfChange = ((frequencyNew - frequencyOld) * 2 * SAMPLING_FREQUENCY) / (sampleCountNew + sampleCountOld);

            // Send frequency/ROC data to information task
            outInformationItem.informationType = FREQ;
            outInformationItem.value = frequencyNew;
            xQueueSend(qInformation, &outInformationItem, pdFALSE);
            outInformationItem.informationType = ROC;
            outInformationItem.value = rateOfChange;
            xQueueSend(qInformation, &outInformationItem, pdFALSE);

            // Calculated if there's a violation.
            violationOccured = ((frequencyNew < frequencyThreshold) || ((float)(fabs(rateOfChange)) > rocThreshold));

            switch (currentMode)
            {
                // Stable Mode, Check if need to start load management
                case STABLE:
                    if (violationOccured)
                    {
                        // Change the mode and send to information task to display
                        if (xSemaphoreTake(mutexMode, portMAX_DELAY))
                        {
                            currentMode = LOAD_MANAGEMENT;
                            xSemaphoreGive(mutexMode);
                        }
                        outInformationItem.informationType = MODE;
                        outInformationItem.value = currentMode;
                        xQueueSend(qInformation, &outInformationItem, pdFALSE);
                        // Sleep switchPollingTask and start load management task
                        vTaskSuspend(switchPollingTaskHandle);
                        xTaskCreate(loadManagementTask, "loadManagementTask", configMINIMAL_STACK_SIZE, NULL, 4, loadManagementTaskHandle);
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
    Keeps track of min, max, and past execution times from isr trigger to load shed. 
    Encodes the information strng and sends to serial port. 
    First characters is used to differenentiate the type of message , then the value is followed.
    Encoding : 
    Z  = on time
    f = current frequency
    r = current rate of change
    M = current mode 
    t = execution times
    Tm = min exec time
    TM = max exec time
    R = rate of change threshold
    F = frequency threshold
*/
void informationTask()
{
    // Queues
    QInformationStruct incomingInformationItem;
    uint32_t ticktime = 0;
    uint8_t min, sec;
    int execTimes[SHIFT_REG_LEN] = {0};
    int i; // index counter for execTimes[]
    uint8_t maxExecTime = 0;
    uint8_t minExecTime = 0xff;

    // Serial UART init
    FILE *serialUart;
    serialUart = fopen("/dev/uart", "w");

    if (serialUart == NULL)
    {
        printf("ERROR CONNECTING TO UART\n");
        return;
    }

    // Loop
    while (1)
    {
        if (xQueueReceive(qInformation, &incomingInformationItem, portMAX_DELAY))
        {
            if (xTaskGetTickCount() - ticktime >= MS_TO_SEC) // send on time every second
            {
                min = xTaskGetTickCount() / (MS_TO_SEC * SEC_TO_MIN);
                sec = xTaskGetTickCount() / MS_TO_SEC % SEC_TO_MIN;
                fprintf(serialUart, "ZLCFR has been operational for %d min %d sec.\n", min, sec);
                ticktime = xTaskGetTickCount(); // Reset the count
            }
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
                    // Keep track of past 5 values
                    shiftReg((int)incomingInformationItem.value, execTimes);
                    // Send array to uart in a format t1,2,3,4,5\n
                    fprintf(serialUart, "t");
                    for (i = 0; i < SHIFT_REG_LEN; i++)
                    {
                        fprintf(serialUart, "%d,", execTimes[i]);
                    }
                    fprintf(serialUart, "\n");
                    // Keep track of minimum execution time
                    if (incomingInformationItem.value <= minExecTime)
                    {
                        minExecTime = (uint8_t)incomingInformationItem.value;
                        fprintf(serialUart, "Tm%d\n", minExecTime);
                    }
                    // Keep track of maximum execution time
                    if (incomingInformationItem.value >= maxExecTime)
                    {
                        maxExecTime = (uint8_t)incomingInformationItem.value;
                        fprintf(serialUart, "TM%d\n", maxExecTime);
                    }
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
    Uses: Runs when in load management mode, Uses FSM to keep track and balance loads. 
    Turns off LSB load every 500 ms if unstable
    Turns on MSB load every 500 ms if stable
    Switches between states if not consistent condition for 500ms. 
    When all loads are connected and turned back on finish this task. 
    When this task is started switchPollingTask is suspended, this task resumes switchPolling when finished.
*/
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

    // Initialise Switch Position record
    swPosMax = SLIDE_SWITCH_CURRENT;
    swPosPrev = SLIDE_SWITCH_CURRENT;
    swPosCurrent = SLIDE_SWITCH_CURRENT;
    outLedItem.ledG = 0;

    // Loop
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
                    loadChange = (swPosCurrent ^ swPosPrev);            // Which one got turned changed.
                    outLedItem.ledG = GREEN_LEDS_CURRENT & ~loadChange; // Turn off any matching green.
                    swPosMax = swPosMax & swPosCurrent;                 // Mask switch with the max allowed switch positions, allows turn load off but not on.
                }
                else if (currentState == CONNECT_LOAD)
                {
                    swPosMax = swPosCurrent; // In reconnect mode allow more to go up.
                }
                outLedItem.ledR = (swPosMax & loadState);
                outLedItem.isrTickCount = FROM_SWITCH_POLLING;
                xQueueSend(qLed, &outLedItem, portMAX_DELAY);

                // Reset the 500 ms count because conditions have changed
                loadManagementDelayStartTick = xTaskGetTickCount();
            }

            // State machine, for disconnecting and reconnecting loads
            switch (currentState)
            {
                // Immediately shed one load and go to Shed load state where loads will continue to shed every 500ms if unstable
                case INIT:
                    loadState = RED_LEDS_CURRENT;                           // Grab current load status
                    loadNew = (loadState & (loadState - 1));                // Turns off right most set bit (load)
                    outLedItem.ledR = loadNew;                              // Add to queue
                    loadChange = loadState ^ loadNew;                       // Which load got turned
                    outLedItem.ledG = ((GREEN_LEDS_CURRENT) | loadChange);  // Turn on the correponding green led
                    outLedItem.isrTickCount = inViolationItem.isrTickCount; // Pass tick count along for exec time calculations
                    xQueueSend(qLed, &outLedItem, portMAX_DELAY);           // Send to queue
                    currentState = DISCONNECT_LOAD;
                    loadManagementDelayStartTick = xTaskGetTickCount();
                    break;

                // Shed a load every 500 if unstable
                case DISCONNECT_LOAD:
                    if (inViolationItem.violationOccured) // Still unstable
                    {
                        if (xTaskGetTickCount() - loadManagementDelayStartTick >= LOAD_MANAGEMENT_DELAY)
                        {
                            if (RED_LEDS_CURRENT == 0) // If all the Loads have been turned off, reset timer to wait for 500 ms before checking again
                            {
                                loadManagementDelayStartTick = xTaskGetTickCount;
                                continue;
                            }
                            loadState = RED_LEDS_CURRENT;                           // Grab current load status
                            loadNew = (loadState & (loadState - 1));                // Turns off right most set bit (load)
                            outLedItem.ledR = loadNew;                              // Add to queue
                            loadChange = loadState ^ loadNew;                       // Which load got turned off
                            outLedItem.ledG = ((GREEN_LEDS_CURRENT) | loadChange);  // Turn on the corresponding green led
                            outLedItem.isrTickCount = inViolationItem.isrTickCount; // Pass tick count along for exec time calculations
                            xQueueSend(qLed, &outLedItem, portMAX_DELAY);           // Send to queue
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
                            if (SLIDE_SWITCH_CURRENT == loadState) // All loads have been turned back on
                            {
                                currentState = FINISH;
                            }
                            else
                            {
                                uint8_t skip = (loadState | ~(SLIDE_SWITCH_CURRENT));         // Skip loads that are already on and if the switch is off.
                                loadNew = (setLeftMostUnsetBit(skip) & SLIDE_SWITCH_CURRENT); // Turn on the highest priority, and mask with switches
                                outLedItem.ledR = loadNew;                                    // Add to queue
                                loadChange = loadNew ^ loadState;                             // Which load was turned on
                                outLedItem.ledG = (GREEN_LEDS_CURRENT & ~(loadChange));       // Turn off the corresponding green led
                                outLedItem.isrTickCount = inViolationItem.isrTickCount;       // Pass tick count along for exec time calculations
                                xQueueSend(qLed, &outLedItem, portMAX_DELAY);                 // Send to queue
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

                // Load management finished. Sent current mode to STABLE , send to information task
                // Resume switch polling and delete this task.
                case FINISH:
                    if (xSemaphoreTake(mutexMode, portMAX_DELAY))
                    {
                        currentMode = STABLE;
                        outInformationItem.informationType = MODE;
                        outInformationItem.value = currentMode;
                        xSemaphoreGive(mutexMode);
                    }
                    xQueueSend(qInformation, &outInformationItem, portMAX_DELAY);
                    vTaskResume(switchPollingTaskHandle);
                    vTaskDelete(NULL);
                    break;

                default:
                    break;
            }
        }
    }
}

/*
    Polls the switchs every SWITCH_POLLING_DELAY (currently 100ms)
    Sends the data to ledDriver task.
*/
void switchPollingTask()
{
    QLedStruct outgoingLedQueueItem;
    outgoingLedQueueItem.ledG = FROM_SWITCH_POLLING;
    outgoingLedQueueItem.isrTickCount = FROM_SWITCH_POLLING;

    // Loop
    while (1)
    {
        outgoingLedQueueItem.ledR = SLIDE_SWITCH_CURRENT;
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
    QInformationStruct outInformationItem;
    uint32_t executionTime;

    // Loop
    while (1)
    {
        if (xQueueReceive(qLed, &inLedQueueItem, portMAX_DELAY))
        {
            if (inLedQueueItem.isrTickCount != FROM_SWITCH_POLLING)
            {
                executionTime = xTaskGetTickCount() - inLedQueueItem.isrTickCount;
                outInformationItem.informationType = EXEC_TIME;
                outInformationItem.value = (float)executionTime;
                xQueueSend(qInformation, &outInformationItem, pdFALSE);
            }
            IOWR(RED_LEDS_BASE, NO_OFF_SET, inLedQueueItem.ledR);
            IOWR(GREEN_LEDS_BASE, NO_OFF_SET, inLedQueueItem.ledG);
        }
    }
}

/*
    Taking inputs from the keyboard and changing thresholds
    pressing 'f' starts the frequency building process
    pressing 'r' starts the rate of change building process
    "enter" finishes the process and converts string to float and sets the respective thresholds
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

                // Keeps building freq until done, then send to info task and go back to idle
                case BUILDING_FREQ:
                    buildStatus = constructFloat(&resultFloat, inKeyboardQueueItem, charBuffer);
                    if (buildStatus == CONSTRUCT_FLOAT_DONE)
                    {
                        if (mutexFreq != NULL)
                        {
                            if (xSemaphoreTake(mutexFreq, portMAX_DELAY))
                            {
                                frequencyThreshold = resultFloat;
                                xSemaphoreGive(mutexFreq);
                            }
                        }
                        outInformationItem.informationType = FREQ_TRHESH;
                        outInformationItem.value = frequencyThreshold;
                        xQueueSend(qInformation, &outInformationItem, pdFALSE);
                        printf("New Freq Threshold: %.4f\n", frequencyThreshold);
                        state = IDLE;
                    }
                    break;

                // Keeps building freq until done, then send to info task and go back to idle
                case BUILDING_ROC:
                    buildStatus = constructFloat(&resultFloat, inKeyboardQueueItem, charBuffer);
                    if (buildStatus == CONSTRUCT_FLOAT_DONE)
                    {
                        if (mutexRoc != NULL)
                        {
                            if (xSemaphoreTake(mutexRoc, portMAX_DELAY))
                            {
                                rocThreshold = (float)(fabs(resultFloat));
                                xSemaphoreGive(mutexRoc);
                            }
                        }
                        outInformationItem.informationType = ROC_THRESH;
                        outInformationItem.value = rocThreshold;
                        xQueueSend(qInformation, &outInformationItem, pdFALSE);
                        printf("New ROC Threshold: %.4f\n", rocThreshold);
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
    Button to change to maintenance mode.
*/
void buttonTask()
{
    QInformationStruct outgoingInfoItem;
    while (1)
    {
        if (xSemaphoreTake(semaphoreButton, portMAX_DELAY))
        {
            if (xSemaphoreTake(mutexMode, portMAX_DELAY))
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
                xSemaphoreGive(mutexMode);
            }
            outgoingInfoItem.informationType = MODE;
            xQueueSend(qInformation, &outgoingInfoItem, pdFALSE);
        }
    }
}

/*
    Initialise the variables and send to information task to display runs once only
*/
void initVariablesTask()
{
    QInformationStruct outInformationItem;
    // Global variables
    if (xSemaphoreTake(mutexMode, portMAX_DELAY))
    {
        currentMode = STABLE;
        frequencyThreshold = FREQ_INIT;
        rocThreshold = ROC_INIT;
        xSemaphoreGive(mutexMode);
    }

    // Send all the information to be displayed on the labview program
    outInformationItem.informationType = MODE;
    outInformationItem.value = STABLE;
    xQueueSend(qInformation, &outInformationItem, portMAX_DELAY);

    outInformationItem.informationType = FREQ_TRHESH;
    outInformationItem.value = frequencyThreshold;
    xQueueSend(qInformation, &outInformationItem, portMAX_DELAY);

    outInformationItem.informationType = ROC_THRESH;
    outInformationItem.value = rocThreshold;
    xQueueSend(qInformation, &outInformationItem, portMAX_DELAY);

    vTaskDelete(NULL); // End task
}

void createTasks()
{
    xTaskCreate(informationTask, "informationTask", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
    xTaskCreate(frequencyViolationTask, "frequencyViolationTask", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
    xTaskCreate(switchPollingTask, "switchPollingTask", configMINIMAL_STACK_SIZE, NULL, 4, &switchPollingTaskHandle);
    xTaskCreate(ledDriverTask, "ledDriverTask", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(keyboardTask, "keyboardTask", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(buttonTask, "buttonTask", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(initVariablesTask, "initVariableTask", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}
