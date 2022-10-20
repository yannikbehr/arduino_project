#!/bin/bash

date=$1
if [ -z "$date" ]; then
	echo "Usage: $0 <date> "
	echo "e.g. $0 02/Apr"
	echo "Months: Jan Feb Mar Apr... "
	exit 2
fi

source DyDB/DyDB_object.sh

for partition in WeightKg TempC Volt; do
    #DyDB_query MyBeeDataTable SortKey_$date --partition "$partition" --raw-output | jq --arg partition "$partition" -r '.Items[] | ["partition", .MyValue.S, .mySortKey.S ] | @tsv'
    DyDB_query MyBeeDataTable SortKey_$date --partition "$partition" --raw-output | jq -r --arg partition "$partition" '.Items[] | [$partition, .MyValue.S, .mySortKey.S ] | @tsv'
done


#DyDB_query MyBeeDataTable SortKey_$date --partition TempC --raw-output | jq -r '.Items[] | ["TempC", .MyValue.S, .mySortKey.S ] | @tsv'
#DyDB_query MyBeeDataTable SortKey_$date --partition Volt --raw-output | jq -r '.Items[] | ["Volt", .MyValue.S, .mySortKey.S ] | @tsv'
