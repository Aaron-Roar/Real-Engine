#include "rohr.h"

#include <stddef.h>

int main(void) {
    return rohr_error_code_message_get(ERROR_NONE) == NULL;
}
