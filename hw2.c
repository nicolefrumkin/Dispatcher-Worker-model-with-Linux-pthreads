#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct JobQueue { // thread safe queue
    char* jobs[1024];
    int front; // position of the first element
    int rear; // position where the next element will be added
    int count; // total number of items currently in the queue
    pthread_mutex_t lock; // binary semaphore
    pthread_cond_t not_empty; // command variables
    pthread_cond_t not_full;
    bool shutdown; // New flag to signal shutdown
} JobQueue;


void* worker_thread(void* arg);
void create_counter_files(int num_counters);
void create_threads(int num_threads, int* thread_ids,pthread_t* threads, JobQueue* queue);
void read_lines(FILE* cmdfile, int* thread_ids, pthread_t* threads, JobQueue* queue, int num_threads);
void init_queue(JobQueue* queue);
void enqueue(JobQueue* queue, const char* job);
char* dequeue(JobQueue* queue);
void execute_command(char* cmd);

int main(int argc, char *argv[]) {
    time_t start_time = time(NULL); // save start time of the program
    
    if (argc != 5) { // should it be 4 or 5?
        perror("Error! Number of arguments isn't 5");
    }

    // convert str to int
    int num_counters = atoi(argv[3]); 
    int num_threads = atoi(argv[2]);
    bool log_enabled = atoi(argv[4]);
    long long int counter_arr[100] = {0};
    memset(counter_arr,0,sizeof(counter_arr));
    int thread_ids[4096] = {0}; //its required to create num_threads new threads
    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    JobQueue* queue = (JobQueue*)malloc(sizeof(JobQueue)); //make sure its one queue

    FILE* cmdfile = fopen(argv[1], "r");
    if (cmdfile == NULL) {
        perror("Error opening file");
        return 1;
    }

    init_queue(queue);

    create_counter_files(num_counters);

    create_threads(num_threads,thread_ids,threads, queue);

    read_lines(cmdfile, thread_ids, threads, queue, num_threads);
    fclose(cmdfile); 

    // wait for background  to empty
    pthread_mutex_lock(&queue->lock);
    queue->shutdown = true;
    pthread_cond_broadcast(&queue->not_empty); // Wake all threads
    pthread_mutex_unlock(&queue->lock);

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("All workers have terminated.\n");

    free(threads);
    free(queue);

    return 0;
}

void* worker_thread(void* arg) {
    JobQueue* queue = (JobQueue*)arg;
    while (1) {
        pthread_mutex_lock(&queue->lock);
        if (queue->shutdown && queue->count == 0) {
            pthread_mutex_unlock(&queue->lock);
            break; // Exit the loop and thread
        }
        pthread_mutex_unlock(&queue->lock);

        char* job = dequeue(queue);

        if (job == NULL) {
            continue;
        }
        // Parse and execute commands in the job
        //printf("job: %s\n", job);
        char* token = strtok(job, ";");
        while (token != NULL) {
            printf("token: %s\n", token);
            execute_command(token);
            token = strtok(NULL, ";");
        }
        token ='\0';
        free(job);
    }
    return NULL;
}


void init_queue(JobQueue* queue) {
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    queue->shutdown = false;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
}

void enqueue(JobQueue* queue, const char* job) {
    pthread_mutex_lock(&queue->lock); // lock the mutex - ensures only one thread can modify the queue at a time
    while (queue->count == 1024) { // if queue full - wait
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }
    queue->jobs[queue->rear] = strdup(job); // adds the job
    queue->rear = (queue->rear + 1) % 1024;
    queue->count = queue->count + 1;
    pthread_cond_broadcast(&queue->not_empty); // ells all threads that the queue isnt empty
    pthread_mutex_unlock(&queue->lock); // unlocks mutex
}

//our problen is that queue count is always 0!!! 
char* dequeue(JobQueue* queue) {
    //printf("inside dequeue");
    pthread_mutex_lock(&queue->lock);
    while (queue->count == 0) { // if queue empty - wait
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }
    char* job = queue->jobs[queue->front]; // get job
    queue->front = (queue->front + 1)%1024;
    queue->count = queue->count - 1;
    pthread_cond_broadcast(&queue->not_full); // tells all threads that there is space in the queue for new jobs
    pthread_mutex_unlock(&queue->lock); 
    return job;
}

void execute_command(char* cmd) {

    char* cmd1 = strtok(cmd, " ");
    char* cmd2 = strtok(NULL, "");
    char filename[14];
    int number = atoi(cmd2); // Convert cmd2 to an integer
    snprintf(filename, sizeof(filename), "count%02d.txt", number);
    printf("cmd1: %s, cmd2: %s\n",cmd1,cmd2);

    if (strcmp(cmd1, "msleep") == 0) {
        int x = atoi(cmd2);
        usleep(x * 1000); // Sleep in microseconds
    } else if (strcmp(cmd1, "increment") == 0) {
        FILE* file = fopen(filename, "r"); // opens file for reading and writing    
        int value;
        fscanf(file, "%d", &value); // read value in file
        fclose(file);
        file = fopen(filename, "w");
        fprintf(file, "%d\n", value + 1); // write the new value
        fclose(file);
        
    } else if (strcmp(cmd1, "decrement") == 0) {
        FILE* file = fopen(filename, "r"); // opens file for reading and writing    
        int value;
        fscanf(file, "%d", &value); // read value in file
        fclose(file);
        file = fopen(filename, "w");
        fprintf(file, "%d\n", value - 1); // write the new value
        fclose(file);
    }
    cmd1 ='\0';
    cmd2 ='\0';
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
        fprintf(file, "0\n");
        fclose(file);
    }
}

void create_threads(int num_threads, int* thread_ids, pthread_t* threads, JobQueue* queue) { 
    if (threads == NULL) {
        perror("Failed to allocate memory");
    }

    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i + 1; // Assign a unique ID to each thread
        if (pthread_create(&threads[i], NULL, worker_thread, queue) != 0) {
            perror("Failed to create thread");
            free(threads);
        }
    }
}


void read_lines(FILE* cmdfile, int* thread_ids, pthread_t* threads, JobQueue* queue, int num_threads) {
    char line[1024]; // Buffer to hold each line
    char* token;     // Token for parsing
    const char* delimiter = ";"; // Command delimiter

    while (fgets(line, sizeof(line), cmdfile)) {
        line[strcspn(line, "\n")] = '\0';
        // check if worker or dispatcher
        token = strtok(line, " "); // Split by space
        if (token == NULL) {
            continue; // Skip empty lines
        }

        else if (strcmp(token, "dispatcher_msleep") == 0) {
            int x = atoi(strtok(NULL, "\n"));
            // time_t start_time = time(NULL);
            // char* ts = ctime(&start_time);
            // printf("time is: %s\n", ts);
            usleep(x*1000);
            // time_t end_time = time(NULL);
            // char* te = ctime(&end_time);
            // printf("time is: %s\n", te);
            continue;
        }
        else if (strcmp(token, "dispatcher_wait") == 0) {
            for (int i=0; i < num_threads; i++){
                pthread_join(threads[i],NULL);
            }
            continue;
        }

        else if (strcmp(token, "worker") == 0) {
                enqueue(queue, strtok(NULL, ""));
            }
        }

}


