provider "aws" {
  region = "eu-west-3"
}

terraform {
  backend "s3" {
    bucket         = "jb-terraform-state-137637932857"
    key            = "user_config.tfstate"
    region         = "eu-west-3"
  }
}

resource "aws_iam_user" "nina" {
  name = "Nina_Behr"
}

# give access to console
resource "aws_iam_user_policy_attachment" "nina_console_access" {
  user       = aws_iam_user.nina.name
  policy_arn = aws_iam_policy.nina_role_policy.arn
}

resource "aws_iam_role" "nina_role" {
  name               = "nina-role"
  assume_role_policy = jsonencode({
    "Version" : "2012-10-17",
    "Statement" : [{
      "Effect"   : "Allow",
      "Principal": { "AWS": aws_iam_user.nina.arn },
      "Action"   : "sts:AssumeRole"
    }]
  })
}

resource "aws_iam_policy" "nina_role_policy" {
  name        = "nina-role-policy"
  description = "Grant Nina access to bucket"
  policy      = jsonencode({
    "Version": "2012-10-17",
    "Statement": [
    {
      "Effect": "Allow",
      "Action": "s3:*",
      "Resource": [
        "arn:aws:s3:::jb-shared-137637932857",
        "arn:aws:s3:::jb-shared-137637932857/*"
      ]
    },
    {
        "Effect": "Allow",
        "Action": "s3:ListAllMyBuckets",
        "Resource": "*"
    },
    ]
  })
}

resource "aws_iam_policy_attachment" "nina_role_attachment" {
  name       = "nina-role-attachment"
  policy_arn = aws_iam_policy.nina_role_policy.arn
  roles      = [aws_iam_role.nina_role.name]
}

###########################################
# role and authorization for phone backup
###########################################
resource "aws_iam_user" "phone_backup" {
  name = "phone-backup"
}

resource "aws_iam_user_policy_attachment" "phone_access" {
  user       = aws_iam_user.phone_backup.name
  policy_arn = aws_iam_policy.phone_role_policy.arn
}

resource "aws_iam_role" "phone_role" {
  name  = "phone-role"
  assume_role_policy = jsonencode({
    "Version" : "2012-10-17",
    "Statement" : [{
      "Effect"   : "Allow",
      "Principal": { "AWS": aws_iam_user.phone_backup.arn },
      "Action"   : "sts:AssumeRole"
    }]
  })

}

resource "aws_iam_policy" "phone_role_policy" {
  name        = "phone-role-policy"
  description = "Grant phone access to bucket"
  policy      = jsonencode({
    "Version": "2012-10-17",
    "Statement": [
    {
      "Effect": "Allow",
      "Action": "s3:*",
      "Resource": [
        "arn:aws:s3:::jb-phone-backup-137637932857",
        "arn:aws:s3:::jb-phone-backup-137637932857/*"
      ]
    },
    {
        "Effect": "Allow",
        "Action": "s3:ListAllMyBuckets",
        "Resource": "*"
    },
    ]
  })
}

resource "aws_iam_policy_attachment" "phone_role_attachment" {
  name       = "phone-role-attachment"
  policy_arn = aws_iam_policy.phone_role_policy.arn
  roles      = [aws_iam_role.phone_role.name]
}

