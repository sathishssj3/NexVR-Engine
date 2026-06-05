import { useState } from 'react';
import type { GameEntry } from '../types';

export function Sidebar({ games, waitingGames = [], selectedId, onSelect, onRescan, onRestore, onIgnore, onRestoreIgnored }: any) {
  const [search, setSearch] = useState('');
  const [isScanning, setIsScanning] = useState(false);
  const [isWaitListOpen, setIsWaitListOpen] = useState(false);
  
  const handleRescan = async () => {
    setIsScanning(true);
    await onRescan();
    setTimeout(() => setIsScanning(false), 500);
  };
  
  const handleAddCustom = async () => {
    if (window.ag && window.ag.library) {
      const res = await window.ag.library.addCustom();
      if (res.success) await onRescan();
    }
  };
  
  const filtered = games.filter((g: GameEntry) => g.name.toLowerCase().includes(search.toLowerCase()));

  return (
    <div className="glass-panel" style={{ width: 280, borderRight: '1px solid var(--ag-border)', display: 'flex', flexDirection: 'column', zIndex: 5, borderTop: 'none', borderBottom: 'none', borderLeft: 'none' }}>
      <div style={{ padding: 15 }}>
        <input 
          placeholder="SEARCH LIBRARY..." 
          value={search}
          onChange={e => setSearch(e.target.value)}
          style={{ width: '100%', boxSizing: 'border-box', padding: '12px 14px', background: 'rgba(5, 8, 12, 0.6)', border: '1px solid var(--ag-border)', color: 'var(--ag-accent)', outline: 'none', borderRadius: 8, fontFamily: 'var(--ag-font-mono)', fontSize: 13, letterSpacing: '1px', transition: 'all 0.3s cubic-bezier(0.2, 0.8, 0.2, 1)', boxShadow: 'inset 0 2px 6px rgba(0,0,0,0.8), 0 1px 1px rgba(255,255,255,0.03)' }}
          onFocus={e => { e.target.style.borderColor = 'rgba(0, 240, 255, 0.5)'; e.target.style.boxShadow = '0 0 15px rgba(0, 240, 255, 0.15), inset 0 2px 6px rgba(0,0,0,0.8)'; }}
          onBlur={e => { e.target.style.borderColor = 'var(--ag-border)'; e.target.style.boxShadow = 'inset 0 2px 6px rgba(0,0,0,0.8), 0 1px 1px rgba(255,255,255,0.03)'; }}
        />
      </div>
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        {isWaitListOpen ? (
           <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
             <div style={{ padding: '10px 15px 0 15px', flexShrink: 0 }}>
               <h3 style={{ color: 'var(--ag-text-primary)', fontFamily: 'var(--ag-font-mono)', fontSize: 13, marginBottom: 15, borderBottom: '1px solid var(--ag-border)', paddingBottom: 10 }}>WAITING LIST</h3>
             </div>
             <div style={{ flex: 1, overflowY: 'auto', padding: '0 15px 15px 15px' }}>
               {waitingGames.length === 0 ? (
                 <div style={{ color: 'var(--ag-text-muted)', fontSize: 12, textAlign: 'center', marginTop: 20 }}>No games waiting.</div>
               ) : (
                 waitingGames.map((g: GameEntry) => (
                   <div key={g.id} style={{ padding: '10px', background: 'rgba(0,0,0,0.3)', border: '1px solid var(--ag-border)', marginBottom: 8, borderRadius: 4 }}>
                     <div style={{ fontSize: 13, color: '#fff', marginBottom: 8, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>{g.name}</div>
                     <div style={{ display: 'flex', gap: 5 }}>
                       <button className="btn-glow" onClick={() => onRestore(g.id)} style={{ flex: 1, padding: '6px', background: 'rgba(0,240,255,0.1)', border: '1px solid var(--ag-accent)', color: 'var(--ag-accent)', cursor: 'pointer', borderRadius: 4, fontFamily: 'var(--ag-font-mono)', fontSize: 10 }}>RESTORE</button>
                       <button className="btn-glow" onClick={() => onIgnore(g.id)} style={{ flex: 1, padding: '6px', background: 'rgba(255,0,60,0.1)', border: '1px solid #ff003c', color: '#ff003c', cursor: 'pointer', borderRadius: 4, fontFamily: 'var(--ag-font-mono)', fontSize: 10 }}>REMOVE</button>
                     </div>
                   </div>
                 ))
               )}
             </div>
             <div style={{ padding: '0 15px 15px 15px', flexShrink: 0 }}>
               <button onClick={onRestoreIgnored} className="btn-glow" style={{ width: '100%', padding: '8px', background: 'rgba(255,255,255,0.03)', border: '1px solid var(--ag-border)', color: 'var(--ag-text-muted)', cursor: 'pointer', borderRadius: 4, fontFamily: 'var(--ag-font-mono)', fontSize: 10, letterSpacing: '1px' }}>
                 RESTORE IGNORED GAMES
               </button>
             </div>
           </div>
        ) : (
          <div style={{ flex: 1, overflowY: 'auto', padding: '0 5px' }}>
            {filtered.map((g: GameEntry) => (
            <div 
              key={g.id} 
              className="glitch-hover"
              onClick={() => onSelect(g)}
              style={{ 
                padding: '12px 15px', 
                cursor: 'pointer', 
                display: 'flex', 
                alignItems: 'center',
                borderLeft: selectedId === g.id ? '4px solid var(--ag-accent)' : '4px solid transparent',
                background: selectedId === g.id ? 'radial-gradient(150% 100% at 0% 50%, rgba(0,240,255,0.15), rgba(0,240,255,0.01) 80%, transparent)' : 'transparent',
                marginBottom: 5,
                borderRadius: '8px',
                transition: 'all 0.3s cubic-bezier(0.2, 0.8, 0.2, 1)',
                boxShadow: selectedId === g.id ? '-1px 0 15px rgba(0,240,255,0.2), inset 0 1px 1px rgba(255,255,255,0.03)' : 'none',
                position: 'relative'
              }}
              onMouseEnter={e => { if (selectedId !== g.id) e.currentTarget.style.background = 'rgba(255,255,255,0.02)'; }}
              onMouseLeave={e => { if (selectedId !== g.id) e.currentTarget.style.background = 'transparent'; }}
            >
              <div style={{ width: 48, height: 64, borderRadius: 8, background: selectedId === g.id ? 'linear-gradient(135deg, rgba(40,50,75,0.6), #050608)' : 'linear-gradient(135deg, rgba(20,25,35,0.4), #050608)', display: 'flex', alignItems: 'center', justifyContent: 'center', marginRight: 15, fontSize: 22, fontWeight: 'bold', color: selectedId === g.id ? '#fff' : 'var(--ag-text-muted)', flexShrink: 0, boxShadow: selectedId === g.id ? '0 8px 15px rgba(0,240,255,0.25), inset 0 1px 2px rgba(255,255,255,0.2)' : '0 4px 10px rgba(0,0,0,0.5), inset 0 1px 1px rgba(255,255,255,0.05)', overflow: 'hidden', border: selectedId === g.id ? '1px solid rgba(0,240,255,0.4)' : '1px solid rgba(255,255,255,0.05)', transition: 'all 0.3s cubic-bezier(0.2, 0.8, 0.2, 1)' }}>
                {g.iconBase64 ? (
                   <img src={g.iconBase64} style={{ width: '100%', height: '100%', objectFit: 'contain' }} alt={g.name} />
                ) : (
                   g.name.substring(0, 2).toUpperCase()
                )}
              </div>
              <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                <span style={{ fontSize: 15, fontWeight: 600, color: selectedId === g.id ? '#fff' : 'var(--ag-text-primary)', whiteSpace: 'nowrap', textOverflow: 'ellipsis', overflow: 'hidden', letterSpacing: '0.5px' }}>
                  {g.name}
                </span>
                <span style={{ fontSize: 11, color: selectedId === g.id ? 'var(--ag-accent)' : 'var(--ag-text-muted)', marginTop: 6, fontFamily: 'var(--ag-font-mono)', letterSpacing: '1px' }}>
                  {g.api}
                </span>
              </div>
            </div>
          ))
          }
          </div>
        )}
      </div>
      <div style={{ padding: 15, borderTop: '1px solid var(--ag-border)', background: 'linear-gradient(0deg, rgba(0,0,0,0.4), rgba(0,0,0,0.1))' }}>
        <button onClick={() => setIsWaitListOpen(!isWaitListOpen)} className="btn-glow" style={{ width: '100%', padding: '10px', background: isWaitListOpen ? 'rgba(0,240,255,0.1)' : 'rgba(255,255,255,0.02)', border: isWaitListOpen ? '1px solid rgba(0,240,255,0.4)' : '1px solid var(--ag-border)', color: isWaitListOpen ? 'var(--ag-accent)' : 'var(--ag-text-primary)', cursor: 'pointer', marginBottom: 10, borderRadius: 6, fontFamily: 'var(--ag-font-mono)', fontSize: 12, letterSpacing: '1px', boxShadow: isWaitListOpen ? '0 0 15px rgba(0,240,255,0.15), inset 0 1px 1px rgba(255,255,255,0.1)' : 'inset 0 1px 1px rgba(255,255,255,0.03)' }}>
          {isWaitListOpen ? 'CLOSE WAIT LIST' : `WAIT LIST (${waitingGames?.length || 0})`}
        </button>
        {!isWaitListOpen && (
          <>
            <button onClick={handleRescan} className="btn-glow" style={{ width: '100%', padding: '10px', background: 'rgba(255,255,255,0.02)', border: '1px solid var(--ag-border)', color: 'var(--ag-text-primary)', cursor: 'pointer', marginBottom: 10, borderRadius: 6, fontFamily: 'var(--ag-font-mono)', fontSize: 12, letterSpacing: '1px', boxShadow: 'inset 0 1px 1px rgba(255,255,255,0.03)' }}>
              {isScanning ? 'SCANNING...' : 'RESCAN LIBRARY'}
            </button>
            <button onClick={handleAddCustom} className="btn-glow" style={{ width: '100%', padding: '10px', background: 'rgba(0,240,255,0.05)', border: '1px dashed rgba(0,240,255,0.5)', color: 'var(--ag-accent)', cursor: 'pointer', borderRadius: 6, fontFamily: 'var(--ag-font-mono)', fontSize: 12, letterSpacing: '1px', textShadow: '0 0 8px rgba(0,240,255,0.6)', boxShadow: 'inset 0 1px 2px rgba(255,255,255,0.1)' }}>
              [+] ADD CUSTOM
            </button>
          </>
        )}
      </div>
    </div>
  );
}
