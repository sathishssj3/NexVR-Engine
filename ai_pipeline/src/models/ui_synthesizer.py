import torch
import torch.nn as nn
try:
    from ultralytics import YOLO
except ImportError:
    # Fallback if ultralytics is not installed in current environment
    YOLO = None

class HolographicUISynthesizer(nn.Module):
    """
    Holographic UI Synthesizer (Model 5)
    
    Parses flat-screen frames to detect bounding boxes around 2D UI elements.
    Outputs boxes that the engine will render on a 3D hologram plane to avoid stereo depth-clashing.
    """
    def __init__(self, pretrained=True):
        super().__init__()
        if YOLO is None:
            raise ImportError("ultralytics package is required for HolographicUISynthesizer")
        
        # We use YOLOv8 nano for real-time <1.5ms budget capability
        if pretrained:
            # Loads pretrained weights and extracts the underlying PyTorch nn.Module
            self.model = YOLO('yolov8n.pt').model
        else:
            self.model = YOLO('yolov8n.yaml').model
            
    def forward(self, x):
        # Expected input: [1, 3, 640, 640] normalized RGB tensor
        # Returns bounding boxes [x, y, w, h, conf, class...]
        return self.model(x)
