#ifndef INTERRUPTS_H_
#define INTERRUPTS_H_
#define ENTER_KEY 0x5a

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

void registerInterrupts();

#endif /* INTERRUPTS_H_ */
