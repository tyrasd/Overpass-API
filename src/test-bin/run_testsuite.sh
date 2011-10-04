#!/bin/bash

if [[ -z $1  ]]; then
{
  echo "Usage: $0 test_size"
  echo
  echo "An appropriate value for a fast test is 40, a comprehensive value is 2000."
  exit 0
};
fi

# The size of the test pattern. Asymptotically, the test pattern consists of
# size^2 elements. The size must be divisible by ten. For a full featured test,
# set the value to 2000.
DATA_SIZE="$1"
BASEDIR="$(cd `dirname $0` && pwd)/.."
NOTIMES="$2"

$BASEDIR/test-bin/run_testsuite_template_db.sh $DATA_SIZE $2

$BASEDIR/test-bin/run_testsuite_osm_backend.sh $DATA_SIZE $2

$BASEDIR/test-bin/run_unittests_statements.sh $DATA_SIZE $2

$BASEDIR/test-bin/run_testsuite_osm3s_query.sh $DATA_SIZE $2

$BASEDIR/test-bin/run_testsuite_diff_updater.sh $DATA_SIZE $2

$BASEDIR/test-bin/run_unittests_areas.sh $DATA_SIZE $2

if [[ $DATA_SIZE -gt 800 ]]; then
{
  DATA_SIZE=800
}; fi

$BASEDIR/test-bin/run_unittests_meta.sh $DATA_SIZE $2
