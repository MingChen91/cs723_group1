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

#include "altera_up_avalon_ps2.h"
#include "altera_up_ps2_keyboard.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/*
	Button used to change the current mode
*/
void button_isr()
{
	if (xSemaphoreTakeFromISR(modeMutex, NULL))
	{
		if ((currentMode == NORMAL) || (currentMode == LOAD_MANAGEMENT))
		{
			currentMode = MAINTANENCE;
		}
		else
		{
			currentMode = NORMAL;
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
	freqQueueItem.adcSampleCount = IORD(FREQUENCY_ANALYSER_BASE, 0);
	freqQueueItem.isrTickCount = xTaskGetTickCountFromISR();
	xQueueSendFromISR(qFreq, &freqQueueItem, pdFALSE);
}

alt_up_ps2_dev *ps2_device;

void keyboard_isr(void *context, alt_u32 id)
{
	static uint8_t count = 0;
	char ascii;
	int status = 0;
	unsigned char key = 0;
	KB_CODE_TYPE decode_mode;
	status = decode_scancode(context, &decode_mode, &key, &ascii);
	if ((status == 0) && (count >= 2)) //success
	{
		count = 0;
		//print out the result
		switch (decode_mode)
		{
		case KB_ASCII_MAKE_CODE:
			printf("ASCII   : %c\n", ascii);
			break;
		case KB_LONG_BINARY_MAKE_CODE:
			// do nothing
		case KB_BINARY_MAKE_CODE:
			printf("MAKE CODE : %x\n", key);
			break;
		case KB_BREAK_CODE:
			// do nothing
		default:
			printf("DEFAULT   : %x\n", key);
			break;
		}
	}
	else
	{
		count++;
	}
}

/* 
	Registers all the interrupts
*/
void registerInterrupts()

{
	int a;
	alt_u8 keys = 0;
	ps2_device = alt_up_ps2_open_dev(PS2_NAME);

	alt_up_ps2_init(ps2_device);
	alt_up_ps2_clear_fifo(ps2_device);

	alt_irq_register(PS2_IRQ, ps2_device, keyboard_isr);
	reset_keyboard();
	IOWR_8DIRECT(PS2_BASE, 4, 1); // enabling(?)

	// a = set_keyboard_rate(ps2_device, keys);
	// printf("%d", a);
	// IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7); // clear the edge cap reg of buttons
	// IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PUSH_BUTTON_BASE, 0x7); // enable for all buttons
	// alt_irq_register(PUSH_BUTTON_IRQ, NULL, button_isr);
	// alt_irq_register(FREQUENCY_ANALYSER_IRQ, NULL, freq_adc_isr);
}
