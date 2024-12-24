# Dispatcher

Dispatcher is a multithreaded C program for Linux that uses the dispatcher/worker model. It reads commands from an input file, processes dispatcher tasks, and offloads jobs to worker threads for parallel execution.

## Features
- Executes dispatcher tasks like `msleep` and `wait` serially.
- Offloads jobs to multiple worker threads using a shared work queue.
- Manages counters as files and updates them in real-time.
- Generates logs for both dispatcher and worker threads.
- Outputs runtime statistics, including job turnaround times.

## How to Use
1. Compile the program using the provided `Makefile`:
   ```bash
   make
   ```
2. Run the program with the following format:
   ```bash
   ./hw2 cmdfile.txt num_threads num_counters log_enabled
   ```
   - `cmdfile.txt`: Input file with commands.
   - `num_threads`: Number of worker threads (max 4096).
   - `num_counters`: Number of counter files (max 100).
   - `log_enabled`: `1` to enable logging, `0` to disable it.

3. View logs and statistics after execution.

## Clean Up
To remove compiled files, run:
```bash
make clean
```
