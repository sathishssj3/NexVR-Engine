# NexVR System Overview & Monorepo Architecture

## 1. Executive Summary

NexVR is an enterprise-grade Universal VR Injection platform enabling flat-screen PC titles (e.g. Sekiro: Shadows Die Twice, Elden Ring, Cyberpunk 2077) to be experienced in full 6DOF Virtual Reality via OpenXR.

The project is structured as an **Enterprise Monorepo** composed of five decoupled, specialized subsystems:

```
NexVR-Engine/
├── nexvr-client/         # 100% Native Windows C++ VR Engine & Electron UI Launcher
├── nexvr-backend/        # Node.js/TypeScript Microservices API Gateway (EKS Cloud)
├── nexvr-infrastructure/ # AWS Multi-AZ Cloud Infrastructure (Terraform)
├── nexvr-deployment/     # Kubernetes Helm Charts, Argo CD GitOps, Prometheus & Grafana
└── nexvr-docs/           # Enterprise Architecture, OpenAPI Specs, and DevOps Playbooks
```

---

## 2. The Golden Architectural Boundary: Native Client vs. Cloud Services

A critical architectural mandate of NexVR is the **strict separation between real-time local VR computation and cloud services**:

```
┌─────────────────────────────────────────────────────────────┐
│                 USER'S LOCAL WINDOWS PC                     │
│                                                             │
│   Target Game (Sekiro) ──▶ DirectX 11/12 / Vulkan Hooks     │
│                                   │                         │
│                                   ▼                         │
│                           vrinject.dll                      │
│                                   │                         │
│          ┌────────────────────────┴────────────────┐        │
│          ▼                                         ▼        │
│   Stereo Reprojection                       DirectML / ONNX │
│   & Camera Extraction                       Depth Predictor │
│          │                                         │        │
│          └────────────────────────┬────────────────┘        │
│                                   ▼                         │
│                        OpenXR Frame Submitter               │
│                                   │                         │
│                                   ▼                         │
│                          VR Headset (Meta/Pico)             │
│                                                             │
│   NexVR Electron Launcher ◀─────── Local Configs (Offline)  │
└─────────────────────────┬───────────────────▲───────────────┘
                          │ HTTPS             │ Signed OTA Updates,
                          │ Telemetry/Auth    │ Profiles & Releases
                          ▼                   │
┌─────────────────────────────────────────────┴───────────────┐
│                    AWS CLOUD PLATFORM                       │
│                                                             │
│   CloudFront CDN (Fast Global Downloads) ──▶ S3 (Releases)  │
│                                                             │
│   WAF ──▶ AWS ALB ──▶ EKS API Gateway (nexvr-backend)       │
│                                                             │
│         ┌──────────────────────┬──────────────────┐         │
│         ▼                      ▼                  ▼         │
│    Aurora Postgres      ElastiCache Redis    Telemetry/Logs │
└─────────────────────────────────────────────────────────────┘
```

### Zero-Collapse Guarantee
1. **Offline Autonomy**: If the user's internet is disconnected or AWS servers are down, `nexvr-client` continues to function 100% standalone. Injection, stereo rendering, profile loading, and headset display do NOT require cloud connectivity.
2. **GPU Performance Isolation**: All frame timing, vertex/pixel shader interception, and DirectML neural inference execute strictly on the client's local GPU within an 11.1ms (90 Hz) frame budget. Cloud microservices handle metadata, authentication, analytics, and OTA package delivery.

---

## 3. Subsystem Breakdown

### 3.1 `nexvr-client`
- **Language**: C++20 (MSVC x64) and TypeScript (Electron/React).
- **Core Targets**:
  - `vrinject.dll`: Runtime injection payload loaded into target game process.
  - `vr-inject-cli.exe`: Elevated helper executing memory scanning and DLL injection.
  - `launcher`: Modern desktop interface for one-click game launch, profile switching, and OTA updates.
- **Key Modules**:
  - `src/hooks/`: Direct3D 11, Direct3D 12, Vulkan, and Windows input hooking via MinHook.
  - `src/openxr/`: Head tracking, swapchain creation, and pose synchronization.
  - `src/rendering/`: Stereo reprojection compute shaders, ASW frame generation, comfort guards.
  - `src/ai/`: DirectML / ONNX Runtime neural inpainting and depth extraction.

### 3.2 `nexvr-backend`
- **Language**: Node.js 20, TypeScript, Prisma ORM.
- **Architecture**: Modular microservices behind an API Gateway:
  - `auth`: JWT token authentication & role authorization.
  - `user`: Account profiles and licensing tiers.
  - `game`: Game compatibility detection engine (`/detect?exe=sekiro.exe`).
  - `profile`: Community-shared VR configurations (FOV, eye separation).
  - `update`: Signed binary release distribution and OTA update checker.
  - `telemetry`: Crash report ingestion and anonymized GPU telemetry.

### 3.3 `nexvr-infrastructure`
- **Tooling**: Terraform (HashiCorp AWS Provider ~> 5.40).
- **Architecture**: Multi-AZ VPC across 3 Availability Zones:
  - High-availability EKS cluster with managed node groups and autoscaling.
  - Aurora PostgreSQL Serverless v2 with automated backups.
  - Multi-AZ ElastiCache Redis replication group.
  - S3 bucket for releases protected by KMS encryption and CloudFront Origin Access Control (OAC).
  - AWS WAF with rate limiting and AWS Managed Common Rule Set.

### 3.4 `nexvr-deployment`
- **Tooling**: Kubernetes 1.30, Helm 3, Argo CD, Prometheus, Grafana, OpenTelemetry.
- **Architecture**:
  - Declarative GitOps via Argo CD (`app-of-apps` pattern).
  - Production Helm chart with Horizontal Pod Autoscaling (HPA) and rolling updates.
  - Prometheus alerts for HTTP 5xx rate > 5%, latency p99 > 500ms, and pod crashes.

### 3.5 `nexvr-docs`
- Comprehensive architecture manuals, OpenAPI 3.0 documentation, and DevOps playbooks.
