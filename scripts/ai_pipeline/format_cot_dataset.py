"""
Stereix Engine - Hybrid Dataset Component D: Chain-of-Thought (CoT) Formatter
Assembles raw sample data into production ChatML format with <reasoning> blocks
and target JSON, formatted for Unsloth / TRL SFTTrainer with response-only loss masking.
"""

import json
from typing import Dict, Any, List

SYSTEM_PROMPT = (
    "You are Stereix's frontier reverse-engineering specialist. Your task is to analyze assembly traces, "
    "memory dumps, and register dataflow to detect 3D camera structures, view/projection matrices, "
    "player positions, and generate self-healing Array-of-Bytes (AOB) signatures."
)

def format_sample_chatml(sample: Dict[str, Any]) -> Dict[str, Any]:
    """
    Formats a raw sample into standard Hugging Face ChatML message list,
    plus a formatted full text string for tokenizer verification.
    """
    prompt = sample["prompt"]
    reasoning = sample["reasoning"].strip()
    target_json_str = json.dumps(sample["target"], indent=2)
    
    assistant_content = f"<reasoning>\n{reasoning}\n</reasoning>\n```json\n{target_json_str}\n```"
    
    messages = [
        {"role": "system", "content": SYSTEM_PROMPT},
        {"role": "user", "content": prompt},
        {"role": "assistant", "content": assistant_content}
    ]
    
    full_text = (
        f"<|im_start|>system\n{SYSTEM_PROMPT}<|im_end|>\n"
        f"<|im_start|>user\n{prompt}<|im_end|>\n"
        f"<|im_start|>assistant\n{assistant_content}<|im_end|>"
    )
    
    return {
        "id": sample["id"],
        "category": sample["category"],
        "engine": sample["engine"],
        "messages": messages,
        "text": full_text
    }

def format_batch(samples: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    return [format_sample_chatml(s) for s in samples]

if __name__ == "__main__":
    test_sample = {
        "id": "test_001",
        "category": "procedural_struct",
        "engine": "ue5",
        "prompt": "<|engine:ue5|>\nTest assembly trace",
        "reasoning": "1. Test step 1.\n2. Test step 2.",
        "target": {"engine": "UnrealEngine5", "camera_type": "MainPlayerCamera"}
    }
    formatted = format_sample_chatml(test_sample)
    print("Formatted Sample Assistant Output Preview:")
    print(formatted["messages"][2]["content"])
