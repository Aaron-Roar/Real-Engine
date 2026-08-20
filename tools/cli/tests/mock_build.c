/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include <stdio.h>

int main(int count, char **arguments) {
    FILE *output;
    if(count < 3) return 1;
    output = fopen(arguments[1], "ab");
    if(output == NULL) return 1;
    for(int i = 2; i < count; i += 1) {
        if(i > 2 && fputc('|', output) == EOF) {
            fclose(output);
            return 1;
        }
        if(fputs(arguments[i], output) == EOF) {
            fclose(output);
            return 1;
        }
    }
    if(fputc('\n', output) == EOF) {
        fclose(output);
        return 1;
    }
    return fclose(output) == 0 ? 0 : 1;
}
