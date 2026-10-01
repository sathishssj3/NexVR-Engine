"""
NexVR Engine - Hybrid Dataset Component C: Hard Negative Mining Generator
Synthesizes assembly traces and memory snapshots of non-player cameras:
- Cascaded Shadow Map (CSM) directional sun frustums
- Planar water / mirror reflection passes
- 2D Minimap and UI orthographic view matrices
- Depth pre-pass / Hierarchical Z-Buffer (HZB) occlusion culling
Teaches the model to explicitly reject fake cameras with surgical reasoning.
"""

import random
from typing import Dict, Any, List

ENGINES = ["ue5", "ue4", "unity", "custom"]

NEGATIVE_TYPES = [
    {
        "type": "REJECTED_SHADOW_CASCADE",
        "desc": "Cascaded Shadow Map (CSM) Orthographic Frustum",
        "reason": "Matrix exhibits orthographic characteristics (m[3][3] == 1.0f, m[2][3] == 0.0f). Light direction vector detected in register; this is a sun shadow cascade pass, NOT the main player camera.",
        "matrix_sig": "m33=1.0f, m23=0.0f (Orthographic Light Projection)",
        "context_asm": "movss xmm2, dword ptr [rcx + 0x18] ; LightDir Z\nandps xmm0, xmm3 ; Frustum clamp\nmovaps dword ptr [rsp + 0x40], xmm0"
    },
    {
        "type": "REJECTED_PLANAR_REFLECTION",
        "desc": "Planar Reflection Pass (Water / Mirror)",
        "reason": "Oblique near-plane projection matrix detected. Virtual camera is inverted across reflection plane (0.0, 0.0, 1.0); rendering target binds offscreen mirror texture rather than the backbuffer.",
        "matrix_sig": "Oblique Near Plane Clip (Reflection Normal Z=1.0)",
        "context_asm": "vmovups ymm0, ymmword ptr [rax + 0x60] ; ReflectionPlane\nvxorps ymm1, ymm1, ymmword ptr [rbx + 0x80] ; Invert Y\nvmovups ymmword ptr [rsp + 0x20], ymm1"
    },
    {
        "type": "REJECTED_MINIMAP_UI",
        "desc": "2D UI / Minimap Projection Matrix",
        "reason": "Orthographic 2D screen-space projection matrix with Z-range [0.0, 1.0] and zero perspective division. Attached to HUD canvas subsystem.",
        "matrix_sig": "2D Ortho (Width=1920, Height=1080, Perspective=Disabled)",
        "context_asm": "mov dword ptr [rax + 0x10], 0x44F00000 ; 1920.0f\nmov dword ptr [rax + 0x14], 0x44870000 ; 1080.0f\nmov dword ptr [rax + 0x3C], 0x3F800000 ; m33 = 1.0f"
    },
    {
        "type": "REJECTED_OCCLUSION_CULLING",
        "desc": "Hierarchical Z-Buffer (HZB) Occlusion Camera",
        "reason": "Downsampled depth culling bounding box calculation. Function belongs to FRenderer::RenderOcclusion; no color swapchain attachment present.",
        "matrix_sig": "Coarse HZB Frustum Box Test",
        "context_asm": "call Engine.dll+0x2419A0 ; RenderOcclusionCull\ntest al, al\njz 0x7FF728A1B0"
    }
]

def generate_hard_negative_sample(sample_id: int) -> Dict[str, Any]:
    engine = random.choice(ENGINES)
    neg = random.choice(NEGATIVE_TYPES)
    
    base_offset = random.randint(0x1100000, 0x4500000)
    module_name = "Engine.dll" if engine.startswith("ue") else ("GameAssembly.dll" if engine == "unity" else "RendererCore.dll")
    base_ptr = f"{module_name}+0x{base_offset:07X}"
    
    pos_off = random.choice([0x20, 0x30, 0x40, 0x58])
    aob_pattern = f"48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? F3 0F 10 {random.randint(0x80, 0x9F):02X} ?? ?? ?? ??"
    
    disassembly = (
        f"0x{base_offset:08X} | 48 8D 0D 10 20 30 00 | lea rcx, [{base_ptr}] ; {neg['desc']}\n"
        f"0x{base_offset+7:08X} | E8 40 50 60 00       | call {module_name}+0x{base_offset+0x1000:07X}\n"
        f"0x{base_offset+12:08X} | {neg['context_asm']}\n"
        f"0x{base_offset+24:08X} | F3 0F 11 81 {pos_off:02X} 00 00 00 | movss dword ptr [rcx + 0x{pos_off:X}], xmm0"
    )
    
    memory_dump = (
        f"Matrix Type: {neg['matrix_sig']}\n"
        f"Target Pass: {neg['desc']}\n"
        f"[rcx + 0x{pos_off:X}] : Candidate coordinates detected (False Positive)"
    )
    
    reasoning = (
        f"1. Frustum Inspection: Examined matrix coefficients at [{base_ptr}].\n"
        f"2. Mathematical Anomaly: {neg['reason']}\n"
        f"3. Decisive Action: This is a secondary rendering pass. Injecting VR tracking matrices into this structure would distort shadows or reflections rather than player eye cameras.\n"
        f"4. Rejection Tag: Emit strict rejection code '{neg['type']}' to prevent erroneous injection."
    )
    
    target_json = {
        "engine": "UnrealEngine5" if engine == "ue5" else ("UnrealEngine4" if engine == "ue4" else ("UnityIL2CPP" if engine == "unity" else "CustomEngine")),
        "camera_type": neg["type"],
        "base_pointer": base_ptr,
        "offsets": [f"0x{pos_off:X}"],
        "aob_pattern": aob_pattern,
        "rejection_reason": neg["reason"],
        "confidence": 0.999
    }
    
    prompt = (
        f"<|engine:{engine}|>\n"
        f"Analyze the following candidate camera routine and matrix memory dump:\n\n"
        f"[Disassembly Trace]\n{disassembly}\n\n"
        f"[Memory Snapshot]\n{memory_dump}\n\n"
        f"Determine whether this is a legitimate player perspective camera or a non-player rendering pass. Provide rejection rationale if invalid."
    )
    
    return {
        "id": f"hard_neg_{sample_id:05d}",
        "category": "hard_negative",
        "engine": engine,
        "prompt": prompt,
        "reasoning": reasoning,
        "target": target_json
    }

def generate_batch(count: int = 2000) -> List[Dict[str, Any]]:
    return [generate_hard_negative_sample(i) for i in range(count)]

if __name__ == "__main__":
    import json
    samples = generate_batch(5)
    print(f"Generated {len(samples)} hard negative samples. Sample 0:")
    print(json.dumps(samples[0], indent=2))
