#!/bin/bash
# run the sql-bench test-create to try to expose hot backup locking problems.
# is has in the past.
pushd /usr/local/mysql/sql-bench
if [ $? != 0 ] ; then exit 1; fi
while true ; do
    perl ./test-create --create-options=engine=tokudb --socket=/tmp/mysql.sock --verbose
    if [ $? != 0 ] ; then exit 1; fi
done


