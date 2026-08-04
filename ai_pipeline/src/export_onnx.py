import torch
import onnx
from onnxruntime.quantization import quantize_dynamic, QuantType
import os
import sys
import argparse

# Add the models directory to the path so we can import our models
sys.path.append(os.path.join(os.path.dirname(__file__), "models"))
from inpainter_unet import DepthAwareGatedInpainter
from ui_synthesizer import HolographicUISynthesizer

def export_model(model_type):
    if model_type == "depth_inpainter":
        print("Initializing Depth-Aware Gated Inpainter...")
        model = DepthAwareGatedInpainter()
        model.eval()
        dummy_input = torch.randn(1, 4, 256, 256)
        file_name = "depth_inpainter.onnx"
    elif model_type == "ui_synthesizer":
        print("Initializing Holographic UI Synthesizer (YOLOv8n)...")
        model = HolographicUISynthesizer(pretrained=False) # Use untrained for export structural testing
        model.eval()
        dummy_input = torch.randn(1, 3, 640, 640)
        file_name = "ui_synthesizer.onnx"
    else:
        raise ValueError(f"Unknown model_type: {model_type}")
    
    # Define output path
    output_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../models"))
    os.makedirs(output_dir, exist_ok=True)
    onnx_path = os.path.join(output_dir, file_name)
    
    print(f"Exporting model to ONNX format (opset 13)...")
    torch.onnx.export(
        model, 
        dummy_input, 
        onnx_path, 
        export_params=True, 
        opset_version=13, 
        do_constant_folding=True, 
        input_names=['input'], 
        output_names=['output']
    )
    
    print(f"Verifying exported ONNX model at {onnx_path}...")
    onnx_model = onnx.load(onnx_path)
    
    # Downgrade IR version for C++ ONNX Runtime compatibility
    print(f"Original IR version: {onnx_model.ir_version}")
    onnx_model.ir_version = 9
    onnx.save(onnx_model, onnx_path)
    print("Downgraded IR version to 9.")
    
    onnx.checker.check_model(onnx_model)
    print(f"ONNX model {file_name} is valid.")
    
    print("Done! The model is ready for the C++ DirectML execution provider.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=str, default="depth_inpainter", choices=["depth_inpainter", "ui_synthesizer"])
    args = parser.parse_args()
    
    export_model(args.model)
