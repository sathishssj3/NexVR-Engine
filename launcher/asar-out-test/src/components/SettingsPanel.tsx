import type { VRConfig } from '../types';

export function SettingsPanel({ config, onChange }: { config: VRConfig, onChange: (c: VRConfig) => void }) {
  const Toggle = ({ value, onToggle }: { value: boolean, onToggle: () => void }) => (
    <div className={`ag-toggle ${value ? 'on' : 'off'}`} onClick={onToggle} />
  );

  return (
    <div style={{ flexShrink: 0, display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))', gap: 24, marginBottom: 30 }}>
      {/* Card 1 */}
      <div className="glass-card" style={{ padding: '24px 28px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span style={{ fontSize: 15, fontWeight: 500, letterSpacing: '0.5px' }}>Use Recommended Resolution</span>
          <Toggle value={config.useRecommendedResolution} onToggle={() => onChange({...config, useRecommendedResolution: !config.useRecommendedResolution})} />
        </div>
      </div>
      
      {/* Card 2 */}
      <div className="glass-card" style={{ padding: '24px 28px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 16, alignItems: 'center' }}>
          <span style={{ fontSize: 15, fontWeight: 500, letterSpacing: '0.5px' }}>Motion Sensitivity</span>
          <span style={{ fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', textShadow: '0 0 10px rgba(0,240,255,0.5)', fontSize: 16, fontWeight: 'bold' }}>{config.motionAimSensitivity.toFixed(1)}</span>
        </div>
        <input 
          type="range" min="0.1" max="5.0" step="0.1" 
          value={config.motionAimSensitivity} 
          onChange={e => onChange({...config, motionAimSensitivity: parseFloat(e.target.value)})} 
        />
        <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: 24, alignItems: 'center' }}>
          <span style={{ fontSize: 15, fontWeight: 500, letterSpacing: '0.5px' }}>Raw Input Mode</span>
          <Toggle value={config.rawInputMode} onToggle={() => onChange({...config, rawInputMode: !config.rawInputMode})} />
        </div>
      </div>

      {/* Card 3 */}
      <div className="glass-card" style={{ padding: '24px 28px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 24, alignItems: 'center' }}>
          <span style={{ fontSize: 15, fontWeight: 500, letterSpacing: '0.5px' }}>sRGB Correction</span>
          <Toggle value={config.srgbCorrection} onToggle={() => onChange({...config, srgbCorrection: !config.srgbCorrection})} />
        </div>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span style={{ fontSize: 15, fontWeight: 500, letterSpacing: '0.5px' }}>Depth Submission</span>
          <Toggle value={config.depthSubmission} onToggle={() => onChange({...config, depthSubmission: !config.depthSubmission})} />
        </div>
      </div>

      {/* Card 4 */}
      <div className="glass-card" style={{ padding: '24px 28px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span style={{ fontSize: 15, fontWeight: 500, letterSpacing: '0.5px' }}>Auto-Inject on Launch</span>
          <Toggle value={config.autoInjectOnLaunch} onToggle={() => onChange({...config, autoInjectOnLaunch: !config.autoInjectOnLaunch})} />
        </div>
      </div>
    </div>
  );
}
