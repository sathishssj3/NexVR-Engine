# NexVR Cloud Platform Architecture

## 1. Cloud Services Overview

NexVR cloud services (`nexvr-backend`) provide the backend platform for:
1. **User Authentication & Profiles**: Secure JWT management and licensing tiers.
2. **Game Compatibility Database**: Dynamic profile distribution for thousands of titles.
3. **Signed OTA Binary Distribution**: High-speed, globally cached downloads via CloudFront CDN.
4. **Telemetry & Crash Diagnostics**: Anonymized GPU metrics and stack trace ingestion.

```
                      INTERNET (HTTPS)
                             │
                             ▼
                    ┌─────────────────┐
                    │     AWS WAF     │ (DDoS, SQLi, Rate Limiting)
                    └────────┬────────┘
                             │
            ┌────────────────┴────────────────┐
            ▼                                 ▼
   ┌─────────────────┐               ┌─────────────────┐
   │ CloudFront CDN  │ (Fast Binary  │  AWS App Load   │ (API Requests)
   │  Distribution   │  Downloads)   │    Balancer     │
   └────────┬────────┘               └────────┬────────┘
            │ OAC                             │
            ▼                                 ▼
   ┌─────────────────┐               ┌─────────────────┐
   │ S3 Release      │               │ EKS Kubernetes  │
   │ Bucket (KMS)    │               │ (nexvr-backend) │
   └─────────────────┘               └────────┬────────┘
                                              │
                         ┌────────────────────┴────────────────────┐
                         ▼                                         ▼
                ┌─────────────────┐                       ┌─────────────────┐
                │ Aurora Postgres │                       │ ElastiCache     │
                │  Serverless v2  │                       │ Redis Cache     │
                └─────────────────┘                       └─────────────────┘
```

---

## 2. Infrastructure Resilience & Zero Downtime

- **EKS Managed Node Groups**: Deployed across 3 Availability Zones (`us-east-1a`, `us-east-1b`, `us-east-1c`) with Horizontal Pod Autoscaling (HPA) and Cluster Autoscaler.
- **Aurora PostgreSQL**: Serverless v2 auto-scaling from 0.5 to 16 ACUs with automated failover instances and 7-day continuous backup retention.
- **ElastiCache Redis**: Multi-AZ cluster with in-transit and at-rest encryption for game and profile metadata caching.
- **CloudFront Origin Access Control (OAC)**: Releases stored in private S3 buckets are accessible exclusively via CloudFront edge locations, guaranteeing low latency for global user bases.
