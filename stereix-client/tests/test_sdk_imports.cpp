// test_sdk_imports.cpp — Binary audit asserting PE export and import tables of nexvr_sdk.dll.
//
// Verifies:
// 1. All DEC-020 C ABI symbols are exported (both DX11 and DX12).
// 2. Model B device purity: D3D12CreateDevice and D3D11CreateDevice are NEVER imported.
// 3. Zero injector machinery: CreateRemoteThread, VirtualAllocEx, MinHook are NEVER imported.

#include <gtest/gtest.h>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string g_dllPath;

uint32_t RvaToOffset(uint32_t rva, const std::vector<IMAGE_SECTION_HEADER>& sections) {
    for (const auto& sec : sections) {
        if (rva >= sec.VirtualAddress && rva < sec.VirtualAddress + sec.Misc.VirtualSize) {
            return (rva - sec.VirtualAddress) + sec.PointerToRawData;
        }
    }
    return 0;
}

struct PeAnalysis {
    bool valid{false};
    uint16_t dllCharacteristics{0};
    std::set<std::string> exports;
    std::set<std::string> imports;
    std::set<std::string> importedDlls;
};

PeAnalysis AnalyzePeFile(const std::string& path) {
    PeAnalysis result;
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open PE file: " << path << std::endl;
        return result;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize < sizeof(IMAGE_DOS_HEADER)) return result;

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(buffer.data());
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return result;

    if (dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > fileSize) return result;

    auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(buffer.data() + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return result;

    result.dllCharacteristics = ntHeaders->OptionalHeader.DllCharacteristics;

    // Read section headers
    const auto* sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    std::vector<IMAGE_SECTION_HEADER> sections(ntHeaders->FileHeader.NumberOfSections);
    for (size_t i = 0; i < sections.size(); ++i) {
        sections[i] = sectionHeader[i];
    }

    // Parse Exports
    const auto& exportDirHeader = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDirHeader.VirtualAddress != 0 && exportDirHeader.Size != 0) {
        uint32_t exportOffset = RvaToOffset(exportDirHeader.VirtualAddress, sections);
        if (exportOffset != 0 && exportOffset < fileSize) {
            auto* exportDir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(buffer.data() + exportOffset);
            uint32_t namesOffset = RvaToOffset(exportDir->AddressOfNames, sections);
            if (namesOffset != 0) {
                auto* nameRvas = reinterpret_cast<const uint32_t*>(buffer.data() + namesOffset);
                for (DWORD i = 0; i < exportDir->NumberOfNames; ++i) {
                    uint32_t nameOffset = RvaToOffset(nameRvas[i], sections);
                    if (nameOffset != 0 && nameOffset < fileSize) {
                        const char* name = reinterpret_cast<const char*>(buffer.data() + nameOffset);
                        result.exports.insert(name);
                    }
                }
            }
        }
    }

    // Parse Imports
    const auto& importDirHeader = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDirHeader.VirtualAddress != 0 && importDirHeader.Size != 0) {
        uint32_t importOffset = RvaToOffset(importDirHeader.VirtualAddress, sections);
        if (importOffset != 0) {
            auto* importDesc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(buffer.data() + importOffset);
            while (importDesc->Characteristics != 0 || importDesc->Name != 0) {
                uint32_t dllNameOffset = RvaToOffset(importDesc->Name, sections);
                if (dllNameOffset != 0 && dllNameOffset < fileSize) {
                    const char* dllName = reinterpret_cast<const char*>(buffer.data() + dllNameOffset);
                    result.importedDlls.insert(dllName);
                }

                uint32_t thunkRva = importDesc->OriginalFirstThunk ? importDesc->OriginalFirstThunk : importDesc->FirstThunk;
                if (thunkRva != 0) {
                    uint32_t thunkOffset = RvaToOffset(thunkRva, sections);
                    if (thunkOffset != 0) {
                        auto* thunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(buffer.data() + thunkOffset);
                        while (thunk->u1.AddressOfData != 0) {
                            if (!(thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
                                uint32_t ibnOffset = RvaToOffset(static_cast<uint32_t>(thunk->u1.AddressOfData), sections);
                                if (ibnOffset != 0 && ibnOffset + sizeof(IMAGE_IMPORT_BY_NAME) <= fileSize) {
                                    auto* ibn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(buffer.data() + ibnOffset);
                                    result.imports.insert(ibn->Name);
                                }
                            }
                            thunk++;
                        }
                    }
                }
                importDesc++;
            }
        }
    }

    result.valid = true;
    return result;
}

std::string FindSdkDll() {
    if (!g_dllPath.empty() && fs::exists(g_dllPath)) {
        return g_dllPath;
    }

    // Probe common locations relative to current working directory and module
    const std::vector<std::string> candidates = {
        "bin/nexvr_sdk.dll",
        "build/bin/nexvr_sdk.dll",
        "../bin/nexvr_sdk.dll",
        "nexvr_sdk.dll"
    };

    for (const auto& c : candidates) {
        if (fs::exists(c)) {
            return c;
        }
    }

    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        fs::path p(exePath);
        fs::path candidate = p.parent_path() / "nexvr_sdk.dll";
        if (fs::exists(candidate)) {
            return candidate.string();
        }
    }

    return "";
}

}  // namespace

class SdkImportsTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string path = FindSdkDll();
        ASSERT_FALSE(path.empty()) << "nexvr_sdk.dll could not be found. Tested paths: bin/nexvr_sdk.dll, build/bin/nexvr_sdk.dll";
        pe = AnalyzePeFile(path);
        ASSERT_TRUE(pe.valid) << "Failed to parse PE headers of " << path;
    }

    PeAnalysis pe;
};

// ===========================================================================
// Export Surface Tests
// ===========================================================================

TEST_F(SdkImportsTest, ExportsAllDX11Symbols) {
    const std::vector<std::string> dx11Exports = {
        "NexVR_GetGraphicsRequirements",
        "NexVR_InitializeDX11",
        "NexVR_SubmitFrameDX11",
        "NexVR_SubmitFrameWithDepthDX11",
        "NexVR_SupportsDepthSubmission",
        "NexVR_WaitFrame",
        "NexVR_Shutdown",
        "NexVR_GetLastErrorDetail"
    };

    for (const auto& sym : dx11Exports) {
        EXPECT_TRUE(pe.exports.contains(sym)) << "Missing DX11 export symbol: " << sym;
    }
}

TEST_F(SdkImportsTest, ExportsAllDX12Symbols) {
    const std::vector<std::string> dx12Exports = {
        "NexVR_GetGraphicsRequirementsDX12",
        "NexVR_InitializeDX12",
        "NexVR_SubmitFrameDX12",
        "NexVR_SubmitFrameWithDepthDX12"
    };

    for (const auto& sym : dx12Exports) {
        EXPECT_TRUE(pe.exports.contains(sym)) << "Missing DX12 export symbol: " << sym;
    }
}

TEST_F(SdkImportsTest, ExportsAllInputAndHapticSymbols) {
    const std::vector<std::string> inputExports = {
        "NexVR_SyncInput",
        "NexVR_GetControllerState",
        "NexVR_TriggerHaptic",
        "NexVR_StopHaptic"
    };

    for (const auto& sym : inputExports) {
        EXPECT_TRUE(pe.exports.contains(sym)) << "Missing Input/Haptic export symbol: " << sym;
    }
}

TEST_F(SdkImportsTest, ExportsAllStereixSymbols) {
    const std::vector<std::string> stereixExports = {
        "Stereix_GetGraphicsRequirements",
        "Stereix_GetGraphicsRequirementsDX12",
        "Stereix_InitializeDX11",
        "Stereix_InitializeDX12",
        "Stereix_WaitFrame",
        "Stereix_SubmitFrameDX11",
        "Stereix_SubmitFrameWithDepthDX11",
        "Stereix_SupportsDepthSubmission",
        "Stereix_SubmitFrameDX12",
        "Stereix_SubmitFrameWithDepthDX12",
        "Stereix_SyncInput",
        "Stereix_GetControllerState",
        "Stereix_TriggerHaptic",
        "Stereix_StopHaptic",
        "Stereix_Shutdown",
        "Stereix_GetLastErrorDetail"
    };

    for (const auto& sym : stereixExports) {
        EXPECT_TRUE(pe.exports.contains(sym)) << "Missing Stereix export symbol: " << sym;
    }
}

TEST_F(SdkImportsTest, StereixSdkDllAliasBinaryExistsAndMatchesExports) {
    std::string nexvrPath = FindSdkDll();
    fs::path p(nexvrPath);
    fs::path stereixPath = p.parent_path() / "stereix_sdk.dll";
    ASSERT_TRUE(fs::exists(stereixPath)) << "stereix_sdk.dll alias binary must exist next to nexvr_sdk.dll";

    PeAnalysis stereixPe = AnalyzePeFile(stereixPath.string());
    ASSERT_TRUE(stereixPe.valid) << "Failed to parse PE headers of stereix_sdk.dll";

    EXPECT_TRUE(stereixPe.exports.contains("Stereix_SubmitFrameDX11"));
    EXPECT_TRUE(stereixPe.exports.contains("Stereix_SubmitFrameDX12"));
    EXPECT_TRUE(stereixPe.exports.contains("Stereix_WaitFrame"));
    EXPECT_TRUE(stereixPe.exports.contains("NexVR_SubmitFrameDX11"));
}

// ===========================================================================
// Import Hygiene Tests (Purity & Anti-Cheat Safety)
// ===========================================================================

TEST_F(SdkImportsTest, ForbidsDeviceCreationCalls) {
    // Model B operates on the host game's device. Calling D3D12CreateDevice
    // or D3D11CreateDevice violates the purity architecture.
    EXPECT_FALSE(pe.imports.contains("D3D12CreateDevice"))
        << "nexvr_sdk.dll must not import D3D12CreateDevice";
    EXPECT_FALSE(pe.imports.contains("D3D11CreateDevice"))
        << "nexvr_sdk.dll must not import D3D11CreateDevice";
}

TEST_F(SdkImportsTest, ForbidsInjectionPrimitives) {
    // A B2B SDK shipped inside licensed game binaries must have zero injector primitives.
    const std::vector<std::string> forbidden = {
        "CreateRemoteThread",
        "VirtualAllocEx",
        "WriteProcessMemory",
        "OpenProcess",
        "ReadProcessMemory",
        "SetWindowsHookExA",
        "SetWindowsHookExW"
    };

    for (const auto& sym : forbidden) {
        EXPECT_FALSE(pe.imports.contains(sym))
            << "nexvr_sdk.dll imports forbidden injection primitive: " << sym;
    }
}

TEST_F(SdkImportsTest, ForbidsMinHook) {
    // MinHook must never travel with the SDK.
    for (const auto& sym : pe.imports) {
        EXPECT_FALSE(sym.starts_with("MH_"))
            << "nexvr_sdk.dll imports MinHook symbol: " << sym;
    }
}

TEST_F(SdkImportsTest, EnforcesAntiCheatSecurityCharacteristics) {
    // Enterprise anti-cheat engines (Easy Anti-Cheat, BattlEye, Ricochet, Vanguard)
    // require production DLLs to enforce ASLR, High-Entropy 64-bit VA, and Hardware DEP.
    EXPECT_TRUE(pe.dllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE)
        << "ASLR (DYNAMIC_BASE) must be enabled for anti-cheat and security compliance";
    EXPECT_TRUE(pe.dllCharacteristics & IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA)
        << "64-bit High Entropy ASLR must be enabled for anti-cheat and security compliance";
    EXPECT_TRUE(pe.dllCharacteristics & IMAGE_DLLCHARACTERISTICS_NX_COMPAT)
        << "Hardware DEP (NX_COMPAT) must be enabled for anti-cheat and security compliance";
    EXPECT_TRUE(pe.dllCharacteristics & IMAGE_DLLCHARACTERISTICS_GUARD_CF)
        << "Control Flow Guard (GUARD_CF) must be enabled for anti-cheat and security compliance";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (argc > 1) {
        g_dllPath = argv[1];
    }
    return RUN_ALL_TESTS();
}
