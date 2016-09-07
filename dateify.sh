#!/bin/sh
for i in net*.txt
do
    bn=`echo $i | sed 's=\.txt=='`
    bt=`gawk 'END {
        print int(1003878871 - ( ( NR - 2 ) * 60 ))}' $i`
    sarprep -t 60 -s $bt -n $i 
    gawk 'NR == 1 { print; flag = 0 } /22:30/ { flag = 1 }
    flag == 1 { print }' netout.txt > ${bn}_netout.txt
    rm -f netout.txt
done
for i in vm*.txt
do
    bn=`echo $i | sed 's=\.txt=='`
    bt=`gawk 'END {
        cyc = int(NR/21)
        res = NR - cyc*21
        print int(1003878871 - ( cyc * 19 + res -2 ) * 60 )}' $i`
    sarprep -t 60  -s $bt -v $i 
    for j in vmout vmscr vmrunq
    do
    gawk 'NR == 1 { print; flag = 0 } /22:30/ { flag = 1 }
    flag == 1 { print }' $j.txt > ${bn}_$j.txt
    rm -f $j.txt
    done
done
