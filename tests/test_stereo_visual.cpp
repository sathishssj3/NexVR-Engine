#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <iostream>
#include <vector>
#include <fstream>
#include "../src/rendering/stereo_renderer.h"
#include "../src/core/stereo_camera_generator.h"
#include "../src/core/stereo_frame_builder.h"

using namespace Microsoft::WRL;
using namespace vrinject;

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t bfType{0x4D42};
    uint32_t bfSize{0};
    uint16_t bfReserved1{0};
    uint16_t bfReserved2{0};
    uint32_t bfOffBits{54};
    uint32_t biSize{40};
    int32_t  biWidth{0};
    int32_t  biHeight{0};
    uint16_t biPlanes{1};
    uint16_t biBitCount{32};
    uint32_t biCompression{0};
    uint32_t biSizeImage{0};
    int32_t  biXPelsPerMeter{0};
    int32_t  biYPelsPerMeter{0};
    uint32_t biClrUsed{0};
    uint32_t biClrImportant{0};
};
#pragma pack(pop)

void SaveBMP(const char* filename, const uint8_t* bgra_data, int width, int height, int rowPitch) {
    BMPHeader header;
    header.biWidth = width;
    header.biHeight = -height; // Top-down
    header.bfSize = sizeof(BMPHeader) + (width * height * 4);
    
    std::ofstream file(filename, std::ios::binary);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    for (int y = 0; y < height; ++y) {
        file.write(reinterpret_cast<const char*>(bgra_data + y * rowPitch), width * 4);
    }
}

int main() {
    std::cout << "Running visual stereo reprojection test...\n";
    
    // 1. Create DX11 Device
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, &featureLevel, &context);
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 device.\n";
        return 1;
    }

    const int WIDTH = 800;
    const int HEIGHT = 600;

    // 2. Create Dummy Color & Depth Textures with a test pattern
    std::vector<uint32_t> colorData(WIDTH * HEIGHT);
    std::vector<float> depthData(WIDTH * HEIGHT);

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            int idx = y * WIDTH + x;
            
            // Draw a central pillar
            if (x > 350 && x < 450 && y > 100 && y < 500) {
                colorData[idx] = 0xFF0000FF; // Red pillar (BGRA)
                depthData[idx] = 0.9f;       // Close depth
            } else {
                // Background checkerboard
                bool isWhite = ((x / 50) % 2) ^ ((y / 50) % 2);
                colorData[idx] = isWhite ? 0xFFFFFFFF : 0xFF404040;
                depthData[idx] = 0.99f;      // Far depth
            }
        }
    }

    D3D11_TEXTURE2D_DESC colorDesc = {};
    colorDesc.Width = WIDTH;
    colorDesc.Height = HEIGHT;
    colorDesc.MipLevels = 1;
    colorDesc.ArraySize = 1;
    colorDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    colorDesc.SampleDesc.Count = 1;
    colorDesc.Usage = D3D11_USAGE_DEFAULT;
    colorDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_SUBRESOURCE_DATA colorInit = {};
    colorInit.pSysMem = colorData.data();
    colorInit.SysMemPitch = WIDTH * 4;

    ComPtr<ID3D11Texture2D> colorTex;
    device->CreateTexture2D(&colorDesc, &colorInit, &colorTex);
    
    ComPtr<ID3D11ShaderResourceView> colorSRV;
    device->CreateShaderResourceView(colorTex.Get(), nullptr, &colorSRV);

    D3D11_TEXTURE2D_DESC depthDesc = colorDesc;
    depthDesc.Format = DXGI_FORMAT_R32_FLOAT;
    
    D3D11_SUBRESOURCE_DATA depthInit = {};
    depthInit.pSysMem = depthData.data();
    depthInit.SysMemPitch = WIDTH * 4;

    ComPtr<ID3D11Texture2D> depthTex;
    device->CreateTexture2D(&depthDesc, &depthInit, &depthTex);
    
    ComPtr<ID3D11ShaderResourceView> depthSRV;
    device->CreateShaderResourceView(depthTex.Get(), nullptr, &depthSRV);

    // 3. Initialize Stereo Renderer
    StereoResourceManager resourceManager(device.Get());
    resourceManager.Initialize(WIDTH, HEIGHT, DXGI_FORMAT_B8G8R8A8_UNORM);
    
    StereoRenderer renderer;
    renderer.SetState(StereoRendererState::READY);

    // 4. Generate Context (Simulating a camera looking down Z axis)
    CameraSnapshot cam;
    cam.position = {0, 0, 0};
    cam.view = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    // Standard perspective proj, 90 deg FOV
    cam.projection = {
        1, 0, 0, 0,
        0, 1.333f, 0, 0,
        0, 0, 1.0f, 1.0f,
        0, 0, -0.1f, 0
    };
    
    DepthSnapshot depthSnap;
    StereoConstants constants;
    constants.ipd = 0.063f; // 63mm IPD
    EyeView leftEye, rightEye;
    
    StereoCameraGenerator::Generate(cam, constants, leftEye, rightEye);
    
    StereoFrameContext ctx = StereoFrameBuilder::Build(1, cam, depthSnap, leftEye, rightEye, constants, WIDTH, HEIGHT);

    // 5. Render Stereo
    renderer.RenderStereoFrame(context.Get(), &resourceManager, ctx, colorSRV.Get(), depthSRV.Get());

    // 6. Readback
    D3D11_TEXTURE2D_DESC stagingDesc = colorDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    
    ComPtr<ID3D11Texture2D> stagingTex;
    device->CreateTexture2D(&stagingDesc, nullptr, &stagingTex);

    // Left Eye
    context->CopyResource(stagingTex.Get(), resourceManager.GetLeftEyeTexture());
    D3D11_MAPPED_SUBRESOURCE mapped;
    context->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    SaveBMP("left_eye.bmp", static_cast<const uint8_t*>(mapped.pData), WIDTH, HEIGHT, mapped.RowPitch);
    context->Unmap(stagingTex.Get(), 0);

    // Right Eye
    context->CopyResource(stagingTex.Get(), resourceManager.GetRightEyeTexture());
    context->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    SaveBMP("right_eye.bmp", static_cast<const uint8_t*>(mapped.pData), WIDTH, HEIGHT, mapped.RowPitch);
    context->Unmap(stagingTex.Get(), 0);

    std::cout << "Visual test complete. Outputs written to left_eye.bmp and right_eye.bmp.\n";
    return 0;
}
