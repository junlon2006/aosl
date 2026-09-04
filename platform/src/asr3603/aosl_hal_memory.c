#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "osa.h"

#include <hal/aosl_hal_memory.h>

void *aosl_hal_malloc(size_t size)
{
  if (size > (size_t)UINT32_MAX - 31U) {
    return NULL;
  }
  return OsaMemAlloc(NULL, (UINT32)size);
}

void aosl_hal_free(void *ptr)
{
  if (ptr != NULL) {
    OsaMemFree(ptr);
  }
}

void *aosl_hal_calloc(size_t nmemb, size_t size)
{
  size_t bytes;
  void *ptr;

  if (size != 0 && nmemb > SIZE_MAX / size) {
    return NULL;
  }

  bytes = nmemb * size;
  ptr = aosl_hal_malloc(bytes);
  if (ptr != NULL && bytes != 0) {
    memset(ptr, 0, bytes);
  }
  return ptr;
}

void *aosl_hal_realloc(void *ptr, size_t size)
{
  size_t old_size;
  size_t copy_size;
  void *new_ptr;

  if (ptr == NULL) {
    return aosl_hal_malloc(size);
  }
  if (size == 0) {
    aosl_hal_free(ptr);
    return NULL;
  }

  old_size = (size_t)OsaMemGetAllocSize(ptr);
  new_ptr = aosl_hal_malloc(size);
  if (new_ptr == NULL) {
    return NULL;
  }

  copy_size = old_size < size ? old_size : size;
  if (copy_size != 0) {
    memcpy(new_ptr, ptr, copy_size);
  }
  aosl_hal_free(ptr);
  return new_ptr;
}
