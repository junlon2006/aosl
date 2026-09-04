#include <hal/aosl_hal_atomic.h>

#include "aosl_hal_asr3603_internal.h"

static __inline void atomic_barrier(void)
{
#if defined(__clang__) && defined(__arm__)
  __builtin_arm_dmb(0x0f);
#elif defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION < 6000000))
  __dmb(0x0f);
#else
  __sync_synchronize();
#endif
}

intptr_t aosl_hal_atomic_read(const intptr_t *v)
{
  return *(const volatile intptr_t *)v;
}

void aosl_hal_atomic_set(intptr_t *v, intptr_t i)
{
  UINT posture = aosl_asr3603_irq_lock();

  atomic_barrier();
  *(volatile intptr_t *)v = i;
  atomic_barrier();
  aosl_asr3603_irq_unlock(posture);
}

intptr_t aosl_hal_atomic_add(intptr_t i, intptr_t *v)
{
  intptr_t old;
  intptr_t value;
  UINT posture = aosl_asr3603_irq_lock();

  atomic_barrier();
  old = *(volatile intptr_t *)v;
  value = (intptr_t)((uintptr_t)old + (uintptr_t)i);
  *(volatile intptr_t *)v = value;
  atomic_barrier();
  aosl_asr3603_irq_unlock(posture);
  return value;
}

intptr_t aosl_hal_atomic_sub(intptr_t i, intptr_t *v)
{
  intptr_t old;
  intptr_t value;
  UINT posture = aosl_asr3603_irq_lock();

  atomic_barrier();
  old = *(volatile intptr_t *)v;
  value = (intptr_t)((uintptr_t)old - (uintptr_t)i);
  *(volatile intptr_t *)v = value;
  atomic_barrier();
  aosl_asr3603_irq_unlock(posture);
  return value;
}

intptr_t aosl_hal_atomic_inc(intptr_t *v)
{
  return aosl_hal_atomic_add(1, v);
}

intptr_t aosl_hal_atomic_dec(intptr_t *v)
{
  return aosl_hal_atomic_sub(1, v);
}

intptr_t aosl_hal_atomic_cmpxchg(intptr_t *v, intptr_t old, intptr_t new_value)
{
  intptr_t actual;
  UINT posture = aosl_asr3603_irq_lock();

  atomic_barrier();
  actual = *(volatile intptr_t *)v;
  if (actual == old) {
    *(volatile intptr_t *)v = new_value;
  }
  atomic_barrier();
  aosl_asr3603_irq_unlock(posture);
  return actual;
}

intptr_t aosl_hal_atomic_xchg(intptr_t *v, intptr_t new_value)
{
  intptr_t old;
  UINT posture = aosl_asr3603_irq_lock();

  atomic_barrier();
  old = *(volatile intptr_t *)v;
  *(volatile intptr_t *)v = new_value;
  atomic_barrier();
  aosl_asr3603_irq_unlock(posture);
  return old;
}

void aosl_hal_mb(void)
{
  atomic_barrier();
}

void aosl_hal_rmb(void)
{
  atomic_barrier();
}

void aosl_hal_wmb(void)
{
  atomic_barrier();
}
