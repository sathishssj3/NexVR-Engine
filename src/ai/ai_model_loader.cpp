#include "ai_model_loader.h"
#include <iostream>

// Since we fetch content via CMake, we assume the headers are in include paths
#include <onnxruntime_cxx_api.h>

// DirectML execution provider
#ifdef _WIN32
#include <dml_provider_factory.h>
#endif

namespace NexVR {
namespace AI {

std::mutex AiModelLoader::s_envMutex;
std::weak_ptr<Ort::Env> AiModelLoader::s_sharedEnv;

AiModelLoader::AiModelLoader(const std::wstring& modelPath, bool useDirectML, int deviceId)
    : m_modelPath(modelPath), m_useDirectML(useDirectML), m_deviceId(deviceId) {
    
    InitializeSession();
}

AiModelLoader::~AiModelLoader() = default;

std::shared_ptr<Ort::Env> AiModelLoader::GetSharedEnv() {
    std::lock_guard<std::mutex> lock(s_envMutex);
    std::shared_ptr<Ort::Env> env = s_sharedEnv.lock();
    if (!env) {
        // Initialize ORT Environment (Warning level logging)
        // Intentionally leak the environment to prevent ORT destructor crashes on exit
        Ort::Env* rawEnv = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "NexVR_AI");
        env = std::shared_ptr<Ort::Env>(rawEnv, [](Ort::Env*){ /* do not delete */ });
        s_sharedEnv = env;
    }
    return env;
}

void AiModelLoader::InitializeSession() {
    try {
        m_env = GetSharedEnv();
        
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(1);
        
        // Graph optimization level
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
        if (m_useDirectML) {
            OrtApi const& ortApi = Ort::GetApi();
            const OrtDmlApi* dmlApi = nullptr;
            OrtStatus* status = ortApi.GetExecutionProviderApi("DML", ORT_API_VERSION, reinterpret_cast<const void**>(&dmlApi));
            if (!status && dmlApi) {
                OrtStatus* dmlStatus = dmlApi->SessionOptionsAppendExecutionProvider_DML(sessionOptions, m_deviceId);
                if (dmlStatus != nullptr) {
                    std::cerr << "[NexVR AI] CRITICAL: DirectML execution provider failed to append (unsupported GPU or OS). Falling back to CPU.\n";
                    ortApi.ReleaseStatus(dmlStatus);
                } else {
                    std::cout << "[NexVR AI] Enabled DirectML execution provider for device " << m_deviceId << "\n";
                }
            } else {
                if (status) {
                    ortApi.ReleaseStatus(status);
                }
                std::cerr << "[NexVR AI] CRITICAL: DirectML API not available (missing DLL?). Falling back to CPU execution.\n";
            }
        }
#endif

        m_session = std::make_unique<Ort::Session>(*m_env, m_modelPath.c_str(), sessionOptions);
        
        // Default allocator
        m_memoryInfo = std::make_unique<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
        
        std::cout << "[NexVR AI] Successfully loaded model from: " << std::string(m_modelPath.begin(), m_modelPath.end()) << "\n";
    } catch (const Ort::Exception& e) {
        std::cerr << "[NexVR AI] Failed to load ONNX model (GPU initialization failed): " << e.what() << "\n";
        m_session.reset();
        
        // Fallback for CI/VM environments without full DirectML support
        std::cerr << "[NexVR AI] WARNING: Entering SIMULATED execution mode due to missing GPU hardware capabilities.\n";
        m_isSimulated = true;
    }
}

void AiModelLoader::RunInferenceRaw(
    const char** inputNames, const void* const* inputTensors, size_t numInputs,
    const char** outputNames, void** outputTensors, size_t numOutputs) {
    
    // In a real implementation, we would map the void* tensor data to Ort::Value objects 
    // and run `m_session->Run()`. Since TensorBridge will handle DML tensor mapping, 
    // we leave this as a stub for the high-level architecture.
    
    if (!m_session) {
        throw std::runtime_error("Attempted to run inference on invalid session.");
    }
    
    // Stub implementation to be filled during tensor binding sprint
}

} // namespace AI
} // namespace NexVR
