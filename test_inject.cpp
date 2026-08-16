#include <Windows.h>
#include <iostream>
#include <string>
#include <TlHelp32.h>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: test_inject <pid> <dll_path>\n";
        return 1;
    }
    
    DWORD pid = std::stoul(argv[1]);
    std::string dllPath = argv[2];

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "OpenProcess failed: " << GetLastError() << "\n";
        return 1;
    }
    
    void* remoteMem = VirtualAllocEx(hProcess, nullptr, dllPath.size() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        std::cout << "VirtualAllocEx failed: " << GetLastError() << "\n";
        return 1;
    }
    
    WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), dllPath.size() + 1, nullptr);
    
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    auto pLoadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(hKernel32, "LoadLibraryA"));
    
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, pLoadLibrary, remoteMem, 0, nullptr);
    if (!hThread) {
        std::cout << "CreateRemoteThread failed: " << GetLastError() << "\n";
        return 1;
    }
    
    WaitForSingleObject(hThread, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    std::cout << "Thread exit code: " << exitCode << " (0x" << std::hex << exitCode << ")\n";
    
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    
    return 0;
}
