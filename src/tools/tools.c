#include <time.h>
#include "stdint.h"
#include <stdio.h>
#include <SDL3/SDL.h>
#include <stdlib.h>

//Depends on time being activated

void delay(int number_of_seconds)
{
    // Converting time into milli_seconds
    int milli_seconds = 1000 * number_of_seconds;

    // Storing start time
    clock_t start_time = clock();

    // looping till required time is not achieved
    while (clock() < start_time + milli_seconds);
}


#define MAX_COMBINED_STRING 250
typedef struct {
    char string[MAX_COMBINED_STRING];
    int index;
    size_t length;
} CombinedString;

void binary_to_string(uint32_t value, char* buffer, size_t size) {
    if (size < 33) {
        if (size > 0) buffer[0] = '\0';
        return;
    }

    for (int i = 31; i >= 0; i--) {
        buffer[31 - i] = ((value >> i) & 1) ? '1' : '0';
    }

    buffer[32] = '\0';
}

void tools_append_string(char* src, char* dst, size_t src_size, size_t dst_size) {
  for (uint32_t src_index = 0; src_index < src_size; src_index++) {
    for (uint32_t dst_index = 0; dst_index < dst_size; dst_index++) {
      if(dst[dst_index] == 0) {
        dst[dst_index] = src[src_index];
        break;
      }
    }
  }
}

uint32_t tool_sizeof_string(char* str, char delimiter) {
    int i = 0;
    while (str[i] != delimiter) {
        i += 1;
    }
    return i;
}

int tools_random_range(int min, int max) {
    return (rand() % (max - min + 1)) + min;
}

float tools_random_range_float(float min, float max)
{
    float scale = rand() / (float) RAND_MAX; /* [0, 1.0] */
    return min + scale * ( max - min );      /* [min, max] */
}
