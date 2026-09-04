variable "environment" {
  type = string
}

variable "s3_bucket_id" {
  type = string
}

variable "s3_bucket_arn" {
  type = string
}

variable "s3_bucket_regional_domain_name" {
  type = string
}

variable "web_acl_arn" {
  type    = string
  default = null
}

resource "aws_cloudfront_origin_access_control" "oac" {
  name                              = "${var.environment}-nexvr-oac"
  description                       = "OAC for NexVR release binary distribution"
  origin_access_control_origin_type = "s3"
  signing_behavior                  = "always"
  signing_protocol                  = "sigv4"
}

resource "aws_cloudfront_distribution" "cdn" {
  origin {
    domain_name              = var.s3_bucket_regional_domain_name
    origin_id                = "S3-releases"
    origin_access_control_id = aws_cloudfront_origin_access_control.oac.id
  }

  enabled             = true
  is_ipv6_enabled     = true
  default_root_object = "index.html"
  web_acl_id          = var.web_acl_arn

  default_cache_behavior {
    allowed_methods  = ["GET", "HEAD", "OPTIONS"]
    cached_methods   = ["GET", "HEAD"]
    target_origin_id = "S3-releases"

    forwarded_values {
      query_string = false
      cookies {
        forward = "none"
      }
    }

    viewer_protocol_policy = "redirect-to-https"
    min_ttl                = 0
    default_ttl            = 86400
    max_ttl                = 31536000
    compress               = true
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
    Name        = "${var.environment}-nexvr-cdn"
    Environment = var.environment
  }
}

# Attach S3 bucket policy allowing CloudFront OAC access
resource "aws_s3_bucket_policy" "allow_cloudfront" {
  bucket = var.s3_bucket_id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid       = "AllowCloudFrontServicePrincipalReadOnly"
        Effect    = "Allow"
        Principal = { Service = "cloudfront.amazonaws.com" }
        Action    = "s3:GetObject"
        Resource  = "${var.s3_bucket_arn}/*"
        Condition = {
          StringEquals = {
            "AWS:SourceArn" = aws_cloudfront_distribution.cdn.arn
          }
        }
      }
    ]
  })
}

output "distribution_id" {
  value = aws_cloudfront_distribution.cdn.id
}

output "domain_name" {
  value = aws_cloudfront_distribution.cdn.domain_name
}
