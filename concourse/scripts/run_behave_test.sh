#!/bin/bash

BEHAVE_FLAGS=$@

source /usr/local/greenplum-db-devel/greenplum_path.sh
export PGPORT=5432
export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1
export PGDATABASE=gptest
createdb gptest
gpconfig --skipvalidation -c fsync -v off
gpstop -u
cd /home/gpadmin/gpdb_src/gpMgmt
make -f Makefile.behave behave flags="$BEHAVE_FLAGS"
