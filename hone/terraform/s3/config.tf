
provider "aws" {
  region = "eu-west-3"
}

locals {
    bucket_names = ["jb-shared-137637932857", "jb-phone-backup-137637932857"]
}

resource "aws_s3_bucket" "buckets" {
  count         = length(local.bucket_names)
  bucket        = local.bucket_names[count.index]
}


resource "aws_s3_bucket_ownership_controls" "bucket_ownership_controls" {
  count = length(local.bucket_names)

  bucket = aws_s3_bucket.buckets[count.index].id
  rule {
    object_ownership = "BucketOwnerPreferred"
  }
}

resource "aws_s3_bucket_acl" "buckets_acl" {
  depends_on = [aws_s3_bucket.buckets[count.index]]
  count = length(local.bucket_names)
  bucket = aws_s3_bucket.buckets[count.index].id
  acl    = "private"
}


# terraform state bucket
resource "aws_s3_bucket" "terraform_state" {
  bucket = "jb-terraform-state-137637932857"
}

resource "aws_s3_bucket_ownership_controls" "ownership" {
  bucket = aws_s3_bucket.terraform_state.id
  rule {
    object_ownership = "BucketOwnerPreferred"
  }
}

resource "aws_s3_bucket_acl" "acl" {
  depends_on = [aws_s3_bucket_ownership_controls.ownership]

  bucket = aws_s3_bucket.terraform_state.id
  acl    = "private"
}

