CC = gcc -pthread -std=gnu99 -ggdb

DEPS = threadpool/threadpool.h threadpool/threads.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $<

all: serverthread serverfork

S_FORK = serverfork.o
serverfork: $(S_FORK)
	$(CXX) -o $@ $^

S_THREAD = serverthread.o threadpool/threadpool.o
serverthread: $(S_THREAD)
	$(CC) -o $@ $^

clean:
	rm *.o *.a perf_*.txt  tmp.* serverfork serverthread
