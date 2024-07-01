provider "aws" {
  region = "eu-west-3"
}

resource "aws_ecr_repository" "lambda_ecr_repo" {
  name = "hone-lambda-ecr"
}

output "repository_url" {
  value = aws_ecr_repository.lambda_ecr_repo.repository_url
}
