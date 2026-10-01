"""
NexVR Engine - 72B Model Quantization Pipeline
Quantizes merged Qwen 72B model into:
1. AWQ (4-bit AutoAWQ) for ultra-fast cloud serverless inference (vLLM / Modal)
2. GGUF (Q4_K_M) for local profiling in the NexVR Engine client via llama.cpp
"""

import os
import sys
import argparse

def quantize_awq(model_dir: str, output_dir: str):
    print("=" * 70)
    print(f" NexVR Engine: 4-Bit AWQ Quantization")
    print(f" Source Model: {model_dir}")
    print(f" Target Dir:   {output_dir}")
    print("=" * 70)
    
    from awq import AutoAWQForCausalLM
    from transformers import AutoTokenizer
    
    print("[*] Loading model weights into AWQ engine...")
    model = AutoAWQForCausalLM.from_pretrained(model_dir, **{"low_cpu_mem_usage": True})
    tokenizer = AutoTokenizer.from_pretrained(model_dir, trust_remote_code=True)
    
    quant_config = {
        "zero_point": True,
        "q_group_size": 128,
        "w_bit": 4,
        "version": "GEMM"
    }
    
    print("[*] Quantizing weights with calibration data...")
    model.quantize(tokenizer, quant_config=quant_config)
    
    print(f"[*] Saving AWQ model to {output_dir}...")
    model.save_quantized(output_dir)
    tokenizer.save_pretrained(output_dir)
    print(f"[SUCCESS] AWQ quantization complete: {output_dir}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="NexVR Model Quantizer")
    parser.add_argument("--model", type=str, required=True, help="Input merged model directory")
    parser.add_argument("--output", type=str, required=True, help="Output quantized directory")
    parser.add_argument("--format", type=str, default="awq", choices=["awq", "gguf"], help="Quantization target format")
    
    args = parser.parse_args()
    if args.format == "awq":
        quantize_awq(args.model, args.output)
    else:
        print("[*] For GGUF quantization, use llama.cpp's convert-hf-to-gguf.py --outtype q4_k_m")
