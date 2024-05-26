#
#
# author: Jonas Behr 
# date: Sun Mar  3 15:49:10 2024
######################################

from datetime import datetime
import pytz
import json
import boto3

def create_html_response(temp_c: float):
    header = '''
        <!DOCTYPE html>
        <html>
        <head>
            <title>Hone control page</title>
        </head>
    '''
    body = "<body>\n"
    body += "<h1>Current temp</h1>\n"
    body += f"<p>current temperature is {temp_c}</p>\n"
    body += "</body>\n"
    return header + body + "</html>"

def lambda_handler(event, context):
    dynamodb = boto3.resource('dynamodb')
    table = dynamodb.Table('hone_heat_table') 

    utc_now = datetime.utcnow()
    timezone = pytz.timezone('Europe/Berlin')
    local_time = utc_now.replace(tzinfo=pytz.utc).astimezone(timezone)
    timestamp = str(local_time)

    params = event.get("queryStringParameters", {})

    data = {
        'sensor_id': params.get("Sensor", "default_sensor"),
        'timestamp': timestamp,
        'value': params.get("Weight", 0),
    }

    # Put item to DynamoDB table
    response = table.put_item(Item=data)

    body = {
        "msg": 'Data pushed to DynamoDB successfully!',
        "pushed_data": data,
        "params": params,
        "event": event,
        "context": str(context),
    }

    return {
        'statusCode': 200,
        'headers': {
            'Content-Type': 'text/html',
        },
        'body': create_html_response(22.7), 
    }
    #return {
    #    'statusCode': 200,
    #    'body': json.dumps(body)
    #}

