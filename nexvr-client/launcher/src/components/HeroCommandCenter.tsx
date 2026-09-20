import type { GameEntry, VRStatus } from '../types';

interface HeroCommandCenterProps {
  games: GameEntry[];
  vrStatus: VRStatus;
  onSelectGame: (game: GameEntry) => void;
  onRescan: () => void;
  onAddCustom: () => void;
}

export function HeroCommandCenter({
  games,
  vrStatus: _vrStatus,
  onSelectGame,
  onRescan: _onRescan,
  onAddCustom,
}: HeroCommandCenterProps) {
  return (
    <div 
      className="settings-tab-enter" 
      style={{ 
        display: 'flex', 
        flexDirection: 'column', 
        minHeight: '100%',
        width: '100%',
        boxSizing: 'border-box'
      }}
    >
      {games.length === 0 ? (
        <div 
          className="settings-card"
          style={{
            flex: 1,
            minHeight: 380,
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            textAlign: 'center',
            border: '1px solid #2C2D35',
            background: 'linear-gradient(180deg, rgba(255, 255, 255, 0.015) 0%, rgba(255, 255, 255, 0.004) 100%), #07070A',
            borderRadius: 8,
            padding: '48px 24px'
          }}
        >
          <div style={{
            width: 48,
            height: 48,
            borderRadius: 10,
            background: '#0B0B0E',
            border: '1px solid #2C2D35',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            marginBottom: 16,
            color: 'var(--ag-accent)',
            fontSize: 20
          }}>
            ◈
          </div>
          <div style={{
            fontSize: 16,
            fontWeight: 800,
            color: '#FFF',
            fontFamily: 'var(--ag-font-display)',
            letterSpacing: '0.06em',
            marginBottom: 6,
          }}>
            NO GAMES DETECTED
          </div>
          <div style={{
            fontSize: 12.5,
            color: '#848884',
            fontFamily: 'var(--ag-font-ui)',
            marginBottom: 24,
            maxWidth: 380,
            lineHeight: 1.5,
          }}>
            Use the + ADD GAME button in the sidebar or rescan to automatically discover installed executables.
          </div>
          <button
            onClick={onAddCustom}
            className="btn-primary-laser"
            style={{
              padding: '9px 24px',
              fontSize: 11,
            }}
          >
            + ADD GAME
          </button>
        </div>
      ) : (
        <div 
          style={{ 
            display: 'grid', 
            gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))', 
            gap: 14, 
            width: '100%' 
          }}
        >
          {games.map(game => (
            <div
              key={game.id}
              onClick={() => onSelectGame(game)}
              className="settings-card"
              style={{
                padding: '18px 20px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: 16,
                border: '1px solid #2C2D35',
                background: 'linear-gradient(180deg, rgba(255, 255, 255, 0.02) 0%, rgba(255, 255, 255, 0.005) 100%), #07070A',
                borderRadius: 8,
                transition: 'all 0.18s ease',
              }}
              onMouseEnter={e => {
                e.currentTarget.style.borderColor = '#4A4D5C';
                e.currentTarget.style.transform = 'translateY(-2px)';
                e.currentTarget.style.boxShadow = '0 8px 24px rgba(0, 0, 0, 0.6)';
              }}
              onMouseLeave={e => {
                e.currentTarget.style.borderColor = '#2C2D35';
                e.currentTarget.style.transform = 'translateY(0)';
                e.currentTarget.style.boxShadow = 'none';
              }}
            >
              {/* Game Icon (High quality, no broken pixels, solid smoke gray border) */}
              <div
                style={{
                  width: 50,
                  height: 50,
                  borderRadius: 10,
                  background: '#0B0B0E',
                  border: '1px solid #2C2D35',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  flexShrink: 0,
                  overflow: 'hidden',
                  padding: 3,
                  boxSizing: 'border-box'
                }}
              >
                {game.iconBase64 ? (
                  <img
                    src={game.iconBase64}
                    alt={game.name}
                    style={{
                      width: '100%',
                      height: '100%',
                      objectFit: 'contain',
                      imageRendering: 'auto',
                      borderRadius: 6
                    }}
                  />
                ) : (
                  <span style={{ fontFamily: 'var(--ag-font-display)', fontWeight: 800, fontSize: 16, color: '#C0C0C8', letterSpacing: '0.04em' }}>
                    {game.name.substring(0, 2).toUpperCase()}
                  </span>
                )}
              </div>

              {/* Game Information */}
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{
                  fontSize: 15,
                  fontWeight: 800,
                  color: '#FFFFFF',
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                  fontFamily: 'var(--ag-font-display)',
                  letterSpacing: '0.02em',
                  marginBottom: 6,
                }}>
                  {game.name}
                </div>
                
                <div style={{ display: 'flex', alignItems: 'center', gap: 6, flexWrap: 'wrap' }}>
                  <span style={{
                    fontSize: 9.5,
                    padding: '2px 6px',
                    borderRadius: 3,
                    fontWeight: 700,
                    background: game.api === 'DX12' ? 'rgba(48, 209, 88, 0.12)' : 'rgba(204, 0, 0, 0.12)',
                    color: game.api === 'DX12' ? 'var(--ag-accent-success)' : 'var(--ag-accent)',
                    border: '1px solid #2C2D35',
                    fontFamily: 'var(--ag-font-mono)',
                    letterSpacing: '0.05em',
                  }}>
                    {game.api}
                  </span>

                  {game.compat === 'verified' && (
                    <span style={{
                      fontSize: 9.5,
                      padding: '2px 6px',
                      borderRadius: 3,
                      fontFamily: 'var(--ag-font-mono)',
                      color: 'var(--ag-accent-success)',
                      fontWeight: 700,
                      background: 'rgba(48, 209, 88, 0.12)',
                      border: '1px solid #2C2D35',
                      letterSpacing: '0.05em',
                    }}>
                      VERIFIED 6DOF
                    </span>
                  )}

                  {game.hasInjector && (
                    <span style={{
                      fontSize: 9,
                      padding: '2px 5px',
                      borderRadius: 3,
                      background: 'rgba(48, 209, 88, 0.12)',
                      color: 'var(--ag-accent-success)',
                      fontFamily: 'var(--ag-font-mono)',
                      fontWeight: 700,
                      border: '1px solid #2C2D35'
                    }}>
                      MOD ACTIVE
                    </span>
                  )}

                  {game.hasAntiCheat && (
                    <span style={{
                      fontSize: 9,
                      padding: '2px 5px',
                      borderRadius: 3,
                      background: 'rgba(204, 0, 0, 0.12)',
                      color: 'var(--ag-accent)',
                      fontFamily: 'var(--ag-font-mono)',
                      fontWeight: 700,
                      border: '1px solid #2C2D35'
                    }}>
                      ANTI-CHEAT
                    </span>
                  )}
                </div>
              </div>

              {/* Right Arrow */}
              <div style={{ color: '#6A6D7C', fontSize: 13, fontFamily: 'var(--ag-font-mono)' }}>
                →
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

