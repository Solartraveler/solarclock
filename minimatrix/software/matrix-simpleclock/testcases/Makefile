COMPILER=gcc
FLAGS=-Wall -Wextra -DUNIT_TEST -g -std=c99

all:
	$(COMPILER) -c testdcf77decoder.c $(FLAGS)
	$(COMPILER) -c makedata.c $(FLAGS)
	$(COMPILER) -c ../dcf77statisticsdecode.c $(FLAGS)
	$(COMPILER) -c testcharger.c $(FLAGS)
	$(COMPILER) -c ../charger.c $(FLAGS)
	$(COMPILER) -c main.c $(FLAGS)
	$(COMPILER) -o runtestcases *.o -g

clean:
	rm -f *.o
	rm -f runtestcases

removebackups:
	rm -rf *~
