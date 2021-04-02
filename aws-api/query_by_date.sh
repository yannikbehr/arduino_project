#!/bin/bash

date=$1
if [ -z "$date" ]; then
	echo "Usage: $0 <date> "
	echo "e.g. $0 02/Apr"
	echo "Months: Jan Feb Mar Apr... "
	exit 2
fi

source DyDB/DyDB_object.sh

DyDB_query MyBeeDataTable WeightSort_$date --partition WeightKg --raw-output | jq -r '.Items[] | [.MyValue.S, .mySortKey.S ] | @tsv'
