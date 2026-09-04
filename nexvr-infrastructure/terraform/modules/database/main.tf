variable "environment" {
  type = string
}

variable "db_subnet_group_name" {
  type = string
}

variable "db_security_group_id" {
  type = string
}

variable "database_name" {
  type    = string
  default = "nexvr_db"
}

variable "master_username" {
  type    = string
  default = "nexvr_admin"
}

resource "random_password" "db_password" {
  length           = 32
  special          = true
  override_special = "!#$%&*()-_=+[]{}<>:?"
}

resource "aws_secretsmanager_secret" "db_credentials" {
  name                    = "${var.environment}-nexvr-db-credentials"
  recovery_window_in_days = 0

  tags = {
    Environment = var.environment
  }
}

resource "aws_secretsmanager_secret_version" "db_credentials" {
  secret_id = aws_secretsmanager_secret.db_credentials.id
  secret_string = jsonencode({
    username = var.master_username
    password = random_password.db_password.result
    database = var.database_name
  })
}

resource "aws_rds_cluster" "aurora" {
  cluster_identifier     = "${var.environment}-nexvr-aurora-cluster"
  engine                 = "aurora-postgresql"
  engine_mode            = "provisioned"
  engine_version         = "16.1"
  database_name          = var.database_name
  master_username        = var.master_username
  master_password        = random_password.db_password.result
  db_subnet_group_name   = var.db_subnet_group_name
  vpc_security_group_ids = [var.db_security_group_id]
  storage_encrypted      = true
  deletion_protection    = var.environment == "production" ? true : false

  backup_retention_period = 7
  preferred_backup_window = "02:00-03:00"

  serverlessv2_scaling_configuration {
    min_capacity = 0.5
    max_capacity = var.environment == "production" ? 16.0 : 4.0
  }

  tags = {
    Name        = "${var.environment}-nexvr-aurora"
    Environment = var.environment
  }
}

resource "aws_rds_cluster_instance" "instances" {
  count              = var.environment == "production" ? 2 : 1
  identifier         = "${var.environment}-nexvr-aurora-${count.index}"
  cluster_identifier = aws_rds_cluster.aurora.id
  instance_class     = "db.serverless"
  engine             = aws_rds_cluster.aurora.engine
  engine_version     = aws_rds_cluster.aurora.engine_version

  tags = {
    Name        = "${var.environment}-nexvr-aurora-instance-${count.index}"
    Environment = var.environment
  }
}

output "endpoint" {
  value = aws_rds_cluster.aurora.endpoint
}

output "reader_endpoint" {
  value = aws_rds_cluster.aurora.reader_endpoint
}

output "secret_arn" {
  value = aws_secretsmanager_secret.db_credentials.arn
}
