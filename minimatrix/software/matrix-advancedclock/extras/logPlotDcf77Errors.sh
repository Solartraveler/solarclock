#!/bin/bash

#call ./logPlotDcf77Errors.sh yourlogprefix*.txt

#Input lines are in the format:
#28010000C82CA41F01C32CA41FC300000000000000F40A     296 2016-10-27  3:11:04 SYNC, delta: -5s, error: 195
#You can simply input all logs without preprocessing, extracting data, remove bad data, remove double entries is done automatically
#You will get a nice looking graph as svg and pdf

set -e

#do not add an ending:
FILENAME=dcferrors

# SYNC filters the type of message

#-k2: Second column, -n: Numeric sort

#uniq removes lines if they are in the log files multiple times

#remove out dates where the clock has not been synchronized (year 2000 dates)

#egrep filters out bad lines which are two data sets with a missing end of line

#tr -s ' ' reduces multiple spaces int one

#cut command selects date, time delta and error

#sed removes the s, substring

#awk filters time deta values in the range -600 to 600 seconds

cat "$@" | grep SYNC | sort -k 2 -n | uniq | grep -E '20([1-4])([0-9])' | egrep '^.{1,145}$' | tr -s ' ' | cut -d " " -f 3,4,7,9 | sed 's/s,//g' | awk '$3 < 600' | awk '$3 > -600' > "$FILENAME.txt"

STARTDATE=`cat "$FILENAME.txt" | head -n 1 | cut -d " " -f 1`
ENDDATE=`cat "$FILENAME.txt" | tail -n 1 | cut -d " " -f 1`

#Convert from 2017-12-31 to 31.12.2017
STARTDATE2=`date -d"$STARTDATE" +%d.%m.%Y`
ENDDATE2=`date -d"$ENDDATE" +%d.%m.%Y`

echo '\
set encoding utf8;\
set xlabel "Date";\
set ylabel "Clock delta on sync [s]\n Negative: Clock running too fast, Positive: Clock running too slow";\
set y2label "Signal error\n [Number of times the sampled input signal did not fit to the best hypothesis.\n Sampling 6minutes, 38bits every minute, and every bit 20times]";\
set title "DCF77 errors from '$STARTDATE2' to '$ENDDATE2'";\
set y2tics 100;\
set ytics nomirror;\
set xzeroaxis;\
set key inside;\
set decimalsign ".";\
set output "'$FILENAME-$ENDDATE.svg'";\
set terminal svg size 1280,768 dynamic name "DCF77";\
set style line 1 lt rgb "red" pointtype 7;\
set style line 2 lt rgb "blue" pointtype 7;\
set xdata time;\
set timefmt "%Y-%m-%d %H:%M:%S";\
set xtics format "%d.%m.%Y"
plot "./'$FILENAME.txt'" using 1:3 with linespoints ls 1 pointsize 0.5 title "Clock delta" axis x1y1, "./'$FILENAME.txt'" using 1:4 with points ls 2 pointsize 0.5 title "Signal error" axis x1y2;\
' | gnuplot

inkscape -z --export-area-drawing --file="$FILENAME-$ENDDATE.svg" --export-pdf="$FILENAME-$ENDDATE.pdf"
