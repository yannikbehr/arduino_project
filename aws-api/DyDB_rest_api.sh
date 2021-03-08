#!/bin/bash
set -ex 

#https://docs.aws.amazon.com/lambda/latest/dg/services-apigateway-tutorial.html

option=$1

region="eu-west-3"
log_file=logs/api-DyDb-setup.log
account=137637932857
stage=test1
dataTableName=MyBeeDataTable

rm -f $log_file
source DyDB_API.sh 
source lambda/create_lambda_function.sh
source DyDB/DyDB_object.sh
if [ "${option}" == cleanup ]; then
	API_delete_endpoint WeightKg
	API_delete_endpoint DyDB
	API_remove
	lambda_remove
	exit 0
fi

if ! $(DyDB_table_exists $dataTableName) ; then 
	DyDB_create_table $dataTableName
fi

API_create $region
api_id=$(API_get_id)
root_id=$(API_get_root_id)
lambdaUri=$(lambda_get_uri $region)

################################################################################
# working with endpoint /DyDB
################################################################################
API_create_endpoint $root_id DyDB --lambda-uri $lambdaUri 

################################################################################
# working with endpoint /DyDB/WeightKg
################################################################################
DyDB_id=$(API_get_endpoint_id DyDB)
API_create_endpoint $DyDB_id WeightKg --lambda-uri $lambdaUri

################################################################################
# working with endpoint /DyDB/WeightKg/{Weight}
################################################################################
WeightKg_id=$(API_get_endpoint_id WeightKg)
API_create_endpoint $WeightKg_id Weight --generic --lambda-uri $lambdaUri --request-template-file put_integration/Weight.json
API_add_lambda_permission --lambda-function-name MyPostToDynampDb --arn "arn:aws:execute-api:$region:$account:$api_id/*/GET/DyDB/WeightKg/*"
API_add_lambda_permission --lambda-function-name MyPostToDynampDb --arn "arn:aws:execute-api:$region:$account:$api_id/$stage/GET/DyDB/WeightKg/*"


################################################################################
# deployment
################################################################################
aws apigateway create-deployment --rest-api-id $api_id --region $region --stage-name $stage --stage-description 'Test stage' --description 'First deployment' | tee -a $log_file

#curl -X POST -v https://${api_id}.execute-api.${region}.amazonaws.com/${stage}/DyDB/create -d '{"Item": {"StringValue": "myItem", "TimeStamp": 1232, "myValue" : "17.432" }}, "tableName": "MySimpleEventsTest"}'
#curl -X POST -d "{\"operation\":\"create\",\"tableName\":\"MySimpleEventsTest\",\"payload\":{\"Item\": {\"StringValue\": \"viaAPI\", \"TimeStamp\": 1521, \"myValue\" : \"17.432\" }}}" https://$api_id.execute-api.$region.amazonaws.com/${stage}/DyDB
#aws apigateway test-invoke-method --rest-api-id $api_id --resource-id ${DyDB_id} --http-method POST --path-with-query-string "" --body file://test_lambda/create.json
echo curl -X GET -v https://${api_id}.execute-api.${region}.amazonaws.com/${stage}/DyDB/WeightKg/121.74

Weight_id=$(API_get_endpoint_id Weight)
echo "aws apigateway test-invoke-method --rest-api-id $api_id --resource-id ${Weight_id} --http-method GET --path-with-query-string \"DyDB/WeightKg/12\" --body file://test_lambda/create.json "

