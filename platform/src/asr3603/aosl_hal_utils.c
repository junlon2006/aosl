#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <hal/aosl_hal_atomic.h>
#include <hal/aosl_hal_utils.h>

extern unsigned int geu_random_number(void);

static intptr_t uuid_sequence;

int aosl_hal_rand_bytes(void *buf, int len)
{
  unsigned char *destination = (unsigned char *)buf;
  int offset = 0;

  if (buf == NULL || len <= 0) {
    return -1;
  }

  while (offset < len) {
    uint32_t random = (uint32_t)geu_random_number();
    int chunk = len - offset;

    if (chunk > (int)sizeof(random)) {
      chunk = (int)sizeof(random);
    }
    memcpy(destination + offset, &random, (size_t)chunk);
    offset += chunk;
  }
  return 0;
}

int aosl_hal_get_uuid(char buf[], int buf_sz)
{
  static const char hex[] = "0123456789abcdef";
  uint32_t words[4];
  unsigned char bytes[sizeof(words)];
  int output_length;
  int i;

  if (buf == NULL || buf_sz <= 1) {
    return -1;
  }

  words[0] = (uint32_t)geu_random_number();
  words[1] = (uint32_t)geu_random_number();
  words[2] = (uint32_t)geu_random_number();
  /* The sequence word guarantees distinct consecutive IDs until it wraps. */
  words[3] = (uint32_t)aosl_hal_atomic_inc(&uuid_sequence);
  memcpy(bytes, words, sizeof(bytes));

  output_length = buf_sz - 1;
  if (output_length > 32) {
    output_length = 32;
  }
  for (i = 0; i < output_length; ++i) {
    unsigned char byte = bytes[i / 2];
    buf[i] = (i & 1) ? hex[byte & 0x0f] : hex[byte >> 4];
  }
  buf[output_length] = '\0';
  return 0;
}

int aosl_hal_os_version(char buf[], int buf_sz)
{
  static const char version[] = "ThreadX/ASR3603S-LWG";

  if (buf == NULL || buf_sz <= 1) {
    return -1;
  }
  (void)snprintf(buf, (size_t)buf_sz, "%s", version);
  return 0;
}
