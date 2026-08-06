import os
import sys
import argparse
import onnx
from onnxruntime.quantization import quantize_dynamic, QuantType
from onnxconverter_common import float16

# Add current and models directory to sys.path
sys.path.append(os.path.join(os.path.dirname(__file__), "models"))

def export_model(model_type, quantize=None):
    output_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../models"))
    os.makedirs(output_dir, exist_ok=True)
    
    file_name = f"{model_type}.onnx"
    onnx_path = os.path.join(output_dir, file_name)

    if model_type == "depth_inpainter":
        try:
            import torch
            from inpainter_unet import DepthAwareGatedInpainter
            print("Initializing Depth-Aware Gated Inpainter (PyTorch)...")
            model = DepthAwareGatedInpainter()
            model.eval()
            dummy_input = torch.randn(1, 4, 256, 256)
            
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
        except ImportError:
            print("PyTorch not installed, fallback to generate_dummy_onnx for depth_inpainter")
            from generate_dummy_onnx import create_model, MODELS_CONFIG
            cfg = MODELS_CONFIG['depth_inpainter']
            model_proto = create_model('depth_inpainter', cfg['inputs'], cfg['outputs'])
            onnx.save(model_proto, onnx_path)
    elif model_type == "ui_synthesizer":
        try:
            import torch
            from ui_synthesizer import HolographicUISynthesizer
            print("Initializing Holographic UI Synthesizer (YOLOv8)...")
            model = HolographicUISynthesizer(pretrained=False)
            model.eval()
            dummy_input = torch.randn(1, 3, 640, 640)
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
        except ImportError:
            from generate_dummy_onnx import create_model, MODELS_CONFIG
            cfg = MODELS_CONFIG['ui_synthesizer']
            model_proto = create_model('ui_synthesizer', cfg['inputs'], cfg['outputs'])
            onnx.save(model_proto, onnx_path)
    else:
        # Generic export using generate_dummy_onnx definitions
        sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")))
        import generate_dummy_onnx
        if model_type in generate_dummy_onnx.MODELS_CONFIG:
            cfg = generate_dummy_onnx.MODELS_CONFIG[model_type]
            model_proto = generate_dummy_onnx.create_model(model_type, cfg['inputs'], cfg['outputs'])
            onnx.save(model_proto, onnx_path)
        else:
            raise ValueError(f"Unknown model_type: {model_type}")

    # Set IR version to 9 for ONNX Runtime compatibility
    onnx_model = onnx.load(onnx_path)
    onnx_model.ir_version = 9
    onnx.save(onnx_model, onnx_path)
    onnx.checker.check_model(onnx_model)
    print(f"[OK] Exported {onnx_path} with IR=9, opset=13")

    if quantize == "fp16":
        fp16_path = os.path.join(output_dir, f"{model_type}_fp16.onnx")
        model_fp16 = float16.convert_float_to_float16(onnx_model, keep_io_types=False)
        model_fp16.ir_version = 9
        onnx.save(model_fp16, fp16_path)
        print(f"[OK] Saved FP16 version to {fp16_path}")
    elif quantize == "int8":
        int8_path = os.path.join(output_dir, f"{model_type}_int8.onnx")
        quantize_dynamic(onnx_path, int8_path, weight_type=QuantType.QInt8)
        int8_model = onnx.load(int8_path)
        int8_model.ir_version = 9
        onnx.save(int8_model, int8_path)
        print(f"[OK] Saved INT8 version to {int8_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=str, default="depth_inpainter")
    parser.add_argument("--quantize", type=str, choices=["fp16", "int8", "none"], default="fp16")
    args = parser.parse_args()
    
    export_model(args.model, quantize=args.quantize if args.quantize != "none" else None)
