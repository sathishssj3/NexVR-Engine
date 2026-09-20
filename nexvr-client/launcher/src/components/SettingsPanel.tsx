import type { VRConfig } from '../types';

export function SettingsPanel({ config, onChange }: { config: VRConfig, onChange: (c: VRConfig) => void }) {
  const Toggle = ({ value, onToggle }: { value: boolean, onToggle: () => void }) => (
    <div className={`ag-toggle ${value ? 'on' : 'off'}`} onClick={onToggle} />
  );

  const handleResetToGlobal = () => {
    try {
      const saved = localStorage.getItem('nexvr_global_config');
      if (saved) {
        onChange(JSON.parse(saved));
        return;
      }
    } catch {}
    // Fallback defaults
    onChange({
      ...config,
      useRecommendedResolution: true,
      srgbCorrection: false,
      depthSubmission: false,
      motionAimSensitivity: 1.0,
      rawInputMode: true,
      autoInjectOnLaunch: true,
    });
  };

  return (
    <div className="settings-item-enter stagger-2" style={{ flexShrink: 0, marginBottom: 28 }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: 14, flexWrap: 'wrap', gap: 10 }}>
        <div>
          <div style={{
            fontSize: 13,
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
            }} />
            PER-TITLE VR CONFIGURATION
          </div>
          <div style={{
            fontSize: 11.5,
            fontFamily: 'var(--ag-font-ui)',
            color: 'var(--ag-text-muted)',
            marginTop: 4,
            paddingLeft: 11,
            lineHeight: 1.4,
          }}>
            Custom overrides for this executable. Overrides global engine defaults when injected.
          </div>
        </div>

        <button
          type="button"
          onClick={handleResetToGlobal}
          style={{
            background: 'rgba(255, 255, 255, 0.035)',
            border: '1px solid rgba(255, 255, 255, 0.12)',
            color: 'var(--ag-text-muted)',
            borderRadius: 'var(--ag-radius-sm)',
            padding: '6px 14px',
            fontFamily: 'var(--ag-font-display)',
            fontSize: 10.5,
            fontWeight: 700,
            letterSpacing: '0.06em',
            cursor: 'pointer',
            transition: 'all 0.15s ease'
          }}
          onMouseEnter={e => {
            e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.3)';
            e.currentTarget.style.color = '#FFFFFF';
          }}
          onMouseLeave={e => {
            e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.12)';
            e.currentTarget.style.color = 'var(--ag-text-muted)';
          }}
        >
          RESET TO GLOBAL DEFAULTS
        </button>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(320px, 1fr))', gap: 14 }}>
        
        {/* Card 1: Display & Resolution */}
        <div className="settings-card" style={{ padding: '18px 20px' }}>
          <div style={{ 
            fontSize: 10.5, 
            fontFamily: 'var(--ag-font-mono)', 
            color: 'var(--ag-accent)', 
            letterSpacing: '0.08em', 
            marginBottom: 14, 
            fontWeight: 700,
            textTransform: 'uppercase'
          }}>
            DISPLAY &amp; STEREO REPROJECTION
          </div>
          
          <div className="setting-row" style={{ paddingTop: 0 }}>
            <div className="setting-label">
              <span className="title">Native OpenXR Resolution</span>
              <span className="desc">Render swapchain buffers at native headset display clarity</span>
            </div>
            <Toggle 
              value={config.useRecommendedResolution} 
              onToggle={() => onChange({...config, useRecommendedResolution: !config.useRecommendedResolution})} 
            />
          </div>
          
          <div className="setting-row">
            <div className="setting-label">
              <span className="title">sRGB Lens Tonemapping</span>
              <span className="desc">Apply sRGB linear conversion optimized for VR optical lenses</span>
            </div>
            <Toggle 
              value={config.srgbCorrection} 
              onToggle={() => onChange({...config, srgbCorrection: !config.srgbCorrection})} 
            />
          </div>

          <div className="setting-row">
            <div className="setting-label">
              <span className="title">Depth Buffer Reprojection</span>
              <span className="desc">Submit depth buffer to OpenXR for low-latency positional timewarp</span>
            </div>
            <Toggle 
              value={config.depthSubmission} 
              onToggle={() => onChange({...config, depthSubmission: !config.depthSubmission})} 
            />
          </div>
        </div>

        {/* Card 2: Controls & Motion */}
        <div className="settings-card" style={{ padding: '18px 20px' }}>
          <div style={{ 
            fontSize: 10.5, 
            fontFamily: 'var(--ag-font-mono)', 
            color: 'var(--ag-accent)', 
            letterSpacing: '0.08em', 
            marginBottom: 14, 
            fontWeight: 700,
            textTransform: 'uppercase'
          }}>
            CONTROLS &amp; HEAD TRACKING
          </div>
          
          <div className="setting-row" style={{ paddingTop: 0, flexDirection: 'column' as const, alignItems: 'stretch' as const, gap: 10 }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div className="setting-label">
                <span className="title">Motion Aim Sensitivity</span>
                <span className="desc">Head-tracking rotation multiplier for 6DOF precision aiming</span>
              </div>
              <span style={{ 
                fontFamily: 'var(--ag-font-mono)', 
                color: 'var(--ag-accent)', 
                fontSize: 14, 
                fontWeight: 800
              }}>
                {config.motionAimSensitivity.toFixed(1)}x
              </span>
            </div>
            <input 
              type="range" min="0.1" max="5.0" step="0.1" 
              value={config.motionAimSensitivity} 
              onChange={e => onChange({...config, motionAimSensitivity: parseFloat(e.target.value)})} 
            />
          </div>

          <div className="setting-row">
            <div className="setting-label">
              <span className="title">Raw 6DOF Input Mode</span>
              <span className="desc">Direct motion controller polling bypassing simulated mouse input</span>
            </div>
            <Toggle 
              value={config.rawInputMode} 
              onToggle={() => onChange({...config, rawInputMode: !config.rawInputMode})} 
            />
          </div>

          <div className="setting-row">
            <div className="setting-label">
              <span className="title">Auto-Inject on Game Launch</span>
              <span className="desc">Automatically deploy runtime when target game process starts</span>
            </div>
            <Toggle 
              value={config.autoInjectOnLaunch} 
              onToggle={() => onChange({...config, autoInjectOnLaunch: !config.autoInjectOnLaunch})} 
            />
          </div>
        </div>
      </div>
    </div>
  );
}
