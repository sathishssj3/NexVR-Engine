output "alb_security_group_id" {
  value       = aws_security_group.alb.id
  description = "Security group ID of the ALB"
}

output "eks_nodes_security_group_id" {
  value       = aws_security_group.eks_nodes.id
  description = "Security group ID of the EKS worker nodes"
}

output "db_security_group_id" {
  value       = aws_security_group.db.id
  description = "Security group ID of PostgreSQL database"
}

output "redis_security_group_id" {
  value       = aws_security_group.redis.id
  description = "Security group ID of Redis cluster"
}

output "eks_cluster_role_arn" {
  value       = aws_iam_role.eks_cluster.arn
  description = "IAM Role ARN for EKS Cluster"
}

output "eks_nodes_role_arn" {
  value       = aws_iam_role.eks_nodes.arn
  description = "IAM Role ARN for EKS Node Group"
}
