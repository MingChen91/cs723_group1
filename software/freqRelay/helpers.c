#include "helpers.h"
#include <stddef.h>

/*
	Use :
	Helper function for keeping track of array of rolling numbers
	used in informationTask()
*/
void shiftReg(int newNumber, int array[])
{
    int i;
    for (i = 0; i < SHIFT_REG_LEN - 1; i++)
    {
        array[i] = array[i + 1]; // shift everything along by 1
    }
    array[i] = newNumber;
}


/*
	Use :
	Helper Function to set left most unset bit
	Used in loadManagementTask()
*/
int setLeftMostUnsetBit(int n)
{
    // Number of digits in binary
    const uint8_t digits = 7; // 8 LEDs total, 7 bits to shift
    uint8_t i;

    // Loop through every binary digit
    for (i = 0; i <= digits; i++)
    {
        if (((n >> (digits - i)) & 1) == 0) // Count from the left, if the digit is 0
        {
            return (n | (1 << (digits - i))); // set that digit to 1
        }
    }
    return n;
}

/*
    Helper Function: Constructs a Float from a stream of chars.
    Only accept '0'-'9', '.' and null terminator.
*/
int8_t constructFloat(float *resultFloat, char inputChar, char *charBuffer)
{
    // Static index, persist through calls.
    static uint8_t bufferIndex = 0;

    // Only accept numbers , dot , and null terminator
    if (((inputChar < '0') || (inputChar > '9')) && (inputChar != '.') && (inputChar != '\0'))
        return CONSTRUCT_FLOAT_NOT_DONE;

    // Add new character to buffer
    charBuffer[bufferIndex] = inputChar;

    // Check if overflowing char buffer
    if (bufferIndex >= CHAR_BUFFER_SIZE - 1)
        charBuffer[bufferIndex] = '\0';

    // End of string, convert to float
    if (charBuffer[bufferIndex] == '\0')
    {
        *resultFloat = strtof(charBuffer, NULL);
        // Reset buffer
        for (bufferIndex = 0; bufferIndex < CHAR_BUFFER_SIZE; bufferIndex++)
        {
            charBuffer[bufferIndex] = '\0';
        }
        bufferIndex = 0;
        return CONSTRUCT_FLOAT_DONE;
    }

    printf("Buffer : %s\n", charBuffer);
    bufferIndex++;
    return CONSTRUCT_FLOAT_NOT_DONE;
}
