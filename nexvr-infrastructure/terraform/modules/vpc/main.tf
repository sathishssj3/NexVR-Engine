resource "aws_vpc" "main" {
  cidr_block           = var.vpc_cidr
  enable_dns_hostnames = true
  enable_dns_support   = true

  tags = {
    Name        = "${var.environment}-nexvr-vpc"
    Environment = var.environment
    ManagedBy   = "Terraform"
    "kubernetes.io/cluster/${var.environment}-nexvr-eks" = "shared"
  }
}

resource "aws_internet_gateway" "igw" {
  vpc_id = aws_vpc.main.id

  tags = {
    Name        = "${var.environment}-nexvr-igw"
    Environment = var.environment
  }
}

# Public Subnets (NAT Gateway, ALBs)
resource "aws_subnet" "public" {
  count                   = length(var.public_subnet_cidrs)
  vpc_id                  = aws_vpc.main.id
  cidr_block              = var.public_subnet_cidrs[count.index]
  availability_zone       = var.availability_zones[count.index]
  map_public_ip_on_launch = true

  tags = {
    Name                                                 = "${var.environment}-nexvr-public-${var.availability_zones[count.index]}"
    Environment                                          = var.environment
    "kubernetes.io/role/elb"                             = "1"
    "kubernetes.io/cluster/${var.environment}-nexvr-eks" = "shared"
  }
}

# Private Subnets (EKS Worker Nodes, Internal Services)
resource "aws_subnet" "private" {
  count             = length(var.private_subnet_cidrs)
  vpc_id            = aws_vpc.main.id
  cidr_block        = var.private_subnet_cidrs[count.index]
  availability_zone = var.availability_zones[count.index]

  tags = {
    Name                                                 = "${var.environment}-nexvr-private-${var.availability_zones[count.index]}"
    Environment                                          = var.environment
    "kubernetes.io/role/internal-elb"                    = "1"
    "kubernetes.io/cluster/${var.environment}-nexvr-eks" = "shared"
  }
}

# Database Subnets (RDS PostgreSQL, ElastiCache Redis)
resource "aws_subnet" "database" {
  count             = length(var.database_subnet_cidrs)
  vpc_id            = aws_vpc.main.id
  cidr_block        = var.database_subnet_cidrs[count.index]
  availability_zone = var.availability_zones[count.index]

  tags = {
    Name        = "${var.environment}-nexvr-db-${var.availability_zones[count.index]}"
    Environment = var.environment
  }
}

# Elastic IP for NAT Gateway
resource "aws_eip" "nat" {
  domain = "vpc"

  tags = {
    Name        = "${var.environment}-nexvr-nat-eip"
    Environment = var.environment
  }
}

# Single NAT Gateway for cost-efficiency (can be multi-NAT in high-availability setup)
resource "aws_nat_gateway" "nat" {
  allocation_id = aws_eip.nat.id
  subnet_id     = aws_subnet.public[0].id

  tags = {
    Name        = "${var.environment}-nexvr-nat"
    Environment = var.environment
  }

  depends_on = [aws_internet_gateway.igw]
}

# Public Route Table
resource "aws_route_table" "public" {
  vpc_id = aws_vpc.main.id

  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.igw.id
  }

  tags = {
    Name        = "${var.environment}-nexvr-public-rt"
    Environment = var.environment
  }
}

resource "aws_route_table_association" "public" {
  count          = length(aws_subnet.public)
  subnet_id      = aws_subnet.public[count.index].id
  route_table_id = aws_route_table.public.id
}

# Private Route Table
resource "aws_route_table" "private" {
  vpc_id = aws_vpc.main.id

  route {
    cidr_block     = "0.0.0.0/0"
    nat_gateway_id = aws_nat_gateway.nat.id
  }

  tags = {
    Name        = "${var.environment}-nexvr-private-rt"
    Environment = var.environment
  }
}

resource "aws_route_table_association" "private" {
  count          = length(aws_subnet.private)
  subnet_id      = aws_subnet.private[count.index].id
  route_table_id = aws_route_table.private.id
}

# Database Subnet Group
resource "aws_db_subnet_group" "rds" {
  name       = "${var.environment}-nexvr-db-subnet-group"
  subnet_ids = aws_subnet.database[*].id

  tags = {
    Name        = "${var.environment}-nexvr-db-subnet-group"
    Environment = var.environment
  }
}
