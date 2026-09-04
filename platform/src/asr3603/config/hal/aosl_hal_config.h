#ifndef __AOSL_HAL_CONFIG_H__
#define __AOSL_HAL_CONFIG_H__

#if defined(__CC_ARM) && !defined(__clang__)
#pragma anon_unions
#endif

#ifndef __ARMEL__
#define __ARMEL__ 1
#endif

#define AOSL_HAL_HAVE_EPOLL  0
#define AOSL_HAL_HAVE_POLL   0
#define AOSL_HAL_HAVE_SELECT 1

#define AOSL_HAL_HAVE_COND   0
#define AOSL_HAL_HAVE_SEM    1
#define AOSL_HAL_HAVE_HWRNG  1

#endif /* __AOSL_HAL_CONFIG_H__ */
