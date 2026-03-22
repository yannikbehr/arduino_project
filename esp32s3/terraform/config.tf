 provider "aws" {
  region = "eu-west-3"
}


# 1. Define who can "Assume" this role (The Lambda Service)
data "aws_iam_policy_document" "lambda_assume_role" {
  statement {
    actions = ["sts:AssumeRole"]
    principals {
      type        = "Service"
      identifiers = ["lambda.amazonaws.com"]
    }
  }
}


# 2. Create the IAM Role
resource "aws_iam_role" "iot_lambda_role" {
  name               = "esp32_iot_lambda_role"
  assume_role_policy = data.aws_iam_policy_document.lambda_assume_role.json
}


# 3. Attach Basic Execution Permissions (Allows writing to CloudWatch Logs)
resource "aws_iam_role_policy_attachment" "lambda_logs" {
  role       = aws_iam_role.iot_lambda_role.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole"
}



# 1. Package the code
data "archive_file" "lambda_zip" {
  type        = "zip"
  source_dir  = "${path.module}/src"
  output_path = "${path.module}/lambda_function.zip"
}


# 2. Create the Lambda Function
resource "aws_lambda_function" "iot_processor" {
  filename         = data.archive_file.lambda_zip.output_path
  function_name    = "esp32_sensor_processor"
  role             = aws_iam_role.iot_lambda_role.arn
  handler          = "index.handler"
  runtime          = "python3.12"
  source_code_hash = data.archive_file.lambda_zip.output_base64sha256
  # Environment variables for your future DB connection or AWS IoT Endpoint
  environment {
    variables = {
      IOT_TOPIC = "devices/esp32/config"
    }
  }
}


# 3. Create the Log Group for monitoring
resource "aws_cloudwatch_log_group" "lambda_log_group" {
  name              = "/aws/lambda/${aws_lambda_function.iot_processor.function_name}"
  retention_in_days = 14
}


# 1. Create the "Thing" identity in AWS IoT
resource "aws_iot_thing" "esp32_device" {
  name = "T-SIM7080G_01"
}


# 2. Create the Certificate
resource "aws_iot_certificate" "cert" {
  active = true

}


# 3. Create the Policy (The "Permissions" for your device)
resource "aws_iot_policy" "esp32_policy" {
  name = "Esp32PubSubPolicy"
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action   = ["iot:Connect"]
        Effect   = "Allow"
        Resource = ["*"]
      },
      {
        Action   = ["iot:Publish", "iot:Subscribe"]
        Effect   = "Allow"
        Resource = ["arn:aws:iot:*:*:topic/devices/*"]
      }
    ]
  })
}


# 4. Attach the Certificate to the Thing and the Policy

resource "aws_iot_policy_attachment" "att" {

  policy = aws_iot_policy.esp32_policy.name

  target = aws_iot_certificate.cert.arn

}


resource "aws_iot_thing_principal_attachment" "thing_att" {

  thing     = aws_iot_thing.esp32_device.name

  principal = aws_iot_certificate.cert.arn

}

# 1. The Rule: Routes messages from MQTT to Lambda
resource "aws_iot_topic_rule" "esp32_data_rule" {
  name        = "ESP32ToLambda"
  description = "Routes MQTT messages from ESP32 to Lambda"
  enabled     = true
  # Note: Use single quotes for the topic in SQL
  sql         = "SELECT * FROM 'devices/+/data'"
  sql_version = "2016-03-23"

  lambda {
    function_arn = aws_lambda_function.iot_processor.arn
  }
}

# 2. The Permission: Grants IoT Core permission to invoke your Lambda
resource "aws_lambda_permission" "allow_iot_core" {
  statement_id  = "AllowExecutionFromIoT"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.iot_processor.function_name
  principal     = "iot.amazonaws.com"
  source_arn    = aws_iot_topic_rule.esp32_data_rule.arn
}


# IoT logging role
resource "aws_iam_role" "iot_logging_role" {
  name = "IoTLoggingRole"
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect    = "Allow"
      Principal = { Service = "iot.amazonaws.com" }
      Action    = "sts:AssumeRole"
    }]
  })
}

resource "aws_iam_role_policy_attachment" "iot_logging_cloudwatch" {
  role       = aws_iam_role.iot_logging_role.name
  policy_arn = "arn:aws:iam::aws:policy/CloudWatchLogsFullAccess"
}

# AWS IoT account-level logging
resource "aws_iot_logging_options" "default" {
  default_log_level = "INFO"
  role_arn          = aws_iam_role.iot_logging_role.arn
}

output "certificate_pem" {
  value       = aws_iot_certificate.cert.certificate_pem
  description = "The device certificate"
  sensitive = true
}

output "private_key" {
  value       = aws_iot_certificate.cert.private_key
  description = "The device private key"
  sensitive   = true
}
