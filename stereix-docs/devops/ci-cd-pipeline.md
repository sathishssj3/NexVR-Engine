# NexVR CI/CD & OTA Release Pipeline

## 1. Pipeline Overview

The NexVR platform employs two specialized CI/CD pipelines targeting the dual nature of the monorepo:
1. **Windows Native Engine Pipeline**: Builds MSVC x64 binaries, signs artifacts with EV Authenticode, packages the Electron installer, and uploads to AWS S3 / CloudFront.
2. **Cloud Microservices Pipeline**: Compiles TypeScript, executes tests, builds minimal multi-stage Docker containers, pushes to AWS ECR, and syncs via Argo CD GitOps.

```
                           GitHub Repository
                                  │
                                  ▼
                             CI Pipeline
                                  │
                 ┌────────────────┴────────────────┐
                 ▼                                 ▼
         nexvr-client (Windows)          nexvr-backend (Cloud)
                 │                                 │
                 ▼                                 ▼
           MSVC x64 Build                    Docker Build
         (vrinject.dll + CLI)                      │
                 │                                 ▼
                 ▼                            AWS ECR Push
         Electron Installer                        │
                 │                                 ▼
                 ▼                           Argo CD Sync
           Sign Artifact                           │
          (signtool.exe)                           ▼
                 │                          EKS Production
                 ▼
          Upload to AWS S3
                 │
                 ▼
        AWS CloudFront CDN
                 │
                 ▼
       NexVR Launcher (OTA)
```

---

## 2. Windows Release & OTA Distribution Flow

1. **Tag Trigger**: A developer or maintainer tags a commit with `v*` (e.g. `v0.1.2`).
2. **Build Matrix**: GitHub Actions runs on `windows-latest`:
   - Configures CMake with DirectX, Vulkan, and OpenXR dependencies.
   - Compiles HLSL compute shaders into C++ headers.
   - Builds `vrinject.dll` and `vr-inject-cli.exe`.
   - Embeds the cryptographic SHA256 hash of `vrinject.dll` into the CLI manifest.
3. **Electron Packaging**: Runs `electron-builder` to bundle the native assets into:
   - NSIS Setup Installer (`NexVR Engine Setup X.Y.Z.exe`)
   - Portable Standalone (`NexVR Engine Portable X.Y.Z.exe`)
   - Release Zip (`NexVR Engine-X.Y.Z-win.zip`)
4. **Authenticode Signing**: Signs all executables and DLLs with Digicert timestamping.
5. **CDN Distribution**: Uploads binaries to `s3://production-nexvr-releases/`. CloudFront invalidates edge caches and makes downloads instantly accessible globally.
6. **Launcher Auto-Update**: Users opening the NexVR Launcher receive an instant update notification. If configured with silent OTA, the launcher downloads the delta and updates seamlessly.
