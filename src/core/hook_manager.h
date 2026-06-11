#pragma once
#include "engine_detector.h"

namespace vrinject {

class HookManager {
public:
    static HookManager& Get() {
        static HookManager instance;
        return instance;
    }

    bool InitializeHooks();
    void ShutdownHooks();

private:
    HookManager() = default;
};

} // namespace vrinject
