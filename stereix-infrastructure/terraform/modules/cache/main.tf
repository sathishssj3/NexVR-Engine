variable "environment" {
  type = string
}

variable "subnet_ids" {
  type = list(string)
}

variable "security_group_id" {
  type = string
}

resource "aws_elasticache_subnet_group" "redis" {
  name       = "${var.environment}-nexvr-redis-subnet-group"
  subnet_ids = var.subnet_ids
}

resource "aws_elasticache_replication_group" "redis" {
  replication_group_id       = "${var.environment}-nexvr-redis"
  description                = "NexVR Redis cluster for API gateway cache"
  node_type                  = "cache.t4g.medium"
  port                       = 6379
  parameter_group_name       = "default.redis7"
  subnet_group_name          = aws_elasticache_subnet_group.redis.name
  security_group_ids         = [var.security_group_id]
  automatic_failover_enabled = var.environment == "production" ? true : false
  num_cache_clusters         = var.environment == "production" ? 2 : 1

  at_rest_encryption_enabled = true
  transit_encryption_enabled = true

  tags = {
    Name        = "${var.environment}-nexvr-redis"
    Environment = var.environment
  }
}

output "primary_endpoint_address" {
  value = aws_elasticache_replication_group.redis.primary_endpoint_address
}
