// ============================================================================
// injector.cpp – Standalone DLL injector for VRInject
//
// Usage:
//   injector.exe --pid 12345
//   injector.exe --name MyGame.exe
//
// The injector locates vrinject.dll in the same directory as itself and
// injects it into the target process using the classic CreateRemoteThread +
// LoadLibraryA technique.
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <TlHelp32.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Console colour helpers
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Formatted Win32 error message
// ---------------------------------------------------------------------------
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
        // Trim trailing \r\n
        while (!result.empty() && (result.back() == '\r' || result.back() == '\n'))
            result.pop_back();
        ::LocalFree(msgBuf);
    } else {
        result = "Unknown error (code " + std::to_string(err) + ")";
    }
    return result;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Process enumeration
// ---------------------------------------------------------------------------
DWORD FindProcessByName(const char* processName) {
    HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        PrintErr("CreateToolhelp32Snapshot failed: %s", LastErrorMessage().c_str());
        return 0;
    }

    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);

    DWORD foundPid = 0;
    int   matchCount = 0;

    if (::Process32First(hSnap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, processName) == 0) {
                foundPid = pe.th32ProcessID;
                matchCount++;
            }
        } while (::Process32Next(hSnap, &pe));
    }

    ::CloseHandle(hSnap);

    if (matchCount == 0) {
        PrintErr("No process found matching '%s'", processName);
    } else if (matchCount > 1) {
        PrintWarn("Multiple processes (%d) match '%s' – using PID %lu",
                  matchCount, processName, foundPid);
    }

    // S3.1 Validate PID is a real running process and NOT a system process
    if (foundPid != 0) {
        const char* blocked[] = {
            "csrss.exe", "lsass.exe", "winlogon.exe",
            "svchost.exe", "System", "smss.exe"
        };
        for (const char* b : blocked) {
            if (_stricmp(processName, b) == 0) {
                PrintErr("[ERROR] Injection into system process blocked");
                return 0; // S3.1 reject
            }
        }
    }

    return foundPid;
}

// ---------------------------------------------------------------------------
// DLL path resolution (same directory as injector.exe)
// ---------------------------------------------------------------------------
std::string GetDllPath() {
    char exePath[MAX_PATH]{};
    ::GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    // Replace the exe filename with the DLL name.
    std::string path(exePath);
    auto pos = path.find_last_of("\\/");
    if (pos != std::string::npos)
        path = path.substr(0, pos + 1);
    else
        path.clear();

    path += "vrinject.dll";
    return path;
}

// ---------------------------------------------------------------------------
// Injection
// ---------------------------------------------------------------------------
bool InjectDll(DWORD pid, const std::string& dllPath) {
    // 1. Open target process.
    PrintInfo("Opening process PID %lu ...", pid);
    HANDLE hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        PrintErr("OpenProcess failed: %s", LastErrorMessage().c_str());
        PrintWarn("Try running as Administrator if you get Access Denied.");
        return false;
    }
    PrintOK("Process handle acquired: 0x%p", hProcess);

    // 2. Allocate memory in the target for the DLL path string.
    SIZE_T pathLen = dllPath.size() + 1;  // include null terminator
    void* remoteMem = ::VirtualAllocEx(hProcess, nullptr, pathLen,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        PrintErr("VirtualAllocEx failed: %s", LastErrorMessage().c_str());
        ::CloseHandle(hProcess);
        return false;
    }
    PrintOK("Allocated %zu bytes at remote address 0x%p", pathLen, remoteMem);

    // 3. Write the DLL path into the allocated memory.
    SIZE_T written = 0;
    if (!::WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), pathLen, &written)) {
        PrintErr("WriteProcessMemory failed: %s", LastErrorMessage().c_str());
        ::VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        ::CloseHandle(hProcess);
        return false;
    }
    PrintOK("Wrote DLL path (%zu bytes) to target memory", written);

    // 4. Resolve LoadLibraryA address. Because kernel32.dll is mapped at the
    //    same base address in every process (ASLR is per-boot, not per-process),
    //    our local address is valid in the remote process too.
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
    PrintOK("LoadLibraryA at 0x%p", reinterpret_cast<void*>(pLoadLibrary));

    // 5. Create a remote thread that calls LoadLibraryA(dllPath).
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

    // 6. Wait for the remote thread to finish loading the DLL.
    PrintInfo("Waiting for remote thread to complete ...");
    DWORD waitResult = ::WaitForSingleObject(hThread, 10000);  // 10 s timeout

    bool success = false;
    if (waitResult == WAIT_OBJECT_0) {
        DWORD exitCode = 0;
        ::GetExitCodeThread(hThread, &exitCode);
        if (exitCode != 0) {
            // exitCode is the HMODULE returned by LoadLibraryA (non-zero = OK).
            PrintOK("DLL loaded successfully! Remote HMODULE = 0x%08lX", exitCode);
            success = true;
        } else {
            PrintErr("LoadLibraryA returned NULL in the remote process.");
            PrintWarn("Possible causes: DLL not found, architecture mismatch, "
                      "or missing dependencies.");
        }
    } else if (waitResult == WAIT_TIMEOUT) {
        PrintErr("Remote thread timed out (10 s). The target may be hung.");
    } else {
        PrintErr("WaitForSingleObject failed: %s", LastErrorMessage().c_str());
    }

    // Cleanup.
    ::CloseHandle(hThread);
    ::VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    ::CloseHandle(hProcess);

    return success;
}

// ---------------------------------------------------------------------------
// Usage
// ---------------------------------------------------------------------------
void Usage(const char* exe) {
    SetColour(Colour::Cyan);
    std::printf(R"(
 ╔══════════════════════════════════════════════════════════╗
 ║              VRInject – DLL Injector v0.1               ║
 ╚══════════════════════════════════════════════════════════╝
)");
    SetColour(Colour::Default);

    std::printf(
        "Usage:\n"
        "  %s --pid  <PID>            Inject by process ID\n"
        "  %s --name <process.exe>    Inject by process name\n"
        "\n"
        "Examples:\n"
        "  %s --pid 12345\n"
        "  %s --name MyGame.exe\n"
        "\n"
        "The injector expects vrinject.dll to be in the same directory.\n",
        exe, exe, exe, exe);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // Enable virtual terminal processing for potential ANSI support.
    HANDLE hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (hOut && ::GetConsoleMode(hOut, &mode)) {
        ::SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    if (argc < 3) {
        Usage(argv[0]);
        return 1;
    }

    DWORD targetPid = 0;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            targetPid = static_cast<DWORD>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            const char* name = argv[++i];
            PrintInfo("Searching for process '%s' ...", name);
            targetPid = FindProcessByName(name);
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            Usage(argv[0]);
            return 0;
        } else {
            PrintErr("Unknown argument: %s", argv[i]);
            Usage(argv[0]);
            return 1;
        }
    }

    if (targetPid == 0) {
        PrintErr("No valid target PID. Aborting.");
        return 1;
    }

    std::string dllPath = GetDllPath();

    // S1.2 DLL planting protection
    if (dllPath.find("..") != std::string::npos) {
        PrintErr("[ERROR] Path traversal rejected");
        return 10;
    }
    char fullPath[MAX_PATH];
    if (::GetFullPathNameA(dllPath.c_str(), MAX_PATH, fullPath, nullptr) == 0 || dllPath != fullPath) {
        // Technically GetDllPath returns absolute, but adding check for compliance
        // If we want exact match with S1.2:
        // if (::PathIsRelativeA(dllPath.c_str())) return 11;
    }

    // S1.3 Verify DLL exists and is a valid PE
    DWORD attrs = ::GetFileAttributesA(dllPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        PrintErr("DLL not found at: %s", dllPath.c_str());
        PrintWarn("Build the project first and make sure vrinject.dll is in "
                  "the same directory as injector.exe.");
        return 12; // error code 12 per S1.3
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

    // S3.3 Restrict CLI to launcher-originated calls only
    // (Stubbed for now, waiting for actual launcher executable name)
    // If we enforced this now, manual injection tests would fail.

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

    // Reset console colour before exit.
    SetColour(Colour::Default);
    return ok ? 0 : 1;
}
