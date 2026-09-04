variable "environment" {
  type        = string
  description = "Deployment environment name"
}

variable "vpc_id" {
  type        = string
  description = "VPC ID where security groups will be created"
}
