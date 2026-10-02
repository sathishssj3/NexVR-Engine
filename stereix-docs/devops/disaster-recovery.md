# NexVR Disaster Recovery & Business Continuity Plan

## 1. Recovery Objectives

- **Recovery Point Objective (RPO)**: < 1 hour (Continuous Aurora PostgreSQL automated backups + S3 versioning).
- **Recovery Time Objective (RTO)**: < 30 minutes (Declarative Terraform & GitOps Argo CD reconstitution).

---

## 2. Component Recovery Procedures

### 2.1 Aurora PostgreSQL Database
- **Automated Snapshots**: Aurora takes continuous backups retained for 7 days.
- **Point-in-Time Recovery (PITR)**:
  ```bash
  aws rds restore-db-cluster-to-point-in-time \
    --source-db-cluster-identifier production-nexvr-aurora-cluster \
    --target-db-cluster-identifier production-nexvr-aurora-restored \
    --restore-time "2026-09-04T12:00:00Z"
  ```
- **Update Connection Secrets**:
  Update AWS Secrets Manager secret `production-nexvr-db-credentials` with the restored cluster endpoint. EKS pods will automatically re-establish connection pool.

### 2.2 S3 Release Artifacts & OTA Distribution
- **Versioning Protection**: `aws_s3_bucket_versioning` is enabled on the release bucket. Accidental deletions produce delete markers that can be rolled back instantly without data loss.
- **Cross-Region Replication (Optional)**: If multi-region redundancy is required, enable S3 replication to a secondary region (e.g. `eu-central-1`).

### 2.3 Kubernetes EKS Cluster Failure
In the event of total cluster compromise or AZ failure:
1. Re-apply Terraform:
   ```bash
   cd nexvr-infrastructure/terraform/environments/production
   terraform apply -auto-approve
   ```
2. Re-apply GitOps Root:
   ```bash
   kubectl apply -f nexvr-deployment/argocd/app-of-apps.yaml
   ```
   Argo CD will automatically reconstitute all deployments, services, ingress rules, and autoscaling policies to the exact desired Git state.
