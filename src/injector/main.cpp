// ============================================================================
// main.cpp – Standalone DLL injector for NexVR Engine
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <TlHelp32.h>
#include <Shlwapi.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif
#include <bcrypt.h>


#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "bcrypt.lib")

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

} // namespace

bool InjectDll(DWORD pid, const std::string& dllPath) {
    PrintInfo("Opening process PID %lu ...", pid);
    HANDLE hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        PrintErr("OpenProcess failed: %s", LastErrorMessage().c_str());
        return false;
    }
    PrintOK("Process handle acquired: 0x%p", hProcess);

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
    HANDLE hThread = ::CreateRemoteThread(hProcess, nullptr, 0,
                                          pLoadLibrary, remoteMem,
                                          0, nullptr);
    if (!hThread) {
        PrintErr("CreateRemoteThread failed: %s", LastErrorMessage().c_str());
        ::VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        ::CloseHandle(hProcess);
        return false;
    }

    PrintInfo("Waiting for remote thread to complete ...");
    DWORD waitResult = ::WaitForSingleObject(hThread, 10000); 

    bool success = false;
    if (waitResult == WAIT_OBJECT_0) {
        DWORD exitCode = 0;
        ::GetExitCodeThread(hThread, &exitCode);
        if (exitCode != 0) {
            PrintOK("DLL loaded successfully! Remote HMODULE = 0x%08lX", exitCode);
            success = true;
        } else {
            PrintErr("LoadLibraryA returned NULL in the remote process.");
        }
    } else if (waitResult == WAIT_TIMEOUT) {
        PrintErr("Remote thread timed out (10 s). The target may be hung.");
    } else {
        PrintErr("WaitForSingleObject failed: %s", LastErrorMessage().c_str());
    }

    ::CloseHandle(hThread);
    ::VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
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

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            targetPid = static_cast<DWORD>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--dll") == 0 && i + 1 < argc) {
            dllPath = argv[++i];
        }
    }

    // S3.3: Origin Restriction (Environment variable token and parent process check)
    const char* envToken = std::getenv("NEXVR_AUTH_TOKEN");
    if (!envToken || std::string(envToken).empty()) {
        PrintErr("[ERROR] Unauthorized origin. Missing security token. Please launch via the NexVR Engine UI.");
        return 13;
    }

    DWORD parentPid = 0;
    HANDLE hSnap2 = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
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

    HANDLE hParent = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parentPid);
    if (hParent) {
        wchar_t parentName[MAX_PATH];
        DWORD size = MAX_PATH;
        ::QueryFullProcessImageNameW(hParent, 0, parentName, &size);
        ::CloseHandle(hParent);
        std::wstring pname = parentName;
        if (pname.find(L"Antigravity") == std::wstring::npos &&
            pname.find(L"electron")    == std::wstring::npos &&
            pname.find(L"node")        == std::wstring::npos) {
            PrintErr("[ERROR] Unauthorized caller \u2014 aborting");
            return 22;
        }
    }

    if (targetPid == 0 || dllPath.empty()) {
        PrintErr("[ERROR] Missing required arguments (--pid and --dll).");
        return 1;
    }

    // S3.1: System Process Protection & PID Verification
    HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    bool pidFound = false;
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe{};
        pe.dwSize = sizeof(pe);
        if (::Process32First(hSnap, &pe)) {
            do {
                if (pe.th32ProcessID == targetPid) {
                    pidFound = true;
                    const char* blocked[] = { "csrss.exe", "lsass.exe", "winlogon.exe", "svchost.exe", "System", "smss.exe" };
                    for (const char* b : blocked) {
                        if (_stricmp(pe.szExeFile, b) == 0) {
                            PrintErr("[ERROR] Injection into system process '%s' blocked", pe.szExeFile);
                            ::CloseHandle(hSnap);
                            return 14;
                        }
                    }
                    break;
                }
            } while (::Process32Next(hSnap, &pe));
        }
        ::CloseHandle(hSnap);
    }
    
    if (!pidFound) {
        PrintErr("[ERROR] PID not found in process list");
        return 21;
    }

    // S1.2: Path Traversal Check
    if (dllPath.find("..") != std::string::npos) {
        PrintErr("[ERROR] Path traversal rejected");
        return 10;
    }

    char fullPath[MAX_PATH];
    if (::GetFullPathNameA(dllPath.c_str(), MAX_PATH, fullPath, nullptr) == 0 || dllPath != fullPath) {
        // Enforce absolute paths
        if (::PathIsRelativeA(dllPath.c_str())) {
            PrintErr("[ERROR] Relative DLL paths are blocked for security");
            return 11;
        }
    }

    // S1.3: PE Validation Check
    DWORD attrs = ::GetFileAttributesA(dllPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        PrintErr("DLL not found at: %s", dllPath.c_str());
        return 12;
    }
    
    FILE* f = nullptr;
    fopen_s(&f, dllPath.c_str(), "rb");
    if (f) {
        char mz[2] = {0};
        fread(mz, 1, 2, f);
        fclose(f);
        if (mz[0] != 'M' || mz[1] != 'Z') {
            PrintErr("[ERROR] Invalid PE header in DLL");
            return 12;
        }
    }

    // S1.4: Anti-tamper hash check
#ifdef EXPECTED_DLL_HASH
    std::wstring expectedHash = EXPECTED_DLL_HASH;
#else
    // Fallback if not injected at compile time
    std::wstring expectedHash = L"";
    char hashPath[MAX_PATH];
    strcpy_s(hashPath, dllPath.c_str());
    PathRemoveFileSpecA(hashPath);
    PathAppendA(hashPath, "dll_hash.txt");
    FILE* hf = nullptr;
    if (fopen_s(&hf, hashPath, "r") == 0 && hf) {
        char hBuf[128] = {0};
        fread(hBuf, 1, 64, hf);
        fclose(hf);
        std::string hStr(hBuf);
        expectedHash.assign(hStr.begin(), hStr.end());
    }
#endif

    if (!expectedHash.empty()) {
        std::wstring actualHash = ComputeFileHashSHA256(dllPath);
        if (actualHash.empty() || _wcsicmp(actualHash.c_str(), expectedHash.c_str()) != 0) {
            PrintErr("[ERROR] DLL integrity check failed \u2014 binary may be tampered");
            return 13;
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
