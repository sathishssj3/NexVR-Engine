import { SettingsPanel } from './SettingsPanel';
import { SessionLog } from './SessionLog';

export function GameDetail({ game, config, onConfigChange, logLines, onRemoveGame }: any) {
  const apiColors: Record<string, string> = {
    DX11: 'var(--ag-accent)',
    DX12: 'var(--ag-accent-success)',
    Vulkan: '#ff6b6b',
    Unknown: 'var(--ag-text-muted)'
  };

  const compatLabels: Record<string, { color: string; label: string }> = {
    verified: { color: 'var(--ag-accent-success)', label: '✓ VERIFIED' },
    beta:     { color: 'var(--ag-accent-warn)', label: '◐ BETA' },
    new:      { color: 'var(--ag-accent)', label: '★ NEW' },
    unknown:  { color: 'var(--ag-text-muted)', label: '? UNKNOWN' }
  };

  const apiColor = apiColors[game.api] || apiColors.Unknown;
  const compat = compatLabels[game.compat] || compatLabels.unknown;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', minHeight: '100%' }}>
      {/* Hero Header */}
      <div className="fade-in-up" style={{ 
        flexShrink: 0, position: 'relative', marginBottom: 28, padding: '32px 40px', 
        background: 'linear-gradient(135deg, rgba(20, 26, 38, 0.8) 0%, rgba(10, 13, 19, 0.6) 100%)', 
        borderRadius: 'var(--ag-radius-lg)', 
        border: '1px solid rgba(255,255,255,0.08)', 
        borderLeft: `4px solid ${apiColor}`,
        boxShadow: `0 20px 50px rgba(0,0,0,0.4), inset 0 1px 2px rgba(255,255,255,0.1), -2px 0 30px ${apiColor}20`, 
        overflow: 'hidden' 
      }}>
        {/* Dynamic Abstract Background Elements */}
        <div style={{ position: 'absolute', top: '-20%', right: '-10%', width: '70%', height: '140%', background: `radial-gradient(ellipse at center, ${apiColor}15, transparent 60%)`, pointerEvents: 'none', transform: 'rotate(15deg)' }} />
        <div style={{ position: 'absolute', bottom: '-40%', left: '-20%', width: '60%', height: '120%', background: `radial-gradient(ellipse at center, ${apiColor}10, transparent 60%)`, pointerEvents: 'none', transform: 'rotate(-25deg)' }} />
        <div style={{ position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, background: 'url("data:image/svg+xml,%3Csvg width=\'20\' height=\'20\' viewBox=\'0 0 20 20\' xmlns=\'http://www.w3.org/2000/svg\'%3E%3Ccircle cx=\'2\' cy=\'2\' r=\'1\' fill=\'rgba(255,255,255,0.08)\'/%3E%3C/svg%3E")', pointerEvents: 'none' }} />
        
        {/* Title */}
        <h1 style={{ 
          position: 'relative', margin: '0 0 10px 0', fontSize: 40, fontWeight: 800, 
          letterSpacing: '-0.5px', textShadow: '0 4px 20px rgba(0,0,0,0.6), 0 0 40px rgba(255,255,255,0.1)',
          lineHeight: 1.1, fontFamily: 'var(--ag-font-display)'
        }}>
          {game.name}
        </h1>
        
        {/* Path */}
        <div style={{ 
          position: 'relative', fontFamily: 'var(--ag-font-mono)', fontSize: 13, 
          color: 'var(--ag-text-muted)', marginBottom: 20, display: 'flex', alignItems: 'center',
          opacity: 0.9, background: 'rgba(0,0,0,0.3)', padding: '6px 12px', borderRadius: 4, width: 'fit-content'
        }}>
          <span style={{ color: `${apiColor}`, marginRight: 10, fontWeight: 'bold', textShadow: `0 0 8px ${apiColor}80` }}>PATH //</span> 
          <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', maxWidth: 600 }}>{game.installPath}</span>
        </div>
        
        {/* Tags and Actions */}
        <div style={{ position: 'relative', display: 'flex', gap: 12, alignItems: 'center', flexWrap: 'wrap' }}>
          <span className="api-badge" style={{ background: `linear-gradient(90deg, ${apiColor}20, rgba(0,0,0,0.5))` }}>
            <span style={{ opacity: 0.7, fontSize: 10 }}>API</span>
            <strong style={{ color: apiColor, textShadow: `0 0 10px ${apiColor}90`, fontSize: 13 }}>{game.api}</strong>
          </span>
          
          <span className="api-badge" style={{ background: `linear-gradient(90deg, ${compat.color}20, rgba(0,0,0,0.5))` }}>
            <strong style={{ color: compat.color, textShadow: `0 0 10px ${compat.color}80`, fontSize: 13 }}>{compat.label}</strong>
          </span>

          {game.sizeGB > 0 && (
            <span className="api-badge" style={{ opacity: 0.8 }}>
              <span style={{ opacity: 0.7, fontSize: 10 }}>SIZE</span>
              <span style={{ fontSize: 13 }}>{game.sizeGB.toFixed(1)} GB</span>
            </span>
          )}
          
          <button 
            onClick={onRemoveGame} 
            className="btn-glow" 
            style={{ 
              marginLeft: 'auto', padding: '10px 24px', borderRadius: 'var(--ag-radius-sm)', 
              background: 'rgba(255,0,60,0.08)', border: '1px solid rgba(255,0,60,0.4)', 
              color: 'var(--ag-accent-danger)', fontSize: 12, cursor: 'pointer', 
              fontFamily: 'var(--ag-font-mono)', fontWeight: 'bold', letterSpacing: '1.5px', 
              textShadow: '0 0 10px rgba(255,0,60,0.4)', transition: 'all 0.3s var(--ag-transition)',
              boxShadow: '0 4px 15px rgba(255,0,60,0.1)'
            }}
            onMouseEnter={e => { e.currentTarget.style.background = 'rgba(255,0,60,0.15)'; e.currentTarget.style.boxShadow = '0 6px 20px rgba(255,0,60,0.2)'; e.currentTarget.style.transform = 'translateY(-2px)'; }}
            onMouseLeave={e => { e.currentTarget.style.background = 'rgba(255,0,60,0.08)'; e.currentTarget.style.boxShadow = '0 4px 15px rgba(255,0,60,0.1)'; e.currentTarget.style.transform = 'translateY(0)'; }}
          >
            REMOVE
          </button>
        </div>
      </div>
      
      {/* Settings */}
      <SettingsPanel config={config} onChange={onConfigChange} />
      
      {/* Session Log */}
      <SessionLog logLines={logLines} />
    </div>
  );
}
