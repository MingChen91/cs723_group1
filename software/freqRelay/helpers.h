#ifndef HELPERS_H_
#define HELPERS_H_
#include <stdint.h>

#define SHIFT_REG_LEN            5
#define CHAR_BUFFER_SIZE         10
#define CONSTRUCT_FLOAT_DONE     0
#define CONSTRUCT_FLOAT_NOT_DONE 1

void shiftReg(int newNumber, int array[]);
int setLeftMostUnsetBit(int n);
int8_t constructFloat(float *resultFloat, char inputChar, char *charBuffer);


#endif /* HELPERS_H_ */
