#include "core/engine_detector.h"
#include "core/logger.h"
#include "core/engine_scanners/universal_scanner.h"
#include "core/subsystem_context.h"
#include <TlHelp32.h>
#include <algorithm>
#include <cctype>

namespace vrinject {
namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool Contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

std::string WideToUtf8(const wchar_t* value) {
    if (!value) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};

    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
    return result;
}

EngineTuningProfile TuningFor(EngineType type) {
    EngineTuningProfile profile{};
    switch (type) {
        case EngineType::Unity:
            profile.reverseZ = true;
            profile.rowMajorMatrices = false;
            break;
        case EngineType::UnrealEngine4:
        case EngineType::UnrealEngine5:
            profile.reverseZ = true;
            profile.rowMajorMatrices = true;
            break;
        case EngineType::Unknown:
        default:
            break;
    }
    return profile;
}

} // namespace

void EngineDetector::Detect() {
    LOG_INFO("Detecting game engine...");

    // 1. Check if per-game config defines an explicit profile
    const auto* configMgr = SubsystemContext::Get().GetConfig();
    if (configMgr) {
        const auto& config = configMgr->GetConfig();
        if (!config.engineType.empty()) {
            std::string eng = ToLower(config.engineType);
            EngineDetection profileDetection{};
            if (eng.find("unreal5") != std::string::npos || eng.find("ue5") != std::string::npos) {
                profileDetection.type = EngineType::UnrealEngine5;
                profileDetection.versionString = "UE5.x (Profile Override)";
            } else if (eng.find("unreal") != std::string::npos || eng.find("ue4") != std::string::npos) {
                profileDetection.type = EngineType::UnrealEngine4;
                profileDetection.versionString = "UE4.xx (Profile Override)";
            } else if (eng.find("unity") != std::string::npos) {
                profileDetection.type = EngineType::Unity;
                profileDetection.versionString = "Unity (Profile Override)";
            } else {
                profileDetection.type = EngineType::Unknown;
                profileDetection.versionString = "Custom/Profile";
            }
            profileDetection.confidence = 1.0f;
            profileDetection.tuning = TuningFor(profileDetection.type);
            if (config.hasReverseZOverride) profileDetection.tuning.reverseZ = config.reverseZ;
            if (config.hasRowMajorOverride) profileDetection.tuning.rowMajorMatrices = config.rowMajorMatrices;
            ApplyDetection(profileDetection);
            LOG_INFO("EngineDetector: Applied explicit profile override -> %s", profileDetection.versionString.c_str());
            return;
        }
    }

    // 2. Check loaded modules in current process
    EngineDetection moduleDetection = DetectFromModuleNames(EnumerateCurrentProcessModules());
    if (moduleDetection.type != EngineType::Unknown) {
        ApplyDetection(moduleDetection);
        LOG_INFO("Detected %s engine from modules with confidence %.2f.",
                 m_versionString.c_str(),
                 m_detection.confidence);
        return;
    }

    // 3. Check for UnrealWindow class in current process
    if (CheckForUnrealWindow()) {
        LOG_INFO("Detected UnrealWindow class - Game is running Unreal Engine.");
        m_detection.type = EngineType::UnrealEngine4;
        m_detection.versionString = "UE4.xx";
        m_detection.confidence = 0.80f;
        m_detection.tuning = TuningFor(m_detection.type);
        ApplyDetection(m_detection);
        ScanForUnrealSignatures();
        return;
    }

    // 4. Check executable path structure for standard Unreal Engine folder layout (e.g. Binaries/Win64)
    char exeName[MAX_PATH]{};
    if (GetModuleFileNameA(NULL, exeName, MAX_PATH)) {
        std::string exeStr = ToLower(exeName);
        if (exeStr.find("binaries\\win64") != std::string::npos || exeStr.find("binaries/win64") != std::string::npos) {
            LOG_INFO("Detected Unreal Engine directory structure (Binaries/Win64).");
            m_detection.type = EngineType::UnrealEngine4;
            m_detection.versionString = "Unreal Engine (Path Structure)";
            m_detection.confidence = 0.75f;
            m_detection.tuning = TuningFor(m_detection.type);
            ApplyDetection(m_detection);
            ScanForUnrealSignatures();
            return;
        }
    }

    LOG_WARN("Engine type unknown. Defaulting to generic hooks and Universal Memory Scanner.");
    m_detection = {};
    m_detection.versionString = "Unknown/custom";
    m_detection.tuning = TuningFor(EngineType::Unknown);
    ApplyDetection(m_detection);
    SubsystemContext::Get().GetUniversalScanner()->Initialize();
}

EngineDetection EngineDetector::DetectFromModuleNames(const std::vector<std::string>& moduleNames) {
    EngineDetection detection{};
    detection.versionString = "Unknown/custom";
    detection.tuning = TuningFor(EngineType::Unknown);

    bool hasUnityPlayer = false;
    bool hasIl2cpp = false;
    bool hasMono = false;
    bool hasUnreal = false;
    bool hasUE4 = false;
    bool hasUE5 = false;

    for (const auto& moduleName : moduleNames) {
        std::string name = ToLower(moduleName);
        hasUnityPlayer = hasUnityPlayer || Contains(name, "unityplayer.dll");
        hasIl2cpp = hasIl2cpp || Contains(name, "gameassembly.dll") || Contains(name, "il2cpp");
        hasMono = hasMono || Contains(name, "mono-2.0-bdwgc.dll") || Contains(name, "mono.dll");

        hasUE5 = hasUE5 || Contains(name, "unrealeditor-") || Contains(name, "ue5") || Contains(name, "unrealengine5");
        hasUE4 = hasUE4 || Contains(name, "ue4") || Contains(name, "unrealengine4") || Contains(name, "physx3");
        hasUnreal = hasUnreal || hasUE4 || hasUE5 || Contains(name, "unrealengine") || Contains(name, "unrealpak");
    }

    if (hasUnityPlayer || hasIl2cpp || hasMono) {
        detection.type = EngineType::Unity;
        detection.confidence = hasUnityPlayer ? 0.95f : 0.75f;
        if (hasIl2cpp) {
            detection.versionString = "Unity (IL2CPP)";
            detection.confidence = (std::max)(detection.confidence, 0.9f);
        } else if (hasMono) {
            detection.versionString = "Unity (Mono)";
        } else {
            detection.versionString = "Unity";
        }
        detection.tuning = TuningFor(detection.type);
        return detection;
    }

    if (hasUnreal) {
        detection.type = hasUE5 ? EngineType::UnrealEngine5 : EngineType::UnrealEngine4;
        detection.versionString = hasUE5 ? "UE5.x" : "UE4.xx";
        detection.confidence = (hasUE4 || hasUE5) ? 0.9f : 0.7f;
        detection.tuning = TuningFor(detection.type);
        return detection;
    }

    return detection;
}

bool EngineDetector::CheckForUnrealWindow() {
    // Only search windows belonging to our process
    struct EnumContext {
        DWORD pid;
        bool found;
    } context{GetCurrentProcessId(), false};
    
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* context = reinterpret_cast<EnumContext*>(lParam);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == context->pid) {
            char className[256];
            if (GetClassNameA(hwnd, className, sizeof(className))) {
                if (std::string(className).find("UnrealWindow") != std::string::npos) {
                    context->found = true;
                    return FALSE; // Stop enumerating
                }
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));
    
    return context.found;
}

bool EngineDetector::ScanForUnrealSignatures() {
    // The detailed version detection is now handled by UnrealScanner::DetermineVersion().
    // Here we just set a basic version string.
    if (m_engineType == EngineType::UnrealEngine5) {
        m_versionString = "UE5.x";
    } else if (m_engineType == EngineType::UnrealEngine4) {
        m_versionString = "UE4.xx";
    }
    m_detection.versionString = m_versionString;
    return true;
}

std::vector<std::string> EngineDetector::EnumerateCurrentProcessModules() const {
    std::vector<std::string> modules;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return modules;

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            modules.emplace_back(WideToUtf8(entry.szModule));
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return modules;
}

void EngineDetector::ApplyDetection(const EngineDetection& detection) {
    m_detection = detection;
    m_engineType = detection.type;
    m_versionString = detection.versionString;
}

} // namespace vrinject
