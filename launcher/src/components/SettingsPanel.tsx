import type { VRConfig } from '../types';

export function SettingsPanel({ config, onChange }: { config: VRConfig, onChange: (c: VRConfig) => void }) {
  const Toggle = ({ value, onToggle }: { value: boolean, onToggle: () => void }) => (
    <div className={`ag-toggle ${value ? 'on' : 'off'}`} onClick={onToggle} />
  );

  return (
    <div className="fade-in-up stagger-2" style={{ flexShrink: 0, marginBottom: 30 }}>
      <div className="section-header">INJECTION SETTINGS</div>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(320px, 1fr))', gap: 20 }}>
        {/* Display & Resolution */}
        <div className="glass-card" style={{ padding: '20px 24px' }}>
          <div style={{ fontSize: 11, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', letterSpacing: '2px', marginBottom: 16, opacity: 0.7 }}>
            ◈ DISPLAY
          </div>
          
          <div className="setting-row" style={{ borderBottom: '1px solid rgba(255,255,255,0.03)', paddingTop: 0 }}>
            <div className="setting-label">
              <span className="title">Use Recommended Resolution</span>
              <span className="desc">Match headset native panel resolution</span>
            </div>
            <Toggle value={config.useRecommendedResolution} onToggle={() => onChange({...config, useRecommendedResolution: !config.useRecommendedResolution})} />
          </div>
          
          <div className="setting-row" style={{ borderBottom: '1px solid rgba(255,255,255,0.03)' }}>
            <div className="setting-label">
              <span className="title">sRGB Correction</span>
              <span className="desc">Gamma correction for accurate VR colors</span>
            </div>
            <Toggle value={config.srgbCorrection} onToggle={() => onChange({...config, srgbCorrection: !config.srgbCorrection})} />
          </div>

          <div className="setting-row">
            <div className="setting-label">
              <span className="title">Depth Submission</span>
              <span className="desc">Enable SpaceWarp + better reprojection</span>
            </div>
            <Toggle value={config.depthSubmission} onToggle={() => onChange({...config, depthSubmission: !config.depthSubmission})} />
          </div>
        </div>

        {/* Input & Motion */}
        <div className="glass-card" style={{ padding: '20px 24px' }}>
          <div style={{ fontSize: 11, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', letterSpacing: '2px', marginBottom: 16, opacity: 0.7 }}>
            ◈ INPUT & MOTION
          </div>
          
          <div className="setting-row" style={{ borderBottom: '1px solid rgba(255,255,255,0.03)', paddingTop: 0, flexDirection: 'column' as const, alignItems: 'stretch' as const, gap: 12 }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div className="setting-label">
                <span className="title">Motion Sensitivity</span>
                <span className="desc">Head-tracking movement scale</span>
              </div>
              <span style={{ fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', textShadow: '0 0 10px rgba(0,240,255,0.5)', fontSize: 18, fontWeight: 'bold', minWidth: 40, textAlign: 'right' as const }}>{config.motionAimSensitivity.toFixed(1)}</span>
            </div>
            <input 
              type="range" min="0.1" max="5.0" step="0.1" 
              value={config.motionAimSensitivity} 
              onChange={e => onChange({...config, motionAimSensitivity: parseFloat(e.target.value)})} 
            />
          </div>

          <div className="setting-row" style={{ borderBottom: '1px solid rgba(255,255,255,0.03)' }}>
            <div className="setting-label">
              <span className="title">Raw Input Mode</span>
              <span className="desc">Bypass Windows input for lower latency</span>
            </div>
            <Toggle value={config.rawInputMode} onToggle={() => onChange({...config, rawInputMode: !config.rawInputMode})} />
          </div>

          <div className="setting-row">
            <div className="setting-label">
              <span className="title">Auto-Inject on Launch</span>
              <span className="desc">Inject automatically when game starts</span>
            </div>
            <Toggle value={config.autoInjectOnLaunch} onToggle={() => onChange({...config, autoInjectOnLaunch: !config.autoInjectOnLaunch})} />
          </div>
        </div>
      </div>
    </div>
  );
}
