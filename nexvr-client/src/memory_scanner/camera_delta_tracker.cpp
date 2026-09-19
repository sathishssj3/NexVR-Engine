#include "memory_scanner/camera_delta_tracker.h"
#include "memory_scanner/page_scanner.h"
#include "memory_scanner/pointer_chain_resolver.h"
#include "hooks/input_hook.h"
#include "core/seh_shield.h"
#include "core/engine_detector.h"
#include "core/config_manager.h"
#include "core/subsystem_context.h"
#include "core/logger.h"
#include <cmath>
#include <algorithm>

namespace vrinject {

bool CameraDeltaTracker::SafeReadMatrix(uint8_t* address, bool isDouble, Matrix4x4& outMatrix) {
    if (!address) return false;
    
    // Address can either be a direct dynamic heap address or a static pointer pointing to dynamic heap
    void* matrixAddr = address;
    uintptr_t derefVal = 0;
    if (seh::SafeReadMemory(address, &derefVal, sizeof(derefVal))) {
        if (seh::IsValidMemoryPointer(reinterpret_cast<void*>(derefVal))) {
            matrixAddr = reinterpret_cast<void*>(derefVal);
        }
    }

    if (isDouble) {
        double dmat[16];
        if (!seh::SafeReadMemory(matrixAddr, dmat, sizeof(dmat))) return false;
        // Convert to float Matrix4x4
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                outMatrix.m[r][c] = static_cast<float>(dmat[r * 4 + c]);
            }
        }
    } else {
        if (!seh::SafeReadMemory(matrixAddr, &outMatrix, sizeof(Matrix4x4))) return false;
    }
    return true;
}

float CameraDeltaTracker::CalculateHmdAngularDelta(const XrPosef& p1, const XrPosef& p2) const {
    float dot = std::abs(p1.orientation.x * p2.orientation.x +
                         p1.orientation.y * p2.orientation.y +
                         p1.orientation.z * p2.orientation.z +
                         p1.orientation.w * p2.orientation.w);
    if (dot > 1.0f) dot = 1.0f;
    float angleRad = 2.0f * std::acos(dot);
    return angleRad * (180.0f / 3.1415926535f); // degrees
}

void CameraDeltaTracker::PollAndTrackCandidates() {
    // Determine engine type and explicit matrix precision from config
    m_isEngineUE5 = (SubsystemContext::Get().GetEngineDetector()->GetEngineType() == EngineType::UnrealEngine5);

    bool isDoubleExpected = m_isEngineUE5;
    const auto* configMgr = SubsystemContext::Get().GetConfig();
    if (configMgr) {
        const auto& config = configMgr->GetConfig();
        if (config.matrixPrecision == "Double64") {
            isDoubleExpected = true;
        } else if (config.matrixPrecision == "Float32") {
            isDoubleExpected = false;
        }
    }

    // Pull candidates from PageScanner
    auto dynamicCands = SubsystemContext::Get().GetPageScanner()->GetDynamicCandidates();
    for (const auto& dc : dynamicCands) {
        if (m_candidates.find(dc.address) == m_candidates.end()) {
            DynamicCandidate c;
            c.staticPointer = dc.address;
            c.isDoublePrecision = dc.isDoublePrecision;
            c.isViewMatrix = dc.isViewMatrix;
            c.isProjectionMatrix = dc.isProjectionMatrix;
            c.temporalScore = 0.4f;
            c.hasPrevious = false;
            c.consecutiveMatches = 0;
            m_candidates[dc.address] = c;
        }
    }

    // Read real physical mouse and controller input
    float mouseDelta = 0.0f;
    float stickDelta = 0.0f;
    InputHook::GetInstance().ConsumeAccumulatedInputDeltas(mouseDelta, stickDelta);

    // Compute HMD angular delta
    float hmdAngularDelta = 0.0f;
    if (m_hasPrevHeadsetPose) {
        hmdAngularDelta = CalculateHmdAngularDelta(m_prevHeadsetPose, m_cachedHeadsetPose);
    }
    m_prevHeadsetPose = m_cachedHeadsetPose;
    m_hasPrevHeadsetPose = true;

    // Combined input motion energy
    float inputMotionEnergy = (mouseDelta * 0.1f) + (stickDelta * 8.0f) + (hmdAngularDelta * 1.5f);

    uint8_t* bestPointer = nullptr;
    float bestScore = -1.0f;

    for (auto it = m_candidates.begin(); it != m_candidates.end(); ) {
        Matrix4x4 currentMatrix;
        if (!SafeReadMatrix(it->first, it->second.isDoublePrecision, currentMatrix)) {
            // Memory was freed or unmapped (caught by SEH)
            it = m_candidates.erase(it);
            continue;
        }

        UpdateCandidateMotion(it->second, currentMatrix, inputMotionEnergy);

        // Score weighting: heavily favor View matrices that correlate with physical look
        float score = it->second.temporalScore;
        if (it->second.isViewMatrix) {
            score += 2.0f;
        }
        if (isDoubleExpected && it->second.isDoublePrecision) {
            score += 0.5f;
        } else if (!isDoubleExpected && !it->second.isDoublePrecision) {
            score += 0.5f;
        }

        if (score > bestScore && it->second.isViewMatrix) {
            bestScore = score;
            bestPointer = it->first;
        }
        ++it;
    }

    if (bestPointer && bestScore >= 2.6f) { // base 2.0 + temporalScore >= 0.6
        if (m_lockedPointer != bestPointer) {
            static bool s_firstLockLogged = false;
            if (!s_firstLockLogged) {
                LOG_INFO("[OK] Camera Tracking: 6DOF View Matrix Locked (Confidence: %.0f%%)", (std::min)(100.0f, (bestScore - 2.0f) * 100.0f));
                s_firstLockLogged = true;
            } else {
                LOG_DEBUG("CameraDeltaTracker: Adjusted candidate view matrix at %p (score: %.2f)", bestPointer, bestScore);
            }
            m_lockedPointer = bestPointer;
            FindMatchingProjection(m_lockedPointer, m_candidates[bestPointer].isDoublePrecision);
        }
        m_lockedConfidence = (std::min)(1.0f, (bestScore - 2.0f) / 1.0f);

        // Persistent caching of locked pointer chain once confidence reaches >= 0.85
        static bool s_cachedThisSession = false;
        if (!s_cachedThisSession && m_lockedConfidence >= 0.85f && m_lockedPointer) {
            char exePath[MAX_PATH] = {};
            if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
                std::string fullPath(exePath);
                size_t lastSlash = fullPath.find_last_of("\\/");
                std::string exeName = (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;

                PointerChain chain;
                chain.isDoublePrecision = m_candidates[m_lockedPointer].isDoublePrecision;
                if (SubsystemContext::Get().GetPointerChainResolver()->ResolveChain(m_lockedPointer, chain)) {
                    SubsystemContext::Get().GetPointerChainResolver()->SaveCache(exeName, chain);
                    s_cachedThisSession = true;
                }
            }
        }
    } else if (bestScore < 2.2f) {
        m_lockedConfidence = (std::max)(0.0f, m_lockedConfidence - 0.05f);
        if (m_lockedConfidence <= 0.0f) {
            m_lockedPointer = nullptr;
            m_lockedProjPointer = nullptr;
        }
    }
}

void CameraDeltaTracker::FindMatchingProjection(uint8_t* viewAddress, bool isDouble) {
    if (!viewAddress) return;

    // First, inspect nearby struct offsets: cameras frequently store View and Proj adjacently
    const int nearbyOffsets[] = { 64, -64, 128, -128, 192, -192, 256, -256, 320, 384, 512 };
    for (int offset : nearbyOffsets) {
        uint8_t* probeAddr = viewAddress + offset;
        Matrix4x4 testMat;
        if (SafeReadMatrix(probeAddr, isDouble, testMat)) {
            if (PageScanner::IsValidProjectionMatrixFloat(&testMat.m[0][0], 90.0f)) {
                m_lockedProjPointer = probeAddr;
                LOG_DEBUG("CameraDeltaTracker: Found paired projection matrix at offset %d (%p)", offset, probeAddr);
                return;
            }
        }
    }

    // Second, inspect candidates collected by PageScanner for stable projection matrices
    for (const auto& pair : m_candidates) {
        if (pair.second.isProjectionMatrix && pair.second.temporalScore > 0.5f) {
            m_lockedProjPointer = pair.first;
            LOG_DEBUG("CameraDeltaTracker: Found matching projection matrix candidate at %p", pair.first);
            return;
        }
    }
}

void CameraDeltaTracker::UpdateHeadsetPose(const XrPosef& pose) {
    m_cachedHeadsetPose = pose;
}

static Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = a.m[i][0] * b.m[0][j] +
                             a.m[i][1] * b.m[1][j] +
                             a.m[i][2] * b.m[2][j] +
                             a.m[i][3] * b.m[3][j];
        }
    }
    return result;
}

bool CameraDeltaTracker::GetLockedCamera(Matrix4x4& outMatrix) {
    Matrix4x4 dummyProj;
    return GetLockedCamera(outMatrix, dummyProj);
}

bool CameraDeltaTracker::GetLockedCamera(Matrix4x4& outView, Matrix4x4& outProj) {
    if (!m_lockedPointer) return false;
    
    auto it = m_candidates.find(m_lockedPointer);
    if (it == m_candidates.end()) return false;

    if (!SafeReadMatrix(m_lockedPointer, it->second.isDoublePrecision, outView)) {
        return false;
    }

    // Retrieve projection matrix if paired
    if (m_lockedProjPointer) {
        SafeReadMatrix(m_lockedProjPointer, it->second.isDoublePrecision, outProj);
    }

    return true;
}

float CameraDeltaTracker::CalculateDelta(const Matrix4x4& m1, const Matrix4x4& m2) const {
    float diff = 0.0f;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            diff += std::abs(m1.m[r][c] - m2.m[r][c]);
        }
    }
    return diff;
}

void CameraDeltaTracker::UpdateCandidateMotion(DynamicCandidate& candidate, const Matrix4x4& currentMatrix, float inputMotionEnergy) {
    if (!candidate.hasPrevious) {
        candidate.previousMatrix = currentMatrix;
        candidate.hasPrevious = true;
        return;
    }

    float delta = CalculateDelta(candidate.previousMatrix, currentMatrix);
    candidate.previousMatrix = currentMatrix;
    
    bool hasPhysicalMotion = (inputMotionEnergy > 0.15f);

    if (candidate.isViewMatrix) {
        if (delta > 0.0002f && delta < 2.5f && hasPhysicalMotion) {
            candidate.temporalScore = (std::min)(candidate.temporalScore + 0.15f, 1.0f);
            candidate.consecutiveMatches++;
        } else if (delta < 0.0001f && hasPhysicalMotion) {
            // Player moved view, but matrix didn't rotate (static object / HUD)
            candidate.temporalScore = (std::max)(candidate.temporalScore - 0.10f, 0.0f);
            candidate.consecutiveMatches = 0;
        } else if (delta > 0.002f && !hasPhysicalMotion) {
            // Movement without input (cutscene or unrelated game animation)
            candidate.temporalScore = (std::max)(candidate.temporalScore - 0.08f, 0.0f);
            candidate.consecutiveMatches = 0;
        } else if (delta < 0.0001f && !hasPhysicalMotion) {
            // Static as expected when idle
            if (candidate.consecutiveMatches > 3) {
                // Keep score stable
            }
        }
    } else if (candidate.isProjectionMatrix) {
        // Projection matrices should remain static during normal camera look
        if (delta < 0.0001f) {
            candidate.temporalScore = (std::min)(candidate.temporalScore + 0.05f, 1.0f);
        } else {
            candidate.temporalScore = (std::max)(candidate.temporalScore - 0.10f, 0.0f);
        }
    }
}

} // namespace vrinject
