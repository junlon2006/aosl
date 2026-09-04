#include <stdio.h>

#include "osa.h"
#include "pmic_rtc.h"

#include <hal/aosl_hal_time.h>

#include "aosl_hal_asr3603_internal.h"

static uint32_t tick_last;
static uint64_t tick_wrap_base;

static int is_leap_year(unsigned int year)
{
  return ((year % 4U) == 0U && (year % 100U) != 0U) ||
      (year % 400U) == 0U;
}

static void epoch_to_utc(uint64_t seconds, t_rtc *result)
{
  static const unsigned char month_days[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
  };
  uint64_t days = seconds / 86400U;
  uint64_t day_seconds = seconds % 86400U;
  unsigned int year = 1970;
  unsigned int month = 0;
  unsigned int days_in_period;

  result->tm_hour = (int)(day_seconds / 3600U);
  result->tm_min = (int)((day_seconds % 3600U) / 60U);
  result->tm_sec = (int)(day_seconds % 60U);
  result->tm_wday = (int)((days + 4U) % 7U);

  for (;;) {
    days_in_period = is_leap_year(year) ? 366U : 365U;
    if (days < days_in_period) {
      break;
    }
    days -= days_in_period;
    ++year;
  }

  while (month < 12U) {
    days_in_period = month_days[month];
    if (month == 1U && is_leap_year(year)) {
      ++days_in_period;
    }
    if (days < days_in_period) {
      break;
    }
    days -= days_in_period;
    ++month;
  }

  result->tm_year = (int)year;
  result->tm_mon = (int)month + 1;
  result->tm_mday = (int)days + 1;
}

uint64_t aosl_hal_get_tick_ms(void)
{
  UINT posture;
  uint32_t tick;
  uint64_t extended;

  posture = aosl_asr3603_irq_lock();
  tick = (uint32_t)OsaGetTicks(NULL);
  if (tick < tick_last) {
    tick_wrap_base += (1ULL << 32);
  }
  tick_last = tick;
  extended = tick_wrap_base + tick;
  aosl_asr3603_irq_unlock(posture);

  return extended * AOSL_ASR3603_TICK_MS;
}

uint64_t aosl_hal_get_time_ms(void)
{
  uint64_t value;
  uint32_t rtc_before;
  uint32_t rtc_after;
  uint32_t milliseconds;
  int attempts = 3;

  do {
    rtc_before = PMIC_RTC_GetTime_Count_Without_Timezone(APP_OFFSET);
    milliseconds = (uint32_t)millisecond_get();
    rtc_after = PMIC_RTC_GetTime_Count_Without_Timezone(APP_OFFSET);
  } while (rtc_before != rtc_after && --attempts > 0);

  value = (uint64_t)rtc_after * 1000U + (milliseconds % 1000U);
  return value;
}

int aosl_hal_get_time_str(char *buf, int len)
{
  t_rtc now;
  uint64_t now_ms;
  int result;

  if (buf == NULL || len <= 0) {
    return -1;
  }

  now_ms = aosl_hal_get_time_ms();
  epoch_to_utc(now_ms / 1000U, &now);
  result = snprintf(buf, (size_t)len,
                    "%04d-%02d-%02d %02d:%02d:%02d.%03lu",
                    now.tm_year, now.tm_mon, now.tm_mday,
                    now.tm_hour, now.tm_min, now.tm_sec,
                    (unsigned long)(now_ms % 1000U));
  return result < 0 ? -1 : 0;
}

void aosl_hal_msleep(uint64_t ms)
{
  uint64_t slept_ms;
  uint32_t ticks;

  if (ms == 0) {
    return;
  }

  while (ms != 0) {
    ticks = aosl_asr3603_ms_to_ticks(ms);
    OSATaskSleep(ticks);
    slept_ms = (uint64_t)ticks * AOSL_ASR3603_TICK_MS;
    if (ms <= slept_ms) {
      break;
    }
    ms -= slept_ms;
  }
}
