#!/bin/bash
# Loopgrind kickoff script

LOOPADDR=""
PIPETOPERL=0
OPTERROR=65


while getopts "pa:" flag
do
    case $flag in
        a)
            LOOPADDR=$OPTARG
        ;;
        p)
            PIPETOPERL=1
        ;;
        \?)
            echo "Usage: `basename $0` <program path>"
            echo "       `basename $0` -p <program path>"
            echo "       `basename $0` -a <loop address> <program path>"
            exit $OPTERROR          # Exit and explain usage, if no argument(s) given.
        ;;
    esac    
done


if [[ "" != ${LOOPADDR} ]]
then
    ./valgrind-3.5.0/vg-in-place  --tool=loopgrind --loop-addr=$LOOPADDR --debug=no --log-fd=1 ${@:$OPTIND}


elif [[ $PIPETOPERL -eq 1 ]]
then
    ./valgrind-3.5.0/vg-in-place  --tool=loopgrind --debug=yes --log-fd=1 ${@:$OPTIND}   | perl tools/analyze.pl ${@:$OPTIND}


else
    ./valgrind-3.5.0/vg-in-place  --tool=loopgrind --debug=yes --log-fd=1 ${@:$OPTIND}  # | perl tools/graph.pl ${@:$OPTIND}
fi  


