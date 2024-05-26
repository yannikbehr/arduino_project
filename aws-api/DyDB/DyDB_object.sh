#!/bin/bash

default_partition_key(){
	echo "myPartitionKey"
}

default_partition(){
	echo "myPartition"
}

default_sort_key(){
	echo "mySortKey"
}


DyDB_create_table(){
	local tableName=$1 ; shift
	if [ -z "$tableName" ]; then 
		echo "USAGE: DyDB_table_create <tableName> [partitionKey] [sortKey]"
		return
	fi
	partitionKey=$1 ; shift
	if [ -z "$partitionKey" ]; then 
		partitionKey=$(default_partition_key)
	fi
	sortKey=$1 ; shift
	if [ -z "$sortKey" ]; then 
		sortKey=$(default_sort_key)
	fi

	attrib_def="AttributeName=$partitionKey,AttributeType=S AttributeName=$sortKey,AttributeType=S"
	key_schema="AttributeName=$partitionKey,KeyType=HASH AttributeName=$sortKey,KeyType=RANGE"

	table_id=$(aws dynamodb create-table --table-name $tableName --attribute-definitions $attrib_def --key-schema $key_schema --provisioned-throughput ReadCapacityUnits=1,WriteCapacityUnits=1 | tee -a $log_file  | jq -r '.TableDescription | .TableId')

	echo "Created table \"$tableName\": \"$table_id\""
}

DyDB_list_tables(){

	aws dynamodb list-tables | jq '.TableNames'
}

DyDB_table_exists(){
	local tableName=$1
	if [ -z "${tableName}" ]; then 
		echo "USAGE: DyDB_table_exists <tableName> value"
		return
	fi
	val=$(DyDB_list_tables | jq 'index("'$tableName'")')
	if [ "$val" != "null" ]; then
		echo true
		return
	fi
	echo false
}

DyDB_query(){
	local usage="USAGE: DyDB_query <tableName> <sortPrefix> [--sortKey <sortKey>]  [--partition-key <partitionKey>] [--partition <partition>] [--raw-output]"
	#default values
	local raw_output="false"
	local partition=$(default_partition)

	local tableName=$1 ; shift
	if [ -z "${tableName}" ]; then 
		echo $usage
		return
	fi
	local value=$1 ; shift
	if [ -z "${value}" ]; then 
		echo $usage
		return
	fi
	partitionKey=$(default_partition_key)
	sortKey=$(default_sort_key)

	while [ ! -z "$1" ]; do
		if [ "$1" == "--raw-output" ]; then
			raw_output="true"
		fi
		if [ "$1" == "--partition-key" ]; then
            shift
			partitionKey=$1
		fi
		if [ "$1" == "--sort-key" ]; then
            shift
			sortKey=$1
		fi
		if [ "$1" == "--partition" ]; then
			shift
			partition="$1"
			if [ -z "$partition" ]; then
				echo "DyDB_query: argument expected for --partition"
				echo $usage
				return
			fi
		fi
		shift
	done


	#echo "aws dynamodb query --table-name $tableName --key-conditions '{ \"$partitionKey\": { \"AttributeValueList\": [ {\"S\": \"$partition\"}], \"ComparisonOperator\": \"EQ\" }, \"$sortKey\": { \"AttributeValueList\": [ {\"S\": \"$value\"}], \"ComparisonOperator\": \"EQ\" } }'"
	aws dynamodb query --table-name $tableName --key-conditions '{ "'$partitionKey'": { "AttributeValueList": [ {"S": "'$partition'"}], "ComparisonOperator": "EQ" }, "'$sortKey'": { "AttributeValueList": [ {"S": "'$value'"}], "ComparisonOperator": "BEGINS_WITH" } }' | 
	if $raw_output ; then 
		cat
	else
		jq --raw-output '.Items | .[] | .MyValue | .S'
	fi
}

DyDB_delete_item(){
	usage="USAGE: DyDB_delete_item <tableName> <sortKeyPrefix> [--partition <partitionKey>] "
	#default values
	partition=$(default_partition)

	tableName=$1 ; shift
	if [ -z "${tableName}" ]; then 
		echo $usage
		return
	fi
	value=$1 ; shift
	if [ -z "${value}" ]; then 
		echo $usage
		return
	fi
	while [ ! -z "$1" ]; do
		if [ "$1" == "--partition" ]; then
			shift
			partition="$1"
			if [ -z "$partition" ]; then
				echo "DyDB_delete_item: argument expected for --partition"
				echo $usage
				return
			fi
		fi
		shift
	done

	partitionKey=$(default_partition_key)
	sortKey=$(default_sort_key)

	aws dynamodb delete-item --table-name $tableName --key '{ "'$partitionKey'": {"S": "'$partition'"}, "'$sortKey'": {"S": "'$value'"}}' 
}

DyDB_put_item(){
	partition=$(default_partition)

	usage="USAGE: DyDB_put_item <tableName> key value [--partition <partitionKey>]"
	tableName=$1 ; shift
	if [ -z "${tableName}" ]; then 
		echo $usage ; return
	fi
	key=$1 ; shift
	if [ -z "${key}" ]; then 
		echo $usage ; return
	fi
	value=$1 ; shift
	if [ -z "${value}" ]; then 
		echo $usage ; return
	fi
	while [ ! -z "$1" ]; do
		if [ "$1" == "--partition" ]; then
			shift
			partition="$1"
			if [ -z "$partition" ]; then
				echo "DyDB_put_item: argument expected for --partition"
				echo $usage
			fi
		fi
		shift
	done

	partitionKey=$(default_partition_key)
	sortKey=$(default_sort_key)

	aws dynamodb put-item --table-name $tableName --item '{"'$partitionKey'": {"S": "'$partition'"}, "'$sortKey'":{"S": "'$key'"}, "MyValue": {"S": "'$value'"}}' --return-consumed-capacity TOTAL  
}


DyDB_delete_table(){
	tableName=$1
	if [ -z "${tableName}" ]; then 
		echo "USAGE: DyDB_delete_table <tableName> "
		return
	fi
	aws dynamodb delete-table --table-name $tableName
}

DyDB_scan_table(){
	tableName=$1
	if [ -z "${tableName}" ]; then 
		echo "USAGE: DyDB_scan_table <tableName> "
		return
	fi
	data=$(aws dynamodb scan --table-name $tableName | jq '.Items')
	if [ -z "$data" ]; then 
		if $(DyDB_table_exists $tableName) ; then 
			echo Table $tableName is empty 1>&2
		else 
			echo Table $tableName does not exist 1>&2
			echo Existing tables are: 1>&2
			DyDB_list_tables
		fi
		return
	fi
	echo $data
}

Test1(){
	testId=Test1
	echo Run $testId 1>&2
	DyDB_create_table $testId
	sleep 10
	DyDB_put_item $testId someKey1 someVal1
	DyDB_put_item $testId someKey2 someVal2
	res1=$(DyDB_query $testId someKey1)
	res2=$(DyDB_query $testId someKey2)
	if [ "$res1" != "someVal1" ]; then 
		echo "$testId failed, wrong value \"$res1\" != \"someVal1\""
		DyDB_delete_table $testId
		return
	fi
	if [ "$res2" != "someVal2" ]; then 
		echo "$testId failed, wrong value \"$res2\" != \"someVal2\""
		DyDB_delete_table $testId
		return
	fi
	DyDB_delete_table $testId
	echo $testId succeded 1>&2
}

Test2(){
	testId=Test2
	echo Run $testId 1>&2
	DyDB_create_table $testId
	sleep 10
	DyDB_put_item $testId someKey1 someVal1
	DyDB_put_item $testId someKey2 someVal2
	DyDB_put_item $testId otherKey1 otherVal1
	# query both someKey1 and someKey1, but not otherKey1
	res1=$(DyDB_query $testId someKey --raw-output | jq -r '.Items[] | .MyValue.S' | sort | xargs)
	if [ "$res1" != "someVal1 someVal2" ]; then 
		echo "$testId failed, wrong value \"$res1\" != \"someVal1 someVal2\""
		DyDB_delete_table $testId
		return
	fi
	DyDB_delete_table $testId
	echo $testId succeded 1>&2
}

Test3(){
	testId=Test3
	echo Run $testId \"partitionKey\" 1>&2
	DyDB_create_table $testId
	sleep 10
	DyDB_put_item $testId someKey1 someVal1 --partition partition1
	DyDB_put_item $testId someKey2 someVal2 --partition partition2
	DyDB_put_item $testId someKey3 someVal3 --partition partition2
	# query both someKey1 and someKey1, but not otherKey1
	res1=$(DyDB_query $testId someKey --raw-output --partition partition2 | jq -r '.Items[] | .MyValue.S' | sort | xargs)
	if [ "$res1" != "someVal2 someVal3" ]; then 
		echo "$testId failed, wrong value \"$res1\" != \"someVal2 someVal3\""
		DyDB_delete_table $testId
		return
	fi
	DyDB_delete_table $testId
	echo $testId succeded 1>&2
}

Test4(){
	testId=Test4
	echo Run $testId \"delete_item\" 1>&2
	DyDB_create_table $testId
	sleep 10
	DyDB_put_item $testId someKey1 someVal1 --partition partition1
	DyDB_put_item $testId someKey2 someVal2 --partition partition1
	# query both someKey1 and someKey1, but not otherKey1
	res=$(DyDB_query $testId someKey1 --partition partition1 )
	if [ "$res" != "someVal1" ]; then 
		echo "$testId failed, wrong value \"$res\" != \"someVal1\""
		DyDB_delete_table $testId
		return
	fi
	DyDB_delete_item $testId someKey1 --partition partition1
	res2=$(DyDB_query $testId someKey1 --partition partition1 )
	if [ "$res2" != "" ]; then 
		echo "$testId failed, wrong value \"$res2\" != \"\""
		DyDB_delete_table $testId
		return
	fi
	DyDB_delete_table $testId
	echo $testId succeded 1>&2
}




DyDB_test(){
	Test1
	Test2
	Test3
}
