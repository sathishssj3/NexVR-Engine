#include "memory_scanner/pointer_chain_resolver.h"
#include "core/logger.h"
#include "core/seh_shield.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace vrinject {

void PointerChainResolver::SetModuleBounds(uint8_t* base, size_t size) {
    m_mainModuleBase = base;
    m_mainModuleSize = size;
}

uint8_t* PointerChainResolver::ResolveRIP(uint8_t* instructionAddress, uint32_t instructionSize, uint32_t offsetPosition) {
    if (!instructionAddress) return nullptr;

    uint8_t* readPtr = instructionAddress + offsetPosition;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(readPtr, &mbi, sizeof(mbi)) == 0 ||
        mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) == 0 ||
        (mbi.Protect & PAGE_GUARD) != 0) {
        LOG_WARN("PointerChainResolver: address %p is not in a readable page, skipping.", readPtr);
        return nullptr;
    }

    uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (reinterpret_cast<uintptr_t>(readPtr) + sizeof(int32_t) > regionEnd) {
        LOG_WARN("PointerChainResolver: int32 at %p spans a page boundary, skipping.", readPtr);
        return nullptr;
    }

    int32_t offset = *reinterpret_cast<int32_t*>(readPtr);
    return instructionAddress + instructionSize + offset;
}

uint8_t* PointerChainResolver::ResolvePointerChain(uint8_t* targetAddress, int maxDepth) {
    if (!m_mainModuleBase) return nullptr;

    MEMORY_BASIC_INFORMATION mbi;
    uint8_t* currentAddress = m_mainModuleBase;
    uint8_t* searchEnd = m_mainModuleBase + m_mainModuleSize;

    while (currentAddress < searchEnd) {
        if (VirtualQuery(currentAddress, &mbi, sizeof(mbi)) == 0) break;

        if (mbi.State == MEM_COMMIT && (mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_READONLY)) {
            uint8_t* scanStart = static_cast<uint8_t*>(mbi.BaseAddress);
            uint8_t* scanEnd = scanStart + mbi.RegionSize - sizeof(uintptr_t);
            
            for (uint8_t* p = scanStart; p <= scanEnd; p += sizeof(uintptr_t)) {
                uintptr_t candidatePtr = 0;
                if (seh::SafeReadMemory(p, &candidatePtr, sizeof(candidatePtr))) {
                    if (candidatePtr == reinterpret_cast<uintptr_t>(targetAddress)) {
                        LOG_INFO("PointerChainResolver: Resolved static pointer at %p pointing to %p", p, targetAddress);
                        return p;
                    }
                }
            }
        }
        currentAddress = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return nullptr;
}

} // namespace vrinject
