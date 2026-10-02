output "vpc_id" {
  value       = aws_vpc.main.id
  description = "VPC ID"
}

output "public_subnet_ids" {
  value       = aws_subnet.public[*].id
  description = "List of public subnet IDs"
}

output "private_subnet_ids" {
  value       = aws_subnet.private[*].id
  description = "List of private subnet IDs"
}

output "database_subnet_ids" {
  value       = aws_subnet.database[*].id
  description = "List of database subnet IDs"
}

output "db_subnet_group_name" {
  value       = aws_db_subnet_group.rds.name
  description = "Database subnet group name"
}
