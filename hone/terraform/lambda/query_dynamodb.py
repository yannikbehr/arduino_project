#
#
# author: Jonas Behr 
# date: Sun Jun 23 14:55:56 2024
######################################

from datetime import datetime, timedelta
import pytz
import json
import boto3
import pandas as pd
import plotly.express as plx
import plotly.io as pio
from boto3.dynamodb.conditions import Key

from app import get_recent_data, plot_temp
table_name = 'hone_heat_table'
print(plot_temp(table_name))
