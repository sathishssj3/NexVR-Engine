"""
NexVR Engine - Hybrid Dataset Component E: Pre-Flight Dataset Validator
Verifies dataset integrity before hitting the GPU:
- Hard token length check (<= 4096 tokens)
- Valid JSON schema parsing for all targets
- Verification of <reasoning> and </reasoning> tags
- AOB signature hexadecimal and '??' wildcard validity
- Dataset balance & engine distribution reporting
"""

import sys
import json
import re
from typing import Dict, Any, List, Tuple

def rough_token_count(text: str) -> int:
    """
    Approximates BPE token count for code/assembly.
    Average ratio is ~3.2 characters per token for low-level hex and assembly.
    """
    return max(1, int(len(text) / 3.2))

def validate_aob(pattern: str) -> bool:
    tokens = pattern.strip().split()
    for tok in tokens:
        if tok == "??":
            continue
        if len(tok) != 2 or not all(c in "0123456789ABCDEFabcdef" for c in tok):
            return False
    return True

def validate_sample(sample: Dict[str, Any], max_tokens: int = 4096) -> Tuple[bool, str]:
    if "messages" not in sample or len(sample["messages"]) < 3:
        return False, "Missing or malformed 'messages' field"
    
    assistant_msg = sample["messages"][2]["content"]
    
    # 1. Verify reasoning tags
    if "<reasoning>" not in assistant_msg or "</reasoning>" not in assistant_msg:
        return False, "Missing <reasoning> or </reasoning> tags in assistant response"
    
    # 2. Extract and verify JSON block
    json_match = re.search(r"```json\s*(\{.*?\})\s*```", assistant_msg, re.DOTALL)
    if not json_match:
        return False, "Assistant response does not contain valid ```json block"
    
    try:
        parsed_json = json.loads(json_match.group(1))
    except Exception as e:
        return False, f"JSON parse error: {e}"
    
    # 3. Verify AOB pattern
    aob = parsed_json.get("aob_pattern", "")
    if aob and not validate_aob(aob):
        return False, f"Invalid AOB pattern bytes: '{aob}'"
        
    # 4. Check token length bounds
    total_text = sample.get("text", "")
    if not total_text:
        total_text = " ".join([m["content"] for m in sample["messages"]])
        
    tokens = rough_token_count(total_text)
    if tokens > max_tokens:
        return False, f"Token length {tokens} exceeds maximum allowed {max_tokens}"
        
    return True, "Valid"

def validate_file(filepath: str, max_tokens: int = 4096) -> bool:
    print(f"[*] Validating dataset: {filepath}")
    total = 0
    valid_count = 0
    categories: Dict[str, int] = {}
    engines: Dict[str, int] = {}
    token_lengths: List[int] = []
    
    with open(filepath, "r", encoding="utf-8") as f:
        for line_no, line in enumerate(f, 1):
            if not line.strip():
                continue
            total += 1
            try:
                sample = json.loads(line)
            except Exception as e:
                print(f"[FAIL] Line {line_no}: Malformed JSON line - {e}")
                return False
                
            is_valid, reason = validate_sample(sample, max_tokens)
            if not is_valid:
                print(f"[FAIL] Line {line_no} ({sample.get('id', 'unknown')}): {reason}")
                return False
                
            valid_count += 1
            cat = sample.get("category", "unknown")
            eng = sample.get("engine", "unknown")
            categories[cat] = categories.get(cat, 0) + 1
            engines[eng] = engines.get(eng, 0) + 1
            
            total_text = sample.get("text", "")
            token_lengths.append(rough_token_count(total_text))

    print(f"[PASS] Successfully validated {valid_count}/{total} samples!")
    print("\n--- Distribution Summary ---")
    print(f"Categories: {categories}")
    print(f"Engines:    {engines}")
    if token_lengths:
        print(f"Tokens:     Min={min(token_lengths)}, Max={max(token_lengths)}, Avg={sum(token_lengths)//len(token_lengths)}")
    print("----------------------------\n")
    return True

if __name__ == "__main__":
    from typing import Tuple
    if len(sys.argv) > 1:
        path = sys.argv[1]
        success = validate_file(path)
        sys.exit(0 if success else 1)
    else:
        print("Usage: python validate_dataset.py <path_to_jsonl>")
