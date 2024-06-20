#
#
# author: Jonas Behr 
# date: Sun Mar  3 15:49:10 2024
######################################

from datetime import datetime
import pytz
import json
import boto3

USERS = {
    "010233212": "Nina",
    "201928474": "Jonas",
    "463701923": "ESP32",
}

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
    button += f"   window.location.href = \"https://{host}/dev/dynamodb?heating_switch={status}&user={user_to_id(user)}\";\n"
    button += "  }}\n"
    button += "</script>\n"
    return button

def table_row(description, value):
    body =  f"<tr>\n"
    body += f"  <td style=\"padding-right: 5px\">{description}</td>\n"
    body += f"  <td style=\"background-color: silver\", align=\"center\"><b>{value}</b></td>\n"
    body += f"</tr>\n"
    return body

def create_html_response(temp_c: float, host: str, user):
    header = '''
        <!DOCTYPE html>
        <html>
        <head>
            <title>Hone control page</title>
        </head>
    '''
    body = "<body>\n"
    body += "<h1>Current (fake) Hone Data</h1>\n"
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
    body += table_row("Last heard from Hone", get_ssm_param("last_msg_from_ESP"))
    body += f"</tr>\n"
    body += "</table>\n"
    body += f"<p>Switch heating</p>\n"
    body += add_button(host, user, "on")
    body += add_button(host, user, "off")
    body += f"<p></p>\n"
    body += add_button(host, user, "refresh")
    body += "</body>\n"
    return header + body + "</html>"

def lambda_handler(event, context):
    dynamodb = boto3.resource('dynamodb')
    table = dynamodb.Table('hone_heat_table') 

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
        data = {
            'sensor_id': params.get("sensorId", ""),
            'timestamp': timestamp,
            'value': params.get("value", "0.0"),
        }
        if data['sensor_id'] and data['value']:
            # Put item to DynamoDB table
            response = table.put_item(Item=data)
        return {
            'statusCode': 200,
             'headers': {
                'Content-Type': 'text/html',
            },
            'body': f"switch:{get_ssm_param('heating_switch', switch)}", 
        }

    if user in ["Nina", "Jonas"]:
        # heating switch
        switch = params.get("heating_switch", "")
        if switch == "refresh": 
            pass
        elif switch in ["on", "off"]:
            set_ssm_param("heating_switch", switch)

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
        'body': create_html_response(22.7, my_host, user), 
    }


