#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
#include "heuristics/matrix_classifier.h"
#include "memory_scanner/camera_delta_tracker.h"

using namespace vrinject::ai;

void GenerateRowMajorProj(float* out, float fov, float aspect, float nearZ, float farZ) {
    float f = 1.0f / std::tan(fov * 0.5f);
    out[0] = f / aspect; out[1] = 0; out[2] = 0; out[3] = 0;
    out[4] = 0; out[5] = f; out[6] = 0; out[7] = 0;
    out[8] = 0; out[9] = 0; out[10] = farZ / (farZ - nearZ); out[11] = 1.0f;
    out[12] = 0; out[13] = 0; out[14] = -(farZ * nearZ) / (farZ - nearZ); out[15] = 0;
}

void GenerateColMajorProj(float* out, float fov, float aspect, float nearZ, float farZ) {
    float f = 1.0f / std::tan(fov * 0.5f);
    out[0] = f / aspect; out[4] = 0; out[8] = 0; out[12] = 0;
    out[1] = 0; out[5] = f; out[9] = 0; out[13] = 0;
    out[2] = 0; out[6] = 0; out[10] = farZ / (farZ - nearZ); out[14] = -(farZ * nearZ) / (farZ - nearZ);
    out[3] = 0; out[7] = 0; out[11] = 1.0f; out[15] = 0;
}

void GenerateReversedZProj(float* out, float fov, float aspect, float nearZ) {
    float f = 1.0f / std::tan(fov * 0.5f);
    out[0] = f / aspect; out[1] = 0; out[2] = 0; out[3] = 0;
    out[4] = 0; out[5] = f; out[6] = 0; out[7] = 0;
    out[8] = 0; out[9] = 0; out[10] = 0.0f; out[11] = 1.0f;
    out[12] = 0; out[13] = 0; out[14] = nearZ; out[15] = 0;
}

void GenerateCombinedVP(float* out, float fov, float aspect, float nearZ, float farZ) {
    // Just mock a combined view-projection by applying a simple translation to a projection matrix
    float f = 1.0f / std::tan(fov * 0.5f);
    out[0] = f / aspect; out[1] = 0; out[2] = 0; out[3] = 0;
    out[4] = 0; out[5] = f; out[6] = 0; out[7] = 0;
    out[8] = 0; out[9] = 0; out[10] = farZ / (farZ - nearZ); out[11] = 1.0f;
    // Apply translation of z = 5.0
    out[12] = 0; out[13] = 0; out[14] = -(farZ * nearZ) / (farZ - nearZ) + 5.0f * out[10]; out[15] = 5.0f;
}

void GenerateNoise(float* out, size_t count) {
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-1000.0f, 1000.0f);
    for (size_t i = 0; i < count; ++i) {
        out[i] = dis(gen);
    }
}

int main(int argc, char** argv) {
    std::cout << "==========================================\n";
    std::cout << " NexVR Engine - Matrix Scanning Harness\n";
    std::cout << "==========================================\n\n";

    MatrixClassifier classifier;
    std::wstring modelPath = L"ai_matrix_classifier/matrix_classifier.onnx";
    
    if (!classifier.Initialize(modelPath)) {
        std::cerr << "WARNING: Could not load ONNX model. Ensure 'matrix_classifier.onnx' is present.\n";
        return 1;
    }

    const size_t MEMORY_DUMP_SIZE_FLOATS = 1024 * 64; // 256KB per dummy game dump
    std::vector<float> dummyMemory(MEMORY_DUMP_SIZE_FLOATS);
    
    struct TestCase {
        std::string name;
        void (*generator)(float*, float, float, float, float);
    };
    
    std::vector<TestCase> testCases = {
        {"Row-Major Projection", [](float* o, float f, float a, float n, float z) { GenerateRowMajorProj(o,f,a,n,z); }},
        {"Column-Major Projection", [](float* o, float f, float a, float n, float z) { GenerateColMajorProj(o,f,a,n,z); }},
        {"Reversed-Z Projection", [](float* o, float f, float a, float n, float z) { GenerateReversedZProj(o,f,a,n); }},
        {"Combined View-Projection", [](float* o, float f, float a, float n, float z) { GenerateCombinedVP(o,f,a,n,z); }}
    };

    CameraDeltaTracker tracker;
    
    // To simulate false positives, we'll purposely inject static matrices 
    std::vector<size_t> falsePositiveOffsets = { 20000, 40000 };

    int overallTP = 0, overallFP = 0, overallFN = 0;

    for (const auto& tc : testCases) {
        tracker.Reset();

        // 10 frames of simulated gameplay
        int finalTp = 0, finalFp = 0, finalFn = 0;
        
        for (int frame = 0; frame < 10; ++frame) {
            GenerateNoise(dummyMemory.data(), MEMORY_DUMP_SIZE_FLOATS);

            std::vector<size_t> knownMatrixOffsets = { 1024, 15000, 25000, 35000, 55000 };
            
            // Generate real moving cameras
            for (size_t offset : knownMatrixOffsets) {
                // Simulate slight movement (e.g., player looking around, FOV changing slightly, or translation changing)
                float simulatedMovement = (float)frame * 0.01f;
                tc.generator(&dummyMemory[offset], 1.57f + simulatedMovement, 1.77f, 0.1f, 1000.0f);
            }
            
            // Generate static false positives
            for (size_t offset : falsePositiveOffsets) {
                tc.generator(&dummyMemory[offset], 1.57f, 1.77f, 0.1f, 1000.0f);
            }

            auto scanResults = classifier.ScanBuffer(dummyMemory.data(), dummyMemory.size() * sizeof(float));
            tracker.UpdateCandidates(scanResults, dummyMemory.data());
            
            if (frame == 9) {
                // On the final frame, evaluate what the tracker locked onto
                int lockedOffset = -1;
                DirectX::XMFLOAT4X4 lockedMatrix;
                
                if (tracker.GetBestCamera(lockedOffset, lockedMatrix)) {
                    size_t offsetFloats = lockedOffset / sizeof(float);
                    bool isKnown = false;
                    for (size_t known : knownMatrixOffsets) {
                        if (offsetFloats == known) {
                            isKnown = true; break;
                        }
                    }
                    if (isKnown) {
                        finalTp = 1; // It locked onto a valid camera!
                    } else {
                        finalFp = 1; // It locked onto a false positive!
                    }
                } else {
                    finalFn = 1; // It didn't lock onto anything
                }
                
                overallTP += finalTp;
                overallFP += finalFp;
                overallFN += finalFn;
                
                std::cout << "Case: " << tc.name << "\n";
                std::cout << "  - True Positives:  " << finalTp << " (Locked on valid camera)\n";
                std::cout << "  - False Positives: " << finalFp << " (Locked on static/garbage)\n";
                std::cout << "  - False Negatives: " << finalFn << " (Failed to lock)\n\n";
            }
        }
    }

    std::cout << "==========================================\n";
    std::cout << " TEMPORAL TRACKING METRICS\n";
    std::cout << "==========================================\n";
    std::cout << "Total True Positives (Valid Locks):  " << overallTP << "\n";
    std::cout << "Total False Positives (Bad Locks):   " << overallFP << "\n";
    std::cout << "Total False Negatives (Missed):      " << overallFN << "\n";
    
    if (overallTP + overallFP > 0) {
        float precision = (float)overallTP / (overallTP + overallFP);
        float recall = (float)overallTP / (overallTP + overallFN);
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Precision: " << precision * 100.0f << "%\n";
        std::cout << "Recall:    " << recall * 100.0f << "%\n";
    }

    return 0;
}
