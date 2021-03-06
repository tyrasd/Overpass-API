#!/usr/bin/env bash

# Copyright 2008, 2009, 2010, 2011, 2012 Roland Olbricht
#
# This file is part of Overpass_API.
#
# Overpass_API is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# Overpass_API is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with Overpass_API.  If not, see <http://www.gnu.org/licenses/>.

if [[ -z $4  ]]; then
{
  echo "Usage: $0 replicate_dir start_id --meta=(yes|no) --augmented_diffs=(yes|no)"
  exit 0
}; fi

EXEC_DIR="`dirname $0`/"
if [[ ! ${EXEC_DIR:0:1} == "/" ]]; then
{
  EXEC_DIR="`pwd`/$EXEC_DIR"
}; fi

DB_DIR=`$EXEC_DIR/dispatcher --show-dir`
REPLICATE_DIR="$2"
START=$3
META=

if [[ $4 == "--meta=yes" || $4 == "--meta" ]]; then
{
  META="--meta"
}
elif [[ $4 == "--meta=no" ]]; then
{
  META=
}
else
{
  echo "You must specify --meta=yes or --meta=no"
  exit 0
}; fi

if [[ $5 == "--augmented_diffs=yes" || $5 == "--augmented_diffs" ]]; then
{
  PRODUCE_DIFF="yes"
}
elif [[ $5 == "--augmented_diffs=no" ]]; then
{
  PRODUCE_DIFF=
}
else
{
  echo "You must specify --augmented_diffs=yes or --augmented_diffs=no"
  exit 0
}; fi


get_replicate_filename()
{
  printf -v TDIGIT3 %03u $(($1 % 1000))
  ARG=$(($1 / 1000))
  printf -v TDIGIT2 %03u $(($ARG % 1000))
  ARG=$(($ARG / 1000))
  printf -v TDIGIT1 %03u $ARG
  REPLICATE_TRUNK_DIR=$TDIGIT1/$TDIGIT2/
  REPLICATE_FILENAME=$TDIGIT1/$TDIGIT2/$TDIGIT3
};


collect_minute_diffs()
{
  TEMP_DIR=$1
  TARGET=$(($START + 1))

  get_replicate_filename $TARGET

  while [[ ( -s $REPLICATE_DIR/$REPLICATE_FILENAME.state.txt ) && ( $(($START + 60)) -ge $(($TARGET)) ) ]];
  do
  {
    printf -v TARGET_FILE %09u $TARGET
    gunzip <$REPLICATE_DIR/$REPLICATE_FILENAME.osc.gz >$TEMP_DIR/$TARGET_FILE.osc
    TARGET=$(($TARGET + 1))
    get_replicate_filename $TARGET
  };
  done
  TARGET=$(($TARGET - 1))
};


apply_minute_diffs_augmented()
{
  get_replicate_filename $DIFF_COUNT
  mkdir -p $DB_DIR/augmented_diffs/$REPLICATE_TRUNK_DIR
  mkdir -p $DB_DIR/augmented_diffs/id_sorted/$REPLICATE_TRUNK_DIR
  ./update_from_dir --osc-dir=$1 --version=$DATA_VERSION $META --produce-diff >$DB_DIR/augmented_diffs/id_sorted/$REPLICATE_FILENAME.osc
  EXITCODE=$?
  while [[ $EXITCODE -ne 0 ]];
  do
  {
    sleep 60
    ./update_from_dir --osc-dir=$1 --version=$DATA_VERSION $META --produce-diff >$DB_DIR/augmented_diffs/id_sorted/$REPLICATE_FILENAME.osc
    EXITCODE=$?
  };
  done
  ./process_augmented_diffs <$DB_DIR/augmented_diffs/id_sorted/$REPLICATE_FILENAME.osc | gzip >$DB_DIR/augmented_diffs/$REPLICATE_FILENAME.osc.gz
  gzip <$DB_DIR/augmented_diffs/id_sorted/$REPLICATE_FILENAME.osc >$DB_DIR/augmented_diffs/id_sorted/$REPLICATE_FILENAME.osc.gz
  rm $DB_DIR/augmented_diffs/id_sorted/$REPLICATE_FILENAME.osc
  
  echo "osm_base=$DATA_VERSION" >$DB_DIR/augmented_diffs/$REPLICATE_FILENAME.state.txt
  echo $DIFF_COUNT >$DB_DIR/augmented_diffs/state.txt
  DIFF_COUNT=$(($DIFF_COUNT + 1))
};


apply_minute_diffs()
{
  ./update_from_dir --osc-dir=$1 --version=$DATA_VERSION $META
  EXITCODE=$?
  while [[ $EXITCODE -ne 0 ]];
  do
  {
    sleep 60
    ./update_from_dir --osc-dir=$1 --version=$DATA_VERSION $META
    EXITCODE=$?
  };
  done
  DIFF_COUNT=$(($DIFF_COUNT + 1))
};


update_state()
{
  get_replicate_filename $TARGET
  TIMESTAMP_LINE=`grep "^timestamp" <$REPLICATE_DIR/$REPLICATE_FILENAME.state.txt`
  while [[ -z $TIMESTAMP_LINE ]]; do
  {
    sleep 5
    TIMESTAMP_LINE=`grep "^timestamp" <$REPLICATE_DIR/$REPLICATE_FILENAME.state.txt`
  }; done
  DATA_VERSION=${TIMESTAMP_LINE:10}
};


echo >>$DB_DIR/apply_osc_to_db.log

mkdir -p $DB_DIR/augmented_diffs/
DIFF_COUNT=0

# update_state

pushd "$EXEC_DIR"

while [[ true ]]; do
{
  if [[ $START == "auto" ]]; then
  {
    START=`cat $DB_DIR/replicate_id`
  }; fi

  echo "`date '+%F %T'`: updating from $START" >>$DB_DIR/apply_osc_to_db.log

  TEMP_DIR=`mktemp -d /tmp/osm-3s_update_XXXXXX`
  collect_minute_diffs $TEMP_DIR

  if [[ $TARGET -gt $START ]]; then
  {
    echo "`date '+%F %T'`: updating to $TARGET" >>$DB_DIR/apply_osc_to_db.log

    update_state

    if [[ $PRODUCE_DIFF == "yes" ]]; then
    {
      apply_minute_diffs_augmented $TEMP_DIR
    };
    else
    {
      apply_minute_diffs $TEMP_DIR
    }; fi
    echo "$TARGET" >$DB_DIR/replicate_id

    echo "`date '+%F %T'`: update complete" $TARGET >>$DB_DIR/apply_osc_to_db.log
  };
  else
  {
    sleep 5
  }; fi

  rm -f $TEMP_DIR/*
  rmdir $TEMP_DIR

  START=$TARGET
}; done
