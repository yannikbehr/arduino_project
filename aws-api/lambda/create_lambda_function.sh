#!/bin/bash 
#set -ex 
lambda_log_file=logs/create_lambda_func.log
mkdir -p .deploy
zipName=.deploy/lambdaFunctionOverHttps.zip
roleName=lambda-ex
functionName=MyPostToDynampDb
fn_lambda_id_store=.deploy/lambda_id_store.sh

lambda_remove(){ 
	aws iam detach-role-policy --role-name ${roleName} --policy-arn arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole || echo "could not detach policy"
	aws iam detach-role-policy --role-name ${roleName} --policy-arn arn:aws:iam::aws:policy/AmazonDynamoDBFullAccess || echo "could not detach policy"
	aws iam delete-role --role-name ${roleName} || echo "could not delete role"
	aws lambda delete-function --function-name ${functionName} || echo "could not delete function"
	rm -f $zipName
	rm -f $fn_lambda_id_store
}


lambda_create(){
	local region=$1
	if [ -z "$region" ]; then 
		echo "Usage: lambda_create <region>"
		return 
	fi
	mkdir -p $(dirname $zipName)
	#pip install requests -t $(dirname $zipName)
	zip $zipName -j lambda/lambdaFunctionOverHttps.py 1>&2 # /dev/null

	lambdaRoleArn=$(aws iam create-role --role-name ${roleName} --assume-role-policy-document '{"Version": "2012-10-17","Statement": [{ "Effect": "Allow", "Principal": {"Service": "lambda.amazonaws.com"}, "Action": "sts:AssumeRole"}]}' | tee -a $lambda_log_file | jq -r '.Role | .Arn')
		

	sleep 20 # there is often an error when aws does not have a few seconds before attaching policies to a new role
	aws iam attach-role-policy --role-name ${roleName} --policy-arn arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole
	aws iam attach-role-policy --role-name ${roleName} --policy-arn arn:aws:iam::aws:policy/AmazonDynamoDBFullAccess
	#arn:aws:iam::000000000000:role/service-role/MyRoleName


	# sometimes it takes some time until this works, when the role has been just created. 
	retry=1
	lambdaFunctionArn=""
	while [ -z "$lambdaFunctionArn" -a "$retry" -lt 10 ]; do 
		# handler is the name of the function that should be called
		#aws lambda delete-function --function-name $functionName 
		#sleep 10
		lambdaFunctionArn=$(aws lambda create-function --function-name $functionName --zip-file fileb://$zipName --handler lambdaFunctionOverHttps.handler --runtime python3.8 --role $lambdaRoleArn | tee -a $lambda_log_file | jq -r '.FunctionArn') 
		#aws lambda update-function-code --function-name $functionName --zip-file fileb://$zipName
		retry=$((retry + 1))
		if [ -z "$lambdaFunctionArn" ]; then 
			sleep 10
		fi
	done
	echo "lambdaRoleArn=$lambdaRoleArn"                                                                                         >  $fn_lambda_id_store
	echo "lambdaFunctionArn=$lambdaFunctionArn"                                                                                 >> $fn_lambda_id_store
	echo "lambdaFunctionUri=\"arn:aws:apigateway:${region}:lambda:path/2015-03-31/functions/${lambdaFunctionArn}/invocations\"" >> $fn_lambda_id_store
}

lambda_get_arn(){
	local region=$1
	if [ ! -f "$fn_lambda_id_store" ]; then 
		lambda_create $region
	fi
	source $fn_lambda_id_store
	echo "$lambdaFunctionArn"
}

lambda_get_uri(){
	local region=$1
	if [ ! -f "$fn_lambda_id_store" ]; then 
		lambda_create $region
	fi
	source $fn_lambda_id_store
	echo "$lambdaFunctionUri"
}

lambda_test(){
	#test:
	#echo "aws lambda invoke --function-name $functionName out --log-type Tail --payload fileb://test_lambda/input.txt"
	#aws lambda invoke --function-name $functionName out --log-type Tail --payload '{"operation" : "create", "payload" : {"Item": {"StringValue": "myItem", "TimeStamp": 1232, "myValue" : "17.432" }}, "tableName": "MySimpleEventsTest"}'
	local partitionKey=$(default_partition_key)
	local sortKey=$(default_sort_key)
	#aws lambda invoke --function-name $functionName out --log-type Tail --payload '{"operation" : "create", "payload" :{"Item": { "myPartitionKey": "TestPartitionKey", "mySortKey":"TestSortKey", "MyValue": "1.741"}},"tableName": "MyBeeDataTable"}'
	aws lambda invoke --function-name $functionName out --log-type Tail --payload fileb://lambda/test.json
}
