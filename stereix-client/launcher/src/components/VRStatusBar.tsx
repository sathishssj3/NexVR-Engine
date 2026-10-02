export function VRStatusBar({ status, selectedGame, injectState, onInject, onUninstallMod }: any) {
  const isAntiCheat = selectedGame?.hasAntiCheat;

  const getButtonContent = () => {
    if (!selectedGame) return 'SELECT TARGET GAME';
    if (isAntiCheat) return 'ANTI-CHEAT BLOCKED';
    if (injectState === 'injecting') return 'ABORT SEQUENCE';
    if (injectState === 'success') return 'SYSTEM ACTIVE';
    if (injectState === 'running') return 'CLOSE GAME';
    if (injectState === 'error') return 'INJECTION FAILED';
    if (injectState === 'cancelled') return 'SEQUENCE ABORTED';
    if (!status.connected) return 'HEADSET DISCONNECTED';
    return 'INITIALIZE INJECTION';
  };

  const isActive = injectState === 'injecting' || injectState === 'running';
  const isDisconnected = selectedGame && !status.connected && injectState === 'default' && !isAntiCheat;

  return (
    <div className="glass-panel" style={{ 
      borderTop: '1px solid rgba(255, 255, 255, 0.08)', 
      display: 'flex', flexDirection: 'column',
      zIndex: 20, 
      borderBottom: 'none', borderLeft: 'none', borderRight: 'none', 
      background: 'rgba(8, 8, 12, 0.94)',
      backdropFilter: 'blur(20px) saturate(150%)',
      WebkitBackdropFilter: 'blur(20px) saturate(150%)',
      boxShadow: '0 -10px 40px rgba(0, 0, 0, 0.6), inset 0 1px 0 rgba(255, 255, 255, 0.06)' 
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
          {/* Vertical status bar pill (no glow) */}
          <div 
            className={`status-bar-pill ${status.connected ? 'connected' : 'disconnected'}`} 
            style={{ 
              width: 4, 
              height: 28, 
              borderRadius: 2, 
              background: status.connected ? 'var(--ag-accent)' : '#5A5D6B', 
              marginRight: 14, 
              flexShrink: 0,
              boxShadow: 'none'
            }} 
          />
          <div style={{ display: 'flex', flexDirection: 'column' }}>
            <span style={{ 
              fontSize: 13.5, 
              fontWeight: 800, 
              letterSpacing: '0.04em', 
              fontFamily: 'var(--ag-font-display)',
              color: status.connected ? 'var(--ag-accent)' : '#848884'
            }}>
              {status.connected ? 'CONNECTED' : 'NOT CONNECTED'}
            </span>
            <span style={{ 
              fontSize: 11, 
              color: status.connected ? '#FFFFFF' : '#5A5D6B', 
              fontFamily: 'var(--ag-font-mono)', 
              marginTop: 2, 
              letterSpacing: '0.5px' 
            }}>
              {status.connected ? (status.headset || 'VR HEADSET DETECTED') : 'NO VR HEADSET DETECTED'}
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
            onMouseEnter={e => { if (selectedGame) { e.currentTarget.style.borderColor = '#4A4D5C'; e.currentTarget.style.color = '#FFF'; }}}
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
            onMouseEnter={e => { e.currentTarget.style.borderColor = '#4A4D5C'; e.currentTarget.style.color = '#FFF'; }}
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
                background: 'rgba(255, 255, 255, 0.03)', 
                border: '1px solid #383A44', 
                color: '#C0C0C8', 
                cursor: 'pointer', 
                fontFamily: 'var(--ag-font-display)', 
                fontSize: 11, 
                letterSpacing: '0.08em', 
                fontWeight: 600, 
                padding: '9px 16px', 
                borderRadius: 'var(--ag-radius-sm)',
                transition: 'all 0.25s'
              }}
              onMouseEnter={e => { e.currentTarget.style.borderColor = '#5A5D6C'; e.currentTarget.style.color = '#FFF'; }}
              onMouseLeave={e => { e.currentTarget.style.borderColor = '#383A44'; e.currentTarget.style.color = '#C0C0C8'; }}
            >
              RESTORE FLAT
            </button>
          )}
          
          {/* Primary Injection / Readiness Action Button */}
          <button 
            onClick={onInject}
            disabled={!selectedGame || isAntiCheat || injectState === 'success' || injectState === 'error' || injectState === 'cancelled'}
            title={
              isAntiCheat 
                ? 'Multiplayer Anti-Cheat detected. Injection disabled to prevent bans.' 
                : isDisconnected
                  ? 'No OpenXR / SteamVR headset detected. Click to check connection and launch options.'
                  : !selectedGame
                    ? 'Select a game from the library to configure VR injection'
                    : undefined
            }
            className="btn-glow"
            style={{ 
              background: isActive 
                ? '#B30000' 
                : isAntiCheat 
                  ? 'rgba(204, 0, 0, 0.12)' 
                  : injectState === 'success' 
                    ? 'rgba(48, 209, 88, 0.25)' 
                    : !selectedGame 
                      ? '#1A1B22'
                      : isDisconnected
                        ? 'rgba(255, 159, 10, 0.12)'
                        : 'linear-gradient(135deg, #FF1A1A 0%, #CC0000 55%, #990000 100%)', 
              border: isAntiCheat 
                ? '1px solid #4A2020' 
                : injectState === 'success' 
                  ? '1px solid var(--ag-accent-success)' 
                  : !selectedGame 
                    ? '1px solid #2C2D35' 
                    : isDisconnected
                      ? '1px solid #FF9F0A'
                      : '1px solid #FF4D4D', 
              color: isAntiCheat 
                ? 'var(--ag-accent-danger)' 
                : !selectedGame 
                  ? '#FFFFFF' 
                  : isDisconnected
                    ? '#FFB340'
                    : '#FFF',
              padding: '14px 32px',
              borderRadius: 'var(--ag-radius-sm)',
              cursor: (!selectedGame || isAntiCheat || injectState === 'success' || injectState === 'error' || injectState === 'cancelled') ? 'not-allowed' : 'pointer',
              opacity: 1,
              fontFamily: 'var(--ag-font-display)',
              fontWeight: 900,
              fontSize: 14,
              letterSpacing: '0.12em',
              minWidth: 250,
              boxShadow: (selectedGame && !isAntiCheat && injectState === 'default' && !isDisconnected) 
                ? '0 0 24px rgba(204, 0, 0, 0.5), inset 0 1px 0 rgba(255, 255, 255, 0.35)' 
                : isDisconnected
                  ? '0 0 16px rgba(255, 159, 10, 0.25), inset 0 1px 0 rgba(255, 255, 255, 0.15)'
                  : 'none',
              textShadow: (selectedGame && !isAntiCheat && injectState === 'default' && !isDisconnected) 
                ? '0 1px 3px rgba(0, 0, 0, 0.7)' 
                : 'none',
              transition: 'all 0.2s cubic-bezier(0.16, 1, 0.3, 1)',
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
