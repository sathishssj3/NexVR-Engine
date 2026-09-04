#pragma once

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <mutex>

// Fwd declarations for ONNX Runtime to avoid exposing it everywhere
namespace Ort {
    class Env;
    class Session;
    class MemoryInfo;
    struct SessionOptions;
}

namespace NexVR {
namespace AI {

// Base interface for all ONNX AI models
class AiModelLoader {
public:
    AiModelLoader(const std::wstring& modelPath, bool useDirectML, int deviceId = 0);
    virtual ~AiModelLoader();

    // Prevent copying
    AiModelLoader(const AiModelLoader&) = delete;
    AiModelLoader& operator=(const AiModelLoader&) = delete;

    // Checks if the model loaded successfully
    bool IsValid() const { return m_session != nullptr; }
    bool IsSimulated() const { return m_isSimulated; }

    // Run inference synchronously (blocking)
    // Subclasses should wrap this to provide typed inputs/outputs
    void RunInferenceRaw(
        const char** inputNames, const void* const* inputTensors, size_t numInputs,
        const char** outputNames, void** outputTensors, size_t numOutputs);

protected:
    std::wstring m_modelPath;
    bool m_useDirectML;
    bool m_isSimulated = false;
    int m_deviceId;

    // Shared ONNX environment across all instances (initialized once)
    static std::shared_ptr<Ort::Env> GetSharedEnv();
    static std::mutex s_envMutex;
    static std::weak_ptr<Ort::Env> s_sharedEnv;

    std::shared_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::Session> m_session;
    std::unique_ptr<Ort::MemoryInfo> m_memoryInfo;

    void InitializeSession();
};

} // namespace AI
} // namespace NexVR
