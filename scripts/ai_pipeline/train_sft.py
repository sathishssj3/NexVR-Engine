"""
NexVR Engine - 72B Reverse-Engineering Model Training Harness
Hardware: 1x NVIDIA H100 (80GB SXM5 / PCIe)
Framework: Unsloth AI + PyTorch 2.4 + FlashAttention-2
"""

import os
import torch
from unsloth import FastLanguageModel
from trl import SFTTrainer, DataCollatorForCompletionOnlyLM
from transformers import TrainingArguments
from datasets import load_dataset

# 1. Configuration & Hyperparameters
MODEL_ID = "Qwen/Qwen2.5-Coder-72B-Instruct"
MAX_SEQ_LENGTH = 4096
DTYPE = torch.bfloat16
LOAD_IN_4BIT = True  # Fits 72B onto single 80GB H100 with 4-bit QLoRA

# 2. Load Model & Tokenizer with Unsloth Accelerations
model, tokenizer = FastLanguageModel.from_pretrained(
    model_name=MODEL_ID,
    max_seq_length=MAX_SEQ_LENGTH,
    dtype=DTYPE,
    load_in_4bit=LOAD_IN_4BIT,
)

# 3. Add Custom Domain Special Tokens
custom_tokens = [
    "<reasoning>",
    "</reasoning>",
    "??",  # AOB wildcard
    "<|engine:ue5|>",
    "<|engine:ue4|>",
    "<|engine:unity|>",
    "<|engine:custom|>",
]
num_added = tokenizer.add_special_tokens({"additional_special_tokens": custom_tokens})
model.resize_token_embeddings(len(tokenizer))
print(f"[NexVR] Added {num_added} custom domain tokens. New vocab size: {len(tokenizer)}")

# 4. Attach All-Linear LoRA Adapters (Including Token Embeddings)
model = FastLanguageModel.get_peft_model(
    model,
    r=64,
    lora_alpha=128,
    target_modules=[
        "q_proj", "k_proj", "v_proj", "o_proj",
        "gate_proj", "up_proj", "down_proj"
    ],
    modules_to_save=["embed_tokens", "lm_head"],  # CRITICAL: Train new token weights!
    lora_dropout=0.0,  # Unsloth optimized: 0 dropout for maximum speed
    bias="none",
    use_gradient_checkpointing="unsloth",
    random_state=3407,
)

# 5. Load & Filter Dataset (Pre-Flight Guard)
dataset = load_dataset("json", data_files={"train": "data/stereix_train_cot.jsonl"})
train_data = dataset["train"].filter(lambda x: len(tokenizer.encode(x["text"])) <= MAX_SEQ_LENGTH)
print(f"[Stereix Engine] Verified {len(train_data)} samples within {MAX_SEQ_LENGTH} token limit.")

# 6. Response-Only Loss Masking Data Collator
response_template = "<|im_start|>assistant\n"
collator = DataCollatorForCompletionOnlyLM(
    response_template=response_template,
    tokenizer=tokenizer,
    mlm=False
)

# 7. Training Arguments with NEFTune & Paged 8-Bit AdamW
training_args = TrainingArguments(
    output_dir="checkpoints/qwen_72b_nexvr_sft",
    per_device_train_batch_size=2,
    gradient_accumulation_steps=16,  # Effective Batch Size = 32
    warmup_ratio=0.05,
    max_steps=1200,  # ~2.5 epochs over 8,000 samples (~6.5 hours on H100)
    learning_rate=1.5e-4,
    fp16=False,
    bf16=True,  # Native Hopper Brain Float 16
    logging_steps=10,
    optim="paged_adamw_8bit",
    weight_decay=0.01,
    lr_scheduler_type="cosine",
    seed=3407,
    max_grad_norm=1.0,
    neftune_noise_alpha=5.0,  # CRITICAL: Prevents overfitting to synthetic hex offsets
    save_strategy="steps",
    save_steps=200,
    save_total_limit=3,
    report_to="none",
)

# 8. Initialize Trainer & Launch
trainer = SFTTrainer(
    model=model,
    tokenizer=tokenizer,
    train_dataset=train_data,
    dataset_text_field="text",
    max_seq_length=MAX_SEQ_LENGTH,
    data_collator=collator,
    dataset_num_proc=4,
    packing=False,  # Set to True if using FlashAttention-2 block-diagonal packing
    args=training_args,
)

print("[NexVR] Starting Qwen-2.5-Coder-72B Fine-Tuning Run...")
trainer.train()

# 9. Save LoRA Adapter & Tokenizer
model.save_pretrained("checkpoints/qwen_72b_nexvr_adapter")
tokenizer.save_pretrained("checkpoints/qwen_72b_nexvr_adapter")
print("[NexVR] Training complete. Adapter saved to checkpoints/qwen_72b_nexvr_adapter.")
