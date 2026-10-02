"""
Stereix Engine - Hybrid Dataset Component B: Pointer Chain Generator
Models real-world multi-level pointer chains from commercial game binaries and Cheat Engine tables.
Covers Unreal Engine (UWorld -> GameInstance -> PlayerController -> CameraManager),
Unity IL2CPP (GameAssembly.dll static class -> TypeInfo -> Transform), and Custom Engines.
"""

import random
from typing import Dict, Any, List

ENGINES = ["ue5", "ue4", "unity", "custom"]

ENGINE_CHAINS = {
    "ue5": {
        "base_module": "Engine.dll",
        "root_class": "UWorld",
        "chain_archetypes": [
            ("UWorld", "OwningGameInstance", "LocalPlayers[0]", "PlayerController", "PlayerCameraManager", "CameraCacheEntry"),
            ("GEngine", "GameViewport", "World", "Pawn", "CameraComponent", "Transform"),
            ("FSceneRenderer", "Views[0]", "ViewMatrices", "ViewProjectionMatrix")
        ],
        "default_offsets": [["0x180", "0x38", "0x0", "0x30", "0x340", "0x120"], ["0xF8", "0x80", "0x18", "0x2A0", "0x40"]]
    },
    "ue4": {
        "base_module": "UE4Game-Win64-Shipping.exe",
        "root_class": "UWorld",
        "chain_archetypes": [
            ("UWorld", "GameInstance", "LocalPlayer", "PlayerController", "CameraManager", "ViewTarget"),
            ("ULevel", "AActors", "MainPawn", "SpringArmComponent", "CameraComponent")
        ],
        "default_offsets": [["0x140", "0x38", "0x0", "0x30", "0x2B8", "0x90"], ["0x98", "0xA0", "0x280", "0x40"]]
    },
    "unity": {
        "base_module": "GameAssembly.dll",
        "root_class": "Camera_TypeInfo",
        "chain_archetypes": [
            ("Camera_TypeInfo", "StaticFields", "main_camera", "m_CachedPtr", "Transform", "Position"),
            ("UnityEngine.CoreModule", "GameObjectManager", "ActiveCamera", "ComponentList", "CameraData")
        ],
        "default_offsets": [["0xB8", "0x0", "0x10", "0x38", "0x90"], ["0x20", "0x80", "0x18", "0x40"]]
    },
    "custom": {
        "base_module": "GameClient.exe",
        "root_class": "RenderSubsystem",
        "chain_archetypes": [
            ("CRenderer", "CScene", "CCameraRig", "FViewContext"),
            ("GameManager", "EntitySystem", "LocalPlayer", "ViewMatrixData")
        ],
        "default_offsets": [["0x48", "0x110", "0x20", "0x80"], ["0x30", "0x280", "0x40"]]
    }
}

def generate_pointer_chain_sample(sample_id: int) -> Dict[str, Any]:
    engine = random.choice(ENGINES)
    chain_info = ENGINE_CHAINS[engine]
    module_name = chain_info["base_module"]
    
    archetype = random.choice(chain_info["chain_archetypes"])
    base_offset = random.randint(0x1000000, 0x4800000)
    base_ptr = f"{module_name}+0x{base_offset:07X}"
    
    # Generate 2 to 5 dereference offsets
    depth = random.randint(2, 4)
    offsets_hex = []
    for _ in range(depth):
        off = random.choice([0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x80, 0x90, 0xB8, 0x120, 0x180, 0x280, 0x340])
        offsets_hex.append(f"0x{off:X}")
    
    pos_offset = offsets_hex[-1]
    rot_offset = f"0x{int(pos_offset, 16) + 0x10:X}"
    fov_offset = f"0x{int(pos_offset, 16) + 0x24:X}"
    
    # Generate Cheat Engine XML snippet simulation
    ce_xml = (
        f"<CheatEntry>\n"
        f"  <Description>\"Main Player Camera Transform\"</Description>\n"
        f"  <VariableType>Custom</VariableType>\n"
        f"  <Address>\"{module_name}\"+{base_offset:07X}</Address>\n"
        f"  <Offsets>\n" +
        "\n".join([f"    <Offset>{off}</Offset>" for off in reversed(offsets_hex)]) +
        f"\n  </Offsets>\n"
        f"</CheatEntry>"
    )
    
    # Generate assembly tracing through the pointer chain
    r_trace = ["rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9"]
    curr_r = "rax"
    asm_steps = [
        f"48 8B 05 ?? ?? ?? ??  | mov {curr_r}, qword ptr [{module_name}+{base_offset:07X}] ; Root {archetype[0]}"
    ]
    
    aob_parts = ["48 8B 05 ?? ?? ?? ??"]
    for i, off in enumerate(offsets_hex[:-1]):
        next_r = r_trace[(i + 1) % len(r_trace)]
        off_val = int(off, 16)
        if off_val <= 0x7F:
            disp_hex = f"{off_val:02X}"
            asm_steps.append(f"48 8B {random.randint(0x40, 0x7F):02X} {disp_hex}        | mov {next_r}, qword ptr [{curr_r} + {off}] ; -> {archetype[min(i+1, len(archetype)-1)]}")
            aob_parts.append(f"48 8B ?? {disp_hex}")
        else:
            b0 = f"{off_val & 0xFF:02X}"
            b1 = f"{(off_val >> 8) & 0xFF:02X}"
            asm_steps.append(f"48 8B {random.randint(0x80, 0x8F):02X} {b0} {b1} 00 00 | mov {next_r}, qword ptr [{curr_r} + {off}] ; -> {archetype[min(i+1, len(archetype)-1)]}")
            aob_parts.append(f"48 8B ?? {b0} {b1} ?? ??")
        curr_r = next_r
        
    pos_val = int(pos_offset, 16)
    if pos_val <= 0x7F:
        asm_steps.append(f"F3 0F 10 40 {pos_val:02X}        | movss xmm0, dword ptr [{curr_r} + {pos_offset}] ; Read Position X")
        aob_parts.append(f"F3 0F 10 ?? {pos_val:02X}")
    else:
        p0 = f"{pos_val & 0xFF:02X}"
        p1 = f"{(pos_val >> 8) & 0xFF:02X}"
        asm_steps.append(f"F3 0F 10 80 {p0} {p1} 00 00 | movss xmm0, dword ptr [{curr_r} + {pos_offset}] ; Read Position X")
        aob_parts.append("F3 0F 10 ?? ?? ?? ?? ??")
        
    asm_trace = "\n".join(asm_steps)
    aob_pattern = " ".join(aob_parts)
    
    reasoning = (
        f"1. Pointer Resolution: Root pointer anchor resides at [{base_ptr}] ({archetype[0]}).\n"
        f"2. Chain Traversal: Dereferencing {len(offsets_hex)} levels of pointers across {' -> '.join(archetype[:len(offsets_hex)])}.\n"
        f"3. Terminal Object: Final pointer resolves to camera instance with position vector at offset {pos_offset}.\n"
        f"4. AOB Construction: Preserved root RIP relative opcode and clamped displacement offsets into AOB pattern.\n"
        f"Pattern: {aob_pattern}"
    )
    
    target_json = {
        "engine": "UnrealEngine5" if engine == "ue5" else ("UnrealEngine4" if engine == "ue4" else ("UnityIL2CPP" if engine == "unity" else "CustomEngine")),
        "camera_type": "MainPlayerCamera",
        "base_pointer": base_ptr,
        "offsets": offsets_hex,
        "aob_pattern": aob_pattern,
        "field_offsets": {
            "position": pos_offset,
            "rotation": rot_offset,
            "fov": fov_offset
        },
        "confidence": 0.999
    }
    
    prompt = (
        f"<|engine:{engine}|>\n"
        f"Given the following pointer chain trace and Cheat Engine memory table record:\n\n"
        f"[Pointer Disassembly]\n{asm_trace}\n\n"
        f"[Memory Table Record]\n{ce_xml}\n\n"
        f"Extract the exact pointer traversal path, target camera offsets, and synthesize a self-healing AOB pattern."
    )
    
    return {
        "id": f"ptr_chain_{sample_id:05d}",
        "category": "pointer_chain",
        "engine": engine,
        "prompt": prompt,
        "reasoning": reasoning,
        "target": target_json
    }

def generate_batch(count: int = 2500) -> List[Dict[str, Any]]:
    return [generate_pointer_chain_sample(i) for i in range(count)]

if __name__ == "__main__":
    import json
    samples = generate_batch(5)
    print(f"Generated {len(samples)} pointer chain samples. Sample 0:")
    print(json.dumps(samples[0], indent=2))
