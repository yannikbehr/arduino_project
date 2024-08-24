#
#
# author: Jonas Behr 
# date: Mon Jul  1 10:58:46 2024
######################################

import boto3
import json
import pandas as pd
from datetime import datetime

# Initialize the boto3 client
dynamodb = boto3.client('dynamodb')

# Define the table name and keys
table_name = "MyBeeDataTable"
partition_key = "myPartitionKey"
sort_key = "mySortKey"
date = "29/Jun"
year = "2024"

def query_dynamodb(table_name, partition_key, sort_key, partition, sort_prefix):
    response = dynamodb.query(
        TableName=table_name,
        KeyConditionExpression=f"{partition_key} = :partition and begins_with({sort_key}, :sort_prefix)",
        ExpressionAttributeValues={
            ":partition": {"S": partition},
            ":sort_prefix": {"S": sort_prefix}
        }
    )
    return response['Items']

def parse_items(items, partition, date):
    date_format = "%d/%b:%Y:%H:%M:%S %z"
    results = []
    for item in items:
        date_str =  f"{date}:{item[sort_key]['S'].split('/')[-1]}"
        timestamp = datetime.strptime(date_str, date_format)
        result = {
            partition: item['MyValue']['S'],
            "timestamp": timestamp
        }
        results.append(result)
    return results

partition = "WeightKg"
sort_prefix = f"SortKey_{date}/{year}"
items = query_dynamodb(table_name, partition_key, sort_key, partition, sort_prefix)
df = pd.DataFrame.from_dict(parse_items(items, partition, date))

print(df)
