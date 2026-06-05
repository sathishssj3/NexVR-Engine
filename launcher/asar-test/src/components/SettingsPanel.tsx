import { VRConfig } from '../types';

export function SettingsPanel({ config, onChange }: { config: VRConfig, onChange: (c: VRConfig) => void }) {
  const Toggle = ({ value, onToggle }: { value: boolean, onToggle: () => void }) => (
    <div className={`ag-toggle ${value ? 'on' : 'off'}`} onClick={onToggle} />
  );

  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 15, marginBottom: 20 }}>
      {/* Card 1 */}
      <div style={{ background: 'var(--ag-bg-surface)', padding: 15, borderRadius: 6, border: '1px solid var(--ag-border)' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 10 }}>
          <span>Use Recommended Resolution</span>
          <Toggle value={config.useRecommendedResolution} onToggle={() => onChange({...config, useRecommendedResolution: !config.useRecommendedResolution})} />
        </div>
      </div>
      
      {/* Card 2 */}
      <div style={{ background: 'var(--ag-bg-surface)', padding: 15, borderRadius: 6, border: '1px solid var(--ag-border)' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 10 }}>
          <span>Motion Sensitivity</span>
          <span style={{ fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)' }}>{config.motionAimSensitivity.toFixed(1)}</span>
        </div>
        <input 
          type="range" min="0.1" max="5.0" step="0.1" 
          value={config.motionAimSensitivity} 
          onChange={e => onChange({...config, motionAimSensitivity: parseFloat(e.target.value)})} 
        />
        <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: 15 }}>
          <span>Raw Input Mode</span>
          <Toggle value={config.rawInputMode} onToggle={() => onChange({...config, rawInputMode: !config.rawInputMode})} />
        </div>
      </div>

      {/* Card 3 */}
      <div style={{ background: 'var(--ag-bg-surface)', padding: 15, borderRadius: 6, border: '1px solid var(--ag-border)' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 10 }}>
          <span>sRGB Correction</span>
          <Toggle value={config.srgbCorrection} onToggle={() => onChange({...config, srgbCorrection: !config.srgbCorrection})} />
        </div>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: 15 }}>
          <span>Depth Submission</span>
          <Toggle value={config.depthSubmission} onToggle={() => onChange({...config, depthSubmission: !config.depthSubmission})} />
        </div>
      </div>

      {/* Card 4 */}
      <div style={{ background: 'var(--ag-bg-surface)', padding: 15, borderRadius: 6, border: '1px solid var(--ag-border)' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 10 }}>
          <span>Auto-Inject on Launch</span>
          <Toggle value={config.autoInjectOnLaunch} onToggle={() => onChange({...config, autoInjectOnLaunch: !config.autoInjectOnLaunch})} />
        </div>
      </div>
    </div>
  );
}
