provider "aws" {
  region = "eu-west-3"
}

variable region {
	type = string
	default = "eu-west-3"
}

variable stage {
    type = string
    default = "dev"
}

module "lambda" {
    image  = "137637932857.dkr.ecr.eu-west-3.amazonaws.com/hone-lambda-ecr:13" 
    source = "./modules/lambda"

}

resource "aws_api_gateway_rest_api" "api" {
  name        = "dynamodb-api"
  description = "API to interact with DynamoDB via Lambda"
}

resource "aws_api_gateway_resource" "resource" {
  rest_api_id = aws_api_gateway_rest_api.api.id
  parent_id   = aws_api_gateway_rest_api.api.root_resource_id
  path_part   = "{anyString}"
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
  uri                     = module.lambda.function.invoke_arn
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
  stage_name  = var.stage
}

resource "aws_lambda_permission" "api_gw" {  
    statement_id  = "AllowExecutionFromAPIGateway"  
    action        = "lambda:InvokeFunction"  
    function_name = module.lambda.function.function_name  
    principal     = "apigateway.amazonaws.com"  
    source_arn = "${aws_api_gateway_rest_api.api.execution_arn}/*/*"
}


resource "aws_cloudfront_distribution" "api_gateway_distribution" {
  origin {
    domain_name = "${aws_api_gateway_rest_api.api.id}.execute-api.${var.region}.amazonaws.com"
    origin_id   = "apiGatewayOrigin"

    custom_origin_config {
      http_port              = 80
      https_port             = 443
      origin_protocol_policy = "https-only"
      origin_ssl_protocols   = ["TLSv1.2"]
    }
  }

  enabled             = true
  is_ipv6_enabled     = true
  default_root_object = ""

  default_cache_behavior {
    allowed_methods  = ["GET", "HEAD", "OPTIONS"]
    cached_methods   = ["GET", "HEAD"]
    target_origin_id = "apiGatewayOrigin"

    forwarded_values {
      query_string = true

      cookies {
        forward = "none"
      }
    }

    #viewer_protocol_policy = "redirect-to-https"
    viewer_protocol_policy = "allow-all"
    min_ttl                = 0
    default_ttl            = 3600
    max_ttl                = 86400
  }

  price_class = "PriceClass_100"

  restrictions {
    geo_restriction {
      restriction_type = "none"
    }
  }

  viewer_certificate {
    cloudfront_default_certificate = true
  }

  tags = {
    Name = "api-gateway-distribution"
  }
}


output "api_url" {
  value = "curl -X GET ${aws_api_gateway_deployment.deployment.invoke_url}/${aws_api_gateway_resource.resource.path_part}?Weight=75"
  description = "The URL of the API Gateway"
}

output "cloudfront_domain_name" {
  value       = "curl -X GET \"http://${aws_cloudfront_distribution.api_gateway_distribution.domain_name}/${var.stage}/${aws_api_gateway_resource.resource.path_part}?heating_switch=refresh&html=false\""
  description = "The domain name of the CloudFront distribution"
}


