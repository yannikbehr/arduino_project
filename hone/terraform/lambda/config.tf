provider "aws" {
  region = "eu-west-3"
}

resource "null_resource" "build_lambda" {
  provisioner "local-exec" {
    command = "/bin/bash ./zip_lambda.sh" 
  }
}

resource "aws_lambda_function" "dynamodb_lambda" {
  function_name = "dynamodb_lambda_function"
  filename      = "lambda_function.zip"
  #source_code_hash = local_file.lambda_hash.content
  source_code_hash = "1" # always update the lambda function 
  handler       = "lambda_function.lambda_handler"
  runtime       = "python3.12"
  role          = aws_iam_role.lambda_role.arn
  depends_on = [null_resource.build_lambda]
}

resource "aws_iam_role" "lambda_role" {
  name = "dynamodb_lambda_role"

  assume_role_policy = jsonencode({
    "Version" : "2012-10-17",
    "Statement": [{
      "Action"   : "sts:AssumeRole",
      "Principal": {
        "Service": "lambda.amazonaws.com"
      },
      "Effect"   : "Allow",
      "Sid"      : ""
    }]
  })

  inline_policy {
    name = "lambda_dynamodb_policy"

    policy = jsonencode({
      "Version": "2012-10-17",
      "Statement": [{
        "Effect": "Allow",
        "Action": [
          "dynamodb:*"
        ],
        "Resource": "*"
      }]
    })
  }
}


resource "aws_api_gateway_rest_api" "api" {
  name        = "dynamodb-api"
  description = "API to interact with DynamoDB via Lambda"
}

resource "aws_api_gateway_resource" "resource" {
  rest_api_id = aws_api_gateway_rest_api.api.id
  parent_id   = aws_api_gateway_rest_api.api.root_resource_id
  path_part   = "dynamodb"
}

resource "aws_api_gateway_method" "method" {
  rest_api_id   = aws_api_gateway_rest_api.api.id
  resource_id   = aws_api_gateway_resource.resource.id
  http_method   = "GET"
  authorization = "NONE"
}

resource "aws_api_gateway_integration" "integration" {
  rest_api_id             = aws_api_gateway_rest_api.api.id
  resource_id             = aws_api_gateway_resource.resource.id
  http_method             = aws_api_gateway_method.method.http_method
  integration_http_method = "POST"
  type                    = "AWS_PROXY"
  uri                     = aws_lambda_function.dynamodb_lambda.invoke_arn
  request_templates = {
    "application/json" = <<EOF
    {
      "operation" : "create",
      "tableName" : "MyBeeDataTable",
      "payload" : {
        "Item" : {
          "mySortKey" : "WeightSort_$context.requestTime",
          "myPartitionKey" : "WeightKg",
          "MyValue" : "$input.params('Weight')"
        }
      }
    }
    EOF
  }
}

resource "aws_api_gateway_method_response" "method_response" {
  rest_api_id = aws_api_gateway_rest_api.api.id
  resource_id = aws_api_gateway_resource.resource.id
  http_method = aws_api_gateway_method.method.http_method
  status_code = "200"
  response_models = {
    "application/json" = aws_api_gateway_model.MyDemoResponseModel.name
  }
}

resource "aws_api_gateway_model" "MyDemoResponseModel" {
  rest_api_id  = aws_api_gateway_rest_api.api.id
  name         = "MyResponseModel"
  description  = "API response for dynamodb lambda function"
  content_type = "application/json"
  schema = jsonencode({
    "$schema" = "http://json-schema.org/draft-04/schema#"
    title     = "MyDemoResponse"
    type      = "object"
    properties = {
      Message = {
        type = "string"
      }
    }
  })
}

resource "aws_api_gateway_integration_response" "integration_response" {
  rest_api_id = aws_api_gateway_rest_api.api.id
  resource_id = aws_api_gateway_resource.resource.id
  http_method = aws_api_gateway_method.method.http_method
  status_code = aws_api_gateway_method_response.method_response.status_code
  depends_on = [aws_api_gateway_integration.integration]
}

resource "aws_api_gateway_deployment" "deployment" {
  depends_on = [
    aws_api_gateway_integration.integration,
    aws_api_gateway_method_response.method_response,
    aws_api_gateway_integration_response.integration_response
  ]
  rest_api_id = aws_api_gateway_rest_api.api.id
  stage_name  = "dev"
}

resource "aws_lambda_permission" "api_gw" {  
    statement_id  = "AllowExecutionFromAPIGateway"  
    action        = "lambda:InvokeFunction"  
    function_name = aws_lambda_function.dynamodb_lambda.function_name  
    principal     = "apigateway.amazonaws.com"  
    source_arn = "${aws_api_gateway_rest_api.api.execution_arn}/*/*"
}


output "api_url" {
  value = "curl -X GET ${aws_api_gateway_deployment.deployment.invoke_url}/${aws_api_gateway_resource.resource.path_part}?Weight=75"
  description = "The URL of the API Gateway"
}


