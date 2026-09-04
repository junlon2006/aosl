#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tx_api.h"

#include <api/aosl_defs.h>
#include <api/aosl_mm.h>
#include <hal/aosl_hal_thread.h>

#include "aosl_hal_asr3603_thread.h"

typedef struct aosl_asr3603_thread aosl_asr3603_thread_t;

struct aosl_asr3603_thread {
    /* Keep first: an AOSL handle is also the native ThreadX handle. */
    TX_THREAD tx;

    aosl_asr3603_thread_t *active_next;
    aosl_asr3603_thread_t *reap_next;
    void *(*entry)(void *);
    void *arg;
    void *retval;
    void *stack;
    ULONG stack_size;
    TX_SEMAPHORE done;
    char name[AOSL_ASR3603_THREAD_NAME_MAX + 1U];

    unsigned char completed;
    unsigned char detached;
    unsigned char join_claimed;
    unsigned char enqueued;
    unsigned char reaping;
    unsigned char destroy_requested;
};

typedef struct {
    char padding;
    aosl_static_mutex_t mutex;
} aosl_static_mutex_alignment_probe_t;

typedef union {
    uint64_t align;
    ULONG words[(AOSL_ASR3603_REAPER_STACK_SIZE + sizeof(ULONG) - 1U) /
                sizeof(ULONG)];
} aosl_reaper_stack_t;

typedef struct {
    char padding;
    aosl_reaper_stack_t stack;
} aosl_reaper_stack_alignment_probe_t;

aosl_static_assert(offsetof(aosl_asr3603_thread_t, tx) == 0,
                   asr3603_threadx_control_block_must_be_first);
aosl_static_assert(sizeof(aosl_thread_t) >= sizeof(void *),
                   asr3603_thread_handle_must_hold_pointer);
aosl_static_assert(sizeof(uintptr_t) <= sizeof(ULONG),
                   asr3603_threadx_entry_argument_must_hold_pointer);
aosl_static_assert(sizeof(TX_MUTEX) <= AOSL_STATIC_MUTEX_SIZE,
                   asr3603_static_mutex_storage_too_small);
aosl_static_assert((offsetof(aosl_static_mutex_alignment_probe_t, mutex) %
                    sizeof(ULONG)) == 0,
                   asr3603_static_mutex_storage_is_not_ulong_aligned);
aosl_static_assert((offsetof(aosl_reaper_stack_alignment_probe_t, stack) %
                    8U) == 0,
                   asr3603_reaper_stack_is_not_8_byte_aligned);
aosl_static_assert(AOSL_ASR3603_PRIORITY_LOW < TX_MAX_PRIORITIES,
                   asr3603_low_priority_out_of_range);
aosl_static_assert(AOSL_ASR3603_PRIORITY_NORMAL < TX_MAX_PRIORITIES,
                   asr3603_normal_priority_out_of_range);
aosl_static_assert(AOSL_ASR3603_PRIORITY_HIGH < TX_MAX_PRIORITIES,
                   asr3603_high_priority_out_of_range);
aosl_static_assert(AOSL_ASR3603_PRIORITY_HIGHEST < TX_MAX_PRIORITIES,
                   asr3603_highest_priority_out_of_range);
aosl_static_assert(AOSL_ASR3603_PRIORITY_RT < TX_MAX_PRIORITIES,
                   asr3603_rt_priority_out_of_range);
aosl_static_assert(AOSL_ASR3603_REAPER_PRIORITY < TX_MAX_PRIORITIES,
                   asr3603_reaper_priority_out_of_range);
aosl_static_assert(AOSL_ASR3603_PRIORITY_RT <
                       AOSL_ASR3603_PRIORITY_HIGHEST,
                   asr3603_rt_must_be_higher_than_highest);
aosl_static_assert(AOSL_ASR3603_PRIORITY_HIGHEST <
                       AOSL_ASR3603_PRIORITY_HIGH,
                   asr3603_highest_must_be_higher_than_high);
aosl_static_assert(AOSL_ASR3603_PRIORITY_HIGH <
                       AOSL_ASR3603_PRIORITY_NORMAL,
                   asr3603_high_must_be_higher_than_normal);
aosl_static_assert(AOSL_ASR3603_PRIORITY_NORMAL <
                       AOSL_ASR3603_PRIORITY_LOW,
                   asr3603_normal_must_be_higher_than_low);
aosl_static_assert(AOSL_ASR3603_REAPER_PRIORITY > AOSL_ASR3603_PRIORITY_LOW,
                   asr3603_reaper_must_be_lower_than_workers);
aosl_static_assert(AOSL_ASR3603_REAPER_STACK_SIZE >= TX_MINIMUM_STACK,
                   asr3603_reaper_stack_too_small);
aosl_static_assert(AOSL_ASR3603_TICK_MS > 0U,
                   asr3603_tick_duration_must_be_nonzero);

enum {
    AOSL_REAPER_UNINITIALIZED = 0,
    AOSL_REAPER_INITIALIZING,
    AOSL_REAPER_READY,
    AOSL_REAPER_FAILED
};

static aosl_asr3603_thread_t *g_active_threads;
static aosl_asr3603_thread_t *g_reap_head;
static aosl_asr3603_thread_t *g_reap_tail;
static TX_THREAD g_reaper_thread;
static TX_SEMAPHORE g_reaper_sem;
static aosl_reaper_stack_t g_reaper_stack;
static unsigned int g_reaper_state;

static UINT aosl_irq_lock(void)
{
    return tx_interrupt_control(TX_INT_DISABLE);
}

static void aosl_irq_unlock(UINT previous_posture)
{
    (void)tx_interrupt_control(previous_posture);
}

static void aosl_copy_name(char *dst, size_t dst_size, const char *src)
{
    size_t i;

    if (dst == NULL || dst_size == 0U) {
        return;
    }

    if (src == NULL) {
        src = "aosl";
    }

    for (i = 0U; (i + 1U) < dst_size && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int aosl_native_priority(aosl_thread_proiority_e priority, UINT *native)
{
    if (native == NULL) {
        return -1;
    }

    switch (priority) {
    case AOSL_THRD_PRI_LOW:
        *native = AOSL_ASR3603_PRIORITY_LOW;
        break;
    case AOSL_THRD_PRI_DEFAULT:
    case AOSL_THRD_PRI_NORMAL:
        *native = AOSL_ASR3603_PRIORITY_NORMAL;
        break;
    case AOSL_THRD_PRI_HIGH:
        *native = AOSL_ASR3603_PRIORITY_HIGH;
        break;
    case AOSL_THRD_PRI_HIGHEST:
        *native = AOSL_ASR3603_PRIORITY_HIGHEST;
        break;
    case AOSL_THRD_PRI_RT:
        *native = AOSL_ASR3603_PRIORITY_RT;
        break;
    default:
        return -1;
    }

    return 0;
}

static aosl_asr3603_thread_t *aosl_find_handle_locked(aosl_thread_t thread)
{
    aosl_asr3603_thread_t *cursor;
    void *wanted;

    wanted = (void *)(uintptr_t)thread;
    for (cursor = g_active_threads; cursor != NULL;
         cursor = cursor->active_next) {
        if ((void *)cursor == wanted) {
            return cursor;
        }
    }

    return NULL;
}

static aosl_asr3603_thread_t *aosl_find_native_locked(TX_THREAD *native)
{
    aosl_asr3603_thread_t *cursor;

    for (cursor = g_active_threads; cursor != NULL;
         cursor = cursor->active_next) {
        if (&cursor->tx == native) {
            return cursor;
        }
    }

    return NULL;
}

static void aosl_register_thread_locked(aosl_asr3603_thread_t *thread)
{
    thread->active_next = g_active_threads;
    g_active_threads = thread;
}

static void aosl_unregister_thread_locked(aosl_asr3603_thread_t *thread)
{
    aosl_asr3603_thread_t **link;

    for (link = &g_active_threads; *link != NULL;
         link = &(*link)->active_next) {
        if (*link == thread) {
            *link = thread->active_next;
            thread->active_next = NULL;
            return;
        }
    }
}

/* Caller holds the interrupt lock. Returns nonzero when the reaper needs a kick. */
static int aosl_enqueue_reap_locked(aosl_asr3603_thread_t *thread)
{
    if (thread->enqueued || thread->reaping) {
        return 0;
    }

    thread->enqueued = 1U;
    thread->reap_next = NULL;
    if (g_reap_tail != NULL) {
        g_reap_tail->reap_next = thread;
    } else {
        g_reap_head = thread;
    }
    g_reap_tail = thread;
    return 1;
}

static aosl_asr3603_thread_t *aosl_dequeue_reap(void)
{
    aosl_asr3603_thread_t *thread;
    UINT posture;

    posture = aosl_irq_lock();
    thread = g_reap_head;
    if (thread != NULL) {
        g_reap_head = thread->reap_next;
        if (g_reap_head == NULL) {
            g_reap_tail = NULL;
        }
        thread->reap_next = NULL;
        thread->enqueued = 0U;
        thread->reaping = 1U;
    }
    aosl_irq_unlock(posture);
    return thread;
}

/* The caller must not be the thread being disposed. */
static int aosl_delete_native_thread(aosl_asr3603_thread_t *thread)
{
    UINT status;
    unsigned int attempt;

    for (attempt = 0U; attempt < 3U; ++attempt) {
        (void)tx_thread_terminate(&thread->tx);
        status = tx_thread_delete(&thread->tx);
        if (status == TX_SUCCESS) {
            return 0;
        }
        tx_thread_relinquish();
    }

    return -1;
}

static void aosl_free_thread(aosl_asr3603_thread_t *thread)
{
    void *stack;

    stack = thread->stack;
    (void)tx_semaphore_delete(&thread->done);
    aosl_free(stack);
    aosl_free(thread);
}

static void aosl_requeue_after_delete_failure(aosl_asr3603_thread_t *thread)
{
    UINT posture;
    int notify;

    posture = aosl_irq_lock();
    thread->reaping = 0U;
    notify = aosl_enqueue_reap_locked(thread);
    aosl_irq_unlock(posture);

    if (notify) {
        (void)tx_semaphore_put(&g_reaper_sem);
    }
}

static void aosl_reaper_entry(ULONG unused)
{
    aosl_asr3603_thread_t *thread;
    UINT posture;

    (void)unused;
    for (;;) {
        if (tx_semaphore_get(&g_reaper_sem, TX_WAIT_FOREVER) != TX_SUCCESS) {
            continue;
        }

        while ((thread = aosl_dequeue_reap()) != NULL) {
            if (aosl_delete_native_thread(thread) != 0) {
                aosl_requeue_after_delete_failure(thread);
                (void)tx_thread_sleep(1U);
                continue;
            }

            posture = aosl_irq_lock();
            aosl_unregister_thread_locked(thread);
            thread->reaping = 0U;
            aosl_irq_unlock(posture);
            aosl_free_thread(thread);
        }
    }
}

static int aosl_reaper_init(void)
{
    UINT posture;
    UINT status;
    int initializer;
    int semaphore_created;
    int thread_created;

    initializer = 0;
    for (;;) {
        posture = aosl_irq_lock();
        if (g_reaper_state == AOSL_REAPER_READY) {
            aosl_irq_unlock(posture);
            return 0;
        }
        if (g_reaper_state == AOSL_REAPER_FAILED) {
            aosl_irq_unlock(posture);
            return -1;
        }
        if (g_reaper_state == AOSL_REAPER_UNINITIALIZED) {
            g_reaper_state = AOSL_REAPER_INITIALIZING;
            initializer = 1;
        }
        aosl_irq_unlock(posture);
        if (initializer) {
            break;
        }

        /* Let the initializer run even when this waiter has higher priority. */
        (void)tx_thread_sleep(1U);
    }

    memset(&g_reaper_sem, 0, sizeof(g_reaper_sem));
    memset(&g_reaper_thread, 0, sizeof(g_reaper_thread));
    semaphore_created = 0;
    thread_created = 0;
    status = tx_semaphore_create(&g_reaper_sem, "aosl_reap", 0U);
    if (status == TX_SUCCESS) {
        semaphore_created = 1;
        status = tx_thread_create(&g_reaper_thread,
                                  "aosl_reap",
                                  aosl_reaper_entry,
                                  0U,
                                  g_reaper_stack.words,
                                  (ULONG)sizeof(g_reaper_stack.words),
                                  AOSL_ASR3603_REAPER_PRIORITY,
                                  AOSL_ASR3603_REAPER_PRIORITY,
                                  TX_NO_TIME_SLICE,
                                  TX_DONT_START);
        if (status == TX_SUCCESS) {
            thread_created = 1;
        }
    }

    if (status == TX_SUCCESS) {
        status = tx_thread_resume(&g_reaper_thread);
    }

    if (status != TX_SUCCESS) {
        if (thread_created) {
            (void)tx_thread_terminate(&g_reaper_thread);
            (void)tx_thread_delete(&g_reaper_thread);
        }
        if (semaphore_created) {
            (void)tx_semaphore_delete(&g_reaper_sem);
        }
    }

    posture = aosl_irq_lock();
    g_reaper_state = status == TX_SUCCESS ? AOSL_REAPER_READY
                                          : AOSL_REAPER_FAILED;
    aosl_irq_unlock(posture);

    return status == TX_SUCCESS ? 0 : -1;
}

/* This is the last routine allowed to dereference thread from its own context. */
static void aosl_thread_complete(aosl_asr3603_thread_t *thread, void *retval)
{
    UINT posture;
    int notify_reaper;
    int notify_done;

    notify_reaper = 0;
    notify_done = 0;
    posture = aosl_irq_lock();
    if (!thread->completed) {
        thread->retval = retval;
        thread->completed = 1U;
        notify_done = 1;
        if (thread->detached) {
            notify_reaper = aosl_enqueue_reap_locked(thread);
        }
    }
    aosl_irq_unlock(posture);

    if (notify_done) {
        (void)tx_semaphore_put(&thread->done);
    }
    if (notify_reaper) {
        (void)tx_semaphore_put(&g_reaper_sem);
    }
}

static void aosl_thread_entry(ULONG input)
{
    aosl_asr3603_thread_t *thread;
    void *retval;

    thread = (aosl_asr3603_thread_t *)(uintptr_t)input;
    retval = thread->entry(thread->arg);
    aosl_thread_complete(thread, retval);
}

int aosl_hal_thread_create(aosl_thread_t *thread,
                           aosl_thread_param_t *param,
                           void *(*entry)(void *),
                           void *arg)
{
    aosl_asr3603_thread_t *context;
    const char *name;
    size_t requested_stack;
    size_t aligned_stack;
    aosl_thread_proiority_e priority;
    UINT native_priority;
    UINT posture;
    UINT status;

    if (thread == NULL || entry == NULL) {
        return -1;
    }
    *thread = (aosl_thread_t)0;

    priority = param != NULL ? param->priority : AOSL_THRD_PRI_DEFAULT;
    if (aosl_native_priority(priority, &native_priority) != 0) {
        return -1;
    }
    if (aosl_reaper_init() != 0) {
        return -1;
    }

    requested_stack = (param != NULL && param->stack_size > 0)
                          ? (size_t)param->stack_size
                          : (size_t)AOSL_ASR3603_DEFAULT_STACK_SIZE;
    if (requested_stack < (size_t)TX_MINIMUM_STACK) {
        requested_stack = (size_t)TX_MINIMUM_STACK;
    }
    aligned_stack = (requested_stack + sizeof(ULONG) - 1U) &
                    ~(sizeof(ULONG) - 1U);
    if (aligned_stack > (size_t)((ULONG)-1)) {
        return -1;
    }

    context = (aosl_asr3603_thread_t *)aosl_calloc(1U, sizeof(*context));
    if (context == NULL) {
        return -1;
    }
    context->stack = aosl_malloc(aligned_stack);
    if (context->stack == NULL) {
        aosl_free(context);
        return -1;
    }
    context->stack_size = (ULONG)aligned_stack;
    context->entry = entry;
    context->arg = arg;
    name = (param != NULL) ? param->name : NULL;
    aosl_copy_name(context->name, sizeof(context->name), name);

    status = tx_semaphore_create(&context->done, "aosl_done", 0U);
    if (status != TX_SUCCESS) {
        aosl_free(context->stack);
        aosl_free(context);
        return -1;
    }

    status = tx_thread_create(&context->tx,
                              context->name,
                              aosl_thread_entry,
                              (ULONG)(uintptr_t)context,
                              context->stack,
                              context->stack_size,
                              native_priority,
                              native_priority,
                              TX_NO_TIME_SLICE,
                              TX_DONT_START);
    if (status != TX_SUCCESS) {
        (void)tx_semaphore_delete(&context->done);
        aosl_free(context->stack);
        aosl_free(context);
        return -1;
    }

    posture = aosl_irq_lock();
    aosl_register_thread_locked(context);
    aosl_irq_unlock(posture);
    *thread = (aosl_thread_t)(uintptr_t)context;

    status = tx_thread_resume(&context->tx);
    if (status != TX_SUCCESS) {
        posture = aosl_irq_lock();
        aosl_unregister_thread_locked(context);
        aosl_irq_unlock(posture);
        *thread = (aosl_thread_t)0;
        (void)tx_thread_terminate(&context->tx);
        (void)tx_thread_delete(&context->tx);
        aosl_free_thread(context);
        return -1;
    }

    return 0;
}

void aosl_hal_thread_destroy(aosl_thread_t thread)
{
    aosl_asr3603_thread_t *context;
    TX_THREAD *self;
    UINT posture;
    int notify_reaper;
    int destroying_self;

    if (thread == (aosl_thread_t)0) {
        return;
    }

    self = tx_thread_identify();
    notify_reaper = 0;
    posture = aosl_irq_lock();
    context = aosl_find_handle_locked(thread);
    if (context == NULL || context->join_claimed || context->reaping ||
        context->enqueued || context->destroy_requested) {
        aosl_irq_unlock(posture);
        return;
    }

    context->destroy_requested = 1U;
    destroying_self = (&context->tx == self);
    if (destroying_self) {
        context->detached = 1U;
    }
    aosl_irq_unlock(posture);

    if (destroying_self) {
        aosl_hal_thread_exit(NULL);
        return;
    }

    (void)tx_thread_terminate(&context->tx);

    posture = aosl_irq_lock();
    context = aosl_find_handle_locked(thread);
    if (context != NULL && context->destroy_requested &&
        !context->reaping && !context->enqueued) {
        context->detached = 1U;
        notify_reaper = aosl_enqueue_reap_locked(context);
    }
    aosl_irq_unlock(posture);

    /* The reaper may free context as soon as it is notified. */
    if (notify_reaper) {
        (void)tx_semaphore_put(&g_reaper_sem);
    }
}

void aosl_hal_thread_exit(void *retval)
{
    aosl_asr3603_thread_t *context;
    TX_THREAD *self;
    UINT posture;

    self = tx_thread_identify();
    if (self == NULL) {
        return;
    }

    posture = aosl_irq_lock();
    context = aosl_find_native_locked(self);
    aosl_irq_unlock(posture);
    if (context != NULL) {
        aosl_thread_complete(context, retval);
    }

    (void)tx_thread_terminate(self);
    for (;;) {
        (void)tx_thread_suspend(self);
    }
}

aosl_thread_t aosl_hal_thread_self(void)
{
    TX_THREAD *self;

    self = tx_thread_identify();
    return (aosl_thread_t)(uintptr_t)self;
}

int aosl_hal_thread_set_name(const char *name)
{
    aosl_asr3603_thread_t *context;
    TX_THREAD *self;
    UINT posture;

    if (name == NULL) {
        return -1;
    }

    self = tx_thread_identify();
    posture = aosl_irq_lock();
    context = aosl_find_native_locked(self);
    if (context != NULL) {
        aosl_copy_name(context->name, sizeof(context->name), name);
        context->tx.tx_thread_name = context->name;
    }
    aosl_irq_unlock(posture);
    return context != NULL ? 0 : -1;
}

int aosl_hal_thread_get_name(char *name, size_t size)
{
    aosl_asr3603_thread_t *context;
    TX_THREAD *self;
    CHAR *native_name;
    UINT posture;
    UINT status;

    if (name == NULL || size == 0U) {
        return -1;
    }

    self = tx_thread_identify();
    if (self == NULL) {
        name[0] = '\0';
        return -1;
    }

    posture = aosl_irq_lock();
    context = aosl_find_native_locked(self);
    if (context != NULL) {
        aosl_copy_name(name, size, context->name);
        aosl_irq_unlock(posture);
        return 0;
    }
    aosl_irq_unlock(posture);

    native_name = NULL;
    status = tx_thread_info_get(self, &native_name, NULL, NULL, NULL, NULL,
                                NULL, NULL, NULL);
    if (status != TX_SUCCESS || native_name == NULL) {
        name[0] = '\0';
        return -1;
    }
    aosl_copy_name(name, size, native_name);
    return 0;
}

int aosl_hal_thread_set_priority(aosl_thread_proiority_e priority)
{
    TX_THREAD *self;
    UINT native_priority;
    UINT old_priority;

    if (aosl_native_priority(priority, &native_priority) != 0) {
        return -1;
    }
    self = tx_thread_identify();
    if (self == NULL) {
        return -1;
    }

    return tx_thread_priority_change(self, native_priority, &old_priority) ==
                   TX_SUCCESS
               ? 0
               : -1;
}

int aosl_hal_thread_join(aosl_thread_t thread, void **retval)
{
    aosl_asr3603_thread_t *context;
    TX_THREAD *self;
    void *thread_retval;
    UINT posture;
    UINT wait_status;
    int completed;

    self = tx_thread_identify();
    if (thread == (aosl_thread_t)0 ||
        thread == (aosl_thread_t)(uintptr_t)self) {
        return -1;
    }

    posture = aosl_irq_lock();
    context = aosl_find_handle_locked(thread);
    if (context == NULL || context->detached || context->join_claimed ||
        context->enqueued || context->reaping ||
        context->destroy_requested) {
        aosl_irq_unlock(posture);
        return -1;
    }
    context->join_claimed = 1U;
    completed = context->completed != 0U;
    aosl_irq_unlock(posture);

    if (!completed) {
        wait_status = tx_semaphore_get(&context->done, TX_WAIT_FOREVER);
        if (wait_status != TX_SUCCESS) {
            posture = aosl_irq_lock();
            context = aosl_find_handle_locked(thread);
            if (context != NULL) {
                context->join_claimed = 0U;
            }
            aosl_irq_unlock(posture);
            return -1;
        }
    }

    thread_retval = context->retval;
    if (aosl_delete_native_thread(context) != 0) {
        posture = aosl_irq_lock();
        context->join_claimed = 0U;
        aosl_irq_unlock(posture);
        return -1;
    }

    posture = aosl_irq_lock();
    aosl_unregister_thread_locked(context);
    aosl_irq_unlock(posture);
    aosl_free_thread(context);

    if (retval != NULL) {
        *retval = thread_retval;
    }
    return 0;
}

void aosl_hal_thread_detach(aosl_thread_t thread)
{
    aosl_asr3603_thread_t *context;
    UINT posture;
    int notify_reaper;

    notify_reaper = 0;
    posture = aosl_irq_lock();
    context = aosl_find_handle_locked(thread);
    if (context != NULL && !context->join_claimed &&
        !context->destroy_requested) {
        context->detached = 1U;
        if (context->completed) {
            notify_reaper = aosl_enqueue_reap_locked(context);
        }
    }
    aosl_irq_unlock(posture);

    if (notify_reaper) {
        (void)tx_semaphore_put(&g_reaper_sem);
    }
}

aosl_mutex_t aosl_hal_mutex_create(void)
{
    TX_MUTEX *mutex;

    mutex = (TX_MUTEX *)aosl_calloc(1U, sizeof(*mutex));
    if (mutex == NULL) {
        return NULL;
    }
    if (tx_mutex_create(mutex, "aosl_mutex", TX_INHERIT) != TX_SUCCESS) {
        aosl_free(mutex);
        return NULL;
    }
    return (aosl_mutex_t)mutex;
}

void aosl_hal_mutex_destroy(aosl_mutex_t mutex)
{
    if (mutex == NULL) {
        return;
    }
    if (tx_mutex_delete((TX_MUTEX *)mutex) == TX_SUCCESS) {
        aosl_free(mutex);
    }
}

int aosl_hal_mutex_lock(aosl_mutex_t mutex)
{
    return mutex != NULL &&
                   tx_mutex_get((TX_MUTEX *)mutex, TX_WAIT_FOREVER) == TX_SUCCESS
               ? 0
               : -1;
}

int aosl_hal_mutex_trylock(aosl_mutex_t mutex)
{
    return mutex != NULL &&
                   tx_mutex_get((TX_MUTEX *)mutex, TX_NO_WAIT) == TX_SUCCESS
               ? 0
               : -1;
}

int aosl_hal_mutex_unlock(aosl_mutex_t mutex)
{
    return mutex != NULL && tx_mutex_put((TX_MUTEX *)mutex) == TX_SUCCESS ? 0
                                                                          : -1;
}

int aosl_hal_static_mutex_init(aosl_static_mutex_t *mutex)
{
    TX_MUTEX *native;

    if (mutex == NULL) {
        return -1;
    }
    native = (TX_MUTEX *)(void *)mutex->opaque;
    memset(native, 0, sizeof(*native));
    return tx_mutex_create(native, "aosl_static", TX_INHERIT) == TX_SUCCESS
               ? 0
               : -1;
}

void aosl_hal_static_mutex_fini(aosl_static_mutex_t *mutex)
{
    TX_MUTEX *native;

    if (mutex == NULL) {
        return;
    }
    native = (TX_MUTEX *)(void *)mutex->opaque;
    if (tx_mutex_delete(native) == TX_SUCCESS) {
        memset(native, 0, sizeof(*native));
    }
}

aosl_sem_t aosl_hal_sem_create(void)
{
    TX_SEMAPHORE *sem;

    sem = (TX_SEMAPHORE *)aosl_calloc(1U, sizeof(*sem));
    if (sem == NULL) {
        return NULL;
    }
    if (tx_semaphore_create(sem, "aosl_sem", 0U) != TX_SUCCESS) {
        aosl_free(sem);
        return NULL;
    }
    return (aosl_sem_t)sem;
}

void aosl_hal_sem_destroy(aosl_sem_t sem)
{
    if (sem == NULL) {
        return;
    }
    if (tx_semaphore_delete((TX_SEMAPHORE *)sem) == TX_SUCCESS) {
        aosl_free(sem);
    }
}

int aosl_hal_sem_post(aosl_sem_t sem)
{
    return sem != NULL &&
                   tx_semaphore_put((TX_SEMAPHORE *)sem) == TX_SUCCESS
               ? 0
               : -1;
}

int aosl_hal_sem_wait(aosl_sem_t sem)
{
    return sem != NULL &&
                   tx_semaphore_get((TX_SEMAPHORE *)sem, TX_WAIT_FOREVER) ==
                       TX_SUCCESS
               ? 0
               : -1;
}

int aosl_hal_sem_timedwait(aosl_sem_t sem, intptr_t timeout_ms)
{
    uint64_t ticks64;
    ULONG wait_option;

    if (sem == NULL) {
        return -1;
    }

    if (timeout_ms < 0) {
        wait_option = TX_WAIT_FOREVER;
    } else if (timeout_ms == 0) {
        wait_option = TX_NO_WAIT;
    } else {
        ticks64 = (uint64_t)timeout_ms / AOSL_ASR3603_TICK_MS;
        if (((uint64_t)timeout_ms % AOSL_ASR3603_TICK_MS) != 0U) {
            ++ticks64;
        }
        if (ticks64 >= (uint64_t)TX_WAIT_FOREVER) {
            wait_option = TX_WAIT_FOREVER - 1U;
        } else {
            wait_option = (ULONG)ticks64;
        }
    }

    return tx_semaphore_get((TX_SEMAPHORE *)sem, wait_option) == TX_SUCCESS
               ? 0
               : -1;
}
