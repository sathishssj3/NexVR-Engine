import { GameEntry, VRConfig } from '../types';
import { SettingsPanel } from './SettingsPanel';
import { SessionLog } from './SessionLog';

export function GameDetail({ game, config, onConfigChange, logLines }: any) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Hero */}
      <div style={{ marginBottom: 20 }}>
        <h1 style={{ margin: '0 0 10px 0', fontSize: 32 }}>{game.name}</h1>
        <div style={{ fontFamily: 'var(--ag-font-mono)', fontSize: 12, color: 'var(--ag-text-muted)', marginBottom: 15 }}>
          {game.installPath}
        </div>
        <div style={{ display: 'flex', gap: 10 }}>
          <span style={{ padding: '4px 10px', borderRadius: 4, background: 'var(--ag-bg-surface)', border: '1px solid var(--ag-border)', fontSize: 12 }}>
            API: <strong style={{ color: game.api === 'DX12' ? '#00ffb3' : game.api === 'Vulkan' ? '#e24b4a' : '#4898e0' }}>{game.api}</strong>
          </span>
          <span style={{ padding: '4px 10px', borderRadius: 4, background: 'var(--ag-bg-surface)', border: '1px solid var(--ag-border)', fontSize: 12 }}>
            Compat: {game.compat}
          </span>
        </div>
      </div>
      
      {/* Settings Grid */}
      <SettingsPanel config={config} onChange={onConfigChange} />
      
      {/* Session Log */}
      <SessionLog logLines={logLines} />
    </div>
  );
}
