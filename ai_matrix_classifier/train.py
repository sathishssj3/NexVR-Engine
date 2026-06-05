import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
import math
from model import MatrixPatternClassifier

def generate_synthetic_data(num_samples=10000):
    """
    Generate synthetic 4x4 matrices for training.
    Classes:
    0: Noise
    1: View Matrix (LookAt)
    2: Perspective Projection
    3: Orthographic Projection
    4: MVP (View * Proj)
    """
    X = np.zeros((num_samples, 16), dtype=np.float32)
    Y = np.zeros(num_samples, dtype=np.int64)
    
    for i in range(num_samples):
        cls = i % 5
        Y[i] = cls
        
        if cls == 0: # Noise
            X[i] = np.random.uniform(-100, 100, 16)
        
        elif cls == 1: # View Matrix (Orthonormal 3x3 + Translation)
            # Create a random rotation matrix (simplified using Euler angles)
            yaw, pitch, roll = np.random.uniform(-math.pi, math.pi, 3)
            
            cy, sy = math.cos(yaw), math.sin(yaw)
            cp, sp = math.cos(pitch), math.sin(pitch)
            cr, sr = math.cos(roll), math.sin(roll)
            
            # Rotation
            R = np.array([
                [cy*cp, cy*sp*sr - sy*cr, cy*sp*cr + sy*sr],
                [sy*cp, sy*sp*sr + cy*cr, sy*sp*cr - cy*sr],
                [-sp,   cp*sr,            cp*cr]
            ])
            
            # Translation
            T = np.random.uniform(-1000, 1000, 3)
            
            mat = np.eye(4)
            mat[:3, :3] = R
            mat[3, :3] = T # Row-major DirectX style
            
            X[i] = mat.flatten()
            
        elif cls == 2: # Perspective Projection (DirectX style)
            fov = np.random.uniform(math.radians(30), math.radians(120))
            aspect = np.random.uniform(0.5, 2.5)
            n = np.random.uniform(0.01, 1.0)
            f = np.random.uniform(100.0, 10000.0)
            
            yScale = 1.0 / math.tan(fov / 2.0)
            xScale = yScale / aspect
            
            mat = np.zeros((4,4))
            mat[0,0] = xScale
            mat[1,1] = yScale
            mat[2,2] = f / (f - n)
            mat[3,2] = -n * f / (f - n)
            mat[2,3] = 1.0 # Z-w divide
            
            X[i] = mat.flatten()
            
        elif cls == 3: # Orthographic Projection
            w = np.random.uniform(10, 1000)
            h = np.random.uniform(10, 1000)
            n = np.random.uniform(0.0, 1.0)
            f = np.random.uniform(10.0, 1000.0)
            
            mat = np.eye(4)
            mat[0,0] = 2.0 / w
            mat[1,1] = 2.0 / h
            mat[2,2] = 1.0 / (f - n)
            mat[3,2] = -n / (f - n)
            
            X[i] = mat.flatten()
            
        elif cls == 4: # MVP Matrix (approximate by multiplying View and Proj)
            # Simplification for synthetic generation: mix properties
            # Often has high numbers in translation and perspective w-divide
            X[i] = np.random.uniform(-10, 10, 16)
            X[i][11] = np.random.uniform(-1000, 1000) # W component translation
            X[i][15] = np.random.uniform(0, 100)
            
    # Add some noise to all valid matrices to simulate float precision/engine quirks
    mask = Y != 0
    X[mask] += np.random.normal(0, 0.001, X[mask].shape)
    
    return torch.tensor(X), torch.tensor(Y)

def train_and_export():
    print("Generating synthetic data...")
    X, Y = generate_synthetic_data(20000)
    
    model = MatrixPatternClassifier()
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=0.001)
    
    print("Training model...")
    epochs = 50
    batch_size = 64
    
    for epoch in range(epochs):
        permutation = torch.randperm(X.size()[0])
        total_loss = 0
        
        for i in range(0, X.size()[0], batch_size):
            indices = permutation[i:i+batch_size]
            batch_x, batch_y = X[indices], Y[indices]
            
            optimizer.zero_grad()
            outputs = model(batch_x)
            loss = criterion(outputs, batch_y)
            loss.backward()
            optimizer.step()
            
            total_loss += loss.item()
            
        if epoch % 10 == 0:
            print(f"Epoch {epoch}, Loss: {total_loss / (X.size()[0]/batch_size):.4f}")
            
    print("Training complete. Exporting to ONNX...")
    
    model.eval()
    dummy_input = torch.randn(1, 16, requires_grad=True)
    onnx_path = "matrix_classifier.onnx"
    
    torch.onnx.export(model,               # model being run
                      dummy_input,         # model input
                      onnx_path,           # where to save the model
                      export_params=True,  # store the trained parameter weights inside the model file
                      opset_version=11,    # the ONNX version to export the model to
                      do_constant_folding=True,
                      input_names = ['input'],   # the model's input names
                      output_names = ['output'], # the model's output names
                      dynamic_axes={'input' : {0 : 'batch_size'},    # variable length axes
                                    'output' : {0 : 'batch_size'}})
                                    
    print(f"Model exported successfully to {onnx_path}")

if __name__ == "__main__":
    train_and_export()
