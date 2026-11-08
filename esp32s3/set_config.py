#!/usr/bin/env python3
"""Show or change the device's retained config message on AWS IoT.

Usage: set_config.py DEVICE                  show the current config
       set_config.py DEVICE relay=true       change one or more keys, keep the rest

The device applies the retained message on every connect, so it also survives power cuts.
A retained message replaces the previous one as a whole, so the current config is read and merged first.
"""
import base64
import json
import subprocess
import sys

REGION = "eu-west-3"
ENDPOINT = "https://a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com"
DEVICE = sys.argv[1]
TOPIC = f"devices/{DEVICE}/config"
AWS = ["aws", "iot-data", "--region", REGION, "--endpoint-url", ENDPOINT]


def current_config():
    result = subprocess.run(AWS[:2] + ["get-retained-message"] + AWS[2:] + ["--topic", TOPIC, "--output", "json"],
                            capture_output=True, text=True)
    if result.returncode != 0:
        if "ResourceNotFoundException" in result.stderr:
            return {}
        sys.exit(result.stderr)
    return json.loads(base64.b64decode(json.loads(result.stdout)["payload"]))


config = current_config()
changes = dict(arg.split("=", 1) for arg in sys.argv[2:])
if changes:
    for key, value in changes.items():
        config[key] = json.loads(value)  # true/false/600 become JSON values
    payload = json.dumps(config, separators=(",", ":"))
    subprocess.run(AWS[:2] + ["publish"] + AWS[2:] + ["--topic", TOPIC, "--qos", "1", "--retain", "--payload", payload],
                   check=True)
print(f"{TOPIC}: {json.dumps(config)}")
