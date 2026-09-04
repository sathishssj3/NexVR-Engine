# Subsystem: Heuristics & Matrix Discovery (`src/heuristics/`)

The **Heuristics** subsystem automatically locates, classifies, and tracks camera projection and view matrices within the game process's dynamic memory heaps.

---

## Core Pipeline

```text
Game RAM Heaps (VirtualQuery)
             │
             ▼
┌────────────────────────────────────────────────────────┐
│  MatrixClassifier (matrix_classifier.h/.cpp)           │
│  Validates 4x4 matrix candidates:                      │
│  - Perspective projection bounds: [m00, m11, m22, m23] │
│  - Near/far plane sanity (0.01m to 10000m)             │
│  - Finite floating-point checks (NaN/Inf rejection)    │
└──────────────────────────┬─────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│  CameraDeltaTracker (memory_scanner/)                  │
│  Correlates matrix candidates with mouse / head deltas │
└──────────────────────────┬─────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│  DepthLockManager (depth_lock_manager.h/.cpp)          │
│  Discovers and ranks candidate depth-stencil textures  │
└────────────────────────────────────────────────────────┘
```

---

## Key Files

| File | Purpose |
| :--- | :--- |
| **`matrix_classifier.h/.cpp`** | Mathematical heuristic filtering for 4x4 projection, view, and view-projection matrices. |
| **`camera_classifier.h/.cpp`** | Classifies active game camera states (cinematic, gameplay, 2D UI). |
| **`depth_lock_manager.h/.cpp`** | Discovers valid depth buffers by inspecting dimensions, aspect ratios, and format families. |
| **`render_frame_snapshot.h`** | Thread-safe data structure capturing the state of an individual render frame. |
