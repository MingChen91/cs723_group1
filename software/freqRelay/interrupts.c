/*
 * interrupts.c
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#include "altera_avalon_pio_regs.h" // to use PIO functions
#include "sys/alt_irq.h"			// to register interrupts
#include "variables.h"				// to access queues
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/*
	Button used to change the current mode
*/
void button_isr()
{
	if (xSemaphoreTakeFromISR(modeMutex, (TickType_t)10) == pdTRUE)
	{
		if ((currentMode == 0) || (currentMode == 1))
		{
			currentMode = 2;
		}
		else
		{
			currentMode = 0;
		}
		xSemaphoreGiveFromISR(modeMutex, pdFALSE);
	}
	printf("%d\n\n", currentMode);
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7);
}

/*
	Reads the ADC, used to calculate frequencies
*/
void freq_adc_isr()
{
	QFreqStruct freqQueueItem;
	freqQueueItem.adcCount = IORD(FREQUENCY_ANALYSER_BASE, 0);
	freqQueueItem.isrTickCount = xTaskGetTickCountFromISR();
	xQueueSendFromISR(qFreq, &freqQueueItem, pdFALSE);
}

/* 
	Registers all the interrupts
*/
void resgisterInterrupts()
{
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7); // clear the edge cap reg of buttons
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PUSH_BUTTON_BASE, 0x7); // enable for all buttons
	alt_irq_register(PUSH_BUTTON_IRQ, NULL, button_isr);
	alt_irq_register(FREQUENCY_ANALYSER_IRQ, NULL, freq_adc_isr);
}
