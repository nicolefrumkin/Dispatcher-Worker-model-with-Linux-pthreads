#include <stdio.h>

int main() {
    int number = 5; // Example number
    char filename[20]; // Ensure this is large enough to hold the formatted string

    // Use snprintf to format and copy the string into filename
    snprintf(filename, sizeof(filename), "counter%02d.txt", number);

    printf("Generated filename: %s\n", filename);
    return 0;
}