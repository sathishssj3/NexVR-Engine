output "cluster_name" {
  value       = aws_eks_cluster.main.name
  description = "EKS Cluster Name"
}

output "cluster_endpoint" {
  value       = aws_eks_cluster.main.endpoint
  description = "EKS Cluster API Endpoint"
}

output "cluster_certificate_authority_data" {
  value       = aws_eks_cluster.main.certificate_authority[0].data
  description = "EKS Cluster CA certificate data"
}

output "oidc_provider_arn" {
  value       = aws_iam_openid_connect_provider.eks.arn
  description = "OIDC Provider ARN for IRSA"
}

output "oidc_provider_url" {
  value       = aws_eks_cluster.main.identity[0].oidc[0].issuer
  description = "OIDC Provider URL"
}
