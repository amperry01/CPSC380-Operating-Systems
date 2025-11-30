#include "rw_log.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct {
    rwlog_entry_t *buffer;     // circular buffer for log entries
    size_t capacity;           // max number of entries in buffer
    size_t count;              // current number of entries in buffer
    size_t head;               // next write position in buffer
    uint64_t next_seq;         // next sequence number to assign

    // synchronization primitives
    pthread_mutex_t mutex;     // mutex for protecting shared data
    pthread_cond_t can_read;   // condition variable for readers
    pthread_cond_t can_write;  // condition variable for writers

    int active_readers;        // number of active readers
    int waiting_writers;       // number of waiting writers
    int writer_active;         // flag indicating if a writer is active (1) or not (0)
} rwlog_monitor_t;

static rwlog_monitor_t *monitor = NULL;

/* lifecycle */
int rwlog_create(size_t capacity){
    if (capacity == 0) { errno = EINVAL; return -1; }
    monitor = calloc(1, sizeof(*monitor)); // allocate monitor
    if (!monitor) { return -1; }
    monitor->buffer = calloc(capacity, sizeof(rwlog_entry_t)); // allocate buffer
    if (!monitor->buffer) { free(monitor); monitor = NULL; return -1; } // cleanup on failure

    // initialize monitor fields
    monitor->capacity = capacity;
    monitor->count = 0;
    monitor->head = 0;
    monitor->next_seq = 1;

    // initialize synchronization primitives
    pthread_mutex_init(&monitor->mutex, NULL);
    pthread_cond_init(&monitor->can_read, NULL);
    pthread_cond_init(&monitor->can_write, NULL);

    return 0;
}

int rwlog_destroy(void){
    if (!monitor) { errno = EINVAL; return -1; }

    // destroy synchronization primitives
    pthread_cond_destroy(&monitor->can_read);
    pthread_cond_destroy(&monitor->can_write);
    pthread_mutex_destroy(&monitor->mutex);

    // free allocated memory
    free(monitor->buffer);
    free(monitor);
    monitor = NULL;

    return 0;
}

/* reader ops (API) */
int rwlog_begin_read(void){
    if (!monitor) { errno = EINVAL; return -1; }

    // acquire read lock with writer preference
    pthread_mutex_lock(&monitor->mutex);
    while (monitor->writer_active || monitor->waiting_writers > 0) {
        pthread_cond_wait(&monitor->can_read, &monitor->mutex);
    }
    monitor->active_readers++;
    pthread_mutex_unlock(&monitor->mutex);

    return 0;
}

// readers copy the most recent <= max_entries into buf
ssize_t rwlog_snapshot(rwlog_entry_t *buf, size_t max_entries){
    if (!monitor || !buf || max_entries == 0) { errno = EINVAL; return -1; }

    pthread_mutex_lock(&monitor->mutex); // lock monitor for reading

    // determine how many entries to copy
    size_t available = monitor->count;
    size_t to_copy = (available < max_entries) ? available : max_entries;

    if (to_copy == 0) { pthread_mutex_unlock(&monitor->mutex); return 0; }

    // copy entries from circular buffer in order
    size_t cap = monitor->capacity;
    size_t head = monitor->head;
    size_t start = (head + cap + monitor->count - to_copy) % cap;

    // perform the copy
    for (size_t i = 0; i < to_copy; i++) {
        size_t src = (start + i) % cap;
        buf[i] = monitor->buffer[src];
    }

    pthread_mutex_unlock(&monitor->mutex); // unlock monitor

    return (ssize_t)to_copy;
}

int rwlog_end_read(void){
    if (!monitor) { errno = EINVAL; return -1; }

    // release read lock
    pthread_mutex_lock(&monitor->mutex);
    monitor->active_readers--;
    if (monitor->active_readers == 0 && monitor->waiting_writers > 0) {
        pthread_cond_signal(&monitor->can_write);
    }

    pthread_mutex_unlock(&monitor->mutex);

    return 0;
}

/* writer ops (API) */
int rwlog_begin_write(void){
    if (!monitor) { errno = EINVAL; return -1; }

    // acquire write lock with writer preference
    pthread_mutex_lock(&monitor->mutex);
    monitor->waiting_writers++;
    while (monitor->active_readers > 0 || monitor->writer_active) {
        pthread_cond_wait(&monitor->can_write, &monitor->mutex);
    }
    monitor->waiting_writers--;
    monitor->writer_active = 1;

    // keep mutex locked; caller will append then call end_write

    return 0;
}

int rwlog_append(const rwlog_entry_t *e){
    if (!monitor || !e) { errno = EINVAL; return -1; }
    
    // mutex is held from begin_write
    rwlog_entry_t entry = *e;
    entry.seq = monitor->next_seq++;
    entry.tid = pthread_self();
    clock_gettime(CLOCK_REALTIME, &entry.ts);

    // append entry to circular buffer
    monitor->buffer[monitor->head] = entry;
    monitor->head = (monitor->head + 1) % monitor->capacity;
    if (monitor->count < monitor->capacity) { monitor->count++; }

    return 0;
}

int rwlog_end_write(void){
    if (!monitor) { errno = EINVAL; return -1; }

    // mutex is held from begin_write
    monitor->writer_active = 0;
    if (monitor->waiting_writers > 0) {
        pthread_cond_signal(&monitor->can_write); // give priority to waiting writers
    } else {
        pthread_cond_broadcast(&monitor->can_read); // let readers in
    }

    pthread_mutex_unlock(&monitor->mutex);

    return 0;
}

/* wake all waiting readers and writers (for shutdown) */
void rwlog_wake_all(void){
    if (!monitor) { return; }

    pthread_mutex_lock(&monitor->mutex);
    pthread_cond_broadcast(&monitor->can_write);
    pthread_cond_broadcast(&monitor->can_read);
    pthread_mutex_unlock(&monitor->mutex);
}