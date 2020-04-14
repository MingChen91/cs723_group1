#include "main.h"

static QueueHandle_t Q_Freq;
volatile int currentMode = 0;

SemaphoreHandle_t modeMutex;

typedef struct
{
	int adcCount;
	TickType_t isrTickCount;
} FreqQueueItem;

void ps2_isr(void *context, alt_u32 id)
{
	char ascii;
	static int status = 0;
	unsigned char key = 0;
	KB_CODE_TYPE decode_mode;
	decode_scancode(context, &decode_mode, &key, &ascii);
	// if ( status == 0 ) //success
	// {
	// 	// print out the result
	// 	switch ( decode_mode )
	// 	{
	// 		case KB_ASCII_MAKE_CODE :
	// 		printf ( "ASCII   : %c\n", ascii ) ;
	// 		break ;
	// 		case KB_LONG_BINARY_MAKE_CODE :
	// 		// do nothing
	// 		case KB_BINARY_MAKE_CODE :
	// 		printf ( "MAKE CODE : %x\n", key ) ;
	// 		break ;
	// 		case KB_BREAK_CODE :
	// 		// do nothing
	// 		default :
	// 		printf ( "DEFAULT   : %x\n", key ) ;
	// 		break ;
	// 	}
	// 	IOWR(SEVEN_SEG_BASE,0 ,key);
	// }
	printf("boink %d\n", status);
	status++;
}

void freq_relay()
{
	FreqQueueItem freqQueueItem;
	freqQueueItem.adcCount = IORD(FREQUENCY_ANALYSER_BASE, 0);
	freqQueueItem.isrTickCount = xTaskGetTickCountFromISR();
	xQueueSendFromISR(Q_Freq, &freqQueueItem, pdFALSE);
	return;
}

void button_interrupts_function(void *context, alt_u32 id)
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
	// clears the edge capture register
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7);
}

void conditionViolationTask(void *pvParameters)
{
	// int a = 0;
	while (1)
	{
		FreqQueueItem temp2;
		if (xQueueReceive(Q_Freq, &temp2, portMAX_DELAY) == pdTRUE)
		{
			printf("%f\n", 16000 / ((double)temp2.adcCount));
			printf("%lu\n\n", temp2.isrTickCount);
		}
	}
}

void task2(void *pvParameters)
{
	while (1)
	{
		if (xSemaphoreTakeFromISR(modeMutex, (TickType_t)10) == pdTRUE)
		{
			// printf("current mode %d\n",currentMode);
			xSemaphoreGiveFromISR(modeMutex, pdFALSE);
		}
		vTaskDelay(100);
	}
}

int main(void)
{
	// Create the tasks]
	printf("Welcome to NIOSII\n");

	alt_up_ps2_dev *ps2_device = alt_up_ps2_open_dev(PS2_NAME);

	if (ps2_device == NULL)
	{
		printf("can't find PS/2 device\n");
		return 1;
	}

	alt_up_ps2_enable_read_interrupt(ps2_device);
	// ps2 keyboard
	alt_irq_register(PS2_IRQ, ps2_device, ps2_isr);
	// Freq
	alt_irq_register(FREQUENCY_ANALYSER_IRQ, 0, freq_relay);

	// clears the edge capture register. Writing 1 to bit clears pending interrupt
	// for corresponding button.
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7);

	// enable interrupts for all buttons
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PUSH_BUTTON_BASE, 0x7);

	// buttons irq
	alt_irq_register(PUSH_BUTTON_IRQ, NULL, button_interrupts_function);

	// create the queue for
	Q_Freq = xQueueCreate(100, sizeof(FreqQueueItem));

	modeMutex = xSemaphoreCreateMutex();

	xTaskCreate(conditionViolationTask, "task1", configMINIMAL_STACK_SIZE, NULL,
				5, NULL);
	xTaskCreate(task2, "task2", configMINIMAL_STACK_SIZE, NULL, 4, NULL);

	vTaskStartScheduler();
	/* Will only reach here if there is insufficient heap available to start
   the scheduler. */
	while (1)
	{
	}
}

void createTasks() {}

void startInterrupts() {}
