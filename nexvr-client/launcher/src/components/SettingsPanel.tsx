import type { VRConfig } from '../types';

export function SettingsPanel({ config, onChange }: { config: VRConfig, onChange: (c: VRConfig) => void }) {
  const Toggle = ({ value, onToggle }: { value: boolean, onToggle: () => void }) => (
    <div className={`ag-toggle ${value ? 'on' : 'off'}`} onClick={onToggle} />
  );

  const sectionLabel = (title: string, subtitle?: string) => (
    <div style={{ marginBottom: 14 }}>
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
    <div className="settings-item-enter stagger-2" style={{ flexShrink: 0, marginBottom: 28 }}>
      {sectionLabel(
        'PER-TITLE VR CONFIGURATION',
        'Direct OpenXR stereo reprojection, lens color calibration, and motion response for this executable'
      )}

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
              <span className="title">Match Headset Resolution</span>
              <span className="desc">Render swapchain buffers at native headset display clarity</span>
            </div>
            <Toggle 
              value={config.useRecommendedResolution} 
              onToggle={() => onChange({...config, useRecommendedResolution: !config.useRecommendedResolution})} 
            />
          </div>
          
          <div className="setting-row">
            <div className="setting-label">
              <span className="title">Color &amp; Gamma Correction</span>
              <span className="desc">Apply sRGB linear conversion optimized for VR lenses</span>
            </div>
            <Toggle 
              value={config.srgbCorrection} 
              onToggle={() => onChange({...config, srgbCorrection: !config.srgbCorrection})} 
            />
          </div>

          <div className="setting-row">
            <div className="setting-label">
              <span className="title">Depth Buffer Submission</span>
              <span className="desc">Submit depth buffer to OpenXR for positional timewarp</span>
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
                <span className="title">Tracking Sensitivity</span>
                <span className="desc">Motion response multiplier for 6DOF rotation</span>
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
              <span className="title">Direct Raw Input Mode</span>
              <span className="desc">Bypass Windows input queue for sub-millisecond response</span>
            </div>
            <Toggle 
              value={config.rawInputMode} 
              onToggle={() => onChange({...config, rawInputMode: !config.rawInputMode})} 
            />
          </div>

          <div className="setting-row">
            <div className="setting-label">
              <span className="title">Auto-Start in VR</span>
              <span className="desc">Automatically inject runtime when title process starts</span>
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
