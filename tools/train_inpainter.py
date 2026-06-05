import torch
import torch.nn as nn
import torch.optim as optim
import torch.nn.functional as F
import numpy as np
import os

class DoubleConv(nn.Module):
    def __init__(self, in_channels, out_channels):
        super().__init__()
        self.conv = nn.Sequential(
            nn.Conv2d(in_channels, out_channels, kernel_size=3, padding=1, bias=False),
            nn.BatchNorm2d(out_channels),
            nn.LeakyReLU(0.2, inplace=True),
            nn.Conv2d(out_channels, out_channels, kernel_size=3, padding=1, bias=False),
            nn.BatchNorm2d(out_channels),
            nn.LeakyReLU(0.2, inplace=True)
        )

    def forward(self, x):
        return self.conv(x)

class UNetInpainter(nn.Module):
    def __init__(self, in_channels=4, out_channels=3):
        # in_channels: 3 (color) + 1 (mask)
        super(UNetInpainter, self).__init__()
        
        # Lightweight U-Net to ensure < 0.5ms inference on 480x270
        self.inc = DoubleConv(in_channels, 16)
        
        self.down1 = nn.Sequential(nn.MaxPool2d(2), DoubleConv(16, 32))
        self.down2 = nn.Sequential(nn.MaxPool2d(2), DoubleConv(32, 64))
        self.down3 = nn.Sequential(nn.MaxPool2d(2), DoubleConv(64, 128))
        
        self.up1 = nn.ConvTranspose2d(128, 64, kernel_size=2, stride=2)
        self.conv1 = DoubleConv(128, 64)
        
        self.up2 = nn.ConvTranspose2d(64, 32, kernel_size=2, stride=2)
        self.conv2 = DoubleConv(64, 32)
        
        self.up3 = nn.ConvTranspose2d(32, 16, kernel_size=2, stride=2)
        self.conv3 = DoubleConv(32, 16)
        
        self.outc = nn.Conv2d(16, out_channels, kernel_size=1)

    def forward(self, x):
        x1 = self.inc(x)
        x2 = self.down1(x1)
        x3 = self.down2(x2)
        x4 = self.down3(x3)
        
        x = self.up1(x4)
        # Pad if necessary
        diffY = x3.size()[2] - x.size()[2]
        diffX = x3.size()[3] - x.size()[3]
        x = F.pad(x, [diffX // 2, diffX - diffX // 2, diffY // 2, diffY - diffY // 2])
        x = torch.cat([x3, x], dim=1)
        x = self.conv1(x)
        
        x = self.up2(x)
        diffY = x2.size()[2] - x.size()[2]
        diffX = x2.size()[3] - x.size()[3]
        x = F.pad(x, [diffX // 2, diffX - diffX // 2, diffY // 2, diffY - diffY // 2])
        x = torch.cat([x2, x], dim=1)
        x = self.conv2(x)
        
        x = self.up3(x)
        diffY = x1.size()[2] - x.size()[2]
        diffX = x1.size()[3] - x.size()[3]
        x = F.pad(x, [diffX // 2, diffX - diffX // 2, diffY // 2, diffY - diffY // 2])
        x = torch.cat([x1, x], dim=1)
        x = self.conv3(x)
        
        return torch.sigmoid(self.outc(x))

def generate_synthetic_data(batch_size):
    # Dummy data generator for demonstration
    # H=270, W=480 (Quarter res of 1080p)
    ground_truth = torch.rand(batch_size, 3, 270, 480)
    mask = torch.zeros(batch_size, 1, 270, 480)
    
    # Simulate disocclusion gaps (vertical slits)
    for b in range(batch_size):
        x_start = np.random.randint(50, 430)
        width = np.random.randint(10, 30)
        mask[b, :, :, x_start:x_start+width] = 1.0
        
    masked_input = ground_truth * (1.0 - mask)
    # Network input: concatenated RGB and Mask
    network_input = torch.cat([masked_input, mask], dim=1)
    
    return network_input, ground_truth, mask

def train():
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    model = UNetInpainter().to(device)
    optimizer = optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.L1Loss()
    
    print("Starting lightweight training pass...")
    model.train()
    for epoch in range(1): # Mock single epoch
        inputs, targets, masks = generate_synthetic_data(batch_size=4)
        inputs, targets, masks = inputs.to(device), targets.to(device), masks.to(device)
        
        optimizer.zero_grad()
        outputs = model(inputs)
        
        # We only care about loss in the masked region
        loss = criterion(outputs * masks, targets * masks)
        loss.backward()
        optimizer.step()
        
        print(f"Epoch {epoch+1} | Loss: {loss.item():.4f}")
        
    print("Training complete.")
    export_onnx(model, device)

def export_onnx(model, device):
    model.eval()
    dummy_input = torch.randn(1, 4, 270, 480).to(device)
    
    output_path = "inpainter.onnx"
    
    # Export to ONNX
    torch.onnx.export(
        model,
        dummy_input,
        output_path,
        export_params=True,
        opset_version=14,
        do_constant_folding=True,
        input_names=['input'],
        output_names=['output'],
        dynamic_axes={'input': {0: 'batch_size'}, 'output': {0: 'batch_size'}}
    )
    
    print(f"Exported FP32 model to {output_path}")
    
    # Provide simple instructions for FP16 conversion via onnxruntime.quantization
    print("Note: To convert to FP16, run the following in your environment:")
    print("  python -m onnxruntime.quantization.preprocess --input inpainter.onnx --output inpainter_fp16.onnx")

if __name__ == "__main__":
    train()
