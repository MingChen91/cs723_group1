#include "main.h"

void ps2_isr(void* context, alt_u32 id){
	char ascii;
	int status = 0;
	unsigned char key = 0;
	KB_CODE_TYPE decode_mode;
	status = decode_scancode (context, &decode_mode , &key , &ascii) ;
	if ( status == 0 ) //success
	{
	    // print out the result
	    switch ( decode_mode )
	    {
			case KB_ASCII_MAKE_CODE :
			printf ( "ASCII   : %c\n", ascii ) ;
			break ;
			case KB_LONG_BINARY_MAKE_CODE :
			// do nothing
			case KB_BINARY_MAKE_CODE :
			printf ( "MAKE CODE : %x\n", key ) ;
			break ;
			case KB_BREAK_CODE :
			// do nothing
			default :
			printf ( "DEFAULT   : %x\n", key ) ;
			break ;
		}
	    IOWR(SEVEN_SEG_BASE,0 ,key);
	}
}


void taskone(void *pvParameters){
	int a = 0;
	while(1){
		IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, a);
		vTaskDelay(1000);
		if (a < 7)
		{
			a++;
		} 
		else 
		{
			a = 0;
		}
	}
}

int main(void)
{
	// Create the tasks]
	printf("Welcome to NIOSII\n");

	alt_up_ps2_dev * ps2_device = alt_up_ps2_open_dev(PS2_NAME);

	if(ps2_device == NULL){
		printf("can't find PS/2 device\n");
		return 1;
	}

	alt_up_ps2_enable_read_interrupt(ps2_device);
	alt_irq_register(PS2_IRQ, ps2_device, ps2_isr);

	xTaskCreate( taskone, "task1", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

	vTaskStartScheduler();
	/* Will only reach here if there is insufficient heap available to start
	 the scheduler. */
	while(1) {}
}

void createTasks()
{

}

void startInterrupts()
{
	
}


