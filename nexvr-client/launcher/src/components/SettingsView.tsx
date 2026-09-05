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
  const [feedbackMsg, setFeedbackMsg] = useState<string | null>(null);

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
    };
  });

  const saveGlobalConfig = (updated: VRConfig) => {
    setGlobalConfig(updated);
    try {
      localStorage.setItem('nexvr_global_config', JSON.stringify(updated));
    } catch {}
  };

  const handleCheckUpdate = async () => {
    if (checking) return;
    setChecking(true);
    setFeedbackMsg('Querying GitHub Releases for engine hotfixes...');
    try {
      if (window.ag && window.ag.update) {
        const res = await window.ag.update.check();
        onUpdateStatusChange(res);
        if (res.updated) {
          setFeedbackMsg(`✓ Hotfix v${res.version} installed! Restart launcher or inject to apply.`);
        } else if (res.hasUpdate) {
          setFeedbackMsg(`Update v${res.version} downloaded successfully.`);
        } else {
          setFeedbackMsg('✓ Your NexVR Engine launcher is up to date.');
        }
      }
    } catch (e: any) {
      setFeedbackMsg(`Check failed: ${e?.message || 'Network error'}`);
    } finally {
      setChecking(false);
      setTimeout(() => setFeedbackMsg(null), 8000);
    }
  };

  const handleOpenUpdatesFolder = () => {
    if (window.ag && window.ag.update && window.ag.update.openFolder) {
      window.ag.update.openFolder();
    }
  };

  const Toggle = ({ value, onToggle }: { value: boolean; onToggle: () => void }) => (
    <div className={`ag-toggle ${value ? 'on' : 'off'}`} onClick={onToggle} />
  );

  return (
    <div
      style={{
        flex: 1,
        padding: '32px 40px',
        overflowY: 'auto',
        background: 'radial-gradient(circle at 50% 15%, rgba(0, 240, 255, 0.04), transparent 75%)',
      }}
    >
      <div style={{ maxWidth: 860, margin: '0 auto' }}>
        {/* Header */}
        <div style={{ marginBottom: 28 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 4 }}>
            <span style={{ fontSize: 13, color: 'var(--ag-accent)', fontFamily: 'var(--ag-font-mono)', letterSpacing: '2px' }}>
              ◈ SYSTEM CONTROL
            </span>
            <span
              style={{
                padding: '2px 8px',
                borderRadius: 4,
                fontSize: 10,
                background: 'rgba(0, 240, 255, 0.12)',
                border: '1px solid rgba(0, 240, 255, 0.3)',
                color: 'var(--ag-accent)',
                fontFamily: 'var(--ag-font-mono)',
              }}
            >
              OTA READY
            </span>
          </div>
          <h1
            style={{
              fontSize: 28,
              fontWeight: 700,
              fontFamily: 'var(--ag-font-display)',
              letterSpacing: '2px',
              color: '#fff',
              margin: 0,
            }}
          >
            SETTINGS & LAUNCHER UPDATES
          </h1>
          <p style={{ fontSize: 13, color: 'var(--ag-text-muted)', margin: '6px 0 0 0' }}>
            Check for engine updates, inspect recent bug fixes, and configure global injection defaults.
          </p>
        </div>

        {/* ========================================================= */}
        {/* 1. LAUNCHER & ENGINE UPDATES (HERO SECTION) */}
        {/* ========================================================= */}
        <div
          className="glass-card fade-in-up"
          style={{
            padding: '24px 28px',
            marginBottom: 24,
            borderLeft: '4px solid var(--ag-accent)',
            background: 'linear-gradient(135deg, rgba(0, 240, 255, 0.04) 0%, rgba(10, 15, 25, 0.8) 100%)',
          }}
        >
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', flexWrap: 'wrap', gap: 16 }}>
            <div>
              <div style={{ fontSize: 11, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', letterSpacing: '2px', marginBottom: 6 }}>
                ENGINE UPDATE CHANNEL
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
                <span style={{ fontSize: 22, fontWeight: 700, color: '#fff', fontFamily: 'var(--ag-font-mono)' }}>
                  NexVR Engine {updateStatus?.version ? `v${updateStatus.version}` : 'v0.1.0'}
                </span>
                <span
                  style={{
                    display: 'inline-flex',
                    alignItems: 'center',
                    gap: 6,
                    padding: '3px 10px',
                    borderRadius: 12,
                    fontSize: 10,
                    background: updateStatus?.updated
                      ? 'rgba(0, 230, 118, 0.15)'
                      : 'rgba(0, 240, 255, 0.12)',
                    border: `1px solid ${updateStatus?.updated ? 'rgba(0, 230, 118, 0.4)' : 'rgba(0, 240, 255, 0.3)'}`,
                    color: updateStatus?.updated ? 'var(--ag-accent-success)' : 'var(--ag-accent)',
                    fontFamily: 'var(--ag-font-mono)',
                    fontWeight: 600,
                  }}
                >
                  <span
                    style={{
                      width: 6,
                      height: 6,
                      borderRadius: '50%',
                      background: updateStatus?.updated ? 'var(--ag-accent-success)' : 'var(--ag-accent)',
                      boxShadow: `0 0 8px ${updateStatus?.updated ? 'var(--ag-accent-success)' : 'var(--ag-accent)'}`,
                    }}
                  />
                  {updateStatus?.updated ? 'HOTFIX ACTIVE' : 'UP TO DATE'}
                </span>
              </div>
              <div style={{ fontSize: 12, color: 'var(--ag-text-muted)', marginTop: 4 }}>
                Automated Over-The-Air (OTA) updates deploy verified binaries directly to your PC without full reinstall.
              </div>
            </div>

            <div style={{ display: 'flex', gap: 10, alignItems: 'center' }}>
              <button
                onClick={handleOpenUpdatesFolder}
                style={{
                  background: 'rgba(255,255,255,0.05)',
                  border: '1px solid rgba(255,255,255,0.12)',
                  color: 'var(--ag-text-muted)',
                  borderRadius: 6,
                  padding: '9px 14px',
                  fontFamily: 'var(--ag-font-mono)',
                  fontSize: 11,
                  letterSpacing: '0.5px',
                  cursor: 'pointer',
                  transition: 'all 0.2s ease',
                }}
                onMouseEnter={e => {
                  e.currentTarget.style.borderColor = 'rgba(255,255,255,0.3)';
                  e.currentTarget.style.color = '#fff';
                }}
                onMouseLeave={e => {
                  e.currentTarget.style.borderColor = 'rgba(255,255,255,0.12)';
                  e.currentTarget.style.color = 'var(--ag-text-muted)';
                }}
              >
                📁 Open Folder
              </button>

              <button
                onClick={handleCheckUpdate}
                disabled={checking}
                style={{
                  background: checking
                    ? 'rgba(0, 240, 255, 0.1)'
                    : 'linear-gradient(135deg, rgba(0, 240, 255, 0.25), rgba(0, 240, 255, 0.1))',
                  border: '1px solid rgba(0, 240, 255, 0.4)',
                  color: '#fff',
                  borderRadius: 6,
                  padding: '9px 18px',
                  fontFamily: 'var(--ag-font-mono)',
                  fontSize: 11,
                  letterSpacing: '1px',
                  fontWeight: 600,
                  cursor: checking ? 'wait' : 'pointer',
                  boxShadow: '0 0 15px rgba(0, 240, 255, 0.15)',
                  transition: 'all 0.2s ease',
                }}
                onMouseEnter={e => {
                  if (!checking) e.currentTarget.style.boxShadow = '0 0 25px rgba(0, 240, 255, 0.35)';
                }}
                onMouseLeave={e => {
                  if (!checking) e.currentTarget.style.boxShadow = '0 0 15px rgba(0, 240, 255, 0.15)';
                }}
              >
                {checking ? '⟳ Checking Releases...' : '⟳ Check for Updates'}
              </button>
            </div>
          </div>

          {feedbackMsg && (
            <div
              style={{
                marginTop: 16,
                padding: '8px 12px',
                borderRadius: 4,
                fontSize: 11,
                fontFamily: 'var(--ag-font-mono)',
                background: feedbackMsg.includes('✓') ? 'rgba(0, 230, 118, 0.1)' : 'rgba(0, 240, 255, 0.08)',
                border: `1px solid ${feedbackMsg.includes('✓') ? 'rgba(0, 230, 118, 0.3)' : 'rgba(0, 240, 255, 0.2)'}`,
                color: feedbackMsg.includes('✓') ? 'var(--ag-accent-success)' : 'var(--ag-accent)',
              }}
            >
              {feedbackMsg}
            </div>
          )}

          {/* ========================================================= */}
          {/* CHANGELOG: WHAT IS NEWLY ADDED & WHAT PROBLEM IS FIXED */}
          {/* ========================================================= */}
          <div style={{ marginTop: 22, paddingTop: 18, borderTop: '1px solid rgba(255,255,255,0.06)' }}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 14 }}>
              <div style={{ fontSize: 11, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', letterSpacing: '1.5px' }}>
                ◈ WHAT'S NEW & FIXED IN THIS BUILD
              </div>
              <span style={{ fontSize: 10, color: 'var(--ag-text-muted)', fontFamily: 'var(--ag-font-mono)' }}>
                Release: {updateStatus?.version ? `v${updateStatus.version} Hotfix` : 'v0.1.6 Hotfix'}
              </span>
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(360px, 1fr))', gap: 14 }}>
              {/* Problems Fixed */}
              <div
                style={{
                  background: 'rgba(0, 0, 0, 0.3)',
                  border: '1px solid rgba(0, 230, 118, 0.2)',
                  borderRadius: 6,
                  padding: '14px 16px',
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 12, fontWeight: 600, color: 'var(--ag-accent-success)', marginBottom: 8, fontFamily: 'var(--ag-font-mono)' }}>
                  <span>✓</span> PROBLEMS FIXED
                </div>
                <ul style={{ margin: 0, paddingLeft: 18, fontSize: 12, color: 'var(--ag-text-primary)', lineHeight: 1.6 }}>
                  {updateStatus?.fixes && updateStatus.fixes.length > 0 ? (
                    updateStatus.fixes.map((fix, idx) => (
                      <li key={idx}>
                        <strong style={{ color: '#fff' }}>{fix.split(':')[0]}</strong>
                        {fix.includes(':') ? `:${fix.split(':').slice(1).join(':')}` : ''}
                      </li>
                    ))
                  ) : (
                    <>
                      <li>
                        <strong style={{ color: '#fff' }}>Universal Machine Compatibility</strong>: Fixed Error 22 (Unauthorized caller) on tester machines with standard Windows UAC.
                      </li>
                      <li>
                        <strong style={{ color: '#fff' }}>Protected Game Directories</strong>: Fixed silent injection failure in Sekiro caused by missing elevated permissions to write to Steam directories.
                      </li>
                      <li>
                        <strong style={{ color: '#fff' }}>Dependency Synchronization</strong>: Automatically syncs DirectML.dll, onnxruntime.dll, and vrinject.json into protected target folders.
                      </li>
                    </>
                  )}
                </ul>
              </div>

              {/* Newly Added Features */}
              <div
                style={{
                  background: 'rgba(0, 0, 0, 0.3)',
                  border: '1px solid rgba(0, 240, 255, 0.2)',
                  borderRadius: 6,
                  padding: '14px 16px',
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 12, fontWeight: 600, color: 'var(--ag-accent)', marginBottom: 8, fontFamily: 'var(--ag-font-mono)' }}>
                  <span>★</span> NEWLY ADDED FEATURES
                </div>
                <ul style={{ margin: 0, paddingLeft: 18, fontSize: 12, color: 'var(--ag-text-primary)', lineHeight: 1.6 }}>
                  {updateStatus?.features && updateStatus.features.length > 0 ? (
                    updateStatus.features.map((feat, idx) => (
                      <li key={idx}>
                        <strong style={{ color: '#fff' }}>{feat.split(':')[0]}</strong>
                        {feat.includes(':') ? `:${feat.split(':').slice(1).join(':')}` : ''}
                      </li>
                    ))
                  ) : (
                    <>
                      <li>
                        <strong style={{ color: '#fff' }}>Universal Multi-Machine Injection</strong>: Allowed UAC elevation services (svchost, consent) and NexVR launcher names across all Windows installations.
                      </li>
                      <li>
                        <strong style={{ color: '#fff' }}>Real Exit Code Propagation</strong>: Propagates actual UAC injection exit codes directly to the launcher UI.
                      </li>
                      <li>
                        <strong style={{ color: '#fff' }}>Auto-resolving Copy Paths</strong>: Resolves executable directories automatically if CLI flags are omitted.
                      </li>
                    </>
                  )}
                </ul>
              </div>
            </div>
          </div>
        </div>

        {/* ========================================================= */}
        {/* 2. GLOBAL INJECTION SETTINGS */}
        {/* ========================================================= */}
        <div className="section-header" style={{ marginBottom: 12 }}>
          GLOBAL INJECTION CONFIGURATION
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(360px, 1fr))', gap: 16, marginBottom: 28 }}>
          {/* Display & Resolution */}
          <div className="glass-card" style={{ padding: '20px 24px' }}>
            <div style={{ fontSize: 11, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', letterSpacing: '2px', marginBottom: 16, opacity: 0.8 }}>
              ◈ DISPLAY & PROJECTION
            </div>

            <div className="setting-row" style={{ borderBottom: '1px solid rgba(255,255,255,0.03)', paddingTop: 0 }}>
              <div className="setting-label">
                <span className="title">Match Headset Resolution</span>
                <span className="desc">Render at native OpenXR HMD panel resolution</span>
              </div>
              <Toggle
                value={globalConfig.useRecommendedResolution}
                onToggle={() => saveGlobalConfig({ ...globalConfig, useRecommendedResolution: !globalConfig.useRecommendedResolution })}
              />
            </div>

            <div className="setting-row" style={{ borderBottom: '1px solid rgba(255,255,255,0.03)' }}>
              <div className="setting-label">
                <span className="title">sRGB Color Correction</span>
                <span className="desc">Automatic gamma curve linearization to prevent washout</span>
              </div>
              <Toggle
                value={globalConfig.srgbCorrection}
                onToggle={() => saveGlobalConfig({ ...globalConfig, srgbCorrection: !globalConfig.srgbCorrection })}
              />
            </div>

            <div className="setting-row">
              <div className="setting-label">
                <span className="title">Depth Buffer Submission</span>
                <span className="desc">Submit depth to OpenXR compositor for SpaceWarp timewarp</span>
              </div>
              <Toggle
                value={globalConfig.depthSubmission}
                onToggle={() => saveGlobalConfig({ ...globalConfig, depthSubmission: !globalConfig.depthSubmission })}
              />
            </div>
          </div>

          {/* Input & Automation */}
          <div className="glass-card" style={{ padding: '20px 24px' }}>
            <div style={{ fontSize: 11, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', letterSpacing: '2px', marginBottom: 16, opacity: 0.8 }}>
              ◈ INPUT & AUTOMATION
            </div>

            <div className="setting-row" style={{ borderBottom: '1px solid rgba(255,255,255,0.03)', paddingTop: 0 }}>
              <div className="setting-label">
                <span className="title">Raw Input Mode</span>
                <span className="desc">Low-latency controller polling bypassing Windows message queue</span>
              </div>
              <Toggle
                value={globalConfig.rawInputMode}
                onToggle={() => saveGlobalConfig({ ...globalConfig, rawInputMode: !globalConfig.rawInputMode })}
              />
            </div>

            <div className="setting-row" style={{ borderBottom: '1px solid rgba(255,255,255,0.03)' }}>
              <div className="setting-label">
                <span className="title">Auto-Inject on Launch</span>
                <span className="desc">Automatically deploy hooks when target process is detected</span>
              </div>
              <Toggle
                value={globalConfig.autoInjectOnLaunch}
                onToggle={() => saveGlobalConfig({ ...globalConfig, autoInjectOnLaunch: !globalConfig.autoInjectOnLaunch })}
              />
            </div>

            <div className="setting-row" style={{ flexDirection: 'column', alignItems: 'stretch', gap: 10 }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div className="setting-label">
                  <span className="title">Motion Aim Sensitivity</span>
                  <span className="desc">Controller head-tracking multiplier</span>
                </div>
                <span style={{ fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', fontSize: 16, fontWeight: 700 }}>
                  {globalConfig.motionAimSensitivity.toFixed(1)}x
                </span>
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
        {/* 3. TESTER DIAGNOSTICS & SYSTEM INFO */}
        {/* ========================================================= */}
        <div className="section-header" style={{ marginBottom: 12 }}>
          TESTER DIAGNOSTICS & SYSTEM STATUS
        </div>
        <div className="glass-card" style={{ padding: '20px 24px' }}>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: 16, marginBottom: 18 }}>
            <div>
              <div style={{ fontSize: 10, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-text-muted)' }}>OPENXR RUNTIME</div>
              <div style={{ fontSize: 14, fontWeight: 600, color: vrStatus.connected ? 'var(--ag-accent-success)' : 'var(--ag-accent-warn)' }}>
                {vrStatus.runtime} {vrStatus.connected ? '✓' : '(Not Connected)'}
              </div>
            </div>
            <div>
              <div style={{ fontSize: 10, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-text-muted)' }}>DETECTED HEADSET</div>
              <div style={{ fontSize: 14, fontWeight: 600, color: '#fff' }}>
                {vrStatus.headset}
              </div>
            </div>
            <div>
              <div style={{ fontSize: 10, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-text-muted)' }}>REFRESH RATE</div>
              <div style={{ fontSize: 14, fontWeight: 600, color: 'var(--ag-accent)' }}>
                {vrStatus.refreshRate} Hz (11.1ms frame budget)
              </div>
            </div>
          </div>

          <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap', paddingTop: 14, borderTop: '1px solid rgba(255,255,255,0.05)' }}>
            <button
              onClick={onRescan}
              style={{
                background: 'rgba(255,255,255,0.05)',
                border: '1px solid rgba(255,255,255,0.1)',
                color: '#fff',
                padding: '8px 14px',
                borderRadius: 4,
                fontSize: 11,
                fontFamily: 'var(--ag-font-mono)',
                cursor: 'pointer',
              }}
            >
              ⟳ Rescan Steam & Epic Libraries
            </button>
            <button
              onClick={async () => {
                if (window.ag && window.ag.library) {
                  await window.ag.library.restoreIgnoredGames();
                  await onRescan();
                }
              }}
              style={{
                background: 'rgba(255,255,255,0.05)',
                border: '1px solid rgba(255,255,255,0.1)',
                color: '#fff',
                padding: '8px 14px',
                borderRadius: 4,
                fontSize: 11,
                fontFamily: 'var(--ag-font-mono)',
                cursor: 'pointer',
              }}
            >
              Reset Ignored Games
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
