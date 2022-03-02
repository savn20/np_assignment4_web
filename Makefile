CC_FLAGS= -Wall -I.
LD_FLAGS= -Wall -L./ 

all: libcalc serverthread serverfork

serverthread.o: serverthread.cpp
	$(CXX) $(CC_FLAGS) $(LD_FLAGS) -c serverthread.cpp

serverfork.o: serverfork.cpp
	$(CXX) $(CC_FLAGS) $(LD_FLAGS) -c serverfork.cpp


serverfork: serverfork.o 
	$(CXX) $(LD_FLAGS) -o serverfork serverfork.o -lcalc

serverthread: serverthread.o 
	$(CXX) $(LD_FLAGS) -o serverthread serverthread.o -lpthread -lcalc

calcLib.o: calcLib.c calcLib.h
	gcc -Wall -fPIC -c calcLib.c

libcalc: calcLib.o
	ar -rc libcalc.a -o calcLib.o

clean:
	rm *.o *.a perf_*.txt  tmp.* serverfork serverthread
