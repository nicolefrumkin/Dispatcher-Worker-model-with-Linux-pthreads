#include "hw2.h"

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

    int thread_ids[MAX_THREADS] = {0}; //its required to create num_threads new threads
    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    JobQueue* queue = (JobQueue*)malloc(sizeof(JobQueue)); //make sure its one queue

    FILE* cmdfile = fopen(argv[1], "r");
    if (cmdfile == NULL) {
        perror("Error opening file");
        return 1;
    }

    init_queue(queue, start_time, log_enabled);
    initialize_file_mutexes();
    //init global counter mutex and cond
    pthread_mutex_init(&active_threades_mutex, NULL);
    pthread_cond_init(&active_threades_cond, NULL);

    create_counter_files(num_counters);
    if (log_enabled) {
        create_thread_files(num_threads);
        FILE* file = fopen("dispatcher.txt", "w"); // reset the dispatcher logfile
        if (file == NULL) {
            perror("Error opening file");
            return 1;
        }
        fclose(file);
    }
    create_threads(num_threads,thread_ids,threads, queue);
    read_lines(cmdfile, thread_ids, threads, queue, num_threads, start_time, log_enabled);
    fclose(cmdfile); 

    //wait for background  to empty
    pthread_mutex_lock(&active_threades_mutex);
    while (active_threades > 0) {
        pthread_cond_wait(&active_threades_cond, &active_threades_mutex);
    }
    queue->shutdown = true; // Signal shutdown only after queue is empty
    pthread_cond_broadcast(&queue->not_empty); // Wake all threads to terminate
    pthread_mutex_unlock(&active_threades_mutex);

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(queue);

    // clean up mutexes
    pthread_mutex_destroy(&queue->lock);
    pthread_mutex_destroy(&active_threades_mutex);
    destroy_file_mutexes();
    //clean up cond
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    pthread_cond_destroy(&active_threades_cond);
    
    // calc stats
    total_running_time = get_current_time_in_milliseconds()-start_time;
    avg_turnaround = roundf((float)sum_turnaround / total_jobs_processed * 1000) / 1000; // Round to 3 digits after the point
    create_stats_file();
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
        long long completion_time = get_current_time_in_milliseconds();
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
    memset(queue->dispatch_times,0,sizeof(queue->dispatch_times));
    memset(queue->jobs,0,sizeof(queue->jobs));
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
}

void enqueue(JobQueue* queue, const char* job) {
    pthread_mutex_lock(&queue->lock);
    while (queue->count == MAX_JOBS) { // If queue is full, wait
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }
    // Add the job to the queue
    queue->jobs[queue->rear] = strdup(job); // Duplicate job string
    queue->dispatch_times[queue->rear] = get_current_time_in_milliseconds();
    queue->rear = (queue->rear + 1) % MAX_JOBS; // Circular queue logic
    queue->count++;
    pthread_cond_broadcast(&queue->not_empty);
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
    
    queue->front = (queue->front + 1) % MAX_JOBS;
    queue->count--;
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
        print_to_log_file(curr_time-start_time, cmd, TID,"START");
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
        
        long long value;
        fscanf(file, "%lld", &value);
        fclose(file);

        file = fopen(filename, "w");
        fprintf(file, "%lld\n", value + 1);
        fclose(file);
        pthread_mutex_unlock(&file_mutexes[number]); // Unlock the mutex for the file

    } else if (strcmp(cmd1, "decrement") == 0) {
        pthread_mutex_lock(&file_mutexes[number]); // Lock the mutex for the file
        FILE* file = fopen(filename, "r");
        long long value;
        fscanf(file, "%lld", &value);
        fclose(file);

        file = fopen(filename, "w");
        fprintf(file, "%lld\n", value - 1);
        fclose(file);
        pthread_mutex_unlock(&file_mutexes[number]); // Unlock the mutex for the file
    }
    if (log_enabled) {
        long long curr_time = get_current_time_in_milliseconds();
        print_to_log_file(curr_time-start_time, cmd, TID, "END");
    }
    pthread_mutex_lock(&active_threades_mutex);
    active_threades -= 1;
    if (active_threades == 0) {
        pthread_cond_broadcast(&active_threades_cond);
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
    char line[MAX_LINE]; // Buffer to hold each line
    char* token;     // Token for parsing
    const char* delimiter = ";"; // Command delimiter

    while (fgets(line, sizeof(line), cmdfile)) {
        line[strcspn(line, "\n")] = '\0';
        char line_cpy[MAX_LINE];
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
            
            pthread_mutex_lock(&active_threades_mutex);
            while (active_threades > 0) {
                pthread_cond_wait(&active_threades_cond, &active_threades_mutex);
            }
            pthread_mutex_unlock(&active_threades_mutex);
            continue;
        }

        // done - need to test more
        else if (strcmp(token, "worker") == 0) {
            char temp_line[MAX_LINE];
            char org_line[MAX_LINE];
            int times = 1;

            strcpy(org_line, strtok(NULL,""));// saving the original line
            strcpy(temp_line, org_line); // using temp line to not ruin the original with tokens
            printf("org line before is %s\n", org_line);

            char* token1 = strtok(temp_line, ";");
            while (token1 != NULL){
                printf("token1 is %s\n", token1);
                if (strstr(token1, "repeat") != NULL) { // if token inculds repeat
                    times = atoi(strtok(token1, "repeat ")); //get the repeat times
                    printf("times is: %d\n", times);
                    for (int i = 0 ; i < times; i++){
                        strcpy(temp_line, org_line);
                        token = strtok(temp_line, ";");
                        while (strstr(token, "repeat") == NULL){
                            token = strtok(NULL, ";");
                            }
                        printf("token last is %s\n", token);
                        while (token != NULL){
                            enqueue(queue, token);
                            token = strtok(NULL, ";");
                            }
                        }
                    break;
                    }
                else{
                    enqueue(queue, token1);
                }
                token1 = strtok(NULL, ";");
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

void print_to_log_file(long long curr_time, char* cmd,int TID, char* end_or_start) {
    char filename[16];
    snprintf(filename, sizeof(filename), "thread%04d.txt", TID);

    FILE* file = fopen(filename, "a");
    if (file == NULL) {
        perror("Error opening log file");
    }
    fprintf(file, "TIME %lld: %s job %s\n",curr_time, end_or_start, cmd);
    fclose(file);
}

void create_stats_file(){
    FILE* file = fopen("stats.txt", "w");
    if (file == NULL) {
        perror("Error opening stats file");
    }
    fprintf(file, "total running time: %lld milliseconds\n",total_running_time);
    fprintf(file, "sum of jobs turnaround time: %lld milliseconds\n", sum_turnaround);
    fprintf(file, "min job turnaround time: %lld milliseconds\n", min_turnaround);
    fprintf(file, "average jobs turnaround time: %f milliseconds\n", avg_turnaround);
    fprintf(file, "max job turnaround time: %lld milliseconds\n", max_turnaround);
    fclose(file);
}
