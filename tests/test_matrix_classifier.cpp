#include "gtest/gtest.h"
#include "heuristics/matrix_classifier.h"
#include <DirectXMath.h>

using namespace DirectX;
using namespace vrinject;

class MatrixClassifierTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing state
        MatrixClassifier::Get().Reset();
    }

    void TearDown() override {
        MatrixClassifier::Get().Reset();
    }
};

TEST_F(MatrixClassifierTest, InitialState) {
    MatrixClassifier& classifier = MatrixClassifier::Get();

    XMFLOAT4X4 identity = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    bool result = classifier.GetCameraMatrix(identity);
    EXPECT_FALSE(result); // Should be false initially
}

TEST_F(MatrixClassifierTest, ProjectionMatrixDetection) {
    MatrixClassifier& classifier = MatrixClassifier::Get();

    // Create a simple projection matrix (similar to what D3DXMatrixPerspectiveFovLH would create)
    // 45 degree FOV, 16:9 aspect, near=0.1, far=100.0
    float fovY = XMConvertToRadians(45.0f);
    float aspectRatio = 16.0f / 9.0f;
    float nearZ = 0.1f;
    float farZ = 100.0f;

    float f = 1.0f / tanf(fovY * 0.5f);
    XMFLOAT4X4 projMatrix = {
        f / aspectRatio, 0.0f,                       0.0f,                              0.0f,
        0.0f,            f,                          0.0f,                              0.0f,
        0.0f,            0.0f,                       farZ / (farZ - nearZ),           1.0f,
        0.0f,            0.0f,                       (-nearZ * farZ) / (farZ - nearZ), 0.0f
    };

    // Simulate updating a constant buffer with this matrix
    const size_t bufferSize = sizeof(XMFLOAT4X4);
    XMFLOAT4X4 bufferData = projMatrix;

    classifier.OnConstantBufferUpdate(&bufferData, bufferSize, &bufferData);

    XMFLOAT4X4 resultMatrix;
    bool found = classifier.GetCameraMatrix(resultMatrix);

    // Should find the matrix and it should match what we put in
    EXPECT_TRUE(found);
    EXPECT_EQ(memcmp(&resultMatrix, &projMatrix, sizeof(XMFLOAT4X4)), 0);
}

TEST_F(MatrixClassifierTest, ViewMatrixDetection) {
    MatrixClassifier& classifier = MatrixClassifier::Get();

    // Create a simple view matrix (looking at origin from (0,0,-5))
    XMFLOAT4X4 viewMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 5.0f, 1.0f  // Translation backwards
    };

    // View matrices should have orthogonal basis vectors
    // This one is identity rotation with translation

    const size_t bufferSize = sizeof(XMFLOAT4X4);
    XMFLOAT4X4 bufferData = viewMatrix;

    classifier.OnConstantBufferUpdate(&bufferData, bufferSize, &bufferData);

    XMFLOAT4X4 resultMatrix;
    bool found = classifier.GetCameraMatrix(resultMatrix);

    EXPECT_TRUE(found);
    EXPECT_EQ(memcmp(&resultMatrix, &viewMatrix, sizeof(XMFLOAT4X4)), 0);
}

TEST_F(MatrixClassifierTest, MatrixLocking) {
    MatrixClassifier& classifier = MatrixClassifier::Get();
    classifier.Reset();

    // Use a valid projection matrix structure
    XMFLOAT4X4 testMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.77f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, -0.1f, 0.0f
    };

    const size_t bufferSize = sizeof(XMFLOAT4X4);
    XMFLOAT4X4 bufferData = testMatrix;
    void* bufferAddr = &bufferData;

    // Feed it multiple times to build confidence
    for (int i = 0; i < 15; i++) {
        bufferData._41 += 0.001f;
        classifier.OnConstantBufferUpdate(&bufferData, bufferSize, bufferAddr);
    }

    // Check if it got locked
    void* lockedAddr = classifier.GetLockedAddress();
    EXPECT_NE(lockedAddr, nullptr);
    EXPECT_EQ(lockedAddr, bufferAddr);
}

TEST_F(MatrixClassifierTest, MatrixOverwrite) {
    MatrixClassifier& classifier = MatrixClassifier::Get();
    classifier.Reset();

    // Create valid projection matrix
    XMFLOAT4X4 originalMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.77f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, -0.1f, 0.0f
    };

    // Create VR matrix to write
    XMFLOAT4X4 vrMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.77f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, -0.2f, 0.0f  // Different Z position
    };

    const size_t bufferSize = sizeof(XMFLOAT4X4);
    XMFLOAT4X4 bufferData = originalMatrix;
    void* bufferAddr = &bufferData;

    // Feed it multiple times to build confidence and trigger lock
    for (int i = 0; i < 15; i++) {
        bufferData._41 += 0.001f;
        classifier.OnConstantBufferUpdate(&bufferData, bufferSize, bufferAddr);
    }

    // Verify it's locked
    void* lockedAddr = classifier.GetLockedAddress();
    EXPECT_NE(lockedAddr, nullptr);

    // Now try to overwrite with VR matrix
    bool success = classifier.OverwriteCameraMatrix(vrMatrix, bufferAddr, bufferSize);

    // Should succeed
    EXPECT_TRUE(success);

    // Check that the buffer was actually modified
    EXPECT_EQ(memcmp(bufferAddr, &vrMatrix, sizeof(XMFLOAT4X4)), 0);
}

TEST_F(MatrixClassifierTest, ResetFunctionality) {
    MatrixClassifier& classifier = MatrixClassifier::Get();

    // Add some data
    XMFLOAT4X4 testMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 5.0f, 1.0f
    };

    const size_t bufferSize = sizeof(XMFLOAT4X4);
    XMFLOAT4X4 bufferData = testMatrix;
    void* bufferAddr = &bufferData;

    classifier.OnConstantBufferUpdate(&bufferData, bufferSize, bufferAddr);

    // Verify we have data
    XMFLOAT4X4 resultMatrix;
    bool foundBefore = classifier.GetCameraMatrix(resultMatrix);
    EXPECT_TRUE(foundBefore);

    // Reset
    classifier.Reset();

    // Should be empty now
    bool foundAfter = classifier.GetCameraMatrix(resultMatrix);
    EXPECT_FALSE(foundAfter);

    // Locked address should be null
    EXPECT_TRUE(classifier.GetLockedAddress() == nullptr);
}
