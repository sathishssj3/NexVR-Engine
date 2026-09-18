export function VRStatusBar({ status, selectedGame, injectState, onInject, onUninstallMod }: any) {
  const isAntiCheat = selectedGame?.hasAntiCheat;

  const getButtonContent = () => {
    if (isAntiCheat) return 'ANTI-CHEAT BLOCKED';
    if (injectState === 'injecting') return 'ABORT SEQUENCE';
    if (injectState === 'success') return 'SYSTEM ACTIVE';
    if (injectState === 'running') return 'CLOSE GAME';
    if (injectState === 'error') return 'INJECTION FAILED';
    if (injectState === 'cancelled') return 'SEQUENCE ABORTED';
    return 'INITIALIZE INJECTION';
  };

  const getButtonColor = () => {
    if (isAntiCheat) return 'var(--ag-accent-danger)';
    if (injectState === 'success') return 'var(--ag-accent-success)';
    if (injectState === 'running') return 'var(--ag-accent-danger)';
    if (injectState === 'error') return 'var(--ag-accent-danger)';
    if (injectState === 'cancelled') return 'var(--ag-accent-warn)';
    return 'var(--ag-accent)';
  };

  const isActive = injectState === 'injecting' || injectState === 'running';

  return (
    <div className="glass-panel" style={{ 
      borderTop: '1px solid var(--ag-border)', 
      display: 'flex', flexDirection: 'column',
      zIndex: 20, 
      borderBottom: 'none', borderLeft: 'none', borderRight: 'none', 
      background: 'rgba(9, 9, 13, 0.98)',
      boxShadow: '0 -10px 40px rgba(0, 0, 0, 0.6), inset 0 1px 1px rgba(255, 255, 255, 0.05)' 
    }}>
      {/* Progress bar during injection */}
      {injectState === 'injecting' && (
        <div className="inject-progress" style={{ flexShrink: 0 }}>
          <div className="bar" />
        </div>
      )}
      
      <div style={{ height: 68, display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '0 28px' }}>
        {/* Left: VR Status */}
        <div style={{ display: 'flex', alignItems: 'center' }}>
          <div className={`status-dot ${status.connected ? 'connected' : 'disconnected'}`} style={{ marginRight: 14 }} />
          <div style={{ display: 'flex', flexDirection: 'column' }}>
            <span style={{ 
              fontSize: 14, fontWeight: 700, letterSpacing: '0.04em', 
              fontFamily: 'var(--ag-font-display)',
              color: '#FFF'
            }}>
              {status.headset}
            </span>
            <span style={{ 
              fontSize: 11, color: status.connected ? 'var(--ag-accent-success)' : 'var(--ag-text-muted)', 
              fontFamily: 'var(--ag-font-mono)', 
              marginTop: 2, letterSpacing: '0.5px' 
            }}>
              {status.refreshRate} Hz // {status.runtime}
            </span>
          </div>

          {selectedGame && (
            <div style={{ marginLeft: 24, paddingLeft: 20, borderLeft: '1px solid var(--ag-border)', display: 'flex', flexDirection: 'column' }}>
              <span style={{ fontSize: 9.5, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-text-dim)', letterSpacing: '1px' }}>
                TARGET GAME
              </span>
              <span style={{ fontSize: 13, fontWeight: 600, color: 'var(--ag-text-primary)', fontFamily: 'var(--ag-font-display)' }}>
                {selectedGame.name}
              </span>
            </div>
          )}
        </div>

        {/* Center: In-Game VR Hotkey Shortcut Hint */}
        <div style={{ 
          display: 'flex', 
          alignItems: 'center', 
          gap: 7,
          fontFamily: 'var(--ag-font-display)',
          fontSize: 11,
          fontWeight: 600,
          letterSpacing: '0.08em',
          color: 'var(--ag-text-muted)'
        }}>
          <span style={{ color: 'var(--ag-text-dim)' }}>VR MENU:</span>
          <span style={{ 
            background: 'rgba(255, 255, 255, 0.05)', 
            border: '1px solid var(--ag-border)', 
            padding: '2px 8px', 
            borderRadius: 3, 
            color: 'var(--ag-text-primary)',
            fontWeight: 700
          }}>
            L3 + R3
          </span>
          <span style={{ fontSize: 10, opacity: 0.7 }}>OR</span>
          <span style={{ 
            background: 'rgba(255, 255, 255, 0.05)', 
            border: '1px solid var(--ag-border)', 
            padding: '2px 8px', 
            borderRadius: 3, 
            color: 'var(--ag-text-primary)',
            fontWeight: 700
          }}>
            STICK CLICK
          </span>
        </div>
        
        {/* Right: Action Buttons */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
          <button 
            onClick={() => { if (selectedGame) window.ag.utils.openConfig(selectedGame.id); }} 
            className="btn-glow"
            disabled={!selectedGame}
            style={{ 
              background: 'transparent', 
              border: '1px solid var(--ag-border)', 
              color: 'var(--ag-text-muted)', 
              cursor: selectedGame ? 'pointer' : 'not-allowed', 
              fontFamily: 'var(--ag-font-display)', 
              fontSize: 11, 
              letterSpacing: '0.08em', 
              fontWeight: 600,
              padding: '9px 16px', 
              borderRadius: 'var(--ag-radius-sm)',
              transition: 'all 0.25s var(--ag-transition)',
              opacity: selectedGame ? 1 : 0.35
            }}
            onMouseEnter={e => { if (selectedGame) { e.currentTarget.style.borderColor = 'rgba(204, 0, 0, 0.4)'; e.currentTarget.style.color = '#FFF'; }}}
            onMouseLeave={e => { e.currentTarget.style.borderColor = 'var(--ag-border)'; e.currentTarget.style.color = 'var(--ag-text-muted)'; }}
          >
            CONFIG
          </button>
          
          <button 
            onClick={() => { window.ag.utils.openLog(selectedGame?.id); }} 
            className="btn-glow"
            title="Open active engine & VR log"
            style={{ 
              background: 'transparent', 
              border: '1px solid var(--ag-border)', 
              color: 'var(--ag-text-muted)', 
              cursor: 'pointer', 
              fontFamily: 'var(--ag-font-display)', 
              fontSize: 11, 
              letterSpacing: '0.08em', 
              fontWeight: 600, 
              padding: '9px 16px', 
              borderRadius: 'var(--ag-radius-sm)',
              transition: 'all 0.25s var(--ag-transition)'
            }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = 'rgba(204, 0, 0, 0.4)'; e.currentTarget.style.color = '#FFF'; }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = 'var(--ag-border)'; e.currentTarget.style.color = 'var(--ag-text-muted)'; }}
          >
            LOGS
          </button>
          
          {selectedGame?.hasInjector && !isAntiCheat && onUninstallMod && (
            <button 
              onClick={onUninstallMod} 
              className="btn-glow"
              title="Remove VR Mod files and restore game to original flat screen mode"
              style={{ 
                background: 'rgba(255, 159, 10, 0.08)', 
                border: '1px solid rgba(255, 159, 10, 0.35)', 
                color: 'var(--ag-accent-warn)', 
                cursor: 'pointer', 
                fontFamily: 'var(--ag-font-display)', 
                fontSize: 11, 
                letterSpacing: '0.08em', 
                fontWeight: 600, 
                padding: '9px 16px', 
                borderRadius: 'var(--ag-radius-sm)',
                transition: 'all 0.25s'
              }}
              onMouseEnter={e => { e.currentTarget.style.borderColor = 'rgba(255, 159, 10, 0.6)'; e.currentTarget.style.color = '#FFF'; }}
              onMouseLeave={e => { e.currentTarget.style.borderColor = 'rgba(255, 159, 10, 0.35)'; e.currentTarget.style.color = 'var(--ag-accent-warn)'; }}
            >
              RESTORE FLAT
            </button>
          )}
          
          {/* Big Glowing Primary Button */}
          <button 
            onClick={onInject}
            disabled={!selectedGame || isAntiCheat || injectState === 'success' || injectState === 'error' || injectState === 'cancelled'}
            title={isAntiCheat ? 'Multiplayer Anti-Cheat detected. Injection disabled to prevent bans.' : undefined}
            className="btn-glow"
            style={{ 
              background: isActive 
                ? 'rgba(204, 0, 0, 0.85)' 
                : isAntiCheat 
                  ? 'rgba(204, 0, 0, 0.12)' 
                  : injectState === 'success' 
                    ? 'rgba(48, 209, 88, 0.2)' 
                    : 'linear-gradient(135deg, rgba(204, 0, 0, 0.32), rgba(204, 0, 0, 0.12))', 
              border: `1px solid ${getButtonColor()}`, 
              color: isAntiCheat ? 'var(--ag-accent-danger)' : '#FFF',
              padding: '14px 32px',
              borderRadius: 'var(--ag-radius-sm)',
              cursor: (!selectedGame || isAntiCheat || injectState === 'success' || injectState === 'error' || injectState === 'cancelled') ? 'not-allowed' : 'pointer',
              opacity: (!selectedGame && injectState === 'default') ? 0.35 : 1,
              fontFamily: 'var(--ag-font-display)',
              fontWeight: 800,
              fontSize: 14,
              letterSpacing: '0.12em',
              minWidth: 250,
              boxShadow: 'none',
              transition: 'all 0.3s var(--ag-transition)',
              transform: isActive ? 'scale(0.98)' : 'scale(1)'
            }}
          >
            {getButtonContent()}
          </button>
        </div>
      </div>
    </div>
  );
}
