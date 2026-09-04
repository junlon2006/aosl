#include <stdint.h>
#include <stdio.h>

#include "bsp_common.h"

#include <hal/aosl_hal_log.h>

/* watch_uart_printf has a 128-byte internal buffer and passes 127 to vsnprintf. */
#define AOSL_ASR3603_LOG_BUFFER_SIZE 127

int aosl_hal_printf(const char *format, va_list args)
{
  char buffer[AOSL_ASR3603_LOG_BUFFER_SIZE];
  int length;

  if (format == NULL) {
    return -1;
  }

  length = vsnprintf(buffer, sizeof(buffer), format, args);
  if (length < 0) {
    buffer[sizeof(buffer) - 1] = '\0';
    return -1;
  }

  buffer[sizeof(buffer) - 1] = '\0';
  (void)watch_uart_printf("%s", buffer);
  if (length >= (int)sizeof(buffer)) {
    return (int)sizeof(buffer) - 1;
  }
  return length;
}
