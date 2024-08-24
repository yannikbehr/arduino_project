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
import numpy as np

def temp_calc(r, beta=3850, t0=25, r0=100):
    K = 273.15
    val = 1/(t0+K) + 1/beta*np.log(r/r0)
    return float(1/val - K)

def temp_correction(c, beta_corrected = 3850):
    k0 = 273.15
    t0 = k0 + 25.0
    r0 = 100 # ohm
    beta = 3850
    y = beta * (1/(c+k0) - 1/t0)
    r_orig = np.exp(y) * r0
    t0_corrected = k0 # reference is at 0 degrees celcius
    return 1/(1/t0_corrected + 1/beta_corrected*np.log(r_orig/r0)) -k0


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
month = 7
day = 19
date_start = datetime(current_year, month, day)
df = df[df["datetime"] > date_start]
plx.line(df, x="datetime", y="value").show()

pio.write_html(fig, file='scatter_plot.html', auto_open=True)
#print(response['Items'])
