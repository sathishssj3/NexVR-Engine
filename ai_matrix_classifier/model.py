import torch
import torch.nn as nn
import torch.nn.functional as F

class MatrixPatternClassifier(nn.Module):
    """
    A lightweight 1D CNN + MLP to classify 64-byte chunks of a constant buffer.
    Inputs are 16 floats (64 bytes) representing a potential 4x4 matrix.
    Output is the probability distribution over 5 classes:
    0: Unknown/Noise
    1: View Matrix
    2: Projection Matrix (Perspective)
    3: Orthographic Projection
    4: Combined MVP Matrix
    """
    def __init__(self):
        super(MatrixPatternClassifier, self).__init__()
        # Input shape: (Batch, 1 channel, 16 floats)
        
        # 1D Convolution to find local patterns (e.g., translation column, perspective row)
        self.conv1 = nn.Conv1d(in_channels=1, out_channels=16, kernel_size=4, stride=1, padding=1)
        self.conv2 = nn.Conv1d(in_channels=16, out_channels=32, kernel_size=4, stride=1, padding=1)
        
        # Fully connected layers
        # After conv1: length = 16 - 4 + 1 + 2(padding) = 15
        # After pool1: length = 7
        # After conv2: length = 7 - 4 + 1 + 2 = 6
        # After pool2: length = 3
        # Flattened: 32 * 3 = 96
        
        self.fc1 = nn.Linear(32 * 3, 64)
        self.fc2 = nn.Linear(64, 5) # 5 classes
        self.dropout = nn.Dropout(0.2)

    def forward(self, x):
        # x shape: (B, 16) -> reshape to (B, 1, 16)
        x = x.view(-1, 1, 16)
        
        x = F.relu(self.conv1(x))
        x = F.max_pool1d(x, kernel_size=2)
        
        x = F.relu(self.conv2(x))
        x = F.max_pool1d(x, kernel_size=2)
        
        x = x.view(x.size(0), -1) # Flatten
        
        x = F.relu(self.fc1(x))
        x = self.dropout(x)
        x = self.fc2(x)
        return x
