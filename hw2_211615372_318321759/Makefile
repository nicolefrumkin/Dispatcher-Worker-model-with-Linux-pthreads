SRCS = hw2.c
OBJS = hw2.exe

# for make
all:clean
	gcc -pthread $(SRCS) -o $(OBJS) -lm

# Clean up build files
clean:
	rm -rf *.exe* *.o*

clean_threads:
	rm -rf thread*.txt

clean_counters:
	rm -rf count*.txt
