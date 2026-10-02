# NexVR Enterprise Deployment Guide

## 1. Prerequisites

Ensure you have the following installed on your administrative workstation or deployment bastion:
- **AWS CLI** (>= 2.15) configured with Administrator permissions.
- **Terraform** (>= 1.6.0).
- **kubectl** (matching EKS cluster version 1.30).
- **Helm** (v3.14+).
- **Node.js** (v20+ LTS) and **Docker** for local container builds.

---

## 2. Infrastructure Deployment (Terraform)

### Step 1: Initialize Terraform Backend
Navigate to the production environment directory:
```bash
cd nexvr-infrastructure/terraform/environments/production
```

Create `terraform.tfvars`:
```hcl
aws_region  = "us-east-1"
domain_name = "nexvr.dev"
```

Initialize modules and remote S3 state:
```bash
terraform init
```

### Step 2: Validate & Plan
```bash
terraform plan -out=tfplan
```

### Step 3: Apply Infrastructure
```bash
terraform apply tfplan
```

Once complete, note the outputs:
- `eks_cluster_name`
- `cloudfront_domain`
- `database_endpoint`
- `redis_endpoint`

---

## 3. Kubernetes & EKS Setup

### Step 1: Update Local Kubeconfig
```bash
aws eks update-kubeconfig --region us-east-1 --name production-nexvr-eks
```

Verify connection to nodes:
```bash
kubectl get nodes
```

### Step 2: Install AWS Load Balancer Controller
```bash
helm repo add eks https://aws.github.io/eks-charts
helm repo update

helm install aws-load-balancer-controller eks/aws-load-balancer-controller \
  -n kube-system \
  --set clusterName=production-nexvr-eks \
  --set serviceAccount.create=true
```

---

## 4. Application Deployment (Helm / GitOps)

### Option A: Direct Helm Release
```bash
helm upgrade --install nexvr-api nexvr-deployment/helm/nexvr-services \
  --namespace production \
  --create-namespace \
  --values nexvr-deployment/helm/nexvr-services/values.yaml
```

### Option B: GitOps with Argo CD (Recommended)
1. Install Argo CD:
```bash
kubectl create namespace argocd
kubectl apply -n argocd -f https://raw.githubusercontent.com/argoproj/argo-cd/stable/manifests/install.yaml
```

2. Apply Root Application:
```bash
kubectl apply -f nexvr-deployment/argocd/app-of-apps.yaml
```

Argo CD will automatically detect changes to `nexvr-deployment/helm/nexvr-services` on `main` branch and execute automated rolling deployments.
