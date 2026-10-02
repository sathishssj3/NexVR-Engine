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
    <div style={{ 
      width: 290, 
      borderRight: '1px solid #26262B', 
      display: 'flex', 
      flexDirection: 'column', 
      zIndex: 5, 
      borderTop: 'none', 
      borderBottom: 'none', 
      borderLeft: 'none',
      background: '#040406',
      height: '100%',
      overflow: 'hidden'
    }}>
      {/* Top Header: GAME LIBRARY + ADD GAME */}
      <div style={{ 
        padding: '16px 14px 10px 14px', 
        display: 'flex', 
        alignItems: 'center', 
        justifyContent: 'space-between',
      }}>
        <div style={{
          fontSize: 12.5,
          fontWeight: 800,
          letterSpacing: '0.08em',
          fontFamily: 'var(--ag-font-display)',
          color: '#FFFFFF',
          display: 'flex',
          alignItems: 'center',
          gap: 7
        }}>
          <span style={{ width: 3, height: 12, background: 'var(--ag-accent)', borderRadius: 2 }} />
          GAME LIBRARY
        </div>
        <button 
          onClick={handleAddCustom} 
          className="btn-primary-laser"
          style={{ 
            padding: '5px 12px', 
            fontSize: 10,
            letterSpacing: '0.06em'
          }}
        >
          + ADD GAME
        </button>
      </div>

      {/* Search Library */}
      <div style={{ padding: '4px 14px 14px 14px' }}>
        <div style={{ position: 'relative', display: 'flex', alignItems: 'center' }}>
          <svg 
            width="14" 
            height="14" 
            viewBox="0 0 24 24" 
            fill="none" 
            stroke="rgba(255, 255, 255, 0.4)" 
            strokeWidth="2.2" 
            strokeLinecap="round" 
            strokeLinejoin="round"
            style={{ position: 'absolute', left: 12, pointerEvents: 'none' }}
          >
            <circle cx="11" cy="11" r="8"></circle>
            <line x1="21" y1="21" x2="16.65" y2="16.65"></line>
          </svg>
          <input 
            placeholder="SEARCH TITLES..." 
            value={search}
            onChange={e => setSearch(e.target.value)}
            style={{ 
              width: '100%', 
              boxSizing: 'border-box', 
              padding: '9px 14px 9px 34px', 
              background: '#07070A', 
              border: '1px solid #2C2D35', 
              color: '#FFFFFF', 
              outline: 'none', 
              borderRadius: 'var(--ag-radius-sm)', 
              fontFamily: 'var(--ag-font-display)', 
              fontSize: 11.5, 
              letterSpacing: '0.06em', 
              fontWeight: 700,
              transition: 'all 0.2s ease', 
              boxShadow: 'inset 0 1px 3px rgba(0, 0, 0, 0.6)' 
            }}
            onFocus={e => { 
              e.target.style.borderColor = '#4A4D5C'; 
              e.target.style.boxShadow = 'inset 0 1px 4px rgba(0, 0, 0, 0.8), 0 0 0 1px #2C2D35';
            }}
            onBlur={e => { 
              e.target.style.borderColor = '#2C2D35'; 
              e.target.style.boxShadow = 'inset 0 1px 3px rgba(0, 0, 0, 0.6)'; 
            }}
          />
          {search && (
            <button 
              onClick={() => setSearch('')}
              style={{ 
                position: 'absolute',
                right: 10,
                background: 'transparent', 
                border: 'none', 
                color: '#848884', 
                fontSize: 12, 
                fontFamily: 'var(--ag-font-mono)', 
                cursor: 'pointer', 
                padding: 2, 
                fontWeight: 700 
              }}
              title="Clear search"
            >
              ✕
            </button>
          )}
        </div>
      </div>

      {/* Game List / Wait List */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        {isWaitListOpen ? (
          <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
            <div style={{ padding: '12px 14px 8px 14px', flexShrink: 0 }}>
              <div style={{ 
                color: '#FFF', 
                fontFamily: 'var(--ag-font-display)', 
                fontSize: 11.5, 
                marginBottom: 10, 
                borderBottom: '1px solid #26262B', 
                paddingBottom: 8, 
                letterSpacing: '0.08em', 
                fontWeight: 700,
                display: 'flex',
                alignItems: 'center',
                gap: 8
              }}>
                <span style={{ width: 3, height: 11, background: 'var(--ag-accent)', borderRadius: 2 }} />
                WAITING LIST
              </div>
            </div>
            <div style={{ flex: 1, overflowY: 'auto', padding: '0 12px 12px 12px' }}>
              {waitingGames.length === 0 ? (
                <div style={{ padding: '40px 10px', textAlign: 'center', color: '#848884', fontSize: 12, fontFamily: 'var(--ag-font-ui)' }}>
                  No games waiting
                </div>
              ) : (
                waitingGames.map((g: GameEntry, i: number) => (
                  <div 
                    key={g.id} 
                    className={`settings-card fade-in-up stagger-${Math.min(i + 1, 5)}`} 
                    style={{ padding: '12px 14px', marginBottom: 8, border: '1px solid #2C2D35' }}
                  >
                    <div style={{ 
                      fontSize: 13, 
                      color: '#FFF', 
                      marginBottom: 8, 
                      whiteSpace: 'nowrap', 
                      overflow: 'hidden', 
                      textOverflow: 'ellipsis', 
                      fontFamily: 'var(--ag-font-display)', 
                      fontWeight: 700 
                    }}>
                      {g.name}
                    </div>
                    <div style={{ display: 'flex', gap: 6 }}>
                      <button 
                        onClick={() => onRestore(g.id)} 
                        className="btn-primary-laser"
                        style={{ flex: 1, padding: '6px', fontSize: 10 }}
                      >
                        RESTORE
                      </button>
                      <button 
                        onClick={() => onIgnore(g.id)} 
                        className="btn-outline-laser"
                        style={{ flex: 1, padding: '6px', fontSize: 10, borderColor: '#2C2D35' }}
                      >
                        IGNORE
                      </button>
                    </div>
                  </div>
                ))
              )}
            </div>
            <div style={{ padding: '0 12px 12px 12px', flexShrink: 0 }}>
              <button 
                onClick={onRestoreIgnored} 
                className="btn-outline-laser" 
                style={{ width: '100%', padding: '9px', fontSize: 10.5, borderColor: '#2C2D35' }}
              >
                RESTORE ALL IGNORED
              </button>
            </div>
          </div>
        ) : (
          <div ref={sidebarScrollRef} className="fast-smooth-scroll" style={{ flex: 1, padding: '8px', display: 'flex', flexDirection: 'column' }}>
            {filtered.length === 0 ? (
              <div style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', textAlign: 'center', padding: '24px 16px' }}>
                <div style={{ fontSize: 13, fontWeight: 700, color: '#FFF', marginBottom: 6, fontFamily: 'var(--ag-font-display)', letterSpacing: '0.02em' }}>
                  {search ? 'No matches found' : 'No games detected'}
                </div>
                <div style={{ fontSize: 11, color: '#848884', fontFamily: 'var(--ag-font-ui)' }}>
                  {search ? 'Try a different search term' : 'Click RESCAN LIBRARY below'}
                </div>
              </div>
            ) : (
              filtered.map((g: GameEntry) => {
                const isSelected = selectedId === g.id;
                return (
                  <div 
                    key={g.id} 
                    className={`game-item-banner ${isSelected ? 'game-banner-selected' : ''}`}
                    onClick={() => onSelect(g)}
                  >
                    {/* Game Avatar Box (Clean solid smoke gray, flat, high quality) */}
                    <div 
                      className="game-avatar-box"
                      style={{ 
                        width: 44, 
                        height: 44, 
                        borderRadius: 8, 
                        background: '#0B0B0E', 
                        display: 'flex', 
                        alignItems: 'center', 
                        justifyContent: 'center', 
                        marginRight: 12, 
                        flexShrink: 0, 
                        boxShadow: 'none', 
                        overflow: 'hidden', 
                        border: '1px solid #2C2D35',
                        padding: 3,
                        boxSizing: 'border-box',
                        transition: 'all 0.18s ease',
                        position: 'relative',
                        zIndex: 2
                      }}
                    >
                      {g.iconBase64 ? (
                        <img 
                          src={g.iconBase64} 
                          style={{ 
                            width: '100%', 
                            height: '100%', 
                            objectFit: 'contain', 
                            imageRendering: 'auto',
                            borderRadius: 5
                          }} 
                          alt={g.name} 
                        />
                      ) : (
                        <span style={{ 
                          fontFamily: 'var(--ag-font-display)', 
                          fontWeight: 800, 
                          fontSize: 15, 
                          color: '#C0C0C8',
                          letterSpacing: '0.04em'
                        }}>
                          {g.name.substring(0, 2).toUpperCase()}
                        </span>
                      )}
                    </div>
                    
                    {/* Game Info - Big Title Only (All dx11, verified, active removed!) */}
                    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', position: 'relative', zIndex: 2 }}>
                      <span style={{ 
                        fontSize: 15.5, 
                        fontWeight: 700, 
                        color: '#FFFFFF', 
                        whiteSpace: 'nowrap', 
                        textOverflow: 'ellipsis', 
                        overflow: 'hidden', 
                        letterSpacing: '0.03em',
                        fontFamily: 'var(--ag-font-display)',
                        textTransform: 'uppercase'
                      }}>
                        {g.name}
                      </span>
                    </div>
                  </div>
                );
              })
            )}
          </div>
        )}
      </div>

      {/* Bottom Actions */}
      <div style={{ 
        padding: 14, 
        borderTop: '1px solid #26262B', 
        background: '#060608',
        display: 'flex',
        flexDirection: 'column',
        gap: 8
      }}>
        <button 
          onClick={() => setIsWaitListOpen(!isWaitListOpen)} 
          style={{ 
            width: '100%', 
            padding: '10px 14px', 
            fontSize: 11,
            display: 'flex', 
            alignItems: 'center', 
            justifyContent: 'center',
            background: isWaitListOpen ? '#1E2028' : '#14151B',
            border: isWaitListOpen ? '1px solid #4A4D5C' : '1px solid #2C2D35',
            color: '#FFFFFF',
            fontWeight: 800,
            fontFamily: 'var(--ag-font-display)',
            letterSpacing: '0.08em',
            borderRadius: 'var(--ag-radius-sm)',
            cursor: 'pointer',
            transition: 'all 0.2s ease',
            boxShadow: 'none'
          }}
          onMouseEnter={e => {
            e.currentTarget.style.background = '#1C1E26';
            e.currentTarget.style.borderColor = '#4A4D5C';
          }}
          onMouseLeave={e => {
            e.currentTarget.style.background = isWaitListOpen ? '#1E2028' : '#14151B';
            e.currentTarget.style.borderColor = isWaitListOpen ? '#4A4D5C' : '#2C2D35';
          }}
        >
          {isWaitListOpen ? 'BACK TO LIBRARY' : (
            <span style={{ display: 'inline-flex', alignItems: 'center', gap: 8, color: '#FFFFFF', fontWeight: 800 }}>
              <span style={{ color: '#FFFFFF', fontWeight: 800 }}>WAIT LIST</span>
              <span style={{ 
                fontSize: 13.5, 
                fontWeight: 800, 
                color: '#FF1E27', 
                fontFamily: 'var(--ag-font-mono)'
              }}>
                {waitingGames?.length || 0}
              </span>
            </span>
          )}
        </button>

        {!isWaitListOpen && (
          <button 
            onClick={handleRescan} 
            style={{ 
              width: '100%', 
              padding: '10px 14px', 
              fontSize: 11,
              background: '#14151B',
              border: '1px solid #2C2D35',
              color: '#FFFFFF',
              fontWeight: 800,
              fontFamily: 'var(--ag-font-display)',
              letterSpacing: '0.08em',
              borderRadius: 'var(--ag-radius-sm)',
              cursor: 'pointer',
              transition: 'all 0.2s ease',
              opacity: isScanning ? 0.6 : 1,
              boxShadow: 'none'
            }}
            onMouseEnter={e => {
              e.currentTarget.style.background = '#1C1E26';
              e.currentTarget.style.borderColor = '#4A4D5C';
            }}
            onMouseLeave={e => {
              e.currentTarget.style.background = '#14151B';
              e.currentTarget.style.borderColor = '#2C2D35';
            }}
          >
            {isScanning ? 'SCANNING...' : 'RESCAN LIBRARY'}
          </button>
        )}
      </div>
    </div>
  );
}
