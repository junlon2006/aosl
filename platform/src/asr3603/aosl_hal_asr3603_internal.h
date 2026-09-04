#ifndef __AOSL_HAL_ASR3603_INTERNAL_H__
#define __AOSL_HAL_ASR3603_INTERNAL_H__

#include <stdint.h>

#include "tx_api.h"

#define AOSL_ASR3603_TICK_MS 5U
#define AOSL_ASR3603_MAX_SLEEP_TICKS 0xfffffffeUL

static __inline UINT aosl_asr3603_irq_lock(void)
{
  return tx_interrupt_control(TX_INT_DISABLE);
}

static __inline void aosl_asr3603_irq_unlock(UINT posture)
{
  (void)tx_interrupt_control(posture);
}

static __inline uint32_t aosl_asr3603_ms_to_ticks(uint64_t ms)
{
  uint64_t ticks;

  if (ms == 0) {
    return 0;
  }

  ticks = ms / AOSL_ASR3603_TICK_MS;
  if ((ms % AOSL_ASR3603_TICK_MS) != 0) {
    ++ticks;
  }
  if (ticks > AOSL_ASR3603_MAX_SLEEP_TICKS) {
    return (uint32_t)AOSL_ASR3603_MAX_SLEEP_TICKS;
  }
  return (uint32_t)ticks;
}

#endif /* __AOSL_HAL_ASR3603_INTERNAL_H__ */
