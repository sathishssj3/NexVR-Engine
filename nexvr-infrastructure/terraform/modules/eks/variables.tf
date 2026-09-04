variable "environment" {
  type        = string
  description = "Environment name"
}

variable "kubernetes_version" {
  type        = string
  default     = "1.30"
  description = "EKS Kubernetes version"
}

variable "cluster_role_arn" {
  type        = string
  description = "IAM Role ARN for the EKS Cluster control plane"
}

variable "node_role_arn" {
  type        = string
  description = "IAM Role ARN for the EKS Node Group"
}

variable "subnet_ids" {
  type        = list(string)
  description = "Private Subnet IDs for EKS nodes"
}

variable "nodes_security_group_id" {
  type        = string
  description = "Security Group ID for EKS nodes"
}

variable "desired_capacity" {
  type        = number
  default     = 2
}

variable "min_capacity" {
  type        = number
  default     = 2
}

variable "max_capacity" {
  type        = number
  default     = 10
}

variable "instance_types" {
  type        = list(string)
  default     = ["m6i.large", "t3.xlarge"]
}
