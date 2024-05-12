#
#
# author: Jonas Behr 
# date: Sun Mar  3 15:49:10 2024
######################################

from datetime import datetime
import pytz
import json
import boto3

def lambda_handler(event, context):
    dynamodb = boto3.resource('dynamodb')
    table = dynamodb.Table('hone_heat_table') 

    utc_now = datetime.utcnow()
    timezone = pytz.timezone('Europe/Berlin')
    local_time = utc_now.replace(tzinfo=pytz.utc).astimezone(timezone)
    timestamp = str(local_time)

    # Sample data to be pushed to DynamoDB
    data = {
        'sensor_id': 'test_sensor_1',
        'timestamp': timestamp,
        'value': 123
    }

    # Put item to DynamoDB table
    response = table.put_item(Item=data)

    return {
        'statusCode': 200,
        'body': json.dumps('Data pushed to DynamoDB successfully!')
    }

