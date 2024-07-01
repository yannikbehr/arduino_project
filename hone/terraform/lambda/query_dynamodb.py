#
#
# author: Jonas Behr 
# date: Sun Jun 23 14:55:56 2024
######################################

from datetime import datetime
import pytz
import json
import boto3
from boto3.dynamodb.conditions import Key
import plotly.express as plx
import plotly.io as pio
import pandas as pd


#dt = datetime.fromisoformat(timestamp_str)

table_name = 'hone_heat_table'

dynamodb = boto3.resource('dynamodb')
table = dynamodb.Table(table_name)


response = table.query(
    KeyConditionExpression=Key('sensor_id').eq('temp_1')
)
df = pd.DataFrame.from_dict(response["Items"])

df['datetime'] = df['timestamp'].apply(datetime.fromisoformat)
df["datetime"] = df["datetime"].dt.tz_localize(None)
df["value"] = df["value"].astype(float)


# drop old values: 
current_year = datetime.now().year
month = 6
day = 29
date_start = datetime(current_year, month, day)
df = df[df["datetime"] > date_start]
plx.line(df, x="datetime", y="value").show()

pio.write_html(fig, file='scatter_plot.html', auto_open=True)
#print(response['Items'])
