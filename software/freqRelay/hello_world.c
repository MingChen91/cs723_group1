#include "interrupts.h"
#include "taskslcfr.h"
#include <stdio.h>

int main()
{
    createTasks();
    createVariables();
    registerInterrupts();
    vTaskStartScheduler();
    return 0;
}
