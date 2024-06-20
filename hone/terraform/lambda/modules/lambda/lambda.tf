


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
  timeout       = 30
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
      "Statement": [
      {
        "Effect": "Allow",
        "Action": [
          "dynamodb:*"
        ],
        "Resource": "*"
      },
	  {
        "Effect": "Allow",
        "Action": [
          "ssm:*",
        ],
        "Resource": "*"
      }
      ]
    })
  }
}

output "function" {
    value = aws_lambda_function.dynamodb_lambda
}
