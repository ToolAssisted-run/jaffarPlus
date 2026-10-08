#!/bin/sh
# orun.sh NAME DISK "COMMAND LINE": NAME.script in the headless DOS oracle on DISK; events in NAME-DISK-snap.txt
O=$HOME/pop2dec/oracle; n=$1; d=$2; W=$O/w_$n-$d; mkdir -p $W
nice $HOME/pop2dec/oracle-run-c9ed529 --workdir $W --rom $O/$d.hdd --autoexec "c:" --autoexec "cd prince2" --autoexec "$3" --script $O/$n.script --events $W/snap.txt > $W/log.txt 2>&1
mv $W/snap.txt $O/$n-$d-snap.txt; echo "$n $d done: $(wc -l < $O/$n-$d-snap.txt) events" >> $O/orun.log
