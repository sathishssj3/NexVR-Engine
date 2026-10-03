// ============================================================================
// main.cpp – Standalone DLL injector for Stereix Engine
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <shellapi.h>
#include <TlHelp32.h>
#include <Shlwapi.h>
#include <Psapi.h>

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif
#include <bcrypt.h>
#include "expected_hash.h"


#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "Psapi.lib")

namespace {

enum class Colour : WORD {
    Default = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    Green   = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    Yellow  = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    Red     = FOREGROUND_RED | FOREGROUND_INTENSITY,
    Cyan    = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
};

void SetColour(Colour c) {
    HANDLE hCon = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (hCon && hCon != INVALID_HANDLE_VALUE)
        ::SetConsoleTextAttribute(hCon, static_cast<WORD>(c));
}

void PrintStatus(const char* tag, Colour c, const char* fmt, ...) {
    SetColour(c);
    std::printf("[%s] ", tag);
    SetColour(Colour::Default);

    va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
    std::printf("\n");
}

void PrintOK(const char* fmt, ...)   {
    va_list a; va_start(a, fmt);
    char buf[512]; std::vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
    PrintStatus("  OK  ", Colour::Green, "%s", buf);
}

void PrintInfo(const char* fmt, ...) {
    va_list a; va_start(a, fmt);
    char buf[512]; std::vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
    PrintStatus(" INFO ", Colour::Cyan, "%s", buf);
}

void PrintWarn(const char* fmt, ...) {
    va_list a; va_start(a, fmt);
    char buf[512]; std::vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
    PrintStatus(" WARN ", Colour::Yellow, "%s", buf);
}

void PrintErr(const char* fmt, ...)  {
    va_list a; va_start(a, fmt);
    char buf[512]; std::vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
    PrintStatus(" FAIL ", Colour::Red, "%s", buf);
}

std::string LastErrorMessage() {
    DWORD err = ::GetLastError();
    if (err == 0) return "No error";

    char* msgBuf = nullptr;
    DWORD len = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        reinterpret_cast<LPSTR>(&msgBuf), 0, nullptr);

    std::string result;
    if (len && msgBuf) {
        result.assign(msgBuf, len);
        while (!result.empty() && (result.back() == '\r' || result.back() == '\n'))
            result.pop_back();
        ::LocalFree(msgBuf);
    }
    return result;
}

std::wstring ComputeFileHashSHA256(const std::string& filePath) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    std::wstring hashResult = L"";
    
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) return L"";

    DWORD cbData = 0, cbHashObject = 0;
    if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return L"";
    }

    std::vector<BYTE> pbHashObject(cbHashObject);
    DWORD cbHash = 0;
    if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return L"";
    }

    std::vector<BYTE> pbHash(cbHash);
    if (BCryptCreateHash(hAlg, &hHash, pbHashObject.data(), cbHashObject, NULL, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return L"";
    }

    FILE* f = nullptr;
    if (fopen_s(&f, filePath.c_str(), "rb") == 0 && f) {
        BYTE buffer[8192];
        size_t read;
        while ((read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
            BCryptHashData(hHash, buffer, (ULONG)read, 0);
        }
        fclose(f);
        
        if (BCryptFinishHash(hHash, pbHash.data(), cbHash, 0) == 0) {
            wchar_t hex[3];
            for (DWORD i = 0; i < cbHash; i++) {
                swprintf_s(hex, L"%02X", pbHash[i]);
                hashResult += hex;
            }
        }
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return hashResult;
}

bool IsSameFile(const std::string& path1, const std::string& path2) {
    if (_stricmp(path1.c_str(), path2.c_str()) == 0) return true;
    HANDLE h1 = ::CreateFileA(path1.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h1 == INVALID_HANDLE_VALUE) return false;
    HANDLE h2 = ::CreateFileA(path2.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h2 == INVALID_HANDLE_VALUE) {
        ::CloseHandle(h1);
        return false;
    }
    BY_HANDLE_FILE_INFORMATION info1{}, info2{};
    bool same = false;
    if (::GetFileInformationByHandle(h1, &info1) && ::GetFileInformationByHandle(h2, &info2)) {
        same = (info1.dwVolumeSerialNumber == info2.dwVolumeSerialNumber &&
                info1.nFileIndexHigh == info2.nFileIndexHigh &&
                info1.nFileIndexLow == info2.nFileIndexLow);
    }
    ::CloseHandle(h1);
    ::CloseHandle(h2);
    return same;
}

} // namespace

bool InjectDll(DWORD pid, const std::string& dllPath) {
    PrintInfo("Opening process PID %lu ...", pid);
    HANDLE hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        PrintErr("OpenProcess failed: %s", LastErrorMessage().c_str());
        return false;
    }
    PrintOK("Process handle acquired: 0x%p", hProcess);

    BOOL isWow64 = FALSE;
    if (::IsWow64Process(hProcess, &isWow64) && isWow64) {
        PrintErr("Target process is 32-bit (x86). Stereix Engine only supports 64-bit (x64) games.");
        ::CloseHandle(hProcess);
        return false;
    }

    // L-4: Target process validation - verify working set is > 1MB to avoid uninitialized/ghost processes
    PROCESS_MEMORY_COUNTERS pmc{};
    if (::GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        if (pmc.WorkingSetSize < 1024 * 1024) {
            PrintErr("Target process working set is too small (< 1 MB). Process may be uninitialized or exiting.");
            ::CloseHandle(hProcess);
            return false;
        }
    }

    SIZE_T pathLen = dllPath.size() + 1;
    void* remoteMem = ::VirtualAllocEx(hProcess, nullptr, pathLen,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        PrintErr("VirtualAllocEx failed: %s", LastErrorMessage().c_str());
        ::CloseHandle(hProcess);
        return false;
    }

    SIZE_T written = 0;
    if (!::WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), pathLen, &written)) {
        PrintErr("WriteProcessMemory failed: %s", LastErrorMessage().c_str());
        ::VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        ::CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = ::GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        PrintErr("Could not get kernel32.dll handle");
        ::VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        ::CloseHandle(hProcess);
        return false;
    }

    auto pLoadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        ::GetProcAddress(hKernel32, "LoadLibraryA"));
    if (!pLoadLibrary) {
        PrintErr("Could not resolve LoadLibraryA");
        ::VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        ::CloseHandle(hProcess);
        return false;
    }

    PrintInfo("Creating remote thread ...");
    
    bool success = false;
    for (int attempt = 1; attempt <= 10; ++attempt) {
        HANDLE hThread = ::CreateRemoteThread(hProcess, nullptr, 0,
                                              pLoadLibrary, remoteMem,
                                              0, nullptr);
        if (!hThread) {
            PrintErr("CreateRemoteThread failed: %s", LastErrorMessage().c_str());
            break;
        }

        PrintInfo("Waiting for remote thread to complete (Attempt %d/10)...", attempt);
        DWORD waitResult = ::WaitForSingleObject(hThread, 10000); 

        if (waitResult == WAIT_OBJECT_0) {
            DWORD exitCode = 0;
            ::GetExitCodeThread(hThread, &exitCode);
            bool isLoaded = (exitCode != 0);
            if (!isLoaded) {
                // L-1: Verify load via EnumProcessModules in case low 32 bits of 64-bit HMODULE were 0
                HMODULE hMods[1024];
                DWORD cbNeeded = 0;
                if (::EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
                    DWORD numMods = cbNeeded / sizeof(HMODULE);
                    char modName[MAX_PATH];
                    for (DWORD m = 0; m < numMods; ++m) {
                        if (::GetModuleFileNameExA(hProcess, hMods[m], modName, MAX_PATH)) {
                            if (strstr(modName, "vrinject.dll")) {
                                isLoaded = true;
                                break;
                            }
                        }
                    }
                }
            }

            if (isLoaded) {
                PrintOK("DLL loaded successfully! Remote HMODULE = 0x%08lX", exitCode);
                success = true;
                ::CloseHandle(hThread);
                break;
            } else {
                if (attempt < 10) {
                    PrintWarn("LoadLibraryA returned NULL. Retrying in 500ms... (AV lock suspected)");
                    ::Sleep(500);
                } else {
                    PrintErr("LoadLibraryA returned NULL in the remote process after 10 attempts.");
                }
            }
        } else if (waitResult == WAIT_TIMEOUT) {
            PrintErr("Remote thread timed out (10 s). The target may be hung.");
            ::CloseHandle(hThread);
            // H-4: Do NOT free remoteMem here because the remote thread may still be executing.
            remoteMem = nullptr;
            break;
        } else {
            PrintErr("WaitForSingleObject failed: %s", LastErrorMessage().c_str());
            ::CloseHandle(hThread);
            remoteMem = nullptr;
            break;
        }

        ::CloseHandle(hThread);
    }

    if (remoteMem) {
        ::VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    }
    ::CloseHandle(hProcess);

    return success;
}

int main(int argc, char* argv[]) {
    HANDLE hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (hOut && ::GetConsoleMode(hOut, &mode)) {
        ::SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    DWORD targetPid = 0;
    std::string dllPath;
    std::string copySrc;
    std::string copyDst;
    bool allowUnverifiedHotfix = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            targetPid = static_cast<DWORD>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--dll") == 0 && i + 1 < argc) {
            dllPath = argv[++i];
        } else if (std::strcmp(argv[i], "--copy-src") == 0 && i + 1 < argc) {
            copySrc = argv[++i];
        } else if (std::strcmp(argv[i], "--copy-dst") == 0 && i + 1 < argc) {
            copyDst = argv[++i];
        } else if (std::strcmp(argv[i], "--allow-unverified-hotfix") == 0) {
            allowUnverifiedHotfix = true;
        }
    }

    // S3.3: Origin Restriction (Authentication runs FIRST before any file or process actions - C-3)
    const char* envToken = std::getenv("STEREIX_AUTH_TOKEN");
    if (!envToken) envToken = std::getenv("NEXVR_AUTH_TOKEN");
    if (!envToken || std::string(envToken).empty()) {
        PrintErr("[ERROR] Unauthorized origin. Missing security token. Please launch via the Stereix Engine UI.");
        return 13;
    }

    DWORD parentPid = 0;
    HANDLE hSnap2 = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap2 == INVALID_HANDLE_VALUE) {
        PrintErr("[ERROR] Failed to query caller processes — aborting.");
        return 22;
    }
    PROCESSENTRY32W pe2 = { sizeof(pe2) };
    DWORD selfPid = ::GetCurrentProcessId();
    if (::Process32FirstW(hSnap2, &pe2)) {
        do {
            if (pe2.th32ProcessID == selfPid) {
                parentPid = pe2.th32ParentProcessID;
                break;
            }
        } while (::Process32NextW(hSnap2, &pe2));
    }
    ::CloseHandle(hSnap2);

    if (parentPid == 0) {
        PrintErr("[ERROR] Unable to resolve parent process — aborting");
        return 22;
    }

    HANDLE hParent = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parentPid);
    if (!hParent) {
        PrintErr("[ERROR] Unable to inspect parent process — aborting");
        return 22;
    }
    wchar_t parentName[MAX_PATH] = {0};
    DWORD parentNameSize = MAX_PATH;
    if (!::QueryFullProcessImageNameW(hParent, 0, parentName, &parentNameSize)) {
        ::CloseHandle(hParent);
        PrintErr("[ERROR] Unable to verify caller process identity — aborting");
        return 22;
    }
    ::CloseHandle(hParent);

    std::wstring pname = parentName;
    for (auto& c : pname) c = towlower(c);
    if (pname.find(L"antigravity") == std::wstring::npos &&
        pname.find(L"electron")    == std::wstring::npos &&
        pname.find(L"nexvr")       == std::wstring::npos &&
        pname.find(L"node")        == std::wstring::npos &&
        pname.find(L"powershell")  == std::wstring::npos &&
        pname.find(L"cmd")         == std::wstring::npos &&
        pname.find(L"svchost")     == std::wstring::npos &&
        pname.find(L"consent")     == std::wstring::npos) {
        PrintErr("[ERROR] Unauthorized caller — aborting");
        return 22;
    }

    // S3.4: Anti-Cheat Protection Tripwire (M-5: covers BattlEye variants and active anti-cheat engines)
    const char* strict_ac_blocklist[] = { 
        "vgc.exe", 
        "vgtray.exe", 
        "easyanticheat.exe", 
        "easyanticheat_eos.exe", 
        "beservice.exe",
        "beservice_x64.exe",
        "beservice.dll",
        "ricochet.exe",
        "faceitservice.exe"
    };
    HANDLE hSnapAC = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapAC != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 peAC{};
        peAC.dwSize = sizeof(peAC);
        if (::Process32First(hSnapAC, &peAC)) {
            do {
                std::string pName = peAC.szExeFile;
                for (char& c : pName) c = static_cast<char>(std::tolower(c));
                for (const char* blocked : strict_ac_blocklist) {
                    if (pName.find(blocked) != std::string::npos) {
                        PrintErr("[SECURITY] Active Anti-Cheat process detected (%s). Injection REFUSED to protect account from bans.", peAC.szExeFile);
                        ::CloseHandle(hSnapAC);
                        return 15;
                    }
                }
            } while (::Process32Next(hSnapAC, &peAC));
        }
        ::CloseHandle(hSnapAC);
    }

    // Validate required arguments
    if (targetPid <= 4 || dllPath.empty()) {
        PrintErr("[ERROR] Missing or invalid required arguments (--pid and --dll). Target PID must be > 4.");
        return 1;
    }

    // S3.1: System Process Protection & PID Verification (L-4: fail closed)
    bool pidFound = false;
    for (int retry = 0; retry < 10 && !pidFound; ++retry) {
        HANDLE hTemp = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, targetPid);
        if (hTemp) {
            PROCESS_MEMORY_COUNTERS pmc{};
            if (::GetProcessMemoryInfo(hTemp, &pmc, sizeof(pmc))) {
                if (pmc.WorkingSetSize >= 1024 * 1024) {
                    pidFound = true;
                }
            }
            ::CloseHandle(hTemp);
        }
        if (!pidFound) {
            ::Sleep(200);
        }
    }
    
    if (!pidFound) {
        PrintErr("[ERROR] Real Game Process (PID %lu) not found, exited, or insufficient working set.", targetPid);
        return 21;
    }

    // S1.2: Path Traversal Check
    if (dllPath.find("..") != std::string::npos ||
        (!copySrc.empty() && copySrc.find("..") != std::string::npos) ||
        (!copyDst.empty() && copyDst.find("..") != std::string::npos)) {
        PrintErr("[ERROR] Path traversal rejected");
        return 10;
    }

    char fullPath[MAX_PATH];
    if (::GetFullPathNameA(dllPath.c_str(), MAX_PATH, fullPath, nullptr) == 0 || dllPath != fullPath) {
        if (::PathIsRelativeA(dllPath.c_str())) {
            PrintErr("[ERROR] Relative DLL paths are blocked for security");
            return 11;
        }
    }

    // S1.3: Source PE Validation Check (fail closed)
    DWORD attrs = ::GetFileAttributesA(dllPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        PrintErr("DLL not found at: %s", dllPath.c_str());
        return 12;
    }
    
    FILE* f = nullptr;
    if (fopen_s(&f, dllPath.c_str(), "rb") != 0 || !f) {
        PrintErr("[ERROR] Failed to open DLL for PE header verification: %s", dllPath.c_str());
        return 12;
    }
    char mz[2] = {0};
    size_t bytesRead = fread(mz, 1, 2, f);
    fclose(f);
    if (bytesRead != 2 || mz[0] != 'M' || mz[1] != 'Z') {
        PrintErr("[ERROR] Invalid PE header in DLL");
        return 12;
    }

    // S1.4: DLL Hash Verification (C-1: fail closed unless explicit operator override is granted)
    std::wstring computedHash = ComputeFileHashSHA256(dllPath);
    if (computedHash.empty()) {
        PrintErr("[ERROR] Failed to compute SHA-256 hash of DLL: %s", dllPath.c_str());
        return 14;
    }
    if (computedHash != EXPECTED_DLL_HASH) {
        if (allowUnverifiedHotfix) {
            PrintWarn("[SECURITY WARNING] DLL hash differs from build baseline, but --allow-unverified-hotfix was explicitly granted.");
        } else {
            PrintErr("[SECURITY ERROR] DLL hash verification failed!");
            PrintErr("  Expected: %ls", EXPECTED_DLL_HASH);
            PrintErr("  Computed: %ls", computedHash.c_str());
            PrintErr("Injection aborted to prevent untrusted code execution. Use --allow-unverified-hotfix if deploying an authorized OTA hotfix.");
            return 14;
        }
    } else {
        PrintOK("DLL hash verified against build baseline.");
    }

    // Auto-resolve copy paths if omitted
    char selfExePath[MAX_PATH] = {0};
    ::GetModuleFileNameA(NULL, selfExePath, MAX_PATH);
    std::string selfDir = selfExePath;
    size_t lastSlash = selfDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        selfDir = selfDir.substr(0, lastSlash);
    }

    if (copySrc.empty()) {
        copySrc = selfDir;
    }

    if (copyDst.empty() && targetPid > 4) {
        HANDLE hTargetProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetPid);
        if (hTargetProc) {
            char targetExeBuf[MAX_PATH] = {0};
            DWORD bufSize = MAX_PATH;
            if (::QueryFullProcessImageNameA(hTargetProc, 0, targetExeBuf, &bufSize)) {
                std::string targetExeStr = targetExeBuf;
                size_t slashPos = targetExeStr.find_last_of("\\/");
                if (slashPos != std::string::npos) {
                    copyDst = targetExeStr.substr(0, slashPos);
                }
            }
            ::CloseHandle(hTargetProc);
        }
    }

    // Copy dependencies to game directory using elevated permissions (only runs AFTER auth & origin checks succeed)
    if (!copySrc.empty() && !copyDst.empty()) {
        PrintInfo("Synchronizing engine dependencies from %s to %s", copySrc.c_str(), copyDst.c_str());

        // Parse semicolon- or comma-separated source directories
        std::vector<std::string> srcDirs;
        size_t start = 0;
        while (start < copySrc.size()) {
            size_t delim = copySrc.find_first_of(";,\n\r", start);
            std::string dir = (delim == std::string::npos) ? copySrc.substr(start) : copySrc.substr(start, delim - start);
            while (!dir.empty() && (dir.front() == ' ' || dir.front() == '"' || dir.front() == '\'')) dir.erase(dir.begin());
            while (!dir.empty() && (dir.back() == ' ' || dir.back() == '"' || dir.back() == '\'')) dir.pop_back();
            if (!dir.empty() && ::GetFileAttributesA(dir.c_str()) != INVALID_FILE_ATTRIBUTES) {
                srcDirs.push_back(dir);
            }
            if (delim == std::string::npos) break;
            start = delim + 1;
        }
        if (srcDirs.empty()) {
            srcDirs.push_back(selfDir);
        }

        // 1. Deploy vrinject.dll: prioritize the explicit dllPath requested by launcher
        std::string targetDllDst = copyDst + "\\vrinject.dll";
        std::string dllSourceToDeploy = "";
        if (!dllPath.empty() && ::GetFileAttributesA(dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            char fullDllPath[MAX_PATH] = {0};
            char fullDstPath[MAX_PATH] = {0};
            ::GetFullPathNameA(dllPath.c_str(), MAX_PATH, fullDllPath, nullptr);
            ::GetFullPathNameA(targetDllDst.c_str(), MAX_PATH, fullDstPath, nullptr);
            if (!IsSameFile(fullDllPath, targetDllDst)) {
                dllSourceToDeploy = dllPath;
            }
        }
        if (dllSourceToDeploy.empty()) {
            for (const auto& d : srcDirs) {
                std::string cand = d + "\\vrinject.dll";
                if (::GetFileAttributesA(cand.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    char fullCandPath[MAX_PATH] = {0};
                    char fullDstPath[MAX_PATH] = {0};
                    ::GetFullPathNameA(cand.c_str(), MAX_PATH, fullCandPath, nullptr);
                    ::GetFullPathNameA(targetDllDst.c_str(), MAX_PATH, fullDstPath, nullptr);
                    if (!IsSameFile(fullCandPath, targetDllDst)) {
                        dllSourceToDeploy = cand;
                        break;
                    }
                }
            }
        }

        if (!dllSourceToDeploy.empty()) {
            if (::GetFileAttributesA(targetDllDst.c_str()) != INVALID_FILE_ATTRIBUTES) {
                std::string dstOld = targetDllDst + ".old";
                ::DeleteFileA(dstOld.c_str());
                ::MoveFileA(targetDllDst.c_str(), dstOld.c_str());
            }
            if (::CopyFileA(dllSourceToDeploy.c_str(), targetDllDst.c_str(), FALSE)) {
                PrintOK("Deployed vrinject.dll to target directory from %s", dllSourceToDeploy.c_str());
                dllPath = targetDllDst;
            } else {
                PrintWarn("Could not deploy vrinject.dll to target directory (Error: %lu). Retaining source DLL path.", ::GetLastError());
            }
        }

        // 2. Synchronize support runtime DLLs (onnxruntime.dll, DirectML.dll)
        const char* supportDlls[] = {"onnxruntime.dll", "DirectML.dll"};
        for (const char* sf : supportDlls) {
            std::string dst = copyDst + "\\" + sf;
            for (const auto& d : srcDirs) {
                std::string src = d + "\\" + sf;
                if (::GetFileAttributesA(src.c_str()) != INVALID_FILE_ATTRIBUTES && !IsSameFile(src, dst)) {
                    if (::GetFileAttributesA(dst.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        std::string dstOld = dst + ".old";
                        ::DeleteFileA(dstOld.c_str());
                        ::MoveFileA(dst.c_str(), dstOld.c_str());
                    }
                    if (::CopyFileA(src.c_str(), dst.c_str(), FALSE)) {
                        PrintOK("Deployed %s to target directory.", sf);
                    }
                    break;
                }
            }
        }

        // 3. Synchronize vrinject.json ONLY if destination does not already exist
        std::string jsonDst = copyDst + "\\vrinject.json";
        if (::GetFileAttributesA(jsonDst.c_str()) == INVALID_FILE_ATTRIBUTES) {
            for (const auto& d : srcDirs) {
                std::string jsonSrc = d + "\\vrinject.json";
                if (::GetFileAttributesA(jsonSrc.c_str()) != INVALID_FILE_ATTRIBUTES && !IsSameFile(jsonSrc, jsonDst)) {
                    if (::CopyFileA(jsonSrc.c_str(), jsonDst.c_str(), FALSE)) {
                        PrintOK("Deployed initial vrinject.json to target directory.");
                    }
                    break;
                }
            }
        } else {
            PrintInfo("Preserving existing per-game vrinject.json in target directory.");
        }
    }

    // Re-verify deployed DLL integrity before injection to prevent TOCTOU
    {
        FILE* verifyF = nullptr;
        if (fopen_s(&verifyF, dllPath.c_str(), "rb") != 0 || !verifyF) {
            PrintErr("[ERROR] Unable to open target DLL for final pre-injection check: %s", dllPath.c_str());
            return 12;
        }
        char mz[2] = {0};
        size_t n = fread(mz, 1, 2, verifyF);
        fclose(verifyF);
        if (n != 2 || mz[0] != 'M' || mz[1] != 'Z') {
            PrintErr("[ERROR] Target DLL image corrupted before injection");
            return 12;
        }

        std::wstring finalHash = ComputeFileHashSHA256(dllPath);
        if (finalHash != EXPECTED_DLL_HASH && !allowUnverifiedHotfix) {
            PrintErr("[SECURITY ERROR] Final DLL hash check failed immediately before injection!");
            return 14;
        }
    }

    PrintInfo("Target PID:  %lu", targetPid);
    PrintInfo("DLL path:    %s", dllPath.c_str());
    std::printf("\n");

    bool ok = InjectDll(targetPid, dllPath);

    std::printf("\n");
    if (ok) {
        PrintOK("Injection complete. Check vrinject.log for hook status.");
    } else {
        PrintErr("Injection failed. See messages above for details.");
    }

    SetColour(Colour::Default);
    return ok ? 0 : 1;
}
