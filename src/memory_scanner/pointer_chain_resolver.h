#pragma once

#include <cstdint>

namespace vrinject {

class PointerChainResolver {
public:
    static PointerChainResolver& Get() {
        static PointerChainResolver instance;
        return instance;
    }

    // Resolves a relative instruction pointer (RIP) to an absolute address
    uint8_t* ResolveRIP(uint8_t* instructionAddress, uint32_t instructionSize, uint32_t offsetFromInstructionEnd);

    // Traces a dynamic address back to a static base offset
    uint8_t* ResolvePointerChain(uint8_t* targetAddress, int maxDepth = 3);

    // Sets the main module bounds to restrict static pointer searches
    void SetModuleBounds(uint8_t* base, size_t size);

private:
    PointerChainResolver() = default;

    uint8_t* m_mainModuleBase = nullptr;
    size_t m_mainModuleSize = 0;
};

} // namespace vrinject
