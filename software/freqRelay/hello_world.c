#include "interrupts.h"
#include "taskslcfr.h"
#include <stdio.h>

int main()
{
    createTasks();
    printf("Creating Tasks.\n");
    createVariables();
    printf("Creating Variables.\n");
    registerInterrupts();
    printf("Registering Interrupts\n");
    printf("Starting Scheduler\n\n");
    vTaskStartScheduler();
    return 0;
}
