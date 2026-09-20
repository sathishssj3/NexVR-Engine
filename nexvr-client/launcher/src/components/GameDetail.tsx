import { SettingsPanel } from './SettingsPanel';
import { SessionLog } from './SessionLog';

export function GameDetail({ game, config, onConfigChange, logLines, onRemoveGame, onUninstallMod }: any) {
  const apiColors: Record<string, string> = {
    DX11: 'var(--ag-accent)',
    DX12: 'var(--ag-accent-success)',
    Vulkan: '#FF6B6B',
    Unknown: 'var(--ag-text-muted)'
  };

  const apiColor = apiColors[game.api] || apiColors.Unknown;

  return (
    <div className="settings-tab-enter" style={{ display: 'flex', flexDirection: 'column', minHeight: '100%', paddingBottom: 24 }}>
      {/* Hero Header with Red Full Border, Cool High-Tech Background & 3D Title */}
      <div 
        className="settings-card settings-item-enter" 
        style={{ 
          flexShrink: 0, 
          position: 'relative', 
          marginBottom: 26, 
          padding: '36px 40px', 
          border: '1px solid #0e0e0eff',
          borderRadius: 10,
          background: 'linear-gradient(90deg, rgba(230, 20, 20, 0.45) 0%, rgba(190, 15, 15, 0.2) 42%, rgba(80, 5, 5, 0.06) 75%, transparent 100%), radial-gradient(ellipse at 85% 0%, rgba(255, 40, 40, 0.45) 0%, transparent 65%), linear-gradient(180deg, #1c0608 0%, #0c0406 45%, #07070A 100%)',
          boxShadow: 'none',
          overflow: 'hidden',
          animation: 'settingsOpenAnim 0.35s cubic-bezier(0.16, 1, 0.3, 1) both'
        }}
      >
        {/* Title with Left Accent Pill Line (Solid White, Lower Thickness, Larger Size) */}
        <div style={{ position: 'relative', marginBottom: 24, display: 'flex', alignItems: 'center', gap: 16 }}>
          <span style={{ 
            width: 5, 
            height: 60, 
            borderRadius: 3, 
            background: 'var(--ag-accent)', 
            display: 'inline-block',
            flexShrink: 0 
          }} />
          <h1 style={{ 
            margin: 0, 
            fontSize: 66, 
            fontWeight: 600, 
            letterSpacing: '0.04em', 
            lineHeight: 1.05, 
            fontFamily: 'var(--ag-font-display)',
            color: '#FFFFFF',
            textTransform: 'uppercase',
            textShadow: 'none'
          }}>
            {game.name.toUpperCase()}
          </h1>
        </div>
        
        {/* Tags Row: API + Solid Smoke Gray MOD ACTIVE + Action Buttons */}
        <div style={{ position: 'relative', display: 'flex', gap: 10, alignItems: 'center', flexWrap: 'wrap' }}>
          {/* API Badge */}
          <span style={{ 
            padding: '5px 14px', 
            borderRadius: 4, 
            background: '#121319', 
            border: '1px solid #2C2D35', 
            display: 'inline-flex', 
            alignItems: 'center', 
            gap: 7 
          }}>
            <span style={{ color: '#848884', fontSize: 10.5, fontFamily: 'var(--ag-font-display)', fontWeight: 700 }}>API</span>
            <strong style={{ color: apiColor, fontSize: 12, fontFamily: 'var(--ag-font-mono)', fontWeight: 800 }}>{game.api}</strong>
          </span>

          {/* Solid Smoke Gray MOD ACTIVE Badge */}
          {game.hasInjector && (
            <span style={{ 
              padding: '5px 14px', 
              borderRadius: 4, 
              background: '#181920', 
              border: '1px solid #383A44', 
              display: 'inline-flex', 
              alignItems: 'center' 
            }}>
              <strong style={{ color: '#D5D7E2', fontSize: 11.5, fontFamily: 'var(--ag-font-mono)', fontWeight: 800, letterSpacing: '0.05em' }}>
                MOD ACTIVE
              </strong>
            </span>
          )}

          {game.sizeGB > 0 && (
            <span style={{ 
              padding: '5px 14px', 
              borderRadius: 4, 
              background: '#121319', 
              border: '1px solid #2C2D35', 
              display: 'inline-flex', 
              alignItems: 'center', 
              gap: 6 
            }}>
              <span style={{ color: '#848884', fontSize: 10.5, fontFamily: 'var(--ag-font-display)', fontWeight: 700 }}>SIZE</span>
              <span style={{ fontSize: 12, color: '#FFF', fontFamily: 'var(--ag-font-mono)', fontWeight: 700 }}>{game.sizeGB.toFixed(1)} GB</span>
            </span>
          )}
          
          {/* Action Buttons Right-Aligned */}
          <div style={{ marginLeft: 'auto', display: 'flex', gap: 10, alignItems: 'center' }}>
            {game.hasInjector && !game.hasAntiCheat && onUninstallMod && (
              <button
                onClick={onUninstallMod}
                className="btn-outline-laser"
                style={{
                  padding: '8px 18px',
                  fontSize: 10.5,
                  color: '#C0C0C8',
                  borderColor: '#383A44',
                  background: '#121319'
                }}
              >
                RESTORE FLAT SCREEN
              </button>
            )}

            <button 
              onClick={onRemoveGame} 
              className="btn-outline-laser" 
              style={{ 
                padding: '8px 18px', 
                color: 'var(--ag-accent)', 
                borderColor: '#3d3131ff', 
                fontSize: 10.5, 
                letterSpacing: '0.08em', 
                fontWeight: 700,
                background: '#121319'
              }}
            >
              REMOVE FROM LIBRARY
            </button>
          </div>
        </div>

        {/* Anti-Cheat Safety Guard Banner */}
        {game.hasAntiCheat && (
          <div style={{
            marginTop: 18, 
            padding: '14px 18px', 
            borderRadius: 6,
            background: '#120808', 
            border: '1px solid #4A2020',
            borderLeft: '3px solid var(--ag-accent)',
            display: 'flex', 
            alignItems: 'center', 
            gap: 14
          }}>
            <div>
              <div style={{ color: 'var(--ag-accent)', fontWeight: 700, fontSize: 12.5, fontFamily: 'var(--ag-font-display)', letterSpacing: '0.05em' }}>
                ANTI-CHEAT ACTIVE // {game.antiCheatName || 'Multiplayer Guard'}
              </div>
              <div style={{ color: '#848884', fontSize: 11.5, marginTop: 3, fontFamily: 'var(--ag-font-ui)' }}>
                Injection is locked to preserve online account safety and prevent multiplayer bans.
              </div>
            </div>
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
