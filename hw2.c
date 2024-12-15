#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

typedef struct JobQueue { // thread safe queue
    char* jobs[1024];
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
char* dequeue(JobQueue* queue);
void execute_command(char* cmd, long long start_time, int TID, bool log_enabled);
void create_thread_files(int num_threads);
long long get_current_time_in_milliseconds();
void print_to_log_file(long long curr, char* cmd,int TID);
void print_to_log_file_end(long long curr, char* cmd,int TID);

pthread_mutex_t file_mutexes[100]; // Array of mutexes for files (assuming a maximum of 100 files)

//handling active threads
pthread_cond_t active_threades_cond;
pthread_mutex_t active_threades_mutex;
int active_threades = 0;



// Initialize all mutexes
void initialize_file_mutexes() {
    for (int i = 0; i < 100; i++) {
        pthread_mutex_init(&file_mutexes[i], NULL);
    }
}

int main(int argc, char *argv[]) {
    long long start_time = get_current_time_in_milliseconds(); // save start time of the program
    
    if (argc != 5) { 
        printf("Error! Number of arguments isn't 5\n");
        return 1;
    }

    // convert str to int
    int num_counters = atoi(argv[3]); 
    int num_threads = atoi(argv[2]);
    bool log_enabled = atoi(argv[4]);

    int thread_ids[4096] = {0}; //its required to create num_threads new threads
    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    JobQueue* queue = (JobQueue*)malloc(sizeof(JobQueue)); //make sure its one queue

    FILE* cmdfile = fopen(argv[1], "r");
    if (cmdfile == NULL) {
        perror("Error opening file");
        return 1;
    }

    init_queue(queue, start_time, log_enabled);
    //init global counter mutex and cond
    pthread_mutex_init(&active_threades_mutex, NULL);
    pthread_cond_init(&active_threades_cond, NULL);

    create_counter_files(num_counters);
    if (log_enabled) {
        create_thread_files(num_threads);
        FILE* file = fopen("dispatcher.txt", "w"); // reset the dispatcher logfile
    }
    create_threads(num_threads,thread_ids,threads, queue);
    read_lines(cmdfile, thread_ids, threads, queue, num_threads, start_time, log_enabled);
    fclose(cmdfile); 

    //wait for background  to empty
    pthread_mutex_lock(&queue->lock);
    while (queue->count > 0) {
        pthread_cond_wait(&queue->not_empty, &queue->lock); // Wait for workers to process all jobs
    }
    queue->shutdown = true; // Signal shutdown only after queue is empty
    pthread_cond_broadcast(&queue->not_empty); // Wake all threads to terminate
    pthread_mutex_unlock(&queue->lock);
    //not sure we need:
    // pthread_mutex_lock(&active_threades_mutex);
    // if (active_threades == 0) {
    //     pthread_cond_broadcast(&active_threades_cond);
    //     printf("active therds\n");
    // }
    // pthread_mutex_unlock(&active_threades_mutex);

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(queue);
    pthread_mutex_lock(&queue->lock);
    // clean up mutexes
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&active_threades_cond);
    pthread_cond_destroy(&queue->not_full);

    return 0;
}

void* worker_thread(void* arg) {
    WorkerArgs* args = (WorkerArgs*)arg; // Cast to WorkerArgs*
    JobQueue* queue = args->queue;
    int thread_id = args->thread_id; // Get the thread ID

    while (1) {
        char* job = dequeue(queue);
        if (job == NULL) { // Shutdown signal or no more jobs
            break;
        }
        char* token = strtok(job, ";");
        while (token != NULL) {
            execute_command(token, queue->time, thread_id, queue->log_enabled);
            token = strtok(NULL, ";");
        }
        free(job); // Free the dequeued job
    }
    free(args);
    return NULL;
}


void init_queue(JobQueue* queue, long long start_time, bool log_enabled) {
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    queue->shutdown = false;
    queue->time = start_time;
    queue->log_enabled = log_enabled;
    memset(queue->jobs,0,sizeof(queue->jobs));
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
}

void enqueue(JobQueue* queue, const char* job) {
    pthread_mutex_lock(&queue->lock);

    // If shutdown is active, reject new jobs
    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->lock);
        return; // Do not enqueue
    }

    while (queue->count == 1024) { // If queue is full, wait
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }

    // Add the job to the queue
    queue->jobs[queue->rear] = strdup(job); // Duplicate job string
    queue->rear = (queue->rear + 1) % 1024; // Circular queue logic
    queue->count++;

    pthread_cond_signal(&queue->not_empty); // Notify one waiting thread
    pthread_mutex_unlock(&queue->lock);
}


char* dequeue(JobQueue* queue) {
    pthread_mutex_lock(&queue->lock);
    while (queue->count == 0 && !queue->shutdown) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }
    if (queue->shutdown && queue->count == 0) {
        pthread_mutex_unlock(&queue->lock);
        return NULL; // Return NULL to indicate shutdown
    }
    char* job = queue->jobs[queue->front];
    queue->front = (queue->front + 1) % 1024;
    queue->count--;
    if (queue->count == 0) {
        pthread_cond_broadcast(&queue->not_empty);
    }
    pthread_cond_signal(&queue->not_full); // Signal only one thread
    pthread_mutex_unlock(&queue->lock);
    return job;
}

// exectue a cmd by a worker
void execute_command(char* cmd, long long start_time, int TID, bool log_enabled) {
    char* cmd1 = strtok(cmd, " ");
    char* cmd2 = strtok(NULL, "");
    if (strcmp(cmd1, "increment") != 0 &&
        strcmp(cmd1, "decrement") != 0 &&
        strcmp(cmd1, "msleep") != 0)  {
        return;
    }
    pthread_mutex_lock(&active_threades_mutex);
    active_threades += 1;
    pthread_mutex_unlock(&active_threades_mutex);
    if (log_enabled) {
        long long curr_time = get_current_time_in_milliseconds();
        print_to_log_file(curr_time-start_time, cmd, TID);
    }

    int number = atoi(cmd2); // Convert cmd2 to an integer

    char filename[14];
    snprintf(filename, sizeof(filename), "count%02d.txt", number);

    if (strcmp(cmd1, "msleep") == 0) {
        int x = atoi(cmd2);
        usleep(x * 1000); // Sleep in microseconds
    }    
    else if (strcmp(cmd1, "increment") == 0) {
        pthread_mutex_lock(&file_mutexes[number]); // Lock the mutex for the file
        FILE* file = fopen(filename, "r");
        long long int value;
        fscanf(file, "%lld", &value);
        fclose(file);

        file = fopen(filename, "w");
        fprintf(file, "%lld\n", value + 1);
        fclose(file);
        pthread_mutex_unlock(&file_mutexes[number]); // Unlock the mutex for the file

    } else if (strcmp(cmd1, "decrement") == 0) {
        pthread_mutex_lock(&file_mutexes[number]); // Lock the mutex for the file
        FILE* file = fopen(filename, "r");
        long long int value;
        fscanf(file, "%lld", &value);
        fclose(file);

        file = fopen(filename, "w");
        fprintf(file, "%lld\n", value - 1);
        fclose(file);
        pthread_mutex_unlock(&file_mutexes[number]); // Unlock the mutex for the file
    }
    if (log_enabled) {
        long long curr_time = get_current_time_in_milliseconds();
        print_to_log_file_end(curr_time-start_time, cmd, TID);
    }
    pthread_mutex_lock(&active_threades_mutex);
    active_threades -= 1;
    printf("checking active threads:%d\n", active_threades);
    if (active_threades == 0) {
        pthread_cond_broadcast(&active_threades_cond);
        printf("broadcasting\n");
    }
    pthread_mutex_unlock(&active_threades_mutex);
}


void create_counter_files(int num_counters) {
    // create counter files - should we put it in a function?
    for (int i = 0; i < num_counters; i++) {
        char filename[20]; 
        snprintf(filename, sizeof(filename), "count%02d.txt", i);
        FILE *file = fopen(filename, "w");
        if (file == NULL) {
            perror("Error creating file");
        }
        fprintf(file, "0");
        fclose(file);
    }
}

void create_thread_files(int num_threads) {
    for (int i = 0; i < num_threads; i++) {
        char filename[21]; 
        snprintf(filename, sizeof(filename), "thread%04d.txt", i);
        FILE *file = fopen(filename, "w");
        if (file == NULL) {
            perror("Error creating file");
        }
        fclose(file);
    }
}

void create_threads(int num_threads, int* thread_ids, pthread_t* threads, JobQueue* queue) { 
    if (threads == NULL) {
        perror("Failed to allocate memory");
    }

    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i; // Assign a unique ID to each thread
        WorkerArgs* args = malloc(sizeof(WorkerArgs));
        args->queue = queue;
        args->thread_id = thread_ids[i]; // Pass the unique thread ID

        if (pthread_create(&threads[i], NULL, worker_thread, args) != 0) {
            perror("Failed to create thread");
            free(args); // Free memory if thread creation fails
        }
    }
}


void read_lines(FILE* cmdfile, int* thread_ids, pthread_t* threads, JobQueue* queue, int num_threads, long long start_time, bool log_en) {
    char line[1024]; // Buffer to hold each line
    char* token;     // Token for parsing
    const char* delimiter = ";"; // Command delimiter

    while (fgets(line, sizeof(line), cmdfile)) {
        line[strcspn(line, "\n")] = '\0';
        char line_cpy[1024];
        strcpy(line_cpy, line);
        // check if worker or dispatcher
        token = strtok(line, " "); // Split by space
        if (token == NULL) {
            continue; // Skip empty lines
        }

        else if (strcmp(token, "dispatcher_msleep") == 0) {
            if(log_en){
                    FILE* file = fopen("dispatcher.txt", "a");
                    long long curr_time = get_current_time_in_milliseconds() - start_time;
                    fprintf(file, "TIME %lld: read cmd line: %s\n", curr_time, line_cpy);
                    fclose(file);
            }
            int x = atoi(strtok(NULL, "\n"));
            usleep(x*1000);
            continue;
        }
        else if (strcmp(token, "dispatcher_wait") == 0) {
            if(log_en){
                    FILE* file = fopen("dispatcher.txt", "a");
                    long long curr_time = get_current_time_in_milliseconds() - start_time;
                    fprintf(file, "TIME %lld: read cmd line: %s\n", curr_time, line_cpy);
                    fclose(file);
            }

            //fix me
            printf("active thereads: %d\n", active_threades);
            pthread_mutex_lock(&active_threades_mutex);
            while (active_threades > 0) {
                pthread_cond_wait(&active_threades_cond, &active_threades_mutex);
            }
            pthread_mutex_unlock(&active_threades_mutex);
            continue;

            // pthread_mutex_lock(&queue->lock);

            // while (queue->count > 0) {
            //     pthread_cond_wait(&queue->not_empty, &queue->lock);
            // }
            // pthread_mutex_unlock(&queue->lock);
            // continue;
        }

        else if (strcmp(token, "worker") == 0) {
            char temp_line[1024];
            char org_line[1024];
            int times = 1;

            strcpy(org_line, strtok(NULL,""));
            strcpy(temp_line, org_line);

            char* token1 = strtok(temp_line, " ");
            if (strcmp(token1, "repeat") == 0) { // if repeat from copied line
                times = atoi(strtok(NULL, ";")); 
            }

            for (int i = 0 ; i < times; i++){
                strcpy(temp_line, org_line);
                token = strtok(temp_line,";");
                while (token != NULL){
                    enqueue(queue, token);
                    token = strtok(NULL, ";");
                    }
                }
            }
        }

}

long long get_current_time_in_milliseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL); // Get the current time

    // Convert to milliseconds
    long long milliseconds = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return milliseconds;
}

void print_to_log_file(long long curr_time, char* cmd,int TID) {
    char filename[16];
    snprintf(filename, sizeof(filename), "thread%04d.txt", TID);

    FILE* file = fopen(filename, "a");
    fprintf(file, "TIME %lld: START job %s\n", curr_time, cmd);
    fclose(file);
}

void print_to_log_file_end(long long curr_time, char* cmd,int TID) {
    char filename[16];
    snprintf(filename, sizeof(filename), "thread%04d.txt", TID);

    FILE* file = fopen(filename, "a");
    fprintf(file, "TIME %lld: END job %s\n", curr_time, cmd);
    fclose(file);
}