#ifndef AOSL_HAL_ASR3603_THREAD_INTERNAL_H
#define AOSL_HAL_ASR3603_THREAD_INTERNAL_H

/*
 * ThreadX uses smaller numbers for higher priorities.  These defaults keep
 * AOSL work below modem-critical tasks while leaving distinct service levels.
 * Product integration may override any value from its compile definitions.
 */
#ifndef AOSL_ASR3603_PRIORITY_LOW
#define AOSL_ASR3603_PRIORITY_LOW       220U
#endif

#ifndef AOSL_ASR3603_PRIORITY_NORMAL
#define AOSL_ASR3603_PRIORITY_NORMAL    180U
#endif

#ifndef AOSL_ASR3603_PRIORITY_HIGH
#define AOSL_ASR3603_PRIORITY_HIGH      120U
#endif

#ifndef AOSL_ASR3603_PRIORITY_HIGHEST
#define AOSL_ASR3603_PRIORITY_HIGHEST    70U
#endif

#ifndef AOSL_ASR3603_PRIORITY_RT
#define AOSL_ASR3603_PRIORITY_RT         30U
#endif

/* Must remain numerically greater (lower priority) than every AOSL worker. */
#ifndef AOSL_ASR3603_REAPER_PRIORITY
#define AOSL_ASR3603_REAPER_PRIORITY    252U
#endif

#ifndef AOSL_ASR3603_DEFAULT_STACK_SIZE
#define AOSL_ASR3603_DEFAULT_STACK_SIZE (16U * 1024U)
#endif

#ifndef AOSL_ASR3603_REAPER_STACK_SIZE
#define AOSL_ASR3603_REAPER_STACK_SIZE  1024U
#endif

#ifndef AOSL_ASR3603_THREAD_NAME_MAX
#define AOSL_ASR3603_THREAD_NAME_MAX    31U
#endif

/* ASR3603 ThreadX/OSA is configured for one 5 ms system tick. */
#ifndef AOSL_ASR3603_TICK_MS
#define AOSL_ASR3603_TICK_MS            5U
#endif

#endif /* AOSL_HAL_ASR3603_THREAD_INTERNAL_H */
