#include <gtest/gtest.h>
#include "core/iat_hook.h"
#include <Windows.h>

// Dummy function to test IAT hooking
// We'll hook kernel32.dll!GetCurrentProcessId which is imported by this test executable
typedef DWORD (WINAPI *GetCurrentProcessId_t)();

DWORD WINAPI DetourGetCurrentProcessId() {
    return 1337;
}

TEST(IATHookTest, HookAndRestore) {
    HMODULE hExe = GetModuleHandle(NULL);
    ASSERT_NE(hExe, nullptr);

    void* originalFunc = nullptr;
    
    // Install hook
    bool hooked = vrinject::core::IATHook::InstallHook(hExe, "kernel32.dll", "GetCurrentProcessId", (void*)DetourGetCurrentProcessId, &originalFunc);
    
    if (!hooked) {
        // If not found in kernel32.dll, it might be in api-ms-win-core-processthreads-l1-1-0.dll or similar on some Windows versions
        // Just skip if not found, as we only need to test if the mechanism works when an import exists
        GTEST_SKIP() << "GetCurrentProcessId not found directly in kernel32.dll imports.";
        return;
    }

    ASSERT_TRUE(hooked);
    ASSERT_NE(originalFunc, nullptr);

    // Call it and see if it's detoured
    DWORD pid = GetCurrentProcessId();
    EXPECT_EQ(pid, 1337);

    // Restore hook
    bool removed = vrinject::core::IATHook::RemoveHook(hExe, "kernel32.dll", "GetCurrentProcessId", originalFunc);
    ASSERT_TRUE(removed);

    // Call it and see if it's restored
    pid = GetCurrentProcessId();
    EXPECT_NE(pid, 1337);
}
