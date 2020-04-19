#include <stdio.h>
#include <stddef.h>
#include "io.h"
#include "altera_up_avalon_ps2.h"
#include "altera_up_ps2_keyboard.h"
#include "sys/alt_irq.h"

void ps2_isr(void *context, alt_u32 id)
{
	static unsigned char counter;
	char ascii;
	int status = 0;
	unsigned char key = 0;
	KB_CODE_TYPE decode_mode;
	status = decode_scancode(context, &decode_mode, &key, &ascii);
	if ((status == 0) && (counter == 0)) //success
	{
		// print out the result
		switch (decode_mode)
		{
		case KB_ASCII_MAKE_CODE:
			printf("ASCII   : %c\n", ascii);
			break;
		case KB_BINARY_MAKE_CODE:
			printf("MAKE CODE : %x\n", key);
			break;
		default:
			break;
		}
	}
	counter++;
	if (counter > 2)
	{
		counter = 0;
	}
}
int main()
{
	alt_up_ps2_dev *ps2_device = alt_up_ps2_open_dev(PS2_NAME);

	if (ps2_device == NULL)
	{
		printf("can't find PS/2 device\n");
		return 1;
	}

	alt_up_ps2_clear_fifo(ps2_device);

	alt_irq_register(PS2_IRQ, ps2_device, ps2_isr);
	// register the PS/2 interrupt
	IOWR_8DIRECT(PS2_BASE, 4, 1);
	while (1)
	{
	}
	return 0;
}
