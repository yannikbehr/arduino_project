#
#
# author: Jonas Behr 
# date: Sun Mar  3 15:49:10 2024
######################################

from datetime import datetime, timedelta
import pytz
import json
import boto3
import pandas as pd
import plotly.express as plx
import plotly.io as pio
from boto3.dynamodb.conditions import Key

USERS = {
    "010233212": "Nina",
    "201928474": "Jonas",
    "463701923": "ESP32",
}

def get_recent_data(table, sensor_id, days):
    now = datetime.now()
    past_timestamp = now - timedelta(days=days)
    past_timestamp_str = past_timestamp.isoformat()
    response = table.query(
        KeyConditionExpression=boto3.dynamodb.conditions.Key('sensor_id').eq(sensor_id) &
                               boto3.dynamodb.conditions.Key('timestamp').gte(past_timestamp_str)
    )
    return response['Items']

def plot_temp(table_name, days=7):

    dynamodb = boto3.resource('dynamodb')
    table = dynamodb.Table(table_name)
    
    df1 = pd.DataFrame(get_recent_data(table, "temp_1", days))
    df2 = pd.DataFrame(get_recent_data(table, "temp_2", days))
    df = df1.merge(df2, on="timestamp").rename(columns={"value_x": "temp1", "value_y": "temp2"})    
    
    df['datetime'] = df['timestamp'].apply(datetime.fromisoformat)
    df["datetime"] = df["datetime"].dt.tz_localize(None)
    for field in ["temp1", "temp2"]:
        df[field] = df[field].astype(float)
    
    fig = plx.line(df, x="datetime", y=["temp1", "temp2"])
    plotly_html = pio.to_html(fig, full_html=False, include_plotlyjs='cdn')
    return plotly_html


def parse_user_id(user_id: str) -> str:
    return USERS.get(user_id, "")

def user_to_id(user: str) -> str:
    return [uid for uid, u in USERS.items() if u==user][0]

def set_ssm_param(name: str, value: str):
    try: 
        ssm = boto3.client('ssm')
        ssm.put_parameter(Name=name, Value=value, Type='String', Overwrite=True)
    except Exception as e:
        return str(e)
    return ""


def get_ssm_param(name: str):
    try:
        ssm = boto3.client('ssm')
        parameter = ssm.get_parameter(Name=name, WithDecryption=False)
        return parameter['Parameter']['Value']
    except Exception as e:
        return str(e)

def add_button(host, user, status: str):
    text = f"Switch heating {status}"
    if status == "refresh": 
        text = "Refresh"
    button = f"<button onclick=\"navigateToSwitch{status}()\">{text}</button>\n"
    button += "<script>\n"
    button += f"  function navigateToSwitch{status}()" + " {{ \n" 
    button += f"   window.location.href = \"https://{host}/dev/dynamodb?heating_switch={status}&userId={user_to_id(user)}\";\n"
    button += "  }}\n"
    button += "</script>\n"
    return button

def table_row(description, value):
    body =  f"<tr>\n"
    body += f"  <td style=\"padding-right: 5px\">{description}</td>\n"
    body += f"  <td style=\"background-color: silver\", align=\"center\"><b>{value}</b></td>\n"
    body += f"</tr>\n"
    return body

def create_html_response(temp_c: float, host: str, user, plotly_html_page):
    header = '''
        <!DOCTYPE html>
        <html>
        <head>
            <title>Hone control page</title>
        </head>
    '''
    body = "<body>\n"
    body += "<h1>Current Hone Data</h1>\n"
    body += "<table>\n"
    body += "  <thead>"
    body += "    <tr>"
    body += "      <th scope=\"col\">Category</th>"
    body += "      <th scope=\"col\">Value</th>"
    body += "    </tr>"
    body += "  </thead>"
    body += table_row("Current temperature", temp_c)
    body += table_row("Heating switch", get_ssm_param('heating_switch'))
    body += table_row("Reported by Hone controller", get_ssm_param("heating_hone_status"))
    body += table_row("Last heard from Hone", get_ssm_param("last_msg_from_ESP").split(".")[0])
    body += f"</tr>\n"
    body += "</table>\n"
    body += f"<p>Switch heating</p>\n"
    body += add_button(host, user, "on")
    body += add_button(host, user, "off")
    body += f"<p></p>\n"
    body += add_button(host, user, "refresh")
    if plotly_html_page:
        body += "<h1>Temperatur</h1>"
        body += plotly_html_page
    body += "</body>\n"
    return header + body + "</html>"

def lambda_handler(event, context):
    dynamodb = boto3.resource('dynamodb')
    table_name = 'hone_heat_table'
    table = dynamodb.Table(table_name) 

    utc_now = datetime.utcnow()
    timezone = pytz.timezone('Europe/Berlin')
    local_time = utc_now.replace(tzinfo=pytz.utc).astimezone(timezone)
    timestamp = str(local_time)

    my_host = event.get("requestContext", {}).get("domainName", "")
    params = event.get("queryStringParameters", {})
    user = parse_user_id(params.get("userId", "0000"))

    if not user:
        return {
            'statusCode': 403,
            'headers': {
                'Content-Type': 'text/html',
            },
            'body': "Forbidden....", 
        }

    if user == "ESP32":
        heating_hone_status = params.get("heating", "unknown")
        set_ssm_param("last_msg_from_ESP", timestamp)
        set_ssm_param("heating_hone_status", heating_hone_status)
        value_temp_1 = params.get("tempSensor1", "-1000.0")
        value_temp_2 = params.get("tempSensor2", "-1000.0")
        for sensor_id, value in [("temp_1", value_temp_1), ("temp_2", value_temp_2)]:
            data = {
                'sensor_id': sensor_id,
                'timestamp': timestamp,
                'value': value,
            }
            if value != -1000.0:
                set_ssm_param("last_hone_"+sensor_id, str(value))
                # Put item to DynamoDB table
                response = table.put_item(Item=data)
        return {
            'statusCode': 200,
             'headers': {
                #'Cache-Control': 'no-cache, no-store, must-revalidate',
                #'Pragma': 'no-cache',
                #'Expires': '0',
                'Cache-Control': 'max-age=120',
                'Content-Type': 'text/html',
            },
            'body': f"switch:{get_ssm_param('heating_switch')}", 
        }

    html_page = ""
    if user in ["Nina", "Jonas"]:
        # heating switch
        switch = params.get("heating_switch", "")
        if switch == "refresh": 
            pass
        elif switch in ["on", "off"]:
            set_ssm_param("heating_switch", switch)

        html_page = plot_temp(table_name)

    if params.get("html", "") == "false":
        try: 
            body = {
                "msg": 'Data pushed to DynamoDB successfully!',
                "pushed_data": data,
                "params": params,
                "event": event,
                "context": str(context),
                "host": my_host,
            }
            return {
                'statusCode': 200,
                'body': json.dumps(body)
            }       
        except Exception as e:
            return {
                'statusCode': 401,
                'body': str(e),
            }

    return {
        'statusCode': 200,
        'headers': {
            'Content-Type': 'text/html',
        },
        'body': create_html_response(get_ssm_param("last_hone_temp_1"), my_host, user, html_page), 
    }


