
provider "aws" {
  region = "eu-west-3"
}

resource "aws_dynamodb_table" "hone_temp" {
  name = "hone_heat_table"
  hash_key = "sensor_id" 
  range_key = "timestamp" 
  #read_capacity  = 1 
  #write_capacity = 1
  billing_mode = "PAY_PER_REQUEST" # Change to PROVISIONED if needed

  attribute {
    name = "sensor_id"
    type = "S" # String
  }
  attribute {
    name = "timestamp"
    type = "S" # String
  }
  #attribute {
  #  name = "value"
  #  type = "S" # String
  #}
}

terraform {
  backend "s3" {
    bucket = "jb-terraform-state-137637932857" 
    key    = "main-tf-state" 
    region = "eu-west-3"
  }
}
