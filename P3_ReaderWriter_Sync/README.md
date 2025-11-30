# PA3: My Reader–Writer Synchronization Implementation

For this project, I built my own solution to the Readers–Writers problem using POSIX threads. The goal was to create a writer-preference monitor that lets multiple readers access shared data at the same time but gives writers exclusive access and makes sure they don’t get starved by readers.

## Features
- **Monitor-Based Synchronization:** I used `pthread_mutex_t` and `pthread_cond_t` to implement a writer-preference monitor. This keeps the threads coordinated without busy-waiting.
- **Concurrent Readers:** Multiple readers can read the shared log simultaneously when no writers are waiting, which improves efficiency.
- **Exclusive Writers:** Only one writer can append to the log at a time, and writers get priority so they don’t wait forever.
- **Circular Log Buffer:** The log is a ring buffer that stores recent entries with sequence numbers, timestamps, and messages, keeping memory usage bounded.
- **Thread Management:** You can specify how many reader and writer threads to run, how long the program runs, and the batch size for logging.
- **Graceful Shutdown:** The threads stop cleanly after the set time or if you hit Ctrl+C, so no log entries get lost or corrupted.

## Implementation Details
- **System Calls / APIs:**
  - `pthread_create()` and `pthread_join()` to start and wait for threads.
  - `pthread_mutex_lock()` to protect shared data.
  - `pthread_cond_wait()`, `pthread_cond_signal()`, and `pthread_cond_broadcast()` to manage thread waiting and waking without wasting CPU.
  - `clock_gettime()` to timestamp log entries.
  - `sigaction()` to handle Ctrl+C interrupts gracefully.
  - `sleep()` to control how long the program runs.
- **Data Structures:**
  - **`rwlog_monitor_t`** — the main monitor that coordinates readers and writers.
  - **`log_entry_t`** — each log entry has a sequence number, thread ID, timestamp, and a message:
    ```c
    typedef struct log_entry {
        unsigned long long seq;
        pthread_t tid;
        time_t ts_sec;
        long ts_nsec;
        char msg[64];
    } log_entry_t;
    ```
- **Synchronization Policy:**
  - Readers wait if a writer is active or waiting.
  - Writers get access only when all readers are done.
  - Condition variables make sure threads don’t waste CPU cycles while waiting.

## Usage
Compile the program with:
```
gcc -O2 -Wall -Wextra -pthread rw_main.c rw_log.c -o rwlog_test
```
Run it like this:
```
./rwlog_test -r <num_readers> -w <num_writers> -b <batch_size> -s <seconds> [-R <reader_us>] [-W <writer_us>] [-c <capacity>] [-d]
```

### Example Runs
#### Single Writer
Here’s an example with one writer and no readers:
```
$ ./rwlog_test -r 0 -w 1 -b 3 -s 2
capacity=1024 readers=0 writers=1 batch=3 seconds=2 rd_us=2000 wr_us=3000 dump=0
=== Run Summary ===
Elapsed time: 2010.99 msec
Avg writer wait time: 0.000 msec
Avg reader CS time: 0.000 msec
Total entries appended: 1614
Overall throughput: 802.59 entries/sec
```

#### Mixed Readers and Writers
This one shows mixed threads under load — six readers and one writer:
```
$ ./rwlog_test -r 6 -w 1 -b 8 -s 5 -R 500 -W 2000
capacity=1024 readers=6 writers=1 batch=8 seconds=5 rd_us=500 wr_us=2000 dump=0
=== Run Summary ===
Elapsed time: 5017.09 msec
Avg writer wait time: 0.017 msec
Avg reader CS time: 0.012 msec
Total entries appended: 16472
Overall throughput: 3283.18 entries/sec
```

#### Wraparound with CSV Dump
Here’s a run with a small buffer to test wraparound and CSV dumping:
```
$ ./rwlog_test -c 8 -r 2 -w 2 -b 4 -s 4 -d
wrote log.csv
=== Run Summary ===
Elapsed time: 4013.90 msec
Avg writer wait time: 0.001 msec
Avg reader CS time: 0.000 msec
Total entries appended: 8628
Overall throughput: 2149.53 entries/sec
```

## Testing
I tested both single-threaded and mixed cases to make sure timing stayed consistent and counts matched expectations. I checked that writers really get priority and don’t starve even when readers are busy. I also verified edge cases like zero capacity (which the program rejects) and empty logs dumping valid CSV headers. Interrupting with Ctrl+C triggers a clean shutdown without losing data. Finally, I ran ThreadSanitizer to confirm there are no data races or synchronization issues.