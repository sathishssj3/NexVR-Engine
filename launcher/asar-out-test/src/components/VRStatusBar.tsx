export function VRStatusBar({ status, selectedGame, injectState, onInject }: any) {
  const getButtonContent = () => {
    if (injectState === 'injecting') return '⊗ ABORT SEQUENCE';
    if (injectState === 'success') return '✓ SYSTEM ACTIVE';
    if (injectState === 'error') return '✗ INJECTION FAILED';
    if (injectState === 'cancelled') return '⊗ SEQUENCE ABORTED';
    return '▶ INITIALIZE INJECTION';
  };

  const getButtonColor = () => {
    if (injectState === 'success') return 'var(--ag-accent-success)';
    if (injectState === 'error') return 'var(--ag-accent-danger)';
    if (injectState === 'cancelled') return '#ffaa00';
    return 'var(--ag-accent)';
  };

  const isReady = selectedGame && injectState === 'default';

  return (
    <div className="glass-panel" style={{ height: 76, borderTop: '1px solid var(--ag-border)', display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '0 40px', zIndex: 10, borderBottom: 'none', borderLeft: 'none', borderRight: 'none', boxShadow: '0 -10px 40px rgba(0,0,0,0.5), inset 0 1px 1px rgba(255,255,255,0.06)' }}>
      <div style={{ display: 'flex', alignItems: 'center' }}>
        <div style={{ 
          width: 12, height: 12, borderRadius: '50%', 
          background: status.connected ? 'var(--ag-accent-success)' : '#4a6072',
          boxShadow: status.connected ? '0 0 15px var(--ag-accent-success)' : 'none',
          marginRight: 15,
          border: '1px solid rgba(255,255,255,0.2)'
        }} />
        <div style={{ display: 'flex', flexDirection: 'column' }}>
          <span style={{ fontSize: 15, fontWeight: 'bold', letterSpacing: '1px', textShadow: status.connected ? '0 0 10px rgba(255,255,255,0.3)' : 'none' }}>{status.headset}</span>
          <span style={{ fontSize: 12, color: 'var(--ag-text-muted)', fontFamily: 'var(--ag-font-mono)', marginTop: 2 }}>RUNTIME: {status.runtime} // {status.refreshRate}Hz</span>
        </div>
      </div>
      
      <div style={{ display: 'flex', alignItems: 'center', gap: 20 }}>
        <button onClick={() => alert('This feature will open the game folder in V2!')} className="btn-glow" style={{ background: 'transparent', border: 'none', color: 'var(--ag-text-muted)', cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', fontSize: 12, letterSpacing: '1px' }}>[ CONFIG ]</button>
        <button onClick={() => alert('This feature will open the external log file in V2!')} className="btn-glow" style={{ background: 'transparent', border: 'none', color: 'var(--ag-text-muted)', cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', fontSize: 12, letterSpacing: '1px' }}>[ LOGS ]</button>
        
        <button 
          onClick={onInject}
          disabled={!selectedGame || injectState === 'success' || injectState === 'error' || injectState === 'cancelled'}
          className={isReady ? 'pulse-btn btn-glow' : 'btn-glow'}
          style={{ 
            background: injectState === 'injecting' ? 'rgba(255,0,60,0.15)' : injectState === 'success' ? 'rgba(0,255,136,0.15)' : 'rgba(0,240,255,0.08)', 
            border: `1px solid ${getButtonColor()}`, 
            color: getButtonColor(),
            padding: '14px 30px',
            borderRadius: 8,
            cursor: (!selectedGame || injectState === 'success' || injectState === 'error' || injectState === 'cancelled') ? 'not-allowed' : 'pointer',
            opacity: (!selectedGame && injectState === 'default') ? 0.4 : 1,
            fontFamily: 'var(--ag-font-mono)',
            fontWeight: 'bold',
            fontSize: 15,
            letterSpacing: '2px',
            minWidth: 240,
            textShadow: `0 0 10px ${getButtonColor()}`,
            boxShadow: injectState !== 'default' ? `0 0 20px ${getButtonColor()}60, inset 0 2px 15px ${getButtonColor()}30` : `0 4px 15px rgba(0,0,0,0.3), inset 0 1px 2px rgba(255,255,255,0.1)`,
            transition: 'all 0.4s cubic-bezier(0.2, 0.8, 0.2, 1)'
          }}
        >
          {getButtonContent()}
        </button>
      </div>
    </div>
  );
}
