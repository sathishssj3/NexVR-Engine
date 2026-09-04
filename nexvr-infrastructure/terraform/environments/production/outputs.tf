output "eks_cluster_name" {
  value       = module.eks.cluster_name
  description = "Name of the production EKS cluster"
}

output "eks_cluster_endpoint" {
  value       = module.eks.cluster_endpoint
  description = "Endpoint of the production EKS cluster"
}

output "cloudfront_domain" {
  value       = module.cdn.domain_name
  description = "CloudFront distribution domain for OTA downloads"
}

output "database_endpoint" {
  value       = module.database.endpoint
  description = "Aurora PostgreSQL writer endpoint"
}

output "redis_endpoint" {
  value       = module.cache.primary_endpoint_address
  description = "ElastiCache Redis primary endpoint"
}

output "s3_release_bucket" {
  value       = module.storage.bucket_id
  description = "S3 bucket for storing signed installers and releases"
}
