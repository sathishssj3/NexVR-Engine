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
      srgbCorrection: true,
      contrast: 1.0,
      saturation: 1.0,
      brightness: 1.0,
      depthSubmission: false,
      motionAimSensitivity: 1.0,
      rawInputMode: true,
      autoInjectOnLaunch: true,
    });
  };

  const contrastVal = typeof config.contrast === 'number' ? config.contrast : 1.0;
  const brightnessVal = typeof config.brightness === 'number' ? config.brightness : 1.0;
  const saturationVal = typeof config.saturation === 'number' ? config.saturation : 1.0;

  return (
    <div className="settings-item-enter stagger-2" style={{ flexShrink: 0, marginBottom: 28 }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: 14, flexWrap: 'wrap', gap: 10 }}>
        <div>
          <div style={{
            fontSize: 13,
            fontFamily: 'var(--ag-font-display)',
            color: '#FFFFFF',
            fontWeight: 800,
            letterSpacing: '0.04em',
            marginBottom: 2
          }}>
            PER-TITLE VR CONFIGURATION
          </div>
          <div style={{ fontSize: 11, color: 'var(--ag-text-muted)' }}>
            Hardware optimization, display calibration, and input settings for this specific game
          </div>
        </div>

        <button
          type="button"
          onClick={handleResetToGlobal}
          style={{
            background: 'rgba(255, 255, 255, 0.04)',
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
        
        {/* Card 1: Display & Head Tracking */}
        <div className="settings-card" style={{ padding: '18px 20px', display: 'flex', flexDirection: 'column' }}>
          <div style={{ 
            fontSize: 10.5, 
            fontFamily: 'var(--ag-font-mono)', 
            color: 'var(--ag-accent)', 
            letterSpacing: '0.08em', 
            marginBottom: 14, 
            fontWeight: 700,
            textTransform: 'uppercase'
          }}>
            DISPLAY &amp; HEAD TRACKING
          </div>
          
          <div className="setting-row">
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
              <span className="title">Depth Buffer Reprojection</span>
              <span className="desc">Submit depth buffer to OpenXR for low-latency positional timewarp</span>
            </div>
            <Toggle 
              value={config.depthSubmission} 
              onToggle={() => onChange({...config, depthSubmission: !config.depthSubmission})} 
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

          <div className="setting-row" style={{ flexDirection: 'column' as const, alignItems: 'stretch' as const, gap: 8 }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div className="setting-label">
                <span className="title">Motion Aim Sensitivity</span>
                <span className="desc">Head-tracking rotation multiplier for 6DOF precision aiming</span>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                <span style={{ 
                  fontFamily: 'var(--ag-font-mono)', 
                  color: 'var(--ag-accent)', 
                  fontSize: 13, 
                  fontWeight: 800
                }}>
                  {config.motionAimSensitivity.toFixed(1)}x
                </span>
                {config.motionAimSensitivity !== 1.0 && (
                  <button
                    type="button"
                    onClick={() => onChange({ ...config, motionAimSensitivity: 1.0 })}
                    style={{
                      background: 'rgba(255, 255, 255, 0.06)',
                      border: '1px solid rgba(255, 255, 255, 0.15)',
                      color: 'var(--ag-text-muted)',
                      borderRadius: 4,
                      padding: '2px 6px',
                      fontSize: 10,
                      cursor: 'pointer'
                    }}
                  >
                    1.0x
                  </button>
                )}
              </div>
            </div>
            <input 
              type="range" min="0.1" max="5.0" step="0.1" 
              value={config.motionAimSensitivity} 
              onChange={e => onChange({...config, motionAimSensitivity: parseFloat(e.target.value)})} 
            />
          </div>
        </div>

        {/* Card 2: Color & Display Calibration */}
        <div className="settings-card" style={{ padding: '18px 20px', display: 'flex', flexDirection: 'column' }}>
          <div style={{ 
            display: 'flex', 
            justifyContent: 'space-between', 
            alignItems: 'center', 
            marginBottom: 14 
          }}>
            <div style={{ 
              fontSize: 10.5, 
              fontFamily: 'var(--ag-font-mono)', 
              color: 'var(--ag-accent)', 
              letterSpacing: '0.08em', 
              fontWeight: 700,
              textTransform: 'uppercase'
            }}>
              COLOR &amp; DISPLAY CALIBRATION
            </div>
            {(contrastVal !== 1.0 || brightnessVal !== 1.0 || saturationVal !== 1.0) && (
              <button
                type="button"
                onClick={() => onChange({ ...config, contrast: 1.0, brightness: 1.0, saturation: 1.0 })}
                style={{
                  background: 'rgba(255, 255, 255, 0.05)',
                  border: '1px solid rgba(255, 255, 255, 0.15)',
                  color: 'var(--ag-accent)',
                  borderRadius: 4,
                  padding: '2px 8px',
                  fontSize: 10,
                  fontFamily: 'var(--ag-font-mono)',
                  fontWeight: 700,
                  cursor: 'pointer'
                }}
              >
                RESET (1.0x)
              </button>
            )}
          </div>
          
          <div className="setting-row">
            <div className="setting-label">
              <span className="title">sRGB Display Gamma Calibration</span>
              <span className="desc">Automatic 2.2 gamma cancellation matching desktop monitor shades 1:1</span>
            </div>
            <Toggle 
              value={config.srgbCorrection} 
              onToggle={() => onChange({...config, srgbCorrection: !config.srgbCorrection})} 
            />
          </div>

          <div className="setting-row" style={{ flexDirection: 'column' as const, alignItems: 'stretch' as const, gap: 8 }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div className="setting-label">
                <span className="title">Perceptual Contrast</span>
                <span className="desc">Shadow depth and midtone calibration (1.00x = desktop exact)</span>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                <span style={{ fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', fontSize: 13, fontWeight: 800 }}>
                  {contrastVal.toFixed(2)}x
                </span>
                {contrastVal !== 1.0 && (
                  <button
                    type="button"
                    onClick={() => onChange({ ...config, contrast: 1.0 })}
                    style={{
                      background: 'rgba(255, 255, 255, 0.06)',
                      border: '1px solid rgba(255, 255, 255, 0.15)',
                      color: 'var(--ag-text-muted)',
                      borderRadius: 4,
                      padding: '2px 6px',
                      fontSize: 10,
                      cursor: 'pointer'
                    }}
                  >
                    1.0x
                  </button>
                )}
              </div>
            </div>
            <input 
              type="range" min="0.70" max="1.60" step="0.05" 
              value={contrastVal} 
              onChange={e => onChange({ ...config, contrast: parseFloat(e.target.value) })} 
            />
          </div>

          <div className="setting-row" style={{ flexDirection: 'column' as const, alignItems: 'stretch' as const, gap: 8 }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div className="setting-label">
                <span className="title">VR Brightness Exposure</span>
                <span className="desc">Overall scene brightness gain (1.00x = desktop exact)</span>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                <span style={{ fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', fontSize: 13, fontWeight: 800 }}>
                  {brightnessVal.toFixed(2)}x
                </span>
                {brightnessVal !== 1.0 && (
                  <button
                    type="button"
                    onClick={() => onChange({ ...config, brightness: 1.0 })}
                    style={{
                      background: 'rgba(255, 255, 255, 0.06)',
                      border: '1px solid rgba(255, 255, 255, 0.15)',
                      color: 'var(--ag-text-muted)',
                      borderRadius: 4,
                      padding: '2px 6px',
                      fontSize: 10,
                      cursor: 'pointer'
                    }}
                  >
                    1.0x
                  </button>
                )}
              </div>
            </div>
            <input 
              type="range" min="0.70" max="1.40" step="0.05" 
              value={brightnessVal} 
              onChange={e => onChange({ ...config, brightness: parseFloat(e.target.value) })} 
            />
          </div>

          <div className="setting-row" style={{ flexDirection: 'column' as const, alignItems: 'stretch' as const, gap: 8 }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div className="setting-label">
                <span className="title">Color Saturation</span>
                <span className="desc">Luminance-preserving chroma vibrancy (1.00x = desktop exact)</span>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                <span style={{ fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent)', fontSize: 13, fontWeight: 800 }}>
                  {saturationVal.toFixed(2)}x
                </span>
                {saturationVal !== 1.0 && (
                  <button
                    type="button"
                    onClick={() => onChange({ ...config, saturation: 1.0 })}
                    style={{
                      background: 'rgba(255, 255, 255, 0.06)',
                      border: '1px solid rgba(255, 255, 255, 0.15)',
                      color: 'var(--ag-text-muted)',
                      borderRadius: 4,
                      padding: '2px 6px',
                      fontSize: 10,
                      cursor: 'pointer'
                    }}
                  >
                    1.0x
                  </button>
                )}
              </div>
            </div>
            <input 
              type="range" min="0.70" max="1.60" step="0.05" 
              value={saturationVal} 
              onChange={e => onChange({ ...config, saturation: parseFloat(e.target.value) })} 
            />
          </div>
        </div>
      </div>
    </div>
  );
}
