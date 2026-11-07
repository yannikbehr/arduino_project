#!/usr/bin/env python3
"""Print today's sensor readings (local time) from DynamoDB as a table."""
import datetime
import json
import subprocess
import sys

TABLE = "esp32_sensor_readings"
REGION = "eu-west-3"
DEVICE = sys.argv[1] if len(sys.argv) > 1 else "SIM7080G_01"


def value(item, key):
    attribute = item.get(key, {})
    return next(iter(attribute.values()), "-")


midnight = datetime.datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
output = subprocess.run(
    [
        "aws", "dynamodb", "query",
        "--region", REGION,
        "--table-name", TABLE,
        "--key-condition-expression", "device_id = :d AND #ts >= :t",
        "--expression-attribute-names", json.dumps({"#ts": "ts"}),
        "--expression-attribute-values",
        json.dumps({":d": {"S": DEVICE}, ":t": {"N": str(int(midnight.timestamp()))}}),
        "--output", "json",
    ],
    check=True, capture_output=True, text=True,
).stdout
items = json.loads(output)["Items"]

print(f"{'time':<10}{'temp':>6}{'hum':>6}{'batt_mv':>9}{'batt_pct':>10}{'charging':>10}")
for item in items:
    time = datetime.datetime.fromtimestamp(int(value(item, "ts"))).strftime("%H:%M:%S")
    print(f"{time:<10}{value(item, 'temp'):>6}{value(item, 'hum'):>6}{value(item, 'batt_mv'):>9}"
          f"{value(item, 'batt_pct'):>10}{str(value(item, 'charging')).lower():>10}")
print(f"{len(items)} readings for {DEVICE} since {midnight:%Y-%m-%d %H:%M}")
