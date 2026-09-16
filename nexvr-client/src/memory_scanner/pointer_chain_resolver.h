#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace vrinject {

struct PointerChain {
    uintptr_t baseOffset = 0;
    std::vector<int32_t> offsets;
    bool isDoublePrecision = false;
    bool valid = false;
};

class PointerChainResolver {
public:

    // Resolves a relative instruction pointer (RIP) to an absolute address
    uint8_t* ResolveRIP(uint8_t* instructionAddress, uint32_t instructionSize, uint32_t offsetFromInstructionEnd);

    // Traces a dynamic address back to a static base offset
    uint8_t* ResolvePointerChain(uint8_t* targetAddress, int maxDepth = 3);

    // Resolves a static pointer chain with offset tolerance for candidate structures
    bool ResolveChain(uint8_t* targetAddress, PointerChain& outChain, int maxOffset = 0x2000);

    // Dereferences a resolved pointer chain against the loaded module
    uint8_t* DereferenceChain(const PointerChain& chain);

    // Saves and loads discovered chains to/from persistent disk cache
    bool SaveCache(const std::string& exeName, const PointerChain& chain);
    bool LoadCache(const std::string& exeName, PointerChain& outChain);

    // Sets the main module bounds to restrict static pointer searches
    void SetModuleBounds(uint8_t* base, size_t size);
    uint8_t* GetModuleBase() const { return m_mainModuleBase; }
    size_t GetModuleSize() const { return m_mainModuleSize; }

    PointerChainResolver() = default;

private:

    uint8_t* m_mainModuleBase = nullptr;
    size_t m_mainModuleSize = 0;
    std::string GetCachePath() const;
};

} // namespace vrinject
