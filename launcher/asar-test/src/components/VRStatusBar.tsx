import { VRStatus, GameEntry } from '../types';

export function VRStatusBar({ status, selectedGame, isInjecting, injectState, onInject }: any) {
  const getButtonContent = () => {
    if (injectState === 'injecting') return '◌ INJECTING...';
    if (injectState === 'success') return '✓ INJECTED';
    if (injectState === 'error') return '✗ FAILED';
    return '▶ INJECT';
  };

  const getButtonColor = () => {
    if (injectState === 'success') return '#00ffb3';
    if (injectState === 'error') return '#e24b4a';
    return 'var(--ag-accent)';
  };

  return (
    <div style={{ height: 60, borderTop: '1px solid var(--ag-border)', display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '0 20px', background: 'var(--ag-bg-base)' }}>
      <div style={{ display: 'flex', alignItems: 'center' }}>
        <div style={{ 
          width: 10, height: 10, borderRadius: '50%', 
          background: status.connected ? 'var(--ag-accent)' : '#4a6072',
          boxShadow: status.connected ? '0 0 10px var(--ag-accent)' : 'none',
          marginRight: 15
        }} />
        <div style={{ display: 'flex', flexDirection: 'column' }}>
          <span style={{ fontSize: 14 }}>{status.headset}</span>
          <span style={{ fontSize: 12, color: 'var(--ag-text-muted)' }}>{status.runtime} • {status.refreshRate}Hz</span>
        </div>
      </div>
      
      <div style={{ display: 'flex', alignItems: 'center', gap: 15 }}>
        <button style={{ background: 'transparent', border: 'none', color: 'var(--ag-text-muted)', cursor: 'pointer' }}>Open Config</button>
        <button style={{ background: 'transparent', border: 'none', color: 'var(--ag-text-muted)', cursor: 'pointer' }}>View Logs</button>
        <button 
          onClick={onInject}
          disabled={!selectedGame || isInjecting}
          style={{ 
            background: 'transparent', 
            border: `1px solid ${getButtonColor()}`, 
            color: getButtonColor(),
            padding: '8px 20px',
            borderRadius: 4,
            cursor: (!selectedGame || isInjecting) ? 'not-allowed' : 'pointer',
            opacity: (!selectedGame && injectState === 'default') ? 0.5 : 1,
            fontFamily: 'var(--ag-font-mono)',
            fontWeight: 'bold',
            minWidth: 140
          }}
        >
          {getButtonContent()}
        </button>
      </div>
    </div>
  );
}
