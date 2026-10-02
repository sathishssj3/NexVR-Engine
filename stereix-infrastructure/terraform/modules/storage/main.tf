variable "environment" {
  type = string
}

resource "aws_s3_bucket" "releases" {
  bucket = "${var.environment}-nexvr-releases-${var.environment == "production" ? "prod" : "stg"}"

  tags = {
    Name        = "${var.environment}-nexvr-releases"
    Environment = var.environment
  }
}

resource "aws_s3_bucket_versioning" "releases" {
  bucket = aws_s3_bucket.releases.id
  versioning_configuration {
    status = "Enabled"
  }
}

resource "aws_s3_bucket_server_side_encryption_configuration" "releases" {
  bucket = aws_s3_bucket.releases.id

  rule {
    apply_server_side_encryption_by_default {
      sse_algorithm = "AES256"
    }
  }
}

resource "aws_s3_bucket_public_access_block" "releases" {
  bucket = aws_s3_bucket.releases.id

  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

output "bucket_id" {
  value = aws_s3_bucket.releases.id
}

output "bucket_arn" {
  value = aws_s3_bucket.releases.arn
}

output "bucket_regional_domain_name" {
  value = aws_s3_bucket.releases.bucket_regional_domain_name
}
