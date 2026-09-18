import { SettingsPanel } from './SettingsPanel';
import { SessionLog } from './SessionLog';

export function GameDetail({ game, config, onConfigChange, logLines, onRemoveGame, onUninstallMod }: any) {
  const apiColors: Record<string, string> = {
    DX11: 'var(--ag-accent)',
    DX12: 'var(--ag-accent-success)',
    Vulkan: '#FF6B6B',
    Unknown: 'var(--ag-text-muted)'
  };

  const compatLabels: Record<string, { color: string; label: string }> = {
    verified: { color: 'var(--ag-accent-success)', label: 'VERIFIED' },
    beta:     { color: 'var(--ag-accent-warn)', label: 'BETA' },
    new:      { color: 'var(--ag-accent)', label: 'NEW' },
    unknown:  { color: 'var(--ag-text-muted)', label: 'UNKNOWN' }
  };

  const apiColor = apiColors[game.api] || apiColors.Unknown;
  const compat = compatLabels[game.compat] || compatLabels.unknown;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', minHeight: '100%' }}>
      {/* Hero Header */}
      <div className="fade-in-up" style={{ 
        flexShrink: 0, position: 'relative', marginBottom: 26, padding: '28px 36px', 
        background: 'linear-gradient(135deg, rgba(16, 16, 22, 0.9) 0%, rgba(10, 10, 14, 0.8) 100%)', 
        borderRadius: 'var(--ag-radius-lg)', 
        border: '1px solid var(--ag-border)', 
        borderLeft: `4px solid ${apiColor}`,
        boxShadow: '0 20px 50px rgba(0, 0, 0, 0.5), inset 0 1px 2px rgba(255, 255, 255, 0.08)', 
        overflow: 'hidden' 
      }}>
        {/* Title Header */}
        <div style={{ position: 'relative', marginBottom: 12 }}>
          <h1 style={{ 
            margin: '0 0 8px 0', fontSize: 34, fontWeight: 800, 
            letterSpacing: '0.02em', 
            lineHeight: 1.15, fontFamily: 'var(--ag-font-display)',
            color: '#FFF'
          }}>
            {game.name}
          </h1>
          
          {/* Path */}
          <div style={{ 
            fontFamily: 'var(--ag-font-mono)', fontSize: 11.5, 
            color: 'var(--ag-text-muted)', display: 'flex', alignItems: 'center',
            background: 'rgba(5, 5, 8, 0.65)', padding: '5px 12px', borderRadius: 4, width: 'fit-content',
            border: '1px solid var(--ag-border)'
          }}>
            <span style={{ color: `${apiColor}`, marginRight: 10, fontWeight: 700 }}>PATH //</span> 
            <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', maxWidth: 620 }}>{game.installPath}</span>
          </div>
        </div>
        
        {/* Tags and Actions */}
        <div style={{ position: 'relative', display: 'flex', gap: 12, alignItems: 'center', flexWrap: 'wrap' }}>
          <span className="api-badge" style={{ background: 'rgba(255, 255, 255, 0.04)', border: '1px solid var(--ag-border)' }}>
            <span style={{ opacity: 0.7, fontSize: 10 }}>API</span>
            <strong style={{ color: apiColor, fontSize: 12 }}>{game.api}</strong>
          </span>
          
          <span className="api-badge" style={{ background: 'rgba(255, 255, 255, 0.04)', border: '1px solid var(--ag-border)' }}>
            <strong style={{ color: compat.color, fontSize: 12 }}>{compat.label}</strong>
          </span>

          {game.sizeGB > 0 && (
            <span className="api-badge" style={{ opacity: 0.85, background: 'rgba(255, 255, 255, 0.04)', border: '1px solid var(--ag-border)' }}>
              <span style={{ opacity: 0.7, fontSize: 10 }}>SIZE</span>
              <span style={{ fontSize: 12 }}>{game.sizeGB.toFixed(1)} GB</span>
            </span>
          )}
          
          <button 
            onClick={onRemoveGame} 
            className="btn-outline-laser" 
            style={{ 
              marginLeft: 'auto', padding: '8px 20px', 
              color: 'var(--ag-accent-danger)', 
              borderColor: 'rgba(204, 0, 0, 0.4)', 
              fontSize: 11, letterSpacing: '0.08em', fontWeight: 700 
            }}
          >
            REMOVE
          </button>
        </div>

        {/* Anti-Cheat Safety Guard Banner */}
        {game.hasAntiCheat && (
          <div style={{
            marginTop: 18, padding: '14px 18px', borderRadius: 'var(--ag-radius-sm)',
            background: 'rgba(204, 0, 0, 0.1)', border: '1px solid rgba(204, 0, 0, 0.38)',
            display: 'flex', alignItems: 'center', gap: 14
          }}>
            <div>
              <div style={{ color: 'var(--ag-accent)', fontWeight: 700, fontSize: 13, fontFamily: 'var(--ag-font-display)', letterSpacing: '0.04em' }}>
                ANTI-CHEAT DETECTED ({game.antiCheatName || 'Multiplayer Guard'})
              </div>
              <div style={{ color: 'var(--ag-text-muted)', fontSize: 11.5, marginTop: 3, fontFamily: 'var(--ag-font-ui)' }}>
                Injection is locked to preserve online account safety and prevent multiplayer bans.
              </div>
            </div>
          </div>
        )}

        {/* Active VR Mod / Play Flat Banner */}
        {game.hasInjector && !game.hasAntiCheat && (
          <div style={{
            marginTop: 18, padding: '14px 18px', borderRadius: 'var(--ag-radius-sm)',
            background: 'rgba(48, 209, 88, 0.08)', border: '1px solid rgba(48, 209, 88, 0.3)',
            display: 'flex', alignItems: 'center', justifyContent: 'space-between'
          }}>
            <div>
              <div style={{ color: 'var(--ag-accent-success)', fontWeight: 700, fontSize: 12.5, fontFamily: 'var(--ag-font-mono)' }}>
                VR MOD ACTIVE IN GAME DIRECTORY
              </div>
              <div style={{ color: 'var(--ag-text-muted)', fontSize: 11, marginTop: 2 }}>
                NexVR injection binaries are deployed in this title executable folder.
              </div>
            </div>
            {onUninstallMod && (
              <button
                onClick={onUninstallMod}
                className="btn-outline-laser"
                style={{
                  padding: '7px 16px',
                  fontSize: 11,
                  color: '#FFF',
                  borderColor: 'rgba(255, 255, 255, 0.25)'
                }}
              >
                RESTORE FLAT SCREEN
              </button>
            )}
          </div>
        )}
      </div>
      
      {/* Settings */}
      <SettingsPanel config={config} onChange={onConfigChange} />
      
      {/* Session Log */}
      <SessionLog logLines={logLines} />
    </div>
  );
}
