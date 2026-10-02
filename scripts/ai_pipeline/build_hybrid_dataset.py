"""
NexVR Engine - Master Hybrid Dataset Builder
Orchestrates:
- Component A: 3,500 Procedural C++ Structs
- Component B: 2,500 Real Game Pointer Chains
- Component C: 2,000 Hard Negative Cameras
- Component D: ChatML CoT Formatter (<reasoning> blocks)
- Component E: Automated Pre-Flight Validation
Produces production-ready:
  data/nexvr_train_cot.jsonl (7,200 samples)
  data/nexvr_val_cot.jsonl   (800 samples)
"""

import os
import sys
import argparse
import random
import json
from typing import List, Dict, Any

from generate_procedural_structs import generate_batch as gen_structs
from generate_pointer_chains import generate_batch as gen_pointers
from generate_hard_negatives import generate_batch as gen_negatives
from format_cot_dataset import format_batch
from validate_dataset import validate_file

def build_dataset(
    struct_count: int = 3500,
    pointer_count: int = 2500,
    negative_count: int = 2000,
    train_path: str = "data/stereix_train_cot.jsonl",
    val_path: str = "data/stereix_val_cot.jsonl",
    val_ratio: float = 0.10,
    seed: int = 42
):
    random.seed(seed)
    total_requested = struct_count + pointer_count + negative_count
    print("=" * 70)
    print(f" Stereix Engine: 8,000-Sample Hybrid AI Dataset Generator")
    print(f" Target: {total_requested} Total Samples (Train/Val Split = {1.0 - val_ratio:.0%} / {val_ratio:.0%})")
    print("=" * 70)
    
    # 1. Generate Components
    print(f"[*] Generating Component A: {struct_count} Procedural C++ Structs...")
    structs = gen_structs(struct_count)
    
    print(f"[*] Generating Component B: {pointer_count} Real Game Pointer Chains...")
    pointers = gen_pointers(pointer_count)
    
    print(f"[*] Generating Component C: {negative_count} Hard Negative Cameras...")
    negatives = gen_negatives(negative_count)
    
    all_raw: List[Dict[str, Any]] = structs + pointers + negatives
    print(f"[+] Total raw samples collected: {len(all_raw)}")
    
    # 2. Format with Chain-of-Thought (CoT)
    print("[*] Formatting into ChatML with <reasoning> traces and JSON targets...")
    formatted_samples = format_batch(all_raw)
    
    # 3. Shuffle
    print("[*] Shuffling dataset with deterministic seed...")
    random.shuffle(formatted_samples)
    
    # 4. Train / Val Split
    val_size = int(len(formatted_samples) * val_ratio)
    val_samples = formatted_samples[:val_size]
    train_samples = formatted_samples[val_size:]
    
    print(f"[+] Train Partition: {len(train_samples)} samples")
    print(f"[+] Val Partition:   {len(val_samples)} samples")
    
    # Ensure output directories exist
    os.makedirs(os.path.dirname(os.path.abspath(train_path)), exist_ok=True)
    os.makedirs(os.path.dirname(os.path.abspath(val_path)), exist_ok=True)
    
    # 5. Write to JSONL
    print(f"[*] Writing train dataset to: {train_path}")
    with open(train_path, "w", encoding="utf-8") as f:
        for s in train_samples:
            f.write(json.dumps(s) + "\n")
            
    print(f"[*] Writing val dataset to:   {val_path}")
    with open(val_path, "w", encoding="utf-8") as f:
        for s in val_samples:
            f.write(json.dumps(s) + "\n")
            
    # 6. Validate Output Files
    print("\n" + "=" * 70)
    print(" Running Pre-Flight Dataset Validation on Generated Files")
    print("=" * 70)
    train_ok = validate_file(train_path)
    val_ok = validate_file(val_path)
    
    if train_ok and val_ok:
        print("[SUCCESS] All 8,000 hybrid samples generated, formatted, and validated!")
        print(f"  Train: {os.path.abspath(train_path)} ({os.path.getsize(train_path) / (1024*1024):.2f} MB)")
        print(f"  Val:   {os.path.abspath(val_path)} ({os.path.getsize(val_path) / (1024*1024):.2f} MB)")
    else:
        print("[ERROR] Dataset validation failed! Please check error logs above.")
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="NexVR Hybrid Dataset Builder")
    parser.add_argument("--structs", type=int, default=3500, help="Number of Component A structs")
    parser.add_argument("--pointers", type=int, default=2500, help="Number of Component B pointer chains")
    parser.add_argument("--negatives", type=int, default=2000, help="Number of Component C hard negatives")
    parser.add_argument("--train-out", type=str, default="data/stereix_train_cot.jsonl", help="Train output path")
    parser.add_argument("--val-out", type=str, default="data/stereix_val_cot.jsonl", help="Val output path")
    parser.add_argument("--val-ratio", type=float, default=0.10, help="Validation set split ratio")
    
    args = parser.parse_args()
    build_dataset(
        struct_count=args.structs,
        pointer_count=args.pointers,
        negative_count=args.negatives,
        train_path=args.train_out,
        val_path=args.val_out,
        val_ratio=args.val_ratio
    )
