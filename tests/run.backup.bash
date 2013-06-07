#!/bin/bash
for (( n=1; 1; n=n+1 )) ; do
    echo $n
    mkdir backup.$n
    if [ $? != 0 ] ; then break; fi
    chmod 0777 backup.$n
    if [ $? != 0 ] ; then break; fi
    let tstart=$(date +%s)
    mysql -S/tmp/rfp.sock -uroot -e"backup to '$PWD/backup.$n'"
    if [ $? != 0 ] ; then break; fi
    let t=$(date +%s)
    let d=t-tstart
    echo $(date -d @$t) $d backup.$n
    sleep 30
done
