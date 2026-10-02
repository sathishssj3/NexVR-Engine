terraform {
  required_version = ">= 1.6.0"

  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.40"
    }
    random = {
      source  = "hashicorp/random"
      version = "~> 3.6"
    }
    tls = {
      source  = "hashicorp/tls"
      version = "~> 4.0"
    }
  }

  backend "s3" {
    bucket         = "nexvr-terraform-state-prod"
    key            = "environments/production/terraform.tfstate"
    region         = "us-east-1"
    encrypt        = true
    dynamodb_table = "nexvr-terraform-locks-prod"
  }
}

provider "aws" {
  region = var.aws_region

  default_tags {
    tags = {
      Project     = "NexVR"
      Environment = "production"
      ManagedBy   = "Terraform"
    }
  }
}

# WAF Module
module "waf" {
  source      = "../../modules/waf"
  environment = "production"
}

# Storage Module (S3)
module "storage" {
  source      = "../../modules/storage"
  environment = "production"
}

# CDN Module (CloudFront)
module "cdn" {
  source                         = "../../modules/cdn"
  environment                    = "production"
  s3_bucket_id                   = module.storage.bucket_id
  s3_bucket_arn                  = module.storage.bucket_arn
  s3_bucket_regional_domain_name = module.storage.bucket_regional_domain_name
  web_acl_arn                    = module.waf.web_acl_arn
}

# VPC Module
module "vpc" {
  source      = "../../modules/vpc"
  environment = "production"
  vpc_cidr    = "10.100.0.0/16"
}

# Security Module
module "security" {
  source      = "../../modules/security"
  environment = "production"
  vpc_id      = module.vpc.vpc_id
}

# ECR Repositories Module
module "ecr" {
  source      = "../../modules/ecr"
  environment = "production"
}

# Database Module (Aurora Postgres Serverless v2)
module "database" {
  source               = "../../modules/database"
  environment          = "production"
  db_subnet_group_name = module.vpc.db_subnet_group_name
  db_security_group_id = module.security.db_security_group_id
}

# Cache Module (ElastiCache Redis)
module "cache" {
  source            = "../../modules/cache"
  environment       = "production"
  subnet_ids        = module.vpc.database_subnet_ids
  security_group_id = module.security.redis_security_group_id
}

# EKS Cluster Module
module "eks" {
  source                  = "../../modules/eks"
  environment             = "production"
  kubernetes_version      = "1.30"
  cluster_role_arn        = module.security.eks_cluster_role_arn
  node_role_arn           = module.security.eks_nodes_role_arn
  subnet_ids              = module.vpc.private_subnet_ids
  nodes_security_group_id = module.security.eks_nodes_security_group_id
  desired_capacity        = 3
  min_capacity            = 2
  max_capacity            = 12
  instance_types          = ["m6i.xlarge", "m6a.xlarge"]
}
