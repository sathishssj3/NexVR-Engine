#pragma once
#include "../core/base_hook.h"

namespace vrinject {

namespace DX11Hook {
    // Initializes MinHook, creates dummy D3D11 device, extracts vtables, and hooks Present/OMSetRenderTargets
    bool Initialize();

    // Cleans up hooks and releases COM resources
    void Shutdown();
}

} // namespace vrinject