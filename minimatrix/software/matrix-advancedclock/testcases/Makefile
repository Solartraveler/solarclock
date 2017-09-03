COMPILER=gcc
FLAGS=-Wall -Wextra -DUNIT_TEST -DADVANCEDCLOCK -g -std=c99

all:
	$(COMPILER) -c testfinecalib.c $(FLAGS)
	$(COMPILER) -c ../finecalib.c $(FLAGS)
	$(COMPILER) -c testrfm12.c $(FLAGS)
	$(COMPILER) -c ../rfm12.c $(FLAGS)
	$(COMPILER) -c testtimeconvert.c $(FLAGS)
	$(COMPILER) -c ../timeconvert.c $(FLAGS)
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
