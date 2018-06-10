#!/bin/bash

#call ./logPlotTemperature.sh yourlogprefix*.txt

#Input lines are in the format:
#9400000000B17A1F03670AB5020700E4000B0000009314     148 2016-09-25 16:00:00 BATT, 2663mV, bat: 693mAh, 7mA 22.8°C, consumed: 11mAh
#You can simply input all logs without preprocessing, extracting data, remove bad data, remove double entries is done automatically
#You will get a nice looking graph as svg and pdf

set -e

#do not add an ending:
FILENAME=consumption

# BATT filters the type of message

#-k2: Second column, -n: Numeric sort

#uniq removes lines if they are in the log files multiple times

#remove out dates where the clock has not been synchronized (year 2000 dates)

#egrep filters out bad lines which are two data sets with a missing end of line

#tr -s ' ' reduces multiple spaces int one

#cut command selects date, time and consumed mAh

#rev, reverts string, remove first three chars

#cut: remove time

cat "$@" | grep BATT | sort -k 2 -n | uniq | grep -E '20([1-4])([0-9])-' | egrep '^.{1,145}$' | tr -s ' ' | cut -d " " -f 3,4,12 | rev | cut -c 4- | rev | grep " 0:00:00" | cut -d " " -f 1,3 > "$FILENAME.txt"

# Now replace sum of mAh by delta to the next day.

datelast="2000-01-01"
timestamplast=`date -d $datelast +%s`

#it can be 25hours if summmer -> winter time switch occurrs
oneday=$(expr 25 \* 60 \* 60)
miliamphourslast=0
STARTDATE=""

rm -f "$FILENAME-temp.txt"

while read datec miliamphours
do
#	echo ">>$datec<< >>$miliamphours<<"
	timestamp=`date -d $datec +%s`
	timestamplastoneday=$(expr $timestamplast + $oneday)
	if [ "$timestamplastoneday" -ge "$timestamp" ];
	then
		if [ "$miliamphours" -ge "$miliamphourslast" ];
		then
			milidelta=$(expr $miliamphours - $miliamphourslast)
			ENDDATE=$datec
			echo "$datelast $milidelta" >> "$FILENAME-temp.txt"
			if [ "$STARTDATE" == "" ];
			then
				STARTDATE=$datelast
			fi
		else
			echo "" >> "$FILENAME-temp.txt"
		fi
	else
		echo "" >> "$FILENAME-temp.txt"
	fi
	timestamplast=$timestamp
	datelast=$datec
	miliamphourslast=$miliamphours
done < "$FILENAME.txt"

mv "$FILENAME-temp.txt" "$FILENAME.txt"

#Convert from 2017-12-31 to 31.12.2017
STARTDATE2=`date -d"$STARTDATE" +%d.%m.%Y`
ENDDATE2=`date -d"$ENDDATE" +%d.%m.%Y`

echo '\
set encoding utf8;\
set xlabel "Date";\
set ylabel "Consumption [mAh/day]";\
set title "Estimated consumption from '$STARTDATE2' to '$ENDDATE2'";\
set key off;\
set decimalsign ".";\
set output "'$FILENAME-$ENDDATE.svg'";\
set terminal svg size 1280,768 dynamic name "Consumption";\
set style line 1 lt rgb "blue";\
set xdata time;\
set timefmt "%Y-%m-%d";\
set xtics format "%d.%m.%Y"
plot "'./$FILENAME.txt'" using 1:2 with lines ls 1 title "[mAh]" axis x1y1;\
' | gnuplot

inkscape -z --export-area-drawing --file="$FILENAME-$ENDDATE.svg" --export-pdf="$FILENAME-$ENDDATE.pdf"
