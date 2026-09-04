# The Unified Team Onboarding & Project Building Playbook
### A Standardized Guide for Collaborative, AI-Native Systems Development

---

## Introduction

This playbook establishes a unified software development lifecycle (SDLC) for our engineering team working on the **NexVR Engine**. It merges disciplined software engineering principles with the **PSB (Plan, Setup, Build)** system for AI-native development.

---

## Phase 1: Planning and Scoping

### 1. Define the Goal
Our goal is to create a seamless, high-performance universal VR injector that allows offline games to run in stereo VR using OpenXR and ML-driven depth generation.

### 2. Establish a Ruthless MVP
The Minimum Viable Product must focus strictly on successful DLL injection, intercepting the graphics swapchain (DXGI/D3D11), and forwarding frames to OpenXR. Advanced ML rendering and fancy UI features belong in Version 2.

### 3. AI-Assisted Planning
Use AI to challenge architectural assumptions, particularly around memory safety, threading (render thread vs. worker pools), and graphics API compatibility.

---

## Phase 2: Tech Stack Selection & Technical Architecture

### 1. Rational Tech Stack Selection
*   **C++ & CMake:** The de-facto standard for system-level hooking and high-performance graphics.
*   **OpenXR & Vulkan / DirectX:** Industry-standard graphics and VR APIs.
*   **Electron & React:** Chosen for the Launcher to allow rapid GUI development without the overhead of C++ UI frameworks like Qt.

### 2. Infrastructure
*   Developers must install MSVC (VS 2022+), Vulkan SDK, and Node.js.
*   CMake's `FetchContent` is utilized to pull third-party dependencies (MinHook, ONNX, DirectML, GTest) automatically, reducing manual environment setup.

---

## Phase 3: Directory Structure & Version Control

### 1. Standardized Folder Structures
*   **Clean Root Policy:** Keep the root clean. Subsystems are split naturally: `src/` for C++, `launcher/` for Node.js, `tests/` for GTest.
*   **Proxy DLL Separation:** Proxy DLLs (`dxgi.dll`) are placed in `build/bin/proxy/` to prevent Windows from shadowing system DLLs during CLI execution.

### 2. Atomic Git Commits
Commit early, often, and atomically. Separate hook logic changes from launcher UI updates.

---

## Phase 4: Setting Up the AI-Native Environment

### 1. Memory Configuration (`claude.md`)
The `claude.md` file serves as the system prompt and memory for AI sessions. It links to `docs/project_memory.md` which contains critical audit resolutions (e.g., BUG-06, DEAD-05) that AI and humans must strictly adhere to.

### 2. Automated System Documentation
Keep `architecture.md`, `changelog.md`, and `project-status.md` strictly updated.

---

## Phase 5: The Build Phase and Collaboration

### 1. The Dual-Build Environment
Be aware that this project has two distinct build environments:
1.  **Backend (C++ Engine):** Requires `cmake -B build -S . -A x64` and `cmake --build build --config Release`.
2.  **Frontend (Electron Launcher):** Requires `npm install`, `npm run dev`, and `npm run pack` inside the `launcher/` directory.

### 2. Issue-Based Development
Use GitHub Issues to track capability milestones (e.g., "Add D3D12 Hooking"). Create feature branches like `feature/d3d12-hook` and merge via Pull Request.

---

## Phase 6: Code Quality, Testing, and Governance

### 1. Automated Testing
*   **C++ Tests:** Run `ctest` to ensure no capability regressions occur.
*   **Launcher Tests:** `npm run test:static` runs Playwright structural tests.

### 2. Self-Correction (Project Memory)
If you encounter a new deadlock or anti-cheat conflict, document it in `docs/project_memory.md` immediately with a designated audit tag (e.g., DEAD-XX, QUAL-XX). Never suppress errors silently.
