/* Add appropriate header files */
#include "rw_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdatomic.h>

struct config {
    int capacity;
    int readers;
    int writers;
    int writer_batch;
    int seconds;
    int rd_us;
    int wr_us;
    int dump_csv;
};

static void print_usage(const char *progname) 
{
	fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "-c,  --capacity <N>        Log capacity (default 1024)\n"
        "-r,  --readers <N>         Number of reader threads (default 6)\n"
        "-w,  --writers <N>         Number of writer threads (default 4)\n"
        "-b,  --writer-batch <N>    Entries written per writer section (default 2)\n"
        "-s,  --seconds <N>         Total run time (default 10)\n"
        "-R,  --rd-us <usec>        Reader sleep between operations (default 2000)\n"
        "-W,  --wr-us <usec>        Writer sleep between operations (default 3000)\n"
        "-d,  --dump                Dump final log to log.csv\n"
        "-h,  --help                Show this help message\n",
        progname);
}

static void parse_args(int argc, char **argv, struct config *cfg) 
{
	// Set defaults
    cfg->capacity     = 1024;
    cfg->readers      = 6;
    cfg->writers      = 4;
    cfg->writer_batch = 2;
    cfg->seconds      = 10;
    cfg->rd_us        = 2000;
    cfg->wr_us        = 3000;
    cfg->dump_csv     = 0;
	
	/* define the long_opts options */
    static struct option long_opts[] = {
        {"capacity",      required_argument, 0, 'c'},
        {"readers",       required_argument, 0, 'r'},
        {"writers",       required_argument, 0, 'w'},
        {"writer-batch",  required_argument, 0, 'b'},
        {"seconds",       required_argument, 0, 's'},
        {"rd-us",         required_argument, 0, 'R'},
        {"wr-us",         required_argument, 0, 'W'},
        {"dump",          no_argument,       0, 'd'},
        {"help",          no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
	
	
	/* Parse each of the arguments using getopt_long() function */
    int option;
    while ((option = getopt_long(argc, argv, "c:r:w:b:s:R:W:dh", long_opts, NULL)) != -1) {
        switch (option) {
            case 'c': cfg->capacity = atoi(optarg); break;
            case 'r': cfg->readers = atoi(optarg); break;
            case 'w': cfg->writers = atoi(optarg); break;
            case 'b': cfg->writer_batch = atoi(optarg); break;
            case 's': cfg->seconds = atoi(optarg); break;
            case 'R': cfg->rd_us = atoi(optarg); break;
            case 'W': cfg->wr_us = atoi(optarg); break;
            case 'd': cfg->dump_csv = 1; break;
            case 'h': print_usage(argv[0]); exit(0);
            case '?':
                // getopt_long already printed an error message
                print_usage(argv[0]);
                exit(1);
            default: abort();
        }
    }
}

/* Helper functions for time measurement */
static inline struct timespec ts_now_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts;
}

static inline double ts_diff_msec(struct timespec start, struct timespec end) {
    time_t sec = end.tv_sec - start.tv_sec; long nsec = end.tv_nsec - start.tv_nsec;
    if (nsec < 0) { sec -= 1; nsec += 1000000000L; }
    return (double)sec * 1000.0 + (double)nsec / 1000000.0;
}

/* Global stop flag and signal handler */
static atomic_int stop_flag = 0;
static void handle_sigint(int sig) { 
    (void)sig; 
    atomic_store(&stop_flag, 1);
    rwlog_wake_all(); // wake any blocked threads
}

/* Thread argument and statistics structures */
typedef struct {
    unsigned long long writes_completed;
    unsigned long long entries_appended;
    double wait_time_msec;
    unsigned long long wait_count;
    unsigned long long snapshots_taken;
    double read_cs_time_msec;
    unsigned long long read_cs_count;
} thread_stats_t;

typedef struct {
    int id;
    struct config *cfg;
    thread_stats_t stats;
} thread_arg_t;

/* Writer and Reader thread functions */
static void *writer_main(void *arg);
static void *reader_main(void *arg);

int main(int argc, char **argv) 
{
    struct config cfg;
    parse_args(argc, argv, &cfg);

    printf("capacity=%d readers=%d writers=%d batch=%d seconds=%d rd_us=%d wr_us=%d dump=%d\n",
           cfg.capacity, cfg.readers, cfg.writers, cfg.writer_batch,
           cfg.seconds, cfg.rd_us, cfg.wr_us, cfg.dump_csv);

    /* your remaining initialization here... */
	
	/* Initialize the shm-backed monitor */
    if (rwlog_create((size_t)cfg.capacity) != 0) {
        perror("rwlog_create failed");
        exit(1);
    }
 
    /* Install SIGINT and start wall-clock timer thread */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    struct timespec start_time = ts_now_monotonic();
	
	/* Create the writer threads */
    int total_threads = cfg.readers + cfg.writers;
    pthread_t *threads = calloc((size_t)total_threads, sizeof(pthread_t));
    thread_arg_t *thread_args = calloc((size_t)total_threads, sizeof(thread_arg_t));
    if (!threads || !thread_args) {
        perror("calloc threads/args failed");
        free(threads); free(thread_args);
        rwlog_destroy();
        exit(1);
    }

    int thread_idx = 0;
    for (int i = 0; i < cfg.writers; i++, thread_idx++) {
        thread_args[thread_idx].id = i;
        thread_args[thread_idx].cfg = &cfg;
        if (pthread_create(&threads[thread_idx], NULL, writer_main, &thread_args[thread_idx]) != 0) {
            perror("pthread_create writer failed");
            atomic_store(&stop_flag, 1);
            break;
        }
    }
	 
    /* Create the reader threads */
    for (int i = 0; i < cfg.readers; i++, thread_idx++) {
        thread_args[thread_idx].id = i;
        thread_args[thread_idx].cfg = &cfg;
        if (pthread_create(&threads[thread_idx], NULL, reader_main, &thread_args[thread_idx]) != 0) {
            perror("pthread_create reader failed");
            atomic_store(&stop_flag, 1);
            break;
        }
    }
    
    /* Join reader/writer threads and timer thread */
    for (int s = 0; s < cfg.seconds && !atomic_load(&stop_flag); s++) {
        sleep(1);
    }
    atomic_store(&stop_flag, 1);
    rwlog_wake_all(); // wake any blocked threads

    for (int i = 0; i < total_threads; i++) {
        if (threads[i]) { pthread_join(threads[i], NULL); }
    }

    struct timespec end_time = ts_now_monotonic();
    double elapsed_msec = ts_diff_msec(start_time, end_time);
    
    /* Optional: dump the final log to CSV for inspection/grading. */
    if (cfg.dump_csv) {
        FILE *fp = fopen("log.csv", "w");
        if (!fp) {
            perror("fopen log.csv failed");
        } else {
            rwlog_entry_t *buf = calloc((size_t)cfg.capacity, sizeof(rwlog_entry_t));
            if (!buf) {
                perror("calloc dump buffer failed");
            } else {
                if (rwlog_begin_read() == 0) {
                    ssize_t n = rwlog_snapshot(buf, (size_t)cfg.capacity);
                    rwlog_end_read();
                    if (n >= 0) {
                        fprintf(fp, "seq,tid,ts_sec,ts_nsec,msg\n");
                        for (ssize_t i = 0; i < n; i++) {
                            fprintf(fp, "%llu,%p,%ld,%ld,%s\n",
                                    (unsigned long long)buf[i].seq,
                                    (void*)buf[i].tid,
                                    (long)buf[i].ts.tv_sec,
                                    (long)buf[i].ts.tv_nsec,
                                    buf[i].msg);
                        }
                    } else {
                        perror("rwlog_snapshot failed");
                    }
                } else {
                    perror("rwlog_begin_read failed");
                }

                free(buf);
            }
            fclose(fp);
            printf("wrote log.csv\n");
        }
    }
    
    /* Compute averages only (avg reader wait, avg writer wait, avg throughput) */
    unsigned long long total_entries = 0;
    double writer_wait_sum = 0.0;
    unsigned long long writer_wait_count = 0;
    double reader_cs_time_sum = 0.0;
    unsigned long long reader_cs_count = 0;

    for (int i = 0; i < total_threads; i++) {
        thread_stats_t *ts = &thread_args[i].stats;
        total_entries += ts->entries_appended;
        writer_wait_sum += ts->wait_time_msec;
        writer_wait_count += ts->wait_count;
        reader_cs_time_sum += ts->read_cs_time_msec;
        reader_cs_count += ts->read_cs_count;
    }

    double avg_writer_wait = (writer_wait_count > 0) ? (writer_wait_sum / (double)writer_wait_count) : 0.0;
    double avg_reader_cs = (reader_cs_count > 0) ? (reader_cs_time_sum / (double)reader_cs_count) : 0.0;
    double throughput = (elapsed_msec > 0.0) ? ((double)total_entries / (elapsed_msec / 1000.0)) : 0.0;

    printf("\n=== Run Summary ===\n");
    printf("Elapsed time: %.2f msec\n", elapsed_msec);
    printf("Avg writer wait time: %.3f msec over %llu waits\n", avg_writer_wait, writer_wait_count);
    printf("Avg reader CS time: %.3f msec over %llu reads\n", avg_reader_cs, reader_cs_count);
    printf("Total entries appended: %llu\n", total_entries);
    printf("Overall throughput: %.2f entries/sec\n", throughput);
    
    /* Cleanup heap and monitor resources */
    rwlog_destroy();
    free(threads);
    free(thread_args);
	  
	return 0; 
}

/* Writer and Reader thread implementations */
static void *writer_main(void *arg) {
    thread_arg_t *ctx = (thread_arg_t *)arg;

    while (!atomic_load(&stop_flag)) {
        struct timespec wait_start = ts_now_monotonic();
        if (rwlog_begin_write() != 0) {
            if (atomic_load(&stop_flag) || errno == EINVAL) break;
            perror("rwlog_begin_write failed");
            continue;
        }
        struct timespec wait_end = ts_now_monotonic();
        double wait_time = ts_diff_msec(wait_start, wait_end);
        ctx->stats.wait_time_msec += wait_time;
        ctx->stats.wait_count++;

        for (int i = 0; i < ctx->cfg->writer_batch; i++) {
            rwlog_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            snprintf(entry.msg, RWLOG_MSG_MAX, "Writer %d entry %llu", ctx->id, ctx->stats.entries_appended + 1);
            if (rwlog_append(&entry) == 0) {
                ctx->stats.entries_appended++;
            } else {
                perror("rwlog_append failed");
            }
        }

        rwlog_end_write();
        ctx->stats.writes_completed++;

        if (atomic_load(&stop_flag)) break;
        if (ctx->cfg->wr_us > 0) {
            usleep((useconds_t)ctx->cfg->wr_us);
        }
    }
    return NULL;
}

static void *reader_main(void *arg) {
    thread_arg_t *ctx = (thread_arg_t *)arg;
    rwlog_entry_t *snapshot_buf = calloc((size_t)ctx->cfg->capacity, sizeof(rwlog_entry_t));
    if (!snapshot_buf) {
        perror("calloc reader snapshot buffer failed");
        return NULL;
    }

    while (!atomic_load(&stop_flag)) {
        if (rwlog_begin_read() != 0) {
            if (atomic_load(&stop_flag) || errno == EINVAL) break;
            perror("rwlog_begin_read failed");
            continue;
        }
        struct timespec cs_start = ts_now_monotonic();

        ssize_t n = rwlog_snapshot(snapshot_buf, (size_t)ctx->cfg->capacity);
        if (n >= 0) {
            ctx->stats.snapshots_taken++;
        } else {
            perror("rwlog_snapshot failed");
        }

        struct timespec cs_end = ts_now_monotonic();
        double cs_time = ts_diff_msec(cs_start, cs_end);

        rwlog_end_read();

        ctx->stats.read_cs_time_msec += cs_time;
        ctx->stats.read_cs_count++;

        if (atomic_load(&stop_flag)) break;
        if (ctx->cfg->rd_us > 0) {
            usleep((useconds_t)ctx->cfg->rd_us);
        }
    }

    free(snapshot_buf);
    return NULL;
}