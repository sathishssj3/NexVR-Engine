import { useState, useRef } from 'react';
import type { GameEntry } from '../types';
import { useFastSmoothScroll } from '../hooks/useFastSmoothScroll';

export function Sidebar({ games, waitingGames = [], selectedId, onSelect, onRescan, onRestore, onIgnore, onRestoreIgnored }: any) {
  const [search, setSearch] = useState('');
  const [isScanning, setIsScanning] = useState(false);
  const [isWaitListOpen, setIsWaitListOpen] = useState(false);
  const sidebarScrollRef = useRef<HTMLDivElement>(null);

  // Fast and smooth scrolling for game list
  useFastSmoothScroll(sidebarScrollRef, { speed: 1.5, smoothness: 0.28 });
  
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
      {/* Search */}
      <div style={{ padding: '14px 14px 10px 14px' }}>
        <div style={{ position: 'relative' }}>
          <span style={{ 
            position: 'absolute', left: 12, top: '50%', transform: 'translateY(-50%)', 
            color: 'var(--ag-text-muted)', fontSize: 13, pointerEvents: 'none', opacity: 0.5 
          }}>⌕</span>
          <input 
            placeholder="SEARCH LIBRARY..." 
            value={search}
            onChange={e => setSearch(e.target.value)}
            style={{ 
              width: '100%', boxSizing: 'border-box', padding: '12px 14px 12px 36px', 
              background: 'rgba(5, 8, 12, 0.6)', border: '1px solid var(--ag-border)', 
              color: 'var(--ag-text-primary)', outline: 'none', borderRadius: 'var(--ag-radius-sm)', 
              fontFamily: 'var(--ag-font-mono)', fontSize: 13, letterSpacing: '1px', 
              transition: 'all 0.4s var(--ag-transition)', 
              boxShadow: 'inset 0 2px 6px rgba(0,0,0,0.8), 0 1px 1px rgba(255,255,255,0.03)' 
            }}
            onFocus={e => { e.target.style.borderColor = 'rgba(0, 240, 255, 0.6)'; e.target.style.boxShadow = '0 0 20px rgba(0, 240, 255, 0.2), inset 0 2px 6px rgba(0,0,0,0.8)'; e.target.style.background = 'rgba(10, 15, 25, 0.8)'; }}
            onBlur={e => { e.target.style.borderColor = 'var(--ag-border)'; e.target.style.boxShadow = 'inset 0 2px 6px rgba(0,0,0,0.8), 0 1px 1px rgba(255,255,255,0.03)'; e.target.style.background = 'rgba(5, 8, 12, 0.6)'; }}
          />
        </div>
        {/* Game count */}
        <div style={{ 
          display: 'flex', justifyContent: 'space-between', alignItems: 'center', 
          marginTop: 10, padding: '0 2px' 
        }}>
          <span style={{ 
            fontSize: 10, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-text-muted)', 
            letterSpacing: '1px', opacity: 0.6 
          }}>
            {search ? `${filtered.length} FOUND` : ''}
          </span>
          {search && (
            <button 
              onClick={() => setSearch('')}
              style={{ 
                background: 'transparent', border: 'none', color: 'var(--ag-text-muted)', 
                fontSize: 10, fontFamily: 'var(--ag-font-mono)', cursor: 'pointer', 
                letterSpacing: '1px', opacity: 0.6, padding: 0 
              }}
            >
              CLEAR
            </button>
          )}
        </div>
      </div>

      {/* Game List / Wait List */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        {isWaitListOpen ? (
           <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
             <div style={{ padding: '10px 15px 0 15px', flexShrink: 0 }}>
               <h3 style={{ color: 'var(--ag-text-primary)', fontFamily: 'var(--ag-font-mono)', fontSize: 12, marginBottom: 15, borderBottom: '1px solid var(--ag-border)', paddingBottom: 10, letterSpacing: '2px' }}>WAITING LIST</h3>
             </div>
             <div style={{ flex: 1, overflowY: 'auto', padding: '0 15px 15px 15px' }}>
               {waitingGames.length === 0 ? (
                 <div className="empty-state" style={{ minHeight: 100 }}>
                   <div className="text">No games waiting</div>
                 </div>
               ) : (
                 waitingGames.map((g: GameEntry, i: number) => (
                   <div key={g.id} className={`glass-card fade-in-up stagger-${Math.min(i + 1, 5)}`} style={{ padding: '10px', marginBottom: 8 }}>
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
          <div ref={sidebarScrollRef} className="fast-smooth-scroll" style={{ flex: 1, padding: '0 5px' }}>
            {filtered.length === 0 ? (
              <div className="empty-state" style={{ padding: '40px 20px' }}>
                <div className="icon">🎮</div>
                <div className="text">{search ? 'No matches found' : 'No games detected'}</div>
                <div className="hint">{search ? 'Try a different search term' : 'Click RESCAN LIBRARY below'}</div>
              </div>
            ) : (
              filtered.map((g: GameEntry, i: number) => (
              <div 
                key={g.id} 
                className={`glitch-hover slide-in-left stagger-${Math.min(i + 1, 5)}`}
                onClick={() => onSelect(g)}
                style={{ 
                  padding: '12px 16px', 
                  cursor: 'pointer', 
                  display: 'flex', 
                  alignItems: 'center',
                  borderLeft: selectedId === g.id ? '4px solid var(--ag-accent)' : '4px solid transparent',
                  background: selectedId === g.id ? 'linear-gradient(90deg, rgba(0,240,255,0.15) 0%, rgba(0,240,255,0.02) 100%)' : 'transparent',
                  marginBottom: 6,
                  borderRadius: 'var(--ag-radius-sm)',
                  transition: 'all 0.4s var(--ag-transition)',
                  boxShadow: selectedId === g.id ? 'inset 0 1px 1px rgba(255,255,255,0.05), -2px 0 15px rgba(0,240,255,0.2)' : 'none',
                  position: 'relative'
                }}
                onMouseEnter={e => { if (selectedId !== g.id) { e.currentTarget.style.background = 'rgba(255,255,255,0.04)'; e.currentTarget.style.transform = 'translateX(4px)'; } }}
                onMouseLeave={e => { if (selectedId !== g.id) { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.transform = 'translateX(0)'; } }}
              >
                {/* Game Icon with 12px perfect squircle radius */}
                <div style={{ 
                  width: 52, height: 52, borderRadius: 12, 
                  background: selectedId === g.id ? 'linear-gradient(135deg, rgba(0, 240, 255, 0.2), rgba(15, 20, 30, 0.95))' : 'linear-gradient(135deg, rgba(25, 32, 45, 0.6), rgba(10, 14, 20, 0.9))', 
                  display: 'flex', alignItems: 'center', justifyContent: 'center', 
                  marginRight: 14, 
                  flexShrink: 0, 
                  boxShadow: selectedId === g.id ? '0 8px 20px rgba(0,240,255,0.3), inset 0 1px 2px rgba(255,255,255,0.3)' : '0 4px 12px rgba(0,0,0,0.5), inset 0 1px 1px rgba(255,255,255,0.06)', 
                  overflow: 'hidden', 
                  border: selectedId === g.id ? '1px solid rgba(0,240,255,0.6)' : '1px solid rgba(255,255,255,0.1)', 
                  transition: 'all 0.3s var(--ag-transition)' 
                }}>
                  {g.iconBase64 ? (
                     <img 
                       src={g.iconBase64} 
                       style={{ 
                         width: '100%', height: '100%', 
                         objectFit: 'cover', 
                         imageRendering: '-webkit-optimize-contrast'
                       }} 
                       alt={g.name} 
                     />
                  ) : (
                     <span style={{ 
                       fontFamily: 'var(--ag-font-display)', 
                       fontWeight: 800, fontSize: 16, 
                       color: selectedId === g.id ? 'var(--ag-accent)' : 'var(--ag-text-muted)',
                       letterSpacing: '1px'
                     }}>
                       {g.name.substring(0, 2).toUpperCase()}
                     </span>
                  )}
                </div>
                
                {/* Game Info */}
                <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                  <span style={{ 
                    fontSize: 13, fontWeight: 600, 
                    color: selectedId === g.id ? '#fff' : 'var(--ag-text-primary)', 
                    whiteSpace: 'nowrap', textOverflow: 'ellipsis', overflow: 'hidden', 
                    letterSpacing: '0.3px',
                    fontFamily: 'var(--ag-font-display)'
                  }}>
                    {g.name}
                  </span>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginTop: 4 }}>
                    <span style={{ 
                      fontSize: 9, 
                      padding: '1px 5px',
                      borderRadius: 4,
                      fontWeight: 700,
                      background: g.api === 'DX12' ? 'rgba(0, 230, 118, 0.15)' : 'rgba(0, 240, 255, 0.12)',
                      color: g.api === 'DX12' ? 'var(--ag-accent-success)' : 'var(--ag-accent)', 
                      border: g.api === 'DX12' ? '1px solid rgba(0, 230, 118, 0.3)' : '1px solid rgba(0, 240, 255, 0.3)',
                      fontFamily: 'var(--ag-font-mono)', letterSpacing: '0.5px' 
                    }}>
                      {g.api}
                    </span>
                    {g.compat === 'verified' && (
                      <span style={{ fontSize: 9, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent-success)', fontWeight: 600 }}>
                        ✓
                      </span>
                    )}
                    {g.hasInjector && (
                      <span style={{ width: 6, height: 6, borderRadius: '50%', background: 'var(--ag-accent-success)', boxShadow: '0 0 6px var(--ag-accent-success)' }} title="VR Mod Active" />
                    )}
                  </div>
                </div>
              </div>
            ))
            )}
          </div>
        )}
      </div>

      {/* Bottom Actions */}
      <div style={{ padding: 12, borderTop: '1px solid var(--ag-border)', background: 'linear-gradient(0deg, rgba(0,0,0,0.4), rgba(0,0,0,0.1))' }}>
        <button onClick={() => setIsWaitListOpen(!isWaitListOpen)} className="btn-glow" style={{ 
          width: '100%', padding: '9px', 
          background: isWaitListOpen ? 'rgba(0,240,255,0.1)' : 'rgba(255,255,255,0.02)', 
          border: isWaitListOpen ? '1px solid rgba(0,240,255,0.4)' : '1px solid var(--ag-border)', 
          color: isWaitListOpen ? 'var(--ag-accent)' : 'var(--ag-text-primary)', 
          cursor: 'pointer', marginBottom: 8, borderRadius: 'var(--ag-radius-sm)', 
          fontFamily: 'var(--ag-font-mono)', fontSize: 11, letterSpacing: '1px', 
          boxShadow: isWaitListOpen ? '0 0 15px rgba(0,240,255,0.15), inset 0 1px 1px rgba(255,255,255,0.1)' : 'inset 0 1px 1px rgba(255,255,255,0.03)',
          display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 6
        }}>
          {isWaitListOpen ? '← BACK TO LIBRARY' : <>WAIT LIST <span className="game-count">{waitingGames?.length || 0}</span></>}
        </button>
        {!isWaitListOpen && (
          <>
            <button onClick={handleRescan} className="btn-glow" style={{ 
              width: '100%', padding: '9px', background: 'rgba(255,255,255,0.02)', 
              border: '1px solid var(--ag-border)', color: 'var(--ag-text-primary)', 
              cursor: 'pointer', marginBottom: 8, borderRadius: 'var(--ag-radius-sm)', 
              fontFamily: 'var(--ag-font-mono)', fontSize: 11, letterSpacing: '1px', 
              boxShadow: 'inset 0 1px 1px rgba(255,255,255,0.03)',
              opacity: isScanning ? 0.6 : 1
            }}>
              {isScanning ? '◌ SCANNING...' : '⟲ RESCAN LIBRARY'}
            </button>
            <button onClick={handleAddCustom} className="btn-glow" style={{ 
              width: '100%', padding: '9px', background: 'rgba(0,240,255,0.05)', 
              border: '1px dashed rgba(0,240,255,0.4)', color: 'var(--ag-accent)', 
              cursor: 'pointer', borderRadius: 'var(--ag-radius-sm)', fontFamily: 'var(--ag-font-mono)', 
              fontSize: 11, letterSpacing: '1px', textShadow: '0 0 8px rgba(0,240,255,0.5)', 
              boxShadow: 'inset 0 1px 2px rgba(255,255,255,0.1)' 
            }}>
              + ADD CUSTOM
            </button>
          </>
        )}
      </div>
    </div>
  );
}
