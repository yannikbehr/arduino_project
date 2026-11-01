#
#
# author: Jonas Behr 
# date: Sun Mar 15 13:27:12 2026
######################################
import json
import os
from decimal import Decimal

import boto3

# Anything earlier comes from a device that has not synced its clock yet (2020-01-01)
MIN_VALID_TS = 1577836800

table = boto3.resource("dynamodb").Table(os.environ["TABLE_NAME"])


def handler(event, context):
    print("Received IoT Data:", json.dumps(event))

    stored = 0
    with table.batch_writer(overwrite_by_pkeys=["device_id", "ts"]) as batch:
        for reading in event.get("readings", []):
            if reading["ts"] < MIN_VALID_TS:
                print("Skipping reading without a synced timestamp:", json.dumps(reading))
                continue

            # DynamoDB needs Decimal rather than float; failed sensor reads arrive as null and are left out
            item = json.loads(json.dumps(reading), parse_float=Decimal)
            item = {key: value for key, value in item.items() if value is not None}
            item["device_id"] = event["device_id"]
            batch.put_item(Item=item)
            stored += 1

    print(f"Stored {stored} readings")
    return {
        'statusCode': 200,
        'body': json.dumps({'stored': stored})
    }
