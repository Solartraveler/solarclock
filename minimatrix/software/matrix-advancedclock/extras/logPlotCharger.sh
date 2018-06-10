#!/bin/bash

#call ./logPlotCharger.sh yourlogprefix*.txt

#Input lines are in the format:
#9400000000B17A1F03670AB5020700E4000B0000009314     148 2016-09-25 16:00:00 BATT, 2663mV, bat: 693mAh, 7mA 22.8°C, consumed: 11mAh
#You can simply input all logs without preprocessing, extracting data, remove bad data, remove double entries is done automatically
#You will get a nice looking graph as svg and pdf

set -e

#do not add an ending:
FILENAME=charging

# BATT filters the type of message

#-k2: Second column, -n: Numeric sort

#uniq removes lines if they are in the log files multiple times

#remove out dates where the clock has not been synchronized (year 2000 dates)

#egrep filters out bad lines which are two data sets with a missing end of line

#tr -s ' ' reduces multiple spaces int one

#cut command selects date, time and temperature

#sed removes the mV, mAh, mA, mA prefixes

cat "$@" | grep BATT | sort -k 2 -n | uniq | grep -E '20([1-4])([0-9])-' | egrep '^.{1,145}$' | tr -s ' ' | cut -d " " -f 3,4,6,8,9 | sed 's/mV,//g' | sed 's/mAh,//g' | sed 's/mA,//g' | sed 's/mA//g' > "$FILENAME.txt"

STARTDATE=`cat "$FILENAME.txt" | head -n 1 | cut -d " " -f 1`
ENDDATE=`cat "$FILENAME.txt" | tail -n 1 | cut -d " " -f 1`

#Convert from 2017-12-31 to 31.12.2017
STARTDATE2=`date -d"$STARTDATE" +%d.%m.%Y`
ENDDATE2=`date -d"$ENDDATE" +%d.%m.%Y`

#it can be 25hours if summmer -> winter time switch occurrs
oneday=$(expr 25 \* 60 \* 60)
timestamplast=`date -d "$STARTDATE" +%s`


while read datec timec voltage battery current
do
	timestamp=`date -d $datec +%s`
	timestamplastoneday=$(expr "$timestamplast" + "$oneday")
	if [ "$timestamp" -gt "$timestamplastoneday" ];
	then
		echo "" >> "$FILENAME-temp.txt"
	fi
	echo "$datec $timec $voltage $battery $current" >> "$FILENAME-temp.txt"
	timestamplast=$timestamp
done < "$FILENAME.txt"

mv "$FILENAME-temp.txt" "$FILENAME.txt"


echo '\
set encoding utf8;\
set title "Charger state from '$STARTDATE2' to '$ENDDATE2'";\
set output "'$FILENAME-$ENDDATE.svg'";\
set terminal svg size 1280,768 dynamic name "Charger";\
set style line 1 lt rgb "blue";\
set style line 2 lt rgb "red";\
set style line 3 lt rgb "green";\
set xdata time;\
set timefmt "%Y-%m-%d %H:%M:%S";\
set xtics format "";\
set lmargin 10;\
set rmargin 10;\
set bmargin 0;\
set multiplot layout 2, 1;\
set ylabel "Voltage [mV]";\
set ytics nomirror;\
set y2label "Battery state [mAh]";\
set y2tics 100;\
plot "'./$FILENAME.txt'" using 1:3 with lines ls 1 title "Voltage" axis x1y1, "'./$FILENAME.txt'" using 1:4 with lines ls 2 title "Battery state" axis x1y2;\
unset y2label;\
unset y2tics;\
unset title;\
set tmargin 1;\
unset bmargin;\
set xtics format "%d.%m.%Y";\
set xlabel "Date";\
set ylabel "Charging current [mA]";\
plot "'./$FILENAME.txt'" using 1:5 with lines ls 3 title "Charging current" axis x1y1;\
' | gnuplot

inkscape -z --export-area-drawing --file="$FILENAME-$ENDDATE.svg" --export-pdf="$FILENAME-$ENDDATE.pdf"
