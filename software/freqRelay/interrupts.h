/*
 * interrupts.h
 *
 *  Created on: 15/04/2020
 *      Author: mingc
 */

#ifndef INTERRUPTS_H_
#define INTERRUPTS_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

void button_isr();
void freq_adc_isr();
void registerInterrupts();

#endif /* INTERRUPTS_H_ */
