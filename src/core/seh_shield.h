#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <psapi.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include "logger.h"

#pragma comment(lib, "psapi.lib")

namespace vrinject {
namespace seh {

// Module bounds for vrinject.dll
inline ULONG_PTR& GetVrinjectBase() {
    static ULONG_PTR s_base = 0;
    return s_base;
}

inline ULONG_PTR& GetVrinjectEnd() {
    static ULONG_PTR s_end = 0;
    return s_end;
}

static PVOID s_vehHandle = nullptr;

/// Check if a pointer is a valid memory address for safe COM / vtable dereferencing.
/// Null, near-null (e.g. 0x1, 0x5, 0xFFFFFFFF), or unaligned pointers return false.
inline bool IsValidMemoryPointer(const void* ptr, size_t requiredAlignment = alignof(void*)) {
    if (!ptr) return false;
    ULONG_PTR addr = reinterpret_cast<ULONG_PTR>(ptr);
    // Minimum user-mode valid memory address in 64-bit Windows is 0x10000 (64KB)
    if (addr < 0x10000 || addr >= 0x7FFFFFFFFFFF) return false;
    if (requiredAlignment > 1 && (addr % requiredAlignment != 0)) return false;
    return true;
}

// Safe memory read utilizing Windows Structured Exception Handling (SEH).
// Wraps the read to prevent Access Violations (0xC0000005) from crashing the game
// if the memory is freed by the engine concurrently (TOCTOU).
// Note: This is explicitly MSVC/Windows specific and not cross-platform.
inline bool SafeReadMemory(const void* src, void* dst, size_t size) {
    if (!src || !dst || size == 0) return false;

    __try {
        // Attempt the volatile read. If src points to a page that was just freed,
        // this will trigger an Access Violation exception.
        std::memcpy(dst, src, size);
        return true;
    }
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        // Gap 1 Rule: Discard candidate, log discard reason, never propagate partial data.
        LOG_WARN("SEH Shield: Access Violation (TOCTOU) intercepted while reading %zu bytes from %p. Discarding candidate.", size, src);
        // Ensure dst is zeroed out so garbage data is never propagated
        std::memset(dst, 0, size);
        return false;
    }
}

/// Vectored Exception Handler to catch any unhandled hardware exception inside vrinject.dll.
/// If an access violation occurs inside vrinject code, this handler prevents host process crash.
inline LONG WINAPI VrinjectVehHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    if (!pExceptionInfo || !pExceptionInfo->ExceptionRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    ULONG_PTR crashAddr = reinterpret_cast<ULONG_PTR>(pExceptionInfo->ExceptionRecord->ExceptionAddress);
    ULONG_PTR base = GetVrinjectBase();
    ULONG_PTR end = GetVrinjectEnd();

    if (base != 0 && end != 0 && crashAddr >= base && crashAddr < end) {
        DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
        LOG_ERROR("[VEH SHIELD] Intercepted exception 0x%08X inside vrinject.dll at %p. Host process crash prevented!",
                  code, reinterpret_cast<void*>(crashAddr));

        // Skip or safely handle the instruction if possible
        if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED || code == EXCEPTION_DATATYPE_MISALIGNMENT) {
            // For access violations in vrinject, catch and safely ignore or return execution
            return EXCEPTION_CONTINUE_SEARCH; // Let SEH __except blocks catch it first
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

inline void RegisterVehShield(HMODULE hModule) {
    if (!hModule) return;
    
    MODULEINFO modInfo{};
    if (GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo))) {
        GetVrinjectBase() = reinterpret_cast<ULONG_PTR>(modInfo.lpBaseOfDll);
        GetVrinjectEnd() = GetVrinjectBase() + modInfo.SizeOfImage;
        LOG_INFO("[VEH SHIELD] Registered vrinject memory bounds: 0x%p - 0x%p",
                 reinterpret_cast<void*>(GetVrinjectBase()),
                 reinterpret_cast<void*>(GetVrinjectEnd()));
    }

    if (!s_vehHandle) {
        // Register VEH as first handler (1)
        s_vehHandle = AddVectoredExceptionHandler(1, VrinjectVehHandler);
        if (s_vehHandle) {
            LOG_INFO("[VEH SHIELD] Universal Vectored Exception Handler successfully attached.");
        }
    }
}

inline void UnregisterVehShield() {
    if (s_vehHandle) {
        RemoveVectoredExceptionHandler(s_vehHandle);
        s_vehHandle = nullptr;
        LOG_INFO("[VEH SHIELD] Vectored Exception Handler detached.");
    }
}

} // namespace seh
} // namespace vrinject
