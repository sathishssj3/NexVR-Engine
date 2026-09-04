variable "environment" {
  type = string
}

resource "aws_wafv2_web_acl" "main" {
  name        = "${var.environment}-nexvr-web-acl"
  description = "WAF Web ACL for NexVR Cloud Services"
  scope       = "CLOUDFRONT"

  default_action {
    allow {}
  }

  rule {
    name     = "AWSManagedRulesCommonRuleSet"
    priority = 1

    override_action {
      none {}
    }

    statement {
      managed_rule_group_statement {
        name        = "AWSManagedRulesCommonRuleSet"
        vendor_name = "AWS"
      }
    }

    visibility_config {
      cloudwatch_metrics_enabled = true
      metric_name                = "AWSManagedRulesCommonRuleSetMetric"
      sampled_requests_enabled   = true
    }
  }

  rule {
    name     = "RateLimit1000Per5Min"
    priority = 2

    action {
      block {}
    }

    statement {
      rate_based_statement {
        limit              = 1000
        aggregate_key_type = "IP"
      }
    }

    visibility_config {
      cloudwatch_metrics_enabled = true
      metric_name                = "RateLimitMetric"
      sampled_requests_enabled   = true
    }
  }

  visibility_config {
    cloudwatch_metrics_enabled = true
    metric_name                = "${var.environment}NexVRWAF"
    sampled_requests_enabled   = true
  }

  tags = {
    Name        = "${var.environment}-nexvr-waf"
    Environment = var.environment
  }
}

output "web_acl_arn" {
  value = aws_wafv2_web_acl.main.arn
}
