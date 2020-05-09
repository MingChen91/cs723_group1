#include "interrupts.h"
#include "altera_avalon_pio_regs.h" // to use PIO functions
#include "sys/alt_irq.h"            // to register interrupts
#include "variables.h"              // to access queues
#include <stdio.h>

#include "altera_up_avalon_ps2.h"
#include "altera_up_ps2_keyboard.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/*
	Button used to change the current mode
*/
void buttonIsr()
{
    xSemaphoreGiveFromISR(semaphoreButton, NULL);           // Gives a semaphore for the button task, used for syncing
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7); // Clears edge cap for buttons
}

/*
	Reads the ADC, used to calculate frequencies
*/
void freqAdcIsr()
{
    QFreqStruct freqQueueItem;
    freqQueueItem.adcSampleCount = IORD(FREQUENCY_ANALYSER_BASE, 0);
    freqQueueItem.isrTickCount = xTaskGetTickCountFromISR();
    xQueueSendFromISR(qFreq, &freqQueueItem, pdFALSE);
}

/*
	Reads bytes from the keyboard, only sends ascii values to the queue and enter as a null terminator.
*/
void keyboardIsr(void *ps2Keyboard)
{
    static uint8_t debounceCounter = 0;
    unsigned char ascii;
    int decodeStatus = 0;
    unsigned char makeCode = 0;
    const unsigned char nullTerminator = '\0';
    KB_CODE_TYPE decodeMode;

    decodeStatus = decode_scancode(ps2Keyboard, &decodeMode, &makeCode, &ascii);
    if ((decodeStatus == 0) && (debounceCounter >= 2)) //success
    {
        debounceCounter = 0;
        switch (decodeMode)
        {
            case KB_ASCII_MAKE_CODE:
                xQueueSendFromISR(qKeyBoard, &ascii, NULL);
                break;
            case KB_BINARY_MAKE_CODE:
                if (makeCode == ENTER_KEY) // enter indicates end of input sequence
                    xQueueSendFromISR(qKeyBoard, &nullTerminator, NULL);
                break;
            default:
                break;
        }
    }
    else
    {
        debounceCounter++;
    }
}

/* 
	Registers all the interrupts
*/
void registerInterrupts()
{
    alt_up_ps2_dev *ps2Keyboard = alt_up_ps2_open_dev(PS2_NAME);
    if (ps2Keyboard == NULL)
    {
        printf("can't find PS/2 device\n");
        return;
    }

    alt_up_ps2_clear_fifo(ps2Keyboard);                     // Clear keyboard fifo buffer
    alt_irq_register(PS2_IRQ, ps2Keyboard, keyboardIsr);    // Register keyboard isr
    IOWR_8DIRECT(PS2_BASE, 4, 1);                           // Writes to RE bit in the control reg of ps2. enables interrupts
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7); // Clear the edge cap reg of buttons
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PUSH_BUTTON_BASE, 0x7); // Enable for all buttons
    alt_irq_register(PUSH_BUTTON_IRQ, NULL, buttonIsr);
    alt_irq_register(FREQUENCY_ANALYSER_IRQ, NULL, freqAdcIsr);
}
