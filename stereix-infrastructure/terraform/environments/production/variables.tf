variable "aws_region" {
  type        = string
  default     = "us-east-1"
  description = "AWS deployment region"
}

variable "domain_name" {
  type        = string
  default     = "nexvr.dev"
  description = "Apex domain name for NexVR services"
}
