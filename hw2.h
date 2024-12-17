#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#define MAX_THREADS 4096
#define MAX_LINE 1024
#define MAX_COUNTERS 100
#define MAX_JOBS 1024

typedef struct JobQueue { // thread safe queue
    char* jobs[MAX_JOBS];
    long long dispatch_times[MAX_JOBS];
    int front; // position of the first element
    int rear; // position where the next element will be added
    int count; // total number of items currently in the queue
    pthread_mutex_t lock; // binary semaphore
    pthread_cond_t not_empty; // command variables
    pthread_cond_t not_full;
    bool shutdown; // New flag to signal shutdown
    long long time;
    bool log_enabled;
} JobQueue;

typedef struct {
    JobQueue* queue;
    int thread_id; // Unique thread ID
} WorkerArgs;


void* worker_thread(void* arg);
void create_counter_files(int num_counters);
void create_threads(int num_threads, int* thread_ids,pthread_t* threads, JobQueue* queue);
void read_lines(FILE* cmdfile, int* thread_ids, pthread_t* threads, JobQueue* queue, int num_threads, long long start_time, bool log_en);
void init_queue(JobQueue* queue, long long start_time, bool log_enabled);
void enqueue(JobQueue* queue, const char* job);
char* dequeue(JobQueue* queue, long long *dispatch_time);
void execute_command(char* cmd, long long start_time, int TID, bool log_enabled);
void create_thread_files(int num_threads);
long long get_current_time_in_milliseconds();
void print_to_log_file(long long curr, char* cmd,int TID, char* end_or_start);
void create_stats_file();

pthread_mutex_t file_mutexes[MAX_COUNTERS]; // Array of mutexes for files (assuming a maximum of 100 files)

//handling active threads
pthread_cond_t active_threades_cond;
pthread_mutex_t active_threades_mutex;
int active_threades = 0;
long long total_running_time = 0;
long long sum_turnaround = 0;
long long min_turnaround = 0;
long long max_turnaround = 0;
float avg_turnaround = 0;
int total_jobs_processed = 0;

// Initialize all mutexes
void initialize_file_mutexes() {
    for (int i = 0; i < MAX_COUNTERS; i++) {
        pthread_mutex_init(&file_mutexes[i], NULL);
    }
}

// Initialize all mutexes
void destroy_file_mutexes() {
    for (int i = 0; i < MAX_COUNTERS; i++) {
        pthread_mutex_destroy(&file_mutexes[i]);
    }
}
