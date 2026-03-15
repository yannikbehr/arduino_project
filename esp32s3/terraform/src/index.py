#
#
# author: Jonas Behr 
# date: Sun Mar 15 13:27:12 2026
######################################
import json

def handler(event, context):
    print("Received IoT Data:", json.dumps(event))

    # This is where we will eventually add logic to send
    # config back to the ESP32 (MQTT Publish)
    return {
        'statusCode': 200,
        'body': json.dumps('Data processed successfully')
    }
