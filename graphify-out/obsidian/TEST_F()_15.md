---
source_file: "tests/test_dx12_stress.cpp"
type: "code"
community: "Core Engine Scanners Unreal Scanner..."
location: "L77"
tags:
  - graphify/code
  - graphify/INFERRED
  - community/Core_Engine_Scanners_Unreal_Scanner
---

# TEST_F()

## Connections
- [[dot-GetFenceManager()]] - `calls` [INFERRED]
- [[DX12StressTest]] - `references` [EXTRACTED]
- [[Dx12LifecycleManager]] - `references` [EXTRACTED]
- [[GetState_3]] - `calls` [INFERRED]
- [[Initialize_36]] - `calls` [INFERRED]
- [[Render100kFramesWithoutLeaks]] - `references` [EXTRACTED]
- [[RenderStereo_4]] - `calls` [INFERRED]
- [[Shutdown_27]] - `calls` [INFERRED]
- [[test_dx12_stress.cpp]] - `contains` [EXTRACTED]

#graphify/code #graphify/INFERRED #community/Core_Engine_Scanners_Unreal_Scanner