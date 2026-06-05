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
        flexShrink: 0, position: 'relative', marginBottom: 30, padding: '28px 32px', 
        background: 'linear-gradient(135deg, rgba(20, 30, 45, 0.7) 0%, rgba(10, 15, 25, 0.4) 100%)', 
        borderRadius: 'var(--ag-radius-lg)', 
        border: '1px solid rgba(255,255,255,0.05)', 
        borderLeft: `4px solid ${apiColor}`,
        boxShadow: `0 15px 40px rgba(0,0,0,0.3), inset 0 1px 2px rgba(255,255,255,0.1), -2px 0 20px ${apiColor}15`, 
        overflow: 'hidden' 
      }}>
        {/* Background glow effect */}
        <div style={{ position: 'absolute', top: 0, right: 0, width: '60%', height: '100%', background: `radial-gradient(ellipse at 100% 0%, ${apiColor}10, transparent 70%)`, pointerEvents: 'none' }} />
        <div style={{ position: 'absolute', bottom: 0, left: 0, width: '40%', height: '60%', background: `radial-gradient(ellipse at 0% 100%, ${apiColor}08, transparent 60%)`, pointerEvents: 'none' }} />
        
        {/* Title */}
        <h1 style={{ 
          position: 'relative', margin: '0 0 8px 0', fontSize: 36, fontWeight: 700, 
          letterSpacing: '-0.5px', textShadow: '0 4px 15px rgba(0,0,0,0.5)',
          lineHeight: 1.1
        }}>
          {game.name}
        </h1>
        
        {/* Path */}
        <div style={{ 
          position: 'relative', fontFamily: 'var(--ag-font-mono)', fontSize: 12, 
          color: 'var(--ag-text-muted)', marginBottom: 20, display: 'flex', alignItems: 'center',
          opacity: 0.8
        }}>
          <span style={{ color: `${apiColor}99`, marginRight: 8, fontWeight: 'bold' }}>PATH //</span> 
          <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{game.installPath}</span>
        </div>
        
        {/* Tags and Actions */}
        <div style={{ position: 'relative', display: 'flex', gap: 10, alignItems: 'center', flexWrap: 'wrap' }}>
          <span className="api-badge">
            <span style={{ opacity: 0.6 }}>API</span>
            <strong style={{ color: apiColor, textShadow: `0 0 8px ${apiColor}80` }}>{game.api}</strong>
          </span>
          
          <span className="api-badge">
            <strong style={{ color: compat.color, textShadow: `0 0 8px ${compat.color}60` }}>{compat.label}</strong>
          </span>

          {game.sizeGB > 0 && (
            <span className="api-badge" style={{ opacity: 0.7 }}>
              <span style={{ opacity: 0.6 }}>SIZE</span>
              <span>{game.sizeGB.toFixed(1)} GB</span>
            </span>
          )}
          
          <button 
            onClick={onRemoveGame} 
            className="btn-glow" 
            style={{ 
              marginLeft: 'auto', padding: '7px 18px', borderRadius: 'var(--ag-radius-sm)', 
              background: 'rgba(255,0,60,0.05)', border: '1px solid rgba(255,0,60,0.3)', 
              color: 'var(--ag-accent-danger)', fontSize: 11, cursor: 'pointer', 
              fontFamily: 'var(--ag-font-mono)', fontWeight: 'bold', letterSpacing: '1px', 
              textShadow: '0 0 8px rgba(255,0,60,0.3)', transition: 'all 0.3s',
              opacity: 0.7
            }}
            onMouseEnter={e => { e.currentTarget.style.opacity = '1'; e.currentTarget.style.background = 'rgba(255,0,60,0.12)'; }}
            onMouseLeave={e => { e.currentTarget.style.opacity = '0.7'; e.currentTarget.style.background = 'rgba(255,0,60,0.05)'; }}
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
