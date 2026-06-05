// removed type imports
import { SettingsPanel } from './SettingsPanel';
import { SessionLog } from './SessionLog';

export function GameDetail({ game, config, onConfigChange, logLines, onRemoveGame }: any) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', minHeight: '100%' }}>
      {/* Hero */}
      <div style={{ flexShrink: 0, position: 'relative', marginBottom: 35, padding: '30px', background: 'linear-gradient(135deg, rgba(20, 30, 45, 0.7) 0%, rgba(10, 15, 25, 0.4) 100%)', borderRadius: 12, border: '1px solid rgba(255,255,255,0.05)', borderLeft: '4px solid var(--ag-accent)', boxShadow: '0 15px 40px rgba(0,0,0,0.3), inset 0 1px 2px rgba(255,255,255,0.1)', overflow: 'hidden' }}>
        <div style={{ position: 'absolute', top: 0, right: 0, width: '50%', height: '100%', background: 'radial-gradient(ellipse at 100% 0%, rgba(0,240,255,0.1), transparent 70%)', pointerEvents: 'none' }} />
        <h1 style={{ position: 'relative', margin: '0 0 10px 0', fontSize: 42, fontWeight: 700, letterSpacing: '-0.5px', textShadow: '0 4px 15px rgba(0,0,0,0.5)' }}>{game.name}</h1>
        <div style={{ position: 'relative', fontFamily: 'var(--ag-font-mono)', fontSize: 13, color: 'var(--ag-text-muted)', marginBottom: 25, display: 'flex', alignItems: 'center' }}>
          <span style={{ color: 'rgba(0,240,255,0.7)', marginRight: 8, fontWeight: 'bold' }}>PATH //</span> {game.installPath}
        </div>
        <div style={{ position: 'relative', display: 'flex', gap: 12, alignItems: 'center', flexWrap: 'wrap' }}>
          <span style={{ padding: '6px 14px', borderRadius: 20, background: 'rgba(0,0,0,0.5)', border: '1px solid rgba(255,255,255,0.08)', fontSize: 12, fontFamily: 'var(--ag-font-mono)', letterSpacing: '1px', display: 'flex', alignItems: 'center', gap: 6, boxShadow: 'inset 0 1px 3px rgba(0,0,0,0.5)' }}>
            API <strong style={{ color: game.api === 'DX12' ? 'var(--ag-accent-success)' : game.api === 'Vulkan' ? 'var(--ag-accent-danger)' : 'var(--ag-accent)', textShadow: `0 0 8px ${game.api === 'DX12' ? 'var(--ag-accent-success)' : game.api === 'Vulkan' ? 'var(--ag-accent-danger)' : 'var(--ag-accent)'}80` }}>{game.api}</strong>
          </span>
          <span style={{ padding: '6px 14px', borderRadius: 20, background: 'rgba(0,0,0,0.5)', border: '1px solid rgba(255,255,255,0.08)', fontSize: 12, fontFamily: 'var(--ag-font-mono)', letterSpacing: '1px', color: 'var(--ag-text-primary)', boxShadow: 'inset 0 1px 3px rgba(0,0,0,0.5)' }}>
            COMPAT <strong style={{ color: '#fff' }}>{game.compat}</strong>
          </span>
          <button onClick={onRemoveGame} className="btn-glow" style={{ marginLeft: 'auto', padding: '8px 20px', borderRadius: 6, background: 'rgba(255,0,60,0.05)', border: '1px solid rgba(255,0,60,0.5)', color: 'var(--ag-accent-danger)', fontSize: 12, cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', fontWeight: 'bold', letterSpacing: '1px', textShadow: '0 0 8px rgba(255,0,60,0.4)', transition: 'all 0.3s' }}>
            REMOVE
          </button>
        </div>
      </div>
      
      {/* Settings Grid */}
      <SettingsPanel config={config} onChange={onConfigChange} />
      
      {/* Session Log */}
      <SessionLog logLines={logLines} />
    </div>
  );
}
