/*
 * interrupts.h
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#ifndef INTERRUPTS_H_
#define INTERRUPTS_H_
#define ENTER_KEY 0x5a


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

void registerInterrupts();

#endif /* INTERRUPTS_H_ */
