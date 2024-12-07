#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char* cmd1;
    int cmd2;      
    char commands[1024];
    char* token;

    printf("Enter commands: ");
    if (fgets(commands, sizeof(commands), stdin)) {
        // Remove the newline character from the input
        commands[strcspn(commands, "\n")] = '\0';

        // Split the commands by `;`
        token = strtok(commands, ";");
        while (token != NULL) {
            printf("Command: %s\n", token);

            // Parse the individual command and its arguments
            cmd1 = strtok(token, " "); // First part of the command
            char* arg = strtok(NULL, " "); // Second part (argument, if any)

            if (cmd1 != NULL) {
                printf("cmd1: %s\n", cmd1);
            }
            if (arg != NULL) {
                cmd2 = atoi(arg); // Convert argument to integer
                printf("cmd2: %d\n", cmd2);
            }

            // Move to the next command
            token = strtok("NULL", ";");
        }
    }

    return 0;
}
