#pragma once
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>

// maximum message payload per log entry (writer fills this)
#define RWLOG_MSG_MAX 64

/* one log record stored in the circular buffer.
 * NOTE: seq/tid/ts are assigned by the monitor during append(). */
typedef struct {
    uint64_t seq;           // global, sequence assigned by monitor
    pthread_t tid;          // thread id of writer
    struct timespec ts;     // timestamp (CLOCK_REALTIME)
    char msg[RWLOG_MSG_MAX];// message payload
} rwlog_entry_t;

/* === Monitor API (pthreads + POSIX shared memory) ===
 * The monitor implements writer-preference RW semantics:
 *  - New readers are blocked if a writer is active OR any writer is waiting.
 *  - Exactly one writer holds the write section at a time.
 *  - No busy waiting: all waits use pthread_cond_wait in a while-loop. */

/* lifecycle */
int rwlog_create(size_t capacity);                                  // allocate/init monitor in POSIX shared memory
int rwlog_destroy(void);                                            // destroy sync, unmap & unlink shm

/* reader ops (API) */
int rwlog_begin_read(void);                                         // enter read section (may block)
ssize_t rwlog_snapshot(rwlog_entry_t *buf, size_t max_entries);     // copy newest <= max_entries to caller buffer
int rwlog_end_read(void);                                           // leave read section 

/* writer ops (API) */
int rwlog_begin_write(void);                                        // enter write section (may block)
int rwlog_append(const rwlog_entry_t *e);                           // append one entry (must be inside write section)
int rwlog_end_write(void);                                          // leave write section 

/* Wake any threads blocked in the monitor (used on shutdown). Safe to call anytime. */
void rwlog_wake_all(void);