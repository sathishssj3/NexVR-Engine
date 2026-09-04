variable "environment" {
  type = string
}

variable "repository_names" {
  type    = list(string)
  default = ["nexvr-backend", "nexvr-gateway"]
}

resource "aws_ecr_repository" "repos" {
  for_each             = toset(var.repository_names)
  name                 = "${var.environment}-${each.key}"
  image_tag_mutability = "MUTABLE"

  image_scanning_configuration {
    scan_on_push = true
  }

  encryption_configuration {
    encryption_type = "KMS"
  }

  tags = {
    Name        = "${var.environment}-${each.key}"
    Environment = var.environment
  }
}

resource "aws_ecr_lifecycle_policy" "repos_policy" {
  for_each   = aws_ecr_repository.repos
  repository = each.value.name

  policy = jsonencode({
    rules = [
      {
        rulePriority = 1
        description  = "Keep last 30 tagged production images"
        selection = {
          tagStatus   = "any"
          countType   = "imageCountMoreThan"
          countNumber = 30
        }
        action = {
          type = "expire"
        }
      }
    ]
  })
}

output "repository_urls" {
  value = { for k, v in aws_ecr_repository.repos : k => v.repository_url }
}
