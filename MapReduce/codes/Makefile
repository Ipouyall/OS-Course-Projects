CC=g++
STD=-std=c++14
CF=$(STD)

all: GenreCounter.out aggregator.out worker.out


GenreCounter.out: master.cpp
	$(CC) $(CF) -o GenreCounter.out master.cpp

worker.out: worker.cpp
	$(CC) $(CF) -o worker.out worker.cpp

aggregator.out: aggregator.cpp
	$(CC) $(CF) -o aggregator.out aggregator.cpp

.PHONY: clean
clean:
	rm *.out

