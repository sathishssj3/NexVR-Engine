#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <tlhelp32.h>
#include <chrono>

bool InjectDLL(DWORD processId, const std::wstring& dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) return false;

    void* pLoc = VirtualAllocEx(hProcess, nullptr, (dllPath.length() + 1) * sizeof(wchar_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pLoc) {
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, pLoc, dllPath.c_str(), (dllPath.length() + 1) * sizeof(wchar_t), nullptr)) {
        VirtualFreeEx(hProcess, pLoc, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    void* pLoadLibraryW = (void*)GetProcAddress(hKernel32, "LoadLibraryW");

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryW, pLoc, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, pLoc, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, 5000); // 5 sec max wait for LoadLibrary
    
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pLoc, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    
    return (exitCode != 0); // HMODULE is not null
}

int main(int argc, char** argv) {
    std::cout << "Starting 200-loop injection stress test..." << std::endl;
    
    std::wstring appPath = L"C:\\Users\\sathi\\.gemini\\antigravity\\scratch\\vr-inject\\build\\bin\\test_app.exe";
    std::wstring dllPath = L"C:\\Users\\sathi\\.gemini\\antigravity\\scratch\\vr-inject\\build\\bin\\vrinject.dll";
    
    int successCount = 0;
    int failCount = 0;
    
    for (int i = 0; i < 200; ++i) {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        
        // Start target app
        if (!CreateProcessW(appPath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            std::cerr << "Loop " << i << ": Failed to start test_app.exe\n";
            continue;
        }
        
        // Wait for it to initialize (100ms is usually enough for a simple app)
        Sleep(100);
        
        // Inject DLL
        if (InjectDLL(pi.dwProcessId, dllPath)) {
            successCount++;
        } else {
            std::cerr << "Loop " << i << ": Injection failed!\n";
            failCount++;
        }
        
        // Let it run for a bit
        Sleep(50);
        
        // Kill target app
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        
        if ((i + 1) % 20 == 0) {
            std::cout << "Completed " << (i + 1) << " iterations...\n";
        }
    }
    
    std::cout << "Stress test complete.\n";
    std::cout << "Success: " << successCount << "\n";
    std::cout << "Failed: " << failCount << "\n";
    
    return (failCount == 0) ? 0 : 1;
}
