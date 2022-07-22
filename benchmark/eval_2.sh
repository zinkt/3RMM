#!/bin/bash

gcc -o tr.out eval_1-2.c ../src/wlmalloc.c -I../src/ -DWL_MALLOC -DNTMR -lpthread
gcc -o pt.out eval_1-2.c -lpthread

i="0"
BASE="10000"
SLOP="10000"
THNUM="20"

while [ $i -lt 15 ]
do
    TIMES=$(($BASE+$i*$SLOP))
    echo -n $TIMES" " >> tr
    echo -n $TIMES" " >> pt
    ./tr.out $TIMES $THNUM >> tr
    ./pt.out $TIMES $THNUM >> pt
    echo "" >> tr
    echo "" >> pt

    i=$(($i+1))
done
python3 static_analy.py 2
rm tr*
rm pt*