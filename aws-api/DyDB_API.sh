#!/bin/bash

log_file=.deploy/api_service.log
tableName=APIMetaData

API_create() {
	usage="API_create <region>"
	local region=$1
	if [ -z $region ]; then 
		echo "API_create: Missing argument: region"
		echo $usage
		return 
	fi
	source DyDB/DyDB_object.sh
	if $(DyDB_table_exists $tableName) ; then 
		echo "API_create: table \"$tableName\" already exists"
		return
	fi
	DyDB_create_table $tableName 

	api_name=MyDyDBRestAPI
	local api_id=$(aws apigateway create-rest-api --name $api_name --region $region | tee -a $log_file | jq -r '.id')
	local root_id=$(aws apigateway get-resources --rest-api-id $api_id --region $region | tee -a $log_file | jq -r '.items[0] | .id')

	DyDB_put_item $tableName apiName "$api_name"
	DyDB_put_item $tableName apiId "$api_id"
	DyDB_put_item $tableName rootId "$root_id"
	DyDB_put_item $tableName region "$region"
}

API_remove(){
	source DyDB/DyDB_object.sh
	if $(DyDB_table_exists $tableName) ; then 
		api_id=$(DyDB_query $tableName apiId)
		retry=1
		ret=1
		while [ "$retry" -lt 5 -a "$ret" -gt 0 ]; do
			aws apigateway delete-rest-api --rest-api-id $api_id
			ret=$?
			retry=$((retry + 1))
			if [ "$ret" -gt 0 ]; then 
				sleep 10
			fi
		done
		DyDB_delete_table $tableName
	else
		echo "API_remove: table $tableName does not exist"
		return
	fi
}

API_create_endpoint(){
	source DyDB/DyDB_object.sh
	usage="API_create_endpoint <parent_id> <endpoint_name> [--generic] [--recreate]"
	local parent=$1; shift
	local endpoint=$1; shift
	local generic="false"
	local recreate="false"
	local lambdaUri=""
	local template_arg=""
	#local request_params=""
	while [ ! -z "$1" ]; do
		if [ "$1" == "--generic" ]; then 
			generic="true"
			#request_params="--request-parameters method.request.path.$endpoint=true"
		elif [ "$1" == "--recreate" ]; then 
			recreate="true"
		elif [ "$1" == "--lambda-uri" ]; then 
			shift
			lambdaUri=$1
		elif [ "$1" == "--request-template-file" ]; then 
			shift
			template_arg="--request-templates file://$1"
		fi
		shift
	done
	for arg in parent endpoint ; do 
		if [ -z "${!arg}" ]; then 
			echo "API_create_endpoint: argument $arg not defined"
			echo $usage
			return 
		fi
	done

	api_id=$(DyDB_query $tableName apiId)
	region=$(DyDB_query $tableName region)

	local path_part=$endpoint
	if $generic ; then 
		path_part="{"$endpoint"}"
	fi

	local endpoint_id=$(aws apigateway create-resource 	--rest-api-id $api_id --region $region --parent-id $parent --path-part $path_part | tee -a $log_file | jq -r '.id')

	if [ -z "$endpoint_id" ]; then 
		echo "API_create_endpoint: creation of enpoint failed" 1>&2
		return
	else
		DyDB_put_item $tableName "${endpoint}_id" "$endpoint_id"
	fi

	aws apigateway put-method --rest-api-id $api_id --resource-id ${endpoint_id} --http-method GET --authorization-type NONE $request_params | tee -a $log_file
	aws apigateway put-integration --rest-api-id $api_id --resource-id ${endpoint_id} --http-method GET --type AWS --integration-http-method POST --uri $lambdaUri $template_arg | tee -a $log_file
#		--request-parameters '{"integration.request.path.op":"method.request.path.'$endpoint'"}' \
	#https://docs.aws.amazon.com/apigateway/latest/developerguide/api-gateway-mapping-template-reference.html#context-variable-reference

	aws apigateway put-method-response --rest-api-id $api_id --resource-id ${endpoint_id} --http-method GET --status-code 200 --response-models application/json=Empty | tee -a $log_file
	aws apigateway put-integration-response --rest-api-id $api_id --resource-id ${endpoint_id} --http-method GET --status-code 200 --response-templates application/json="" | tee -a $log_file
}

API_delete_endpoint(){
	source DyDB/DyDB_object.sh
	local usage="API_delete_endpoint <endpoint path>"
	local endpoint=$1; shift
	if [ -z "$endpoint" ]; then 
		echo $usage
	fi
	resource_id=$(DyDB_query "$tableName" "${endpoint}_id")
	if [ -z "$resource_id" ]; then 
		echo "Could not find key \"${endpoint}_id\" in table \"$tableName\""
		return 
	fi
	aws apigateway delete-resource --resource-id $resource_id --rest-api-id ${api_id}
}

API_add_lambda_permission(){
	source DyDB/DyDB_object.sh
	local arn=""
	local functionName=""
	while [ ! -z "$1" ]; do
		if [ "$1" == "--arn" ]; then 
			shift
			arn=$1
		elif [ "$1" == "--lambda-function-name" ]; then 
			shift
			functionName=$1
		fi
		shift
	done
	for arg in arn functionName ; do 
		if [ -z "${!arg}" ]; then 
			echo "API_add_lambda_permission: argument $arg not defined"
			return
		fi
	done

	aws lambda add-permission --function-name $functionName --statement-id apigateway-test-${RANDOM} --action lambda:InvokeFunction --principal apigateway.amazonaws.com --source-arn "$arn" | tee -a $log_file
}

API_get_endpoint_id(){
	source DyDB/DyDB_object.sh
	usage="API_create_endpoint <region> <parent_id> <endpoint_name>"
	local endpoint=$1; shift
	if [ -z "$endpoint" ]; then 
		echo $usage 1>&2
		return
	fi

	resource_id=$(DyDB_query "$tableName" "${endpoint}_id")
	if [ -z "$resource_id" ]; then 
		echo "Could not find key \"${endpoint}_id\" in table \"$tableName\""
		return 
	fi
	echo $resource_id
}

API_get_root_id(){
	source DyDB/DyDB_object.sh
	root_id=$(DyDB_query "$tableName" rootId)
	echo "$root_id"
}

API_get_id(){
	source DyDB/DyDB_object.sh
	api_id=$(DyDB_query "$tableName" apiId)
	echo "$api_id"
}
