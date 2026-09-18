import { useState } from 'react';
import type { VRConfig, UpdateStatus, VRStatus } from '../types';

interface SettingsViewProps {
  vrStatus: VRStatus;
  updateStatus: UpdateStatus | null;
  onUpdateStatusChange: (st: UpdateStatus) => void;
  onRescan: () => Promise<void>;
}

export function SettingsView({
  vrStatus,
  updateStatus,
  onUpdateStatusChange,
  onRescan,
}: SettingsViewProps) {
  const [checking, setChecking] = useState(false);
  const [isRescanning, setIsRescanning] = useState(false);
  const [feedbackMsg, setFeedbackMsg] = useState<string | null>(null);

  const activeVersion = updateStatus?.version 
    ? (updateStatus.version.startsWith('v') ? updateStatus.version : `v${updateStatus.version}`) 
    : 'v0.1.55';

  // Global default config persisted in localStorage
  const [globalConfig, setGlobalConfig] = useState<VRConfig>(() => {
    try {
      const saved = localStorage.getItem('nexvr_global_config');
      if (saved) return JSON.parse(saved);
    } catch {}
    return {
      useRecommendedResolution: true,
      srgbCorrection: true,
      depthSubmission: false,
      motionAimSensitivity: 1.0,
      rawInputMode: true,
      autoInjectOnLaunch: true,
      aiInpainting: true,
      performanceOverlay: false,
      hapticFeedback: true,
    };
  });

  const saveGlobalConfig = (updated: VRConfig) => {
    setGlobalConfig(updated);
    try {
      localStorage.setItem('nexvr_global_config', JSON.stringify(updated));
    } catch {}
  };

  const handleResetDefaults = () => {
    const defaults: VRConfig = {
      useRecommendedResolution: true,
      srgbCorrection: true,
      depthSubmission: false,
      motionAimSensitivity: 1.0,
      rawInputMode: true,
      autoInjectOnLaunch: true,
      aiInpainting: true,
      performanceOverlay: false,
      hapticFeedback: true,
    };
    saveGlobalConfig(defaults);
    setFeedbackMsg('Global settings restored to recommended factory defaults.');
    setTimeout(() => setFeedbackMsg(null), 4000);
  };

  const handleCheckUpdate = async () => {
    if (checking) return;
    setChecking(true);
    setFeedbackMsg('Querying GitHub Releases for engine updates...');
    try {
      if (window.ag && window.ag.update) {
        const res = await window.ag.update.check();
        onUpdateStatusChange(res);
        if (res.error) {
          setFeedbackMsg(`Notice: ${res.error}`);
        } else if (res.updated) {
          setFeedbackMsg(`Hotfix v${res.version} installed! Restart launcher or inject to apply.`);
        } else if (res.hasUpdate) {
          setFeedbackMsg(`Update v${res.version} downloaded successfully.`);
        } else {
          setFeedbackMsg('Your NexVR Engine launcher is up to date.');
        }
      }
    } catch (e: any) {
      setFeedbackMsg(`Check failed: ${e?.message || 'Network error'}`);
    } finally {
      setChecking(false);
      setTimeout(() => setFeedbackMsg(null), 6000);
    }
  };

  const handleOpenUpdatesFolder = () => {
    if (window.ag && window.ag.update && window.ag.update.openFolder) {
      window.ag.update.openFolder();
    }
  };

  const handleRescanClick = async () => {
    if (isRescanning) return;
    setIsRescanning(true);
    setFeedbackMsg('Scanning Steam, Epic Games, and custom directories for installed games...');
    try {
      await onRescan();
      setFeedbackMsg('Game libraries successfully scanned and synchronized.');
    } catch (e: any) {
      setFeedbackMsg(`Library scan failed: ${e?.message || 'Unknown error'}`);
    } finally {
      setIsRescanning(false);
      setTimeout(() => setFeedbackMsg(null), 4000);
    }
  };

  const Toggle = ({ value, onToggle }: { value: boolean; onToggle: () => void }) => (
    <div className={`ag-toggle ${value ? 'on' : 'off'}`} onClick={onToggle} />
  );

  const sectionLabel = (title: string, subtitle?: string) => (
    <div style={{ marginBottom: 14 }}>
      <div style={{
        fontSize: 13.5,
        fontFamily: 'var(--ag-font-display)',
        color: '#FFFFFF',
        letterSpacing: '0.08em',
        fontWeight: 700,
        textTransform: 'uppercase',
        display: 'flex',
        alignItems: 'center',
        gap: 8,
      }}>
        <span style={{ 
          width: 3, 
          height: 13, 
          background: 'var(--ag-accent)', 
          borderRadius: 2,
          boxShadow: 'none' 
        }} />
        {title}
      </div>
      {subtitle && (
        <div style={{
          fontSize: 11.5,
          fontFamily: 'var(--ag-font-ui)',
          color: '#848884',
          marginTop: 4,
          paddingLeft: 11,
          lineHeight: 1.4,
        }}>
          {subtitle}
        </div>
      )}
    </div>
  );

  return (
    <div
      className="fast-smooth-scroll"
      style={{
        flex: 1,
        padding: '32px 36px',
        overflowY: 'auto',
        background: '#000000',
      }}
    >
      <div className="settings-open-anim settings-tab-enter" style={{ maxWidth: 820, margin: '0 auto' }}>
        
        {/* Header with Title & Live Telemetry Pill */}
        <div 
          className="settings-item-enter"
          style={{ 
            display: 'flex', 
            justifyContent: 'space-between', 
            alignItems: 'flex-start', 
            marginBottom: 28,
            borderBottom: '1px solid rgba(255, 255, 255, 0.06)',
            paddingBottom: 20
          }}
        >
          <div>
            <h1
              style={{
                fontSize: 24,
                fontWeight: 800,
                fontFamily: 'var(--ag-font-display)',
                letterSpacing: '0.08em',
                color: '#FFF',
                margin: '0 0 6px 0',
              }}
            >
              SETTINGS
            </h1>
            <div style={{
              fontSize: 12.5,
              fontFamily: 'var(--ag-font-ui)',
              color: '#848884',
            }}>
              Global Engine Runtime, Stereo Reprojection &amp; Tracking Configuration
            </div>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
            <span style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: 6,
              fontSize: 14,
              fontFamily: 'var(--ag-font-mono)',
              letterSpacing: '0.06em',
              color: 'var(--ag-accent)',
              background: 'transparent',
              border: 'none',
              padding: 0,
              fontWeight: 800,
            }}>
              <span style={{ width: 6, height: 6, borderRadius: '50%', background: 'var(--ag-accent)' }} />
              {activeVersion}
            </span>
          </div>
        </div>

        {/* Global Feedback Banner */}
        {feedbackMsg && (
          <div
            className="fade-in"
            style={{
              marginBottom: 22,
              padding: '10px 14px',
              borderRadius: 4,
              fontSize: 11.5,
              fontFamily: 'var(--ag-font-mono)',
              color: feedbackMsg.includes('installed') || feedbackMsg.includes('up to date') || feedbackMsg.includes('synchronized') || feedbackMsg.includes('restored')
                ? 'var(--ag-accent-success)'
                : 'var(--ag-accent)',
              background: 'rgba(255, 255, 255, 0.02)',
              border: '1px solid rgba(255, 255, 255, 0.08)',
              display: 'flex',
              alignItems: 'center',
              gap: 8,
            }}
          >
            <span style={{ fontSize: 13 }}>ℹ</span>
            <span>{feedbackMsg}</span>
          </div>
        )}

        {/* ========================================================= */}
        {/* 1. ENGINE UPDATE & RUNTIME CHANNEL */}
        {/* ========================================================= */}
        <div className="settings-item-enter stagger-1" style={{ marginBottom: 32 }}>
          {sectionLabel('ENGINE UPDATE CHANNEL', 'Automated Over-The-Air binary delivery & release changelog')}
          
          <div className="settings-card" style={{ padding: '22px 24px' }}>
            {/* Top row: Status & Actions */}
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexWrap: 'wrap', gap: 14, marginBottom: 18 }}>
              <div>
                <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 4 }}>
                  <span style={{ fontSize: 17, fontWeight: 700, color: '#FFF', fontFamily: 'var(--ag-font-display)', letterSpacing: '0.03em' }}>
                    NexVR Engine {activeVersion}
                  </span>
                  <span style={{
                    width: 3.5,
                    height: 14,
                    background: '#CC0000',
                    borderRadius: 999,
                    display: 'inline-block',
                    flexShrink: 0
                  }} />
                  <span style={{
                    fontSize: 11,
                    fontFamily: 'var(--ag-font-mono)',
                    color: 'var(--ag-accent)',
                    background: 'transparent',
                    border: 'none',
                    padding: 0,
                    fontWeight: 800,
                    letterSpacing: '0.06em'
                  }}>
                    {updateStatus?.updated ? 'HOTFIX ACTIVE' : 'UP TO DATE'}
                  </span>
                </div>
                <div style={{ fontSize: 11, color: 'var(--ag-text-dim)', fontFamily: 'var(--ag-font-mono)', letterSpacing: '0.04em' }}>
                  CHANNEL // STABLE RELEASE (MSVC x64 WIN64)
                </div>
              </div>

              <div style={{ display: 'flex', gap: 10, alignItems: 'center' }}>
                <button
                  onClick={handleOpenUpdatesFolder}
                  style={{
                    background: 'rgba(255, 255, 255, 0.035)',
                    border: '1px solid rgba(255, 255, 255, 0.12)',
                    boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.06)',
                    color: 'var(--ag-text-primary)',
                    borderRadius: 4,
                    padding: '7px 15px',
                    fontFamily: 'var(--ag-font-display)',
                    fontSize: 11.5,
                    letterSpacing: '0.06em',
                    fontWeight: 600,
                    cursor: 'pointer',
                    transition: 'all 0.2s ease',
                  }}
                  onMouseEnter={e => { e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.3)'; e.currentTarget.style.background = 'rgba(255, 255, 255, 0.07)'; }}
                  onMouseLeave={e => { e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.12)'; e.currentTarget.style.background = 'rgba(255, 255, 255, 0.035)'; }}
                >
                  OPEN FOLDER
                </button>
                <button
                  onClick={handleCheckUpdate}
                  disabled={checking}
                  style={{
                    background: '#CC0000',
                    border: '1px solid #CC0000',
                    boxShadow: 'none',
                    color: '#FFFFFF',
                    borderRadius: 4,
                    padding: '7px 18px',
                    fontFamily: 'var(--ag-font-display)',
                    fontSize: 11.5,
                    letterSpacing: '0.08em',
                    fontWeight: 800,
                    cursor: checking ? 'wait' : 'pointer',
                    opacity: checking ? 0.6 : 1,
                    transition: 'all 0.2s ease',
                  }}
                  onMouseEnter={e => {
                    if (!checking) {
                      e.currentTarget.style.background = '#E60000';
                      e.currentTarget.style.borderColor = '#FF1A1A';
                    }
                  }}
                  onMouseLeave={e => {
                    if (!checking) {
                      e.currentTarget.style.background = '#CC0000';
                      e.currentTarget.style.borderColor = '#CC0000';
                    }
                  }}
                >
                  {checking ? 'CHECKING...' : 'CHECK FOR UPDATES'}
                </button>
              </div>
            </div>



            {/* What's New Two-Column Changelog with Inset Panels */}
            <div style={{
              paddingTop: 16,
              borderTop: '1px solid rgba(255, 255, 255, 0.05)',
              display: 'grid',
              gridTemplateColumns: '1fr 1fr',
              gap: 14,
            }}>
              <div style={{
                background: 'linear-gradient(180deg, rgba(255, 255, 255, 0.025) 0%, rgba(255, 255, 255, 0.006) 100%), #060608',
                border: '1px solid rgba(255, 255, 255, 0.07)',
                borderTop: '2px solid #CC0000',
                borderRadius: 6,
                padding: '14px 16px',
                boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.04), 0 4px 14px rgba(0, 0, 0, 0.35)',
              }}>
                <div style={{ fontSize: 11, fontFamily: 'var(--ag-font-mono)', color: '#FFFFFF', letterSpacing: '0.08em', fontWeight: 800, marginBottom: 8 }}>
                  PROBLEMS FIXED
                </div>
                <div style={{ fontSize: 12, color: '#848884', lineHeight: '1.6', display: 'flex', flexDirection: 'column', gap: 6, fontWeight: 600 }}>
                  <div>• Filtered out non-game launcher utilities (Epic Online Services, Redistributables) from auto-detection.</div>
                  <div>• Eliminated phantom test-game detection and enforced authentic store installation receipts.</div>
                  <div>• Removed brittle game-specific hardcoding in favor of dynamic heuristic depth extraction.</div>
                </div>
              </div>
              <div style={{
                background: 'linear-gradient(180deg, rgba(255, 255, 255, 0.025) 0%, rgba(255, 255, 255, 0.006) 100%), #060608',
                border: '1px solid rgba(255, 255, 255, 0.07)',
                borderTop: '2px solid #CC0000',
                borderRadius: 6,
                padding: '14px 16px',
                boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.04), 0 4px 14px rgba(0, 0, 0, 0.35)',
              }}>
                <div style={{ fontSize: 11, fontFamily: 'var(--ag-font-mono)', color: '#FFFFFF', letterSpacing: '0.08em', fontWeight: 800, marginBottom: 8 }}>
                  NEW CAPABILITIES
                </div>
                <div style={{ fontSize: 12, color: '#848884', lineHeight: '1.6', display: 'flex', flexDirection: 'column', gap: 6, fontWeight: 600 }}>
                  <div>• <strong style={{ color: '#FFF', fontWeight: 800 }}>Pure Solid Black UI:</strong> Zero background grid lines and removed CRT scanlines for line-free clarity.</div>
                  <div>• <strong style={{ color: '#FFF', fontWeight: 800 }}>DirectML Neural Inpainter:</strong> Sub-1.5ms GPU edge inpainting for occluded stereo pixels.</div>
                  <div>• <strong style={{ color: '#FFF', fontWeight: 800 }}>Discord Community CTA:</strong> One-click verified community server integration.</div>
                </div>
              </div>
            </div>

          </div>
        </div>

        {/* ========================================================= */}
        {/* 2. DISPLAY & STEREO PROJECTION */}
        {/* ========================================================= */}
        <div className="settings-item-enter stagger-2" style={{ marginBottom: 32 }}>
          {sectionLabel('DISPLAY & STEREO PROJECTION', 'Stereoscopic viewport calibration, color gamma, and depth buffers')}

          <div className="settings-card" style={{ padding: '6px 18px' }}>
            <div className="setting-row">
              <div className="setting-label">
                <span className="title">Match Headset Resolution</span>
                <span className="desc">Render game at native OpenXR HMD recommended per-eye resolution for zero pixel blur</span>
              </div>
              <Toggle
                value={globalConfig.useRecommendedResolution}
                onToggle={() => saveGlobalConfig({ ...globalConfig, useRecommendedResolution: !globalConfig.useRecommendedResolution })}
              />
            </div>

            <div className="setting-row">
              <div className="setting-label">
                <span className="title">sRGB Color Correction Pass</span>
                <span className="desc">Linear-to-sRGB shader pass preventing washed out or milky colors on OLED and LCD lenses</span>
              </div>
              <Toggle
                value={globalConfig.srgbCorrection}
                onToggle={() => saveGlobalConfig({ ...globalConfig, srgbCorrection: !globalConfig.srgbCorrection })}
              />
            </div>

            <div className="setting-row">
              <div className="setting-label">
                <span className="title">Depth Buffer Submission</span>
                <span className="desc">Transmit depth texture to OpenXR runtime for SpaceWarp &amp; Asynchronous Timewarp reprojection</span>
              </div>
              <Toggle
                value={globalConfig.depthSubmission}
                onToggle={() => saveGlobalConfig({ ...globalConfig, depthSubmission: !globalConfig.depthSubmission })}
              />
            </div>

            <div className="setting-row">
              <div className="setting-label">
                <span className="title">DirectML Neural Inpainting</span>
                <span className="desc">AI neural edge completion for occluded stereo pixels on graphics worker thread (&lt;1.5ms)</span>
              </div>
              <Toggle
                value={globalConfig.aiInpainting ?? true}
                onToggle={() => saveGlobalConfig({ ...globalConfig, aiInpainting: !(globalConfig.aiInpainting ?? true) })}
              />
            </div>

            <div className="setting-row" style={{ borderBottom: 'none' }}>
              <div className="setting-label">
                <span className="title">In-Headset Performance HUD</span>
                <span className="desc">Display in-game real-time VR frametime counter, FPS, and jitter metrics overlay inside headset</span>
              </div>
              <Toggle
                value={globalConfig.performanceOverlay ?? false}
                onToggle={() => saveGlobalConfig({ ...globalConfig, performanceOverlay: !(globalConfig.performanceOverlay ?? false) })}
              />
            </div>
          </div>
        </div>

        {/* ========================================================= */}
        {/* 3. INPUT & CONTROLLER TRACKING */}
        {/* ========================================================= */}
        <div className="settings-item-enter stagger-3" style={{ marginBottom: 32 }}>
          {sectionLabel('INPUT & CONTROLLER TRACKING', '6DOF controller polling, process watcher, and aim multipliers')}

          <div className="settings-card" style={{ padding: '6px 18px' }}>
            <div className="setting-row">
              <div className="setting-label">
                <span className="title">Raw 6DOF Input Mode</span>
                <span className="desc">Direct motion controller vector polling bypassing simulated Windows mouse events</span>
              </div>
              <Toggle
                value={globalConfig.rawInputMode}
                onToggle={() => saveGlobalConfig({ ...globalConfig, rawInputMode: !globalConfig.rawInputMode })}
              />
            </div>

            <div className="setting-row">
              <div className="setting-label">
                <span className="title">Auto-Inject on Game Launch</span>
                <span className="desc">Background process scanner automatically deploys vrinject.dll when target game starts</span>
              </div>
              <Toggle
                value={globalConfig.autoInjectOnLaunch}
                onToggle={() => saveGlobalConfig({ ...globalConfig, autoInjectOnLaunch: !globalConfig.autoInjectOnLaunch })}
              />
            </div>

            <div className="setting-row">
              <div className="setting-label">
                <span className="title">Haptic Rumble Passthrough</span>
                <span className="desc">Translate in-game vibration signals to OpenXR VR touch controller haptic actuators</span>
              </div>
              <Toggle
                value={globalConfig.hapticFeedback ?? true}
                onToggle={() => saveGlobalConfig({ ...globalConfig, hapticFeedback: !(globalConfig.hapticFeedback ?? true) })}
              />
            </div>

            {/* Motion Aim Sensitivity Slider Panel */}
            <div style={{
              background: 'rgba(255, 255, 255, 0.015)',
              border: '1px solid rgba(255, 255, 255, 0.05)',
              borderRadius: 6,
              padding: '14px 16px',
              margin: '8px 0 10px 0',
              display: 'flex',
              flexDirection: 'column',
              gap: 12
            }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div className="setting-label">
                  <span className="title">Motion Aim Sensitivity</span>
                  <span className="desc">Head-tracking rotation multiplier for 6DOF precision aiming</span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
                  <button
                    onClick={() => saveGlobalConfig({ ...globalConfig, motionAimSensitivity: 1.0 })}
                    style={{
                      background: 'rgba(255, 255, 255, 0.03)',
                      border: '1px solid rgba(255, 255, 255, 0.1)',
                      color: 'var(--ag-text-muted)',
                      borderRadius: 3,
                      padding: '3px 9px',
                      fontFamily: 'var(--ag-font-mono)',
                      fontSize: 10,
                      cursor: 'pointer',
                      transition: 'all 0.15s ease',
                    }}
                    onMouseEnter={e => { e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.25)'; }}
                    onMouseLeave={e => { e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.1)'; }}
                  >
                    RESET 1.0x
                  </button>
                  <span style={{ fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', fontSize: 13, fontWeight: 700 }}>
                    {globalConfig.motionAimSensitivity.toFixed(1)}x
                  </span>
                </div>
              </div>
              <input
                type="range"
                min="0.1"
                max="5.0"
                step="0.1"
                value={globalConfig.motionAimSensitivity}
                onChange={e => saveGlobalConfig({ ...globalConfig, motionAimSensitivity: parseFloat(e.target.value) })}
              />
            </div>
          </div>
        </div>

        {/* ========================================================= */}
        {/* 4. HARDWARE & RUNTIME TELEMETRY (4-Cell Bento Grid) */}
        {/* ========================================================= */}
        <div className="settings-item-enter stagger-4" style={{ marginBottom: 32 }}>
          {sectionLabel('HARDWARE & RUNTIME TELEMETRY', 'Active compositor environment and graphics injection status')}

          <div style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr',
            gap: 12,
          }}>
            <div style={{
              background: 'linear-gradient(180deg, rgba(255, 255, 255, 0.025) 0%, rgba(255, 255, 255, 0.006) 100%), #060608',
              border: '1px solid rgba(255, 255, 255, 0.07)',
              borderTop: '2px solid #CC0000',
              borderRadius: 6,
              padding: '18px 20px',
              boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.05), 0 4px 14px rgba(0, 0, 0, 0.35)',
              display: 'flex',
              flexDirection: 'column',
              gap: 4
            }}>
              <div style={{ color: '#848884', fontSize: 10, fontFamily: 'var(--ag-font-mono)', letterSpacing: '0.08em', fontWeight: 600 }}>
                OPENXR RUNTIME
              </div>
              <div style={{ color: vrStatus.connected ? '#FFF' : 'rgba(255, 255, 255, 0.75)', fontSize: 15.5, fontFamily: 'var(--ag-font-display)', fontWeight: 800, letterSpacing: '0.03em', marginTop: 1 }}>
                {vrStatus.runtime || 'OpenXR 1.0 Runtime'}
              </div>
              <div style={{ color: '#848884', fontSize: 12, fontFamily: 'var(--ag-font-ui)', lineHeight: '1.4', marginTop: 2 }}>
                {vrStatus.connected ? 'Active System Compositor Connected' : 'SteamVR · Quest Link · Virtual Desktop'}
              </div>
            </div>

            <div style={{
              background: 'linear-gradient(180deg, rgba(255, 255, 255, 0.025) 0%, rgba(255, 255, 255, 0.006) 100%), #060608',
              border: '1px solid rgba(255, 255, 255, 0.07)',
              borderTop: '2px solid #CC0000',
              borderRadius: 6,
              padding: '18px 20px',
              boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.05), 0 4px 14px rgba(0, 0, 0, 0.35)',
              display: 'flex',
              flexDirection: 'column',
              gap: 4
            }}>
              <div style={{ color: '#848884', fontSize: 10, fontFamily: 'var(--ag-font-mono)', letterSpacing: '0.08em', fontWeight: 600 }}>
                DETECTED HEADSET
              </div>
              <div style={{ color: vrStatus.connected ? '#FFF' : 'var(--ag-text-muted)', fontSize: 15.5, fontFamily: 'var(--ag-font-display)', fontWeight: 800, letterSpacing: '0.03em', marginTop: 1 }}>
                {vrStatus.connected ? vrStatus.headset : 'No VR Connected'}
              </div>
              <div style={{ color: vrStatus.connected ? 'var(--ag-accent-success)' : '#848884', fontSize: 12, fontFamily: 'var(--ag-font-ui)', lineHeight: '1.4', marginTop: 2 }}>
                {vrStatus.connected ? `${vrStatus.refreshRate} Hz Target Refresh Rate` : 'Connect an OpenXR or SteamVR headset'}
              </div>
            </div>

            <div style={{
              background: 'linear-gradient(180deg, rgba(255, 255, 255, 0.025) 0%, rgba(255, 255, 255, 0.006) 100%), #060608',
              border: '1px solid rgba(255, 255, 255, 0.07)',
              borderTop: '2px solid #CC0000',
              borderRadius: 6,
              padding: '18px 20px',
              boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.05), 0 4px 14px rgba(0, 0, 0, 0.35)',
              display: 'flex',
              flexDirection: 'column',
              gap: 4
            }}>
              <div style={{ color: '#848884', fontSize: 10, fontFamily: 'var(--ag-font-mono)', letterSpacing: '0.08em', fontWeight: 600 }}>
                GRAPHICS HOOKS
              </div>
              <div style={{ color: '#FFF', fontSize: 15.5, fontFamily: 'var(--ag-font-display)', fontWeight: 800, letterSpacing: '0.03em', marginTop: 1 }}>
                DX11 · DX12 · Vulkan
              </div>
              <div style={{ color: '#848884', fontSize: 12, fontFamily: 'var(--ag-font-ui)', lineHeight: '1.4', marginTop: 2 }}>
                Low-Latency VTable Swapchain &amp; CommandQueue Detours
              </div>
            </div>

            <div style={{
              background: 'linear-gradient(180deg, rgba(255, 255, 255, 0.025) 0%, rgba(255, 255, 255, 0.006) 100%), #060608',
              border: '1px solid rgba(255, 255, 255, 0.07)',
              borderTop: '2px solid #CC0000',
              borderRadius: 6,
              padding: '18px 20px',
              boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.05), 0 4px 14px rgba(0, 0, 0, 0.35)',
              display: 'flex',
              flexDirection: 'column',
              gap: 4
            }}>
              <div style={{ color: '#848884', fontSize: 10, fontFamily: 'var(--ag-font-mono)', letterSpacing: '0.08em', fontWeight: 600 }}>
                NEURAL ACCELERATION
              </div>
              <div style={{ color: '#FFF', fontSize: 15.5, fontFamily: 'var(--ag-font-display)', fontWeight: 800, letterSpacing: '0.03em', marginTop: 1 }}>
                DirectML / ONNX
              </div>
              <div style={{ color: '#848884', fontSize: 12, fontFamily: 'var(--ag-font-ui)', lineHeight: '1.4', marginTop: 2 }}>
                Hardware Direct3D 12 Tensor Inpainting Pipeline
              </div>
            </div>
          </div>
        </div>

        {/* ========================================================= */}
        {/* 5. LIBRARY & STORAGE UTILITIES */}
        {/* ========================================================= */}
        <div className="settings-item-enter stagger-5" style={{ marginBottom: 36 }}>
          {sectionLabel('LIBRARY & STORAGE UTILITIES', 'Maintenance actions, library rescan, and configuration reset')}

          <div className="settings-card" style={{
            padding: '20px 24px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            flexWrap: 'wrap',
            gap: 14
          }}>
            <div>
              <div style={{ fontSize: 15, fontWeight: 700, color: '#FFF', fontFamily: 'var(--ag-font-display)', letterSpacing: '0.025em', marginBottom: 4 }}>
                Library Synchronization &amp; Storage Maintenance
              </div>
              <div style={{ fontSize: 12, color: '#848884', fontFamily: 'var(--ag-font-ui)' }}>
                Rescan discovered games, un-hide dismissed titles, or restore factory defaults.
              </div>
            </div>

            <div style={{ display: 'flex', gap: 10, flexWrap: 'wrap' }}>
              <button
                onClick={handleRescanClick}
                disabled={isRescanning}
                style={{
                  background: 'rgba(255, 255, 255, 0.035)',
                  border: '1px solid rgba(255, 255, 255, 0.12)',
                  boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.06)',
                  color: 'var(--ag-text-primary)',
                  padding: '8px 15px',
                  borderRadius: 4,
                  fontSize: 11.5,
                  fontFamily: 'var(--ag-font-display)',
                  letterSpacing: '0.06em',
                  fontWeight: 600,
                  cursor: isRescanning ? 'wait' : 'pointer',
                  transition: 'all 0.2s ease',
                }}
                onMouseEnter={e => { e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.3)'; e.currentTarget.style.background = 'rgba(255, 255, 255, 0.07)'; }}
                onMouseLeave={e => { e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.12)'; e.currentTarget.style.background = 'rgba(255, 255, 255, 0.035)'; }}
              >
                {isRescanning ? 'RESCANNING...' : 'RESCAN LIBRARIES'}
              </button>
              <button
                onClick={async () => {
                  if (window.ag && window.ag.library) {
                    await window.ag.library.restoreIgnoredGames();
                    await onRescan();
                    setFeedbackMsg('Ignored games have been restored to your active library.');
                    setTimeout(() => setFeedbackMsg(null), 4000);
                  }
                }}
                style={{
                  background: 'rgba(255, 255, 255, 0.035)',
                  border: '1px solid rgba(255, 255, 255, 0.12)',
                  boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.06)',
                  color: 'var(--ag-text-primary)',
                  padding: '8px 15px',
                  borderRadius: 4,
                  fontSize: 11.5,
                  fontFamily: 'var(--ag-font-display)',
                  letterSpacing: '0.06em',
                  fontWeight: 600,
                  cursor: 'pointer',
                  transition: 'all 0.2s ease',
                }}
                onMouseEnter={e => { e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.3)'; e.currentTarget.style.background = 'rgba(255, 255, 255, 0.07)'; }}
                onMouseLeave={e => { e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.12)'; e.currentTarget.style.background = 'rgba(255, 255, 255, 0.035)'; }}
              >
                RESTORE HIDDEN GAMES
              </button>
              <button
                onClick={handleResetDefaults}
                style={{
                  background: 'rgba(204, 0, 0, 0.06)',
                  border: '1px solid rgba(204, 0, 0, 0.35)',
                  boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.05)',
                  color: 'var(--ag-accent)',
                  padding: '8px 15px',
                  borderRadius: 4,
                  fontSize: 11.5,
                  fontFamily: 'var(--ag-font-display)',
                  letterSpacing: '0.06em',
                  fontWeight: 600,
                  cursor: 'pointer',
                  transition: 'all 0.2s ease',
                }}
                onMouseEnter={e => { e.currentTarget.style.background = 'rgba(204, 0, 0, 0.14)'; }}
                onMouseLeave={e => { e.currentTarget.style.background = 'rgba(204, 0, 0, 0.06)'; }}
              >
                RESET DEFAULTS
              </button>
            </div>
          </div>
        </div>

      </div>
    </div>
  );
}
