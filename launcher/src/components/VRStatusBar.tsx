export function VRStatusBar({ status, selectedGame, injectState, onInject }: any) {
  const getButtonContent = () => {
    if (injectState === 'injecting') return '⊗ ABORT SEQUENCE';
    if (injectState === 'success') return '✓ SYSTEM ACTIVE';
    if (injectState === 'running') return '■ CLOSE GAME';
    if (injectState === 'error') return '✗ INJECTION FAILED';
    if (injectState === 'cancelled') return '⊗ SEQUENCE ABORTED';
    return '▶ INITIALIZE INJECTION';
  };

  const getButtonColor = () => {
    if (injectState === 'success') return 'var(--ag-accent-success)';
    if (injectState === 'running') return 'var(--ag-accent-danger)';
    if (injectState === 'error') return 'var(--ag-accent-danger)';
    if (injectState === 'cancelled') return 'var(--ag-accent-warn)';
    return 'var(--ag-accent)';
  };

  const isReady = selectedGame && injectState === 'default';
  const isActive = injectState === 'injecting' || injectState === 'running';

  return (
    <div className="glass-panel" style={{ 
      borderTop: '1px solid var(--ag-border)', 
      display: 'flex', flexDirection: 'column',
      zIndex: 10, borderBottom: 'none', borderLeft: 'none', borderRight: 'none', 
      boxShadow: '0 -10px 40px rgba(0,0,0,0.5), inset 0 1px 1px rgba(255,255,255,0.06)' 
    }}>
      {/* Progress bar during injection */}
      {injectState === 'injecting' && (
        <div className="inject-progress" style={{ flexShrink: 0 }}>
          <div className="bar" />
        </div>
      )}
      
      <div style={{ height: 72, display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '0 40px' }}>
        {/* VR Status */}
        <div style={{ display: 'flex', alignItems: 'center' }}>
          <div className={`status-dot ${status.connected ? 'connected' : 'disconnected'}`} style={{ marginRight: 15 }} />
          <div style={{ display: 'flex', flexDirection: 'column' }}>
            <span style={{ 
              fontSize: 14, fontWeight: 'bold', letterSpacing: '1px', 
              textShadow: status.connected ? '0 0 10px rgba(255,255,255,0.3)' : 'none' 
            }}>
              {status.headset}
            </span>
            <span style={{ 
              fontSize: 11, color: 'var(--ag-text-muted)', fontFamily: 'var(--ag-font-mono)', 
              marginTop: 2, letterSpacing: '0.5px' 
            }}>
              {status.refreshRate}Hz
            </span>
          </div>
        </div>
        
        {/* Action Buttons */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
          <button 
            onClick={() => { if(selectedGame) window.ag.utils.openConfig(selectedGame.id); }} 
            className="btn-glow"
            style={{ 
              background: 'transparent', border: '1px solid rgba(255,255,255,0.06)', 
              color: 'var(--ag-text-muted)', cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', 
              fontSize: 11, letterSpacing: '1px', padding: '8px 14px', borderRadius: 'var(--ag-radius-sm)',
              transition: 'all 0.3s',
              opacity: selectedGame ? 1 : 0.3
            }}
            onMouseEnter={e => { if(selectedGame) { e.currentTarget.style.borderColor = 'rgba(0,240,255,0.3)'; e.currentTarget.style.color = 'var(--ag-accent)'; }}}
            onMouseLeave={e => { e.currentTarget.style.borderColor = 'rgba(255,255,255,0.06)'; e.currentTarget.style.color = 'var(--ag-text-muted)'; }}
          >
            CONFIG
          </button>
          <button 
            onClick={() => { if(selectedGame) window.ag.utils.openLog(selectedGame.id); }} 
            className="btn-glow"
            style={{ 
              background: 'transparent', border: '1px solid rgba(255,255,255,0.06)', 
              color: 'var(--ag-text-muted)', cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', 
              fontSize: 11, letterSpacing: '1px', padding: '8px 14px', borderRadius: 'var(--ag-radius-sm)',
              transition: 'all 0.3s',
              opacity: selectedGame ? 1 : 0.3
            }}
            onMouseEnter={e => { if(selectedGame) { e.currentTarget.style.borderColor = 'rgba(0,240,255,0.3)'; e.currentTarget.style.color = 'var(--ag-accent)'; }}}
            onMouseLeave={e => { e.currentTarget.style.borderColor = 'rgba(255,255,255,0.06)'; e.currentTarget.style.color = 'var(--ag-text-muted)'; }}
          >
            LOGS
          </button>
          
          <button 
            onClick={onInject}
            disabled={!selectedGame || injectState === 'success' || injectState === 'error' || injectState === 'cancelled'}
            className={isReady ? 'pulse-btn btn-glow' : 'btn-glow'}
            style={{ 
              background: isActive ? 'rgba(255,0,60,0.15)' : injectState === 'success' ? 'rgba(0,255,136,0.15)' : 'linear-gradient(135deg, rgba(0,240,255,0.15), rgba(0,240,255,0.05))', 
              border: `1px solid ${getButtonColor()}`, 
              color: getButtonColor(),
              padding: '16px 36px',
              borderRadius: 'var(--ag-radius-sm)',
              cursor: (!selectedGame || injectState === 'success' || injectState === 'error' || injectState === 'cancelled') ? 'not-allowed' : 'pointer',
              opacity: (!selectedGame && injectState === 'default') ? 0.3 : 1,
              fontFamily: 'var(--ag-font-display)',
              fontWeight: 800,
              fontSize: 16,
              letterSpacing: '2px',
              minWidth: 260,
              textShadow: `0 0 15px ${getButtonColor()}90`,
              boxShadow: injectState !== 'default' ? `0 0 25px ${getButtonColor()}60, inset 0 2px 20px ${getButtonColor()}40` : `0 8px 25px rgba(0,0,0,0.4), 0 0 15px rgba(0,240,255,0.15), inset 0 1px 2px rgba(255,255,255,0.2)`,
              transition: 'all 0.4s var(--ag-transition)',
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
