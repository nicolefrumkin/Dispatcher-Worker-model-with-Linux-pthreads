#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

void* thread_function(void* arg);
void create_counter_files(num_counters);
void create_threads(int num_threads, int* thread_ids,pthread_t* threads);
void read_lines(FILE* cmdfile, int* thread_ids, pthread_t* threads);

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
    FILE* cmdfile = fopen(argv[1], "r");
    if (cmdfile == NULL) {
        perror("Error opening file");
        return 1;
    }

    create_counter_files(num_counters);

    create_threads(num_threads,thread_ids,threads);

    read_lines(cmdfile, thread_ids, threads);

    return 0;
}

void* thread_function(void* arg) {
    int thread_id = *(int*)arg;
    printf("Hello from thread %d!\n", thread_id);
    return NULL;
}

void create_counter_files(int num_counters) {
    // create counter files - should we put it in a function?
    for (int i = 0; i < num_counters; i++) {
        char filename[20]; 
        snprintf(filename, sizeof(filename), "count%02d.txt", i);
        FILE *file = fopen(filename, "w");
        if (file == NULL) {
            perror("Error creating file");
            return 1;
        }
        fprintf(file, "0");
    }
}

void create_threads(int num_threads, int* thread_ids, pthread_t* threads) { 
    if (threads == NULL) {
        perror("Failed to allocate memory");
        return 1;
    }

    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i + 1; // Assign a unique ID to each thread
        if (pthread_create(&threads[i], NULL, thread_function(i+1), &thread_ids[i]) != 0) {
            perror("Failed to create thread");
            free(threads);
            return 1;
        }
    }
}


void read_lines(FILE* cmdfile, int* thread_ids, pthread_t* threads) {
    char line[1024]; // Buffer to hold each line
    char* token;     // Token for parsing
    const char* delimiter = ";"; // Command delimiter
    char* cmd1;
    int cmd2;
    char* tokens_arr[1024];

    while (fgets(line, sizeof(line), cmdfile)) {
        line[strcspn(line, "\n")] = 0;
        // check if worker or dispatcher
        token = strtok(line, " "); // Split by space
        if (token == NULL) {
            continue; // Skip empty lines
        }

        else if (strcmp(token, "dispatcher_msleep") == 0) {
            int x = atoi(strtok(NULL, " "));
            usleep(x);
            continue;
        }
        else if (strcmp(token, "dispatcher_wait") == 0) {
            for (int i=0; i < thread_ids; i++){
                pthread_join(threads[i],NULL);
            }
            continue;
        }

        else if (strcmp(token, "worker") == 0) {
            int i = 0;
            // Parse the remaining commands, separated by ';'
            char* commands = strtok(NULL, ""); // Get the rest of the line
            if (commands != NULL) {
                token = strtok(commands, delimiter);
                while (token != NULL) {
                    printf("Command: %s\n", token);
                    token = strtok(NULL, delimiter);
                    strcpy(tokens_arr[i],token);
                    i++;
                }
            }
        }

    }
}

