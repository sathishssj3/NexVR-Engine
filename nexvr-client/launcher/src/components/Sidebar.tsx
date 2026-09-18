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
    <div className="glass-panel" style={{ 
      width: 280, 
      borderRight: '1px solid var(--ag-border)', 
      display: 'flex', 
      flexDirection: 'column', 
      zIndex: 5, 
      borderTop: 'none', 
      borderBottom: 'none', 
      borderLeft: 'none',
      background: 'var(--ag-bg-surface)' 
    }}>
      {/* Search Library */}
      <div style={{ padding: '14px 14px 10px 14px' }}>
        <div style={{ position: 'relative' }}>
          <input 
            placeholder="SEARCH LIBRARY..." 
            value={search}
            onChange={e => setSearch(e.target.value)}
            style={{ 
              width: '100%', boxSizing: 'border-box', padding: '10px 14px', 
              background: 'rgba(5, 5, 8, 0.8)', 
              border: '1px solid var(--ag-border)', 
              color: '#FFFFFF', outline: 'none', borderRadius: 'var(--ag-radius-sm)', 
              fontFamily: 'var(--ag-font-display)', fontSize: 12, letterSpacing: '0.07em', 
              fontWeight: 800,
              transition: 'all 0.25s var(--ag-transition)', 
              boxShadow: 'inset 0 2px 6px rgba(0, 0, 0, 0.8)' 
            }}
            onFocus={e => { 
              e.target.style.borderColor = 'rgba(255, 255, 255, 0.35)'; 
              e.target.style.boxShadow = 'inset 0 2px 6px rgba(0, 0, 0, 0.8)';
              e.target.style.background = 'rgba(12, 12, 18, 0.95)'; 
            }}
            onBlur={e => { 
              e.target.style.borderColor = 'var(--ag-border)'; 
              e.target.style.boxShadow = 'inset 0 2px 6px rgba(0, 0, 0, 0.8)'; 
              e.target.style.background = 'rgba(5, 5, 8, 0.8)'; 
            }}
          />
        </div>
        
        {/* Game count & clear */}
        <div style={{ 
          display: 'flex', justifyContent: 'space-between', alignItems: 'center', 
          marginTop: 8, padding: '0 2px' 
        }}>
          <span style={{ 
            fontSize: 10, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-text-muted)', 
            letterSpacing: '1px', opacity: 0.7 
          }}>
            {search ? `${filtered.length} FOUND` : `${games.length} DETECTED`}
          </span>
          {search && (
            <button 
              onClick={() => setSearch('')}
              style={{ 
                background: 'transparent', border: 'none', color: 'var(--ag-accent)', 
                fontSize: 10, fontFamily: 'var(--ag-font-mono)', cursor: 'pointer', 
                letterSpacing: '1px', padding: 0, fontWeight: 700 
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
              <h3 style={{ color: 'var(--ag-text-primary)', fontFamily: 'var(--ag-font-display)', fontSize: 12, marginBottom: 15, borderBottom: '1px solid var(--ag-border)', paddingBottom: 10, letterSpacing: '1.5px', fontWeight: 700 }}>
                WAITING LIST
              </h3>
            </div>
            <div style={{ flex: 1, overflowY: 'auto', padding: '0 15px 15px 15px' }}>
              {waitingGames.length === 0 ? (
                <div style={{ padding: '40px 10px', textAlign: 'center', color: 'var(--ag-text-muted)', fontSize: 12, fontFamily: 'var(--ag-font-ui)' }}>
                  No games waiting
                </div>
              ) : (
                waitingGames.map((g: GameEntry, i: number) => (
                  <div key={g.id} className={`glass-card fade-in-up stagger-${Math.min(i + 1, 5)}`} style={{ padding: '10px', marginBottom: 8, background: 'var(--ag-bg-card)' }}>
                    <div style={{ fontSize: 13, color: '#FFF', marginBottom: 8, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis', fontFamily: 'var(--ag-font-display)', fontWeight: 600 }}>{g.name}</div>
                    <div style={{ display: 'flex', gap: 6 }}>
                      <button className="btn-glow" onClick={() => onRestore(g.id)} style={{ flex: 1, padding: '6px', background: 'rgba(204, 0, 0, 0.12)', border: '1px solid var(--ag-accent)', color: 'var(--ag-accent)', cursor: 'pointer', borderRadius: 3, fontFamily: 'var(--ag-font-display)', fontSize: 10, fontWeight: 700 }}>RESTORE</button>
                      <button className="btn-glow" onClick={() => onIgnore(g.id)} style={{ flex: 1, padding: '6px', background: 'rgba(255, 255, 255, 0.05)', border: '1px solid var(--ag-border)', color: 'var(--ag-text-muted)', cursor: 'pointer', borderRadius: 3, fontFamily: 'var(--ag-font-display)', fontSize: 10 }}>REMOVE</button>
                    </div>
                  </div>
                ))
              )}
            </div>
            <div style={{ padding: '0 15px 15px 15px', flexShrink: 0 }}>
              <button onClick={onRestoreIgnored} className="btn-outline-laser" style={{ width: '100%', padding: '8px', fontSize: 10.5 }}>
                RESTORE IGNORED GAMES
              </button>
            </div>
          </div>
        ) : (
          <div ref={sidebarScrollRef} className="fast-smooth-scroll" style={{ flex: 1, padding: '0 6px', display: 'flex', flexDirection: 'column' }}>
            {filtered.length === 0 ? (
              /* Pure Text Empty State Structure - Centered */
              <div style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', textAlign: 'center', padding: '24px 16px' }}>
                <div style={{ fontSize: 13, fontWeight: 700, color: '#FFF', marginBottom: 6, fontFamily: 'var(--ag-font-display)', letterSpacing: '0.02em' }}>
                  {search ? 'No matches found' : 'No games detected'}
                </div>
                <div style={{ fontSize: 11, color: 'var(--ag-text-muted)', fontFamily: 'var(--ag-font-ui)' }}>
                  {search ? 'Try a different search term' : 'Click RESCAN LIBRARY below'}
                </div>
              </div>
            ) : (
              filtered.map((g: GameEntry, i: number) => {
                const isSelected = selectedId === g.id;
                return (
                  <div 
                    key={g.id} 
                    className={`slide-in-left stagger-${Math.min(i + 1, 5)}`}
                    onClick={() => onSelect(g)}
                    style={{ 
                      padding: '10px 14px', 
                      cursor: 'pointer', 
                      display: 'flex', 
                      alignItems: 'center',
                      borderLeft: isSelected ? '3px solid var(--ag-accent)' : '3px solid transparent',
                      background: isSelected ? 'linear-gradient(90deg, rgba(204, 0, 0, 0.16) 0%, rgba(204, 0, 0, 0.02) 100%)' : 'transparent',
                      marginBottom: 4,
                      borderRadius: 'var(--ag-radius-sm)',
                      transition: 'all 0.25s var(--ag-transition)',
                      boxShadow: isSelected ? 'inset 0 1px 1px rgba(255, 255, 255, 0.05)' : 'none',
                      position: 'relative'
                    }}
                    onMouseEnter={e => { 
                      if (!isSelected) { 
                        e.currentTarget.style.background = 'rgba(255, 255, 255, 0.04)'; 
                        e.currentTarget.style.transform = 'translateX(3px)'; 
                      } 
                    }}
                    onMouseLeave={e => { 
                      if (!isSelected) { 
                        e.currentTarget.style.background = 'transparent'; 
                        e.currentTarget.style.transform = 'translateX(0)'; 
                      } 
                    }}
                  >
                    {/* Game Avatar Box */}
                    <div style={{ 
                      width: 44, height: 44, borderRadius: 8, 
                      background: isSelected ? 'linear-gradient(135deg, rgba(204, 0, 0, 0.25), rgba(15, 15, 20, 0.95))' : 'linear-gradient(135deg, rgba(25, 25, 32, 0.8), rgba(10, 10, 14, 0.95))', 
                      display: 'flex', alignItems: 'center', justifyContent: 'center', 
                      marginRight: 12, 
                      flexShrink: 0, 
                      boxShadow: '0 4px 10px rgba(0, 0, 0, 0.5)', 
                      overflow: 'hidden', 
                      border: isSelected ? '1px solid rgba(204, 0, 0, 0.6)' : '1px solid rgba(255, 255, 255, 0.08)', 
                      transition: 'all 0.25s var(--ag-transition)' 
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
                          fontWeight: 800, fontSize: 15, 
                          color: isSelected ? 'var(--ag-accent)' : 'var(--ag-text-muted)',
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
                        color: isSelected ? '#FFF' : 'var(--ag-text-primary)', 
                        whiteSpace: 'nowrap', textOverflow: 'ellipsis', overflow: 'hidden', 
                        letterSpacing: '0.02em',
                        fontFamily: 'var(--ag-font-display)'
                      }}>
                        {g.name}
                      </span>
                      <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginTop: 3 }}>
                        <span style={{ 
                          fontSize: 9, 
                          padding: '1px 5px',
                          borderRadius: 3,
                          fontWeight: 700,
                          background: g.api === 'DX12' ? 'rgba(48, 209, 88, 0.15)' : 'rgba(204, 0, 0, 0.12)',
                          color: g.api === 'DX12' ? 'var(--ag-accent-success)' : 'var(--ag-accent)', 
                          border: g.api === 'DX12' ? '1px solid rgba(48, 209, 88, 0.35)' : '1px solid rgba(204, 0, 0, 0.3)',
                          fontFamily: 'var(--ag-font-mono)', letterSpacing: '0.5px' 
                        }}>
                          {g.api}
                        </span>
                        {g.compat === 'verified' && (
                          <span style={{ fontSize: 9, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent-success)', fontWeight: 600 }}>
                            VERIFIED
                          </span>
                        )}
                        {g.hasInjector && (
                          <span style={{ fontSize: 8.5, padding: '1px 4px', borderRadius: 2, background: 'rgba(48, 209, 88, 0.15)', color: 'var(--ag-accent-success)', fontFamily: 'var(--ag-font-mono)', fontWeight: 700 }}>
                            ACTIVE
                          </span>
                        )}
                      </div>
                    </div>
                  </div>
                );
              })
            )}
          </div>
        )}
      </div>

      {/* Bottom Actions (Pure Text + Landing Page Styling) */}
      <div style={{ padding: 12, borderTop: '1px solid var(--ag-border)', background: 'rgba(8, 8, 12, 0.7)' }}>
        <button onClick={() => setIsWaitListOpen(!isWaitListOpen)} className="btn-glow" style={{ 
          width: '100%', padding: '9px', 
          background: isWaitListOpen ? 'rgba(204, 0, 0, 0.12)' : 'rgba(255, 255, 255, 0.02)', 
          border: isWaitListOpen ? '1px solid rgba(204, 0, 0, 0.4)' : '1px solid var(--ag-border)', 
          color: isWaitListOpen ? 'var(--ag-accent)' : 'var(--ag-text-primary)', 
          cursor: 'pointer', marginBottom: 8, borderRadius: 'var(--ag-radius-sm)', 
          fontFamily: 'var(--ag-font-display)', fontSize: 11, letterSpacing: '0.06em', 
          fontWeight: 600,
          display: 'flex', alignItems: 'center', justifyContent: 'center'
        }}>
          {isWaitListOpen ? 'BACK TO LIBRARY' : (
            <span style={{ display: 'inline-flex', alignItems: 'center', gap: 5 }}>
              <span>WAIT LIST</span>
              <span style={{ fontSize: 13.5, fontWeight: 800, color: 'var(--ag-accent)' }}>
                {waitingGames?.length || 0}
              </span>
            </span>
          )}
        </button>

        {!isWaitListOpen && (
          <>
            <button onClick={handleRescan} className="btn-glow" style={{ 
              width: '100%', padding: '9px', background: 'rgba(255, 255, 255, 0.03)', 
              border: '1px solid var(--ag-border)', color: 'var(--ag-text-primary)', 
              cursor: 'pointer', marginBottom: 8, borderRadius: 'var(--ag-radius-sm)', 
              fontFamily: 'var(--ag-font-display)', fontSize: 11, letterSpacing: '0.06em', 
              fontWeight: 600,
              opacity: isScanning ? 0.6 : 1
            }}>
              {isScanning ? 'SCANNING...' : 'RESCAN LIBRARY'}
            </button>

            <button 
              onClick={handleAddCustom} 
              style={{ 
                width: '100%', padding: '9px', 
                background: 'rgba(132, 136, 132, 0.08)', 
                border: '1px solid #CC0000', 
                color: 'var(--ag-accent)', 
                cursor: 'pointer', 
                borderRadius: 'var(--ag-radius-sm)', 
                fontFamily: 'var(--ag-font-display)', 
                fontSize: 11, 
                letterSpacing: '0.08em', 
                fontWeight: 800,
                boxShadow: 'none',
                transition: 'all 0.2s ease'
              }}
              onMouseEnter={e => { 
                e.currentTarget.style.background = 'rgba(132, 136, 132, 0.16)'; 
                e.currentTarget.style.borderColor = '#FF1A1A'; 
              }}
              onMouseLeave={e => { 
                e.currentTarget.style.background = 'rgba(132, 136, 132, 0.08)'; 
                e.currentTarget.style.borderColor = '#CC0000'; 
              }}
            >
              ADD CUSTOM
            </button>
          </>
        )}
      </div>
    </div>
  );
}
