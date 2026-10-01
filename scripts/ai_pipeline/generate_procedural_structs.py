"""
NexVR Engine - Hybrid Dataset Component A: Procedural C++ Struct Generator
Generates realistic C++ camera, view, and transform structures with disassembly traces,
register dataflow, PDB-like ground truth offsets, and resilient AOB signatures.
"""

import random
from typing import Dict, Any, List, Tuple

ENGINES = ["ue5", "ue4", "unity", "custom"]

STRUCT_NAMES = [
    "FMinimalViewInfo", "FCameraCacheEntry", "FPostProcessSettings", "UCameraComponent",
    "APlayerCameraManager", "FSceneView", "FViewMatrices", "Camera_o", "Transform_o",
    "PlayerCamera_t", "ViewExtensionContext", "RenderCameraRig", "CThirdPersonCamera",
    "CFPSCameraData", "EngineCameraMatrix", "FreeCamController", "SpectatorPawnCamera"
]

MODULE_NAMES = [
    "GameAssembly.dll", "Engine.dll", "UnityPlayer.dll", "Client-Win64-Shipping.exe",
    "UE4Editor-Engine.dll", "Game-Win64-Shipping.exe", "CrySystem.dll", "RenderingCore.dll"
]

REGISTERS_64 = ["rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"]
REGISTERS_SIMD = ["xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"]

def generate_procedural_struct(sample_id: int) -> Dict[str, Any]:
    engine = random.choice(ENGINES)
    struct_name = random.choice(STRUCT_NAMES)
    module_name = random.choice(MODULE_NAMES)
    
    # Generate realistic base offset
    base_rva = random.randint(0x100000, 0x5FFFFFF)
    base_ptr_str = f"{module_name}+0x{base_rva:07X}"
    
    # Generate struct members and offsets
    pos_offset = random.choice([0x20, 0x30, 0x40, 0x58, 0x80, 0x90, 0xA0, 0x120, 0x180, 0x220, 0x280])
    rot_offset = pos_offset + random.choice([0x0C, 0x10, 0x18, 0x20])
    fov_offset = rot_offset + random.choice([0x0C, 0x10, 0x14, 0x24])
    matrix_offset = fov_offset + random.choice([0x10, 0x20, 0x30, 0x40])
    
    # Pointer chain levels (1 to 3)
    num_chain_levels = random.choice([1, 2, 3])
    offsets = []
    curr_off = random.choice([0x18, 0x20, 0x30, 0x48, 0x60])
    offsets.append(f"0x{curr_off:X}")
    for _ in range(num_chain_levels - 1):
        curr_off = random.choice([0x10, 0x28, 0x80, 0x120, 0x280, 0x310])
        offsets.append(f"0x{curr_off:X}")
    offsets.append(f"0x{pos_offset:X}")
    
    # Generate realistic vector coordinates & FOV
    pos_x = round(random.uniform(-1000.0, 1000.0), 3)
    pos_y = round(random.uniform(-1000.0, 1000.0), 3)
    pos_z = round(random.uniform(0.0, 200.0), 3)
    
    pitch = round(random.uniform(-45.0, 45.0), 2)
    yaw = round(random.uniform(-180.0, 180.0), 2)
    roll = round(random.uniform(-5.0, 5.0), 2)
    
    fov_deg = random.choice([80.0, 90.0, 100.0, 103.0, 110.0])
    fov_rad = round(fov_deg * 3.1415926535 / 180.0, 4)
    
    # Generate Disassembly Trace
    r_base = random.choice(["rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9"])
    r_cam = random.choice([r for r in ["rbx", "r12", "r13", "r14", "r15", "rax"] if r != r_base])
    r_simd = random.choice(REGISTERS_SIMD)
    
    # Construct realistic AOB byte sequence
    # e.g., 48 8B 05 [rel32] 48 8B 88 [offset] F3 0F 10 [offset]
    op1_prefix = "48 8B"
    op1_reg = f"{random.randint(0x05, 0x0D):02X}"
    op2 = f"48 8B {random.randint(0x40, 0x90):02X} {random.choice([0x20, 0x30, 0x40]):02X}"
    op3 = f"F3 0F 10 {random.randint(0x80, 0x9F):02X}"
    
    aob_pattern = f"{op1_prefix} {op1_reg} ?? ?? ?? ?? {op2} {op3} ?? ?? ?? ??"
    
    asm_lines = [
        f"0x{base_rva:08X} | 48 8B {op1_reg} 12 34 56 78   | mov {r_base}, qword ptr [{module_name}+{base_rva:07X}]",
        f"0x{base_rva+7:08X} | 48 85 {r_base[:2]} {r_base[:2]}           | test {r_base}, {r_base}",
        f"0x{base_rva+10:08X} | 74 2E                     | jz 0x{base_rva+0x40:08X}",
        f"0x{base_rva+12:08X} | {op2}               | mov {r_cam}, qword ptr [{r_base} + {offsets[0]}]",
        f"0x{base_rva+17:08X} | {op3} 40 00 00 00   | movss {r_simd}, dword ptr [{r_cam} + 0x{pos_offset:X}]",
        f"0x{base_rva+25:08X} | 0F 28 C1                  | movaps xmm0, {r_simd}",
        f"0x{base_rva+28:08X} | F3 0F 10 44 24 10         | movss xmm0, dword ptr [rsp + 0x10]",
        f"0x{base_rva+34:08X} | F3 0F 10 8B {fov_offset & 0xFF:02X} {(fov_offset >> 8) & 0xFF:02X} 00 00 | movss xmm1, dword ptr [{r_cam} + 0x{fov_offset:X}] ; FOV ({fov_deg} deg)"
    ]
    disassembly_trace = "\n".join(asm_lines)
    
    memory_dump = (
        f"[{r_cam} + 0x{pos_offset:X}] : {pos_x:.4f}f, {pos_y:.4f}f, {pos_z:.4f}f (Position XYZ)\n"
        f"[{r_cam} + 0x{rot_offset:X}] : {pitch:.2f}f, {yaw:.2f}f, {roll:.2f}f (Pitch, Yaw, Roll)\n"
        f"[{r_cam} + 0x{fov_offset:X}] : {fov_rad:.4f}f (FOV in Radians = {fov_deg} deg)\n"
        f"[{r_cam} + 0x{matrix_offset:X}] : 4x4 ViewProjection Matrix (Perspective: m33=0.0f, m32=-1.0f)"
    )
    
    reasoning = (
        f"1. Opcode analysis: Register {r_base.upper()} is loaded from module base [{base_ptr_str}] with relative address.\n"
        f"2. Pointer chain dereference: Following pointer [{r_base.upper()} + {offsets[0]}] leads to {struct_name} at heap instance {r_cam.upper()}.\n"
        f"3. Coordinate verification: At offset +0x{pos_offset:X}, found valid 3D translation vector (X={pos_x}, Y={pos_y}, Z={pos_z}).\n"
        f"4. Angle verification: Offset +0x{rot_offset:X} holds rotation Euler angles; offset +0x{fov_offset:X} holds float {fov_rad} rad (~{fov_deg} deg), confirming Main Player Camera.\n"
        f"5. AOB Synthesis: Wildcard relative 32-bit module offset and dynamic stack/heap displacement bytes to ensure signature resilience across compiler builds.\n"
        f"Signature: {aob_pattern}"
    )
    
    target_json = {
        "engine": "UnrealEngine5" if engine == "ue5" else ("UnrealEngine4" if engine == "ue4" else ("UnityIL2CPP" if engine == "unity" else "CustomEngine")),
        "camera_type": "MainPlayerCamera",
        "struct_name": struct_name,
        "base_pointer": base_ptr_str,
        "offsets": offsets,
        "aob_pattern": aob_pattern,
        "field_offsets": {
            "position": f"0x{pos_offset:X}",
            "rotation": f"0x{rot_offset:X}",
            "fov": f"0x{fov_offset:X}",
            "matrix": f"0x{matrix_offset:X}"
        },
        "confidence": 0.998
    }
    
    prompt = (
        f"<|engine:{engine}|>\n"
        f"Analyze the following x86_64 disassembly trace and memory snippet captured at a memory access breakpoint:\n\n"
        f"[Disassembly Trace]\n{disassembly_trace}\n\n"
        f"[Memory Snapshot]\n{memory_dump}\n\n"
        f"Determine the camera structure, base pointer, field offsets, and a resilient AOB signature."
    )
    
    return {
        "id": f"struct_gen_{sample_id:05d}",
        "category": "procedural_struct",
        "engine": engine,
        "prompt": prompt,
        "reasoning": reasoning,
        "target": target_json
    }

def generate_batch(count: int = 3500) -> List[Dict[str, Any]]:
    return [generate_procedural_struct(i) for i in range(count)]

if __name__ == "__main__":
    import json
    samples = generate_batch(5)
    print(f"Generated {len(samples)} samples. Sample 0:")
    print(json.dumps(samples[0], indent=2))
