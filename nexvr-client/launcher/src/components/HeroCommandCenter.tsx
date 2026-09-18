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
  vrStatus,
  onSelectGame,
  onRescan,
  onAddCustom,
}: HeroCommandCenterProps) {
  return (
    <div className="fade-in-up" style={{ display: 'flex', flexDirection: 'column', gap: 0, paddingBottom: 20, height: '100%' }}>

      {/* Compact system status row */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '14px 0',
        marginBottom: 20,
        borderBottom: '1px solid rgba(255, 255, 255, 0.06)',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 16 }}>
          <span style={{
            fontSize: 11,
            fontFamily: 'var(--ag-font-mono)',
            color: 'var(--ag-text-muted)',
            letterSpacing: '1.5px',
          }}>
            STATUS
          </span>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <span style={{
              width: 6, height: 6, borderRadius: '50%',
              background: vrStatus.connected ? 'var(--ag-accent-success)' : 'var(--ag-text-dim)',
            }} />
            <span style={{
              fontSize: 12,
              fontFamily: 'var(--ag-font-display)',
              color: vrStatus.connected ? '#FFF' : 'var(--ag-text-muted)',
              fontWeight: 600,
            }}>
              {vrStatus.connected ? vrStatus.headset : 'VR Disconnected'}
            </span>
            {vrStatus.connected && (
              <span style={{
                padding: '1px 5px',
                borderRadius: 3,
                fontSize: 9,
                fontWeight: 800,
                fontFamily: 'var(--ag-font-mono)',
                letterSpacing: '0.05em',
                background: 'rgba(48, 209, 88, 0.15)',
                border: '1px solid rgba(48, 209, 88, 0.3)',
                color: 'var(--ag-accent-success)'
              }}>
                CONNECTED
              </span>
            )}
          </div>
          {vrStatus.connected && (
            <span style={{
              fontSize: 10,
              fontFamily: 'var(--ag-font-mono)',
              color: '#848884',
              fontWeight: 600,
            }}>
              {vrStatus.refreshRate} Hz  //  {vrStatus.runtime}
            </span>
          )}
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
          <span style={{
            fontSize: 10.5,
            fontFamily: 'var(--ag-font-mono)',
            color: '#848884',
            letterSpacing: '0.06em',
            fontWeight: 700,
          }}>
            {games.length} {games.length === 1 ? 'TITLE' : 'TITLES'}
          </span>
          <span style={{ width: 1, height: 14, background: 'rgba(255, 255, 255, 0.15)' }} />
          <span style={{
            fontSize: 10.5,
            fontFamily: 'var(--ag-font-mono)',
            color: '#848884',
            letterSpacing: '0.06em',
            fontWeight: 700,
          }}>
            DX11 / DX12 / VK
          </span>
        </div>
      </div>

      {/* Games section header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 16 }}>
        <span style={{
          fontSize: 11,
          fontWeight: 700,
          letterSpacing: '1.5px',
          fontFamily: 'var(--ag-font-display)',
          color: 'var(--ag-text-muted)',
        }}>
          LIBRARY
        </span>
        <div style={{ display: 'flex', gap: 8 }}>
          <button
            onClick={onRescan}
            className="btn-outline-laser"
            style={{ padding: '5px 12px', fontSize: 10 }}
          >
            RESCAN
          </button>
          <button
            onClick={onAddCustom}
            className="btn-primary-laser"
            style={{ padding: '5px 12px', fontSize: 10 }}
          >
            ADD GAME
          </button>
        </div>
      </div>

      {/* Empty state or game grid */}
      {games.length === 0 ? (
        <div style={{
          flex: 1,
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          minHeight: 280,
        }}>
          <div style={{
            fontSize: 14,
            fontWeight: 700,
            color: 'var(--ag-text-muted)',
            fontFamily: 'var(--ag-font-display)',
            letterSpacing: '0.06em',
            marginBottom: 8,
          }}>
            No games detected
          </div>
          <div style={{
            fontSize: 12,
            color: 'var(--ag-text-dim)',
            fontFamily: 'var(--ag-font-ui)',
            marginBottom: 24,
            textAlign: 'center',
            lineHeight: 1.6,
          }}>
            Add a game executable or rescan your library
          </div>
          <button
            onClick={onAddCustom}
            style={{
              padding: '9px 22px',
              borderRadius: 'var(--ag-radius-sm)',
              background: 'transparent',
              border: '1px solid var(--ag-accent)',
              color: 'var(--ag-accent)',
              fontSize: 11,
              cursor: 'pointer',
              fontFamily: 'var(--ag-font-display)',
              fontWeight: 700,
              letterSpacing: '0.08em',
              transition: 'all 0.2s ease',
            }}
            onMouseEnter={e => {
              e.currentTarget.style.background = 'rgba(204, 0, 0, 0.08)';
            }}
            onMouseLeave={e => {
              e.currentTarget.style.background = 'transparent';
            }}
          >
            BROWSE .EXE
          </button>
        </div>
      ) : (
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: 12 }}>
          {games.map(game => (
            <div
              key={game.id}
              onClick={() => onSelectGame(game)}
              className="glass-card"
              style={{
                padding: '14px 16px',
                borderRadius: 'var(--ag-radius-md)',
                border: '1px solid var(--ag-border)',
                background: 'var(--ag-bg-card)',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: 14,
                transition: 'all 0.25s var(--ag-transition)',
                boxShadow: '0 4px 16px rgba(0, 0, 0, 0.3)',
              }}
              onMouseEnter={e => {
                e.currentTarget.style.borderColor = 'rgba(204, 0, 0, 0.35)';
                e.currentTarget.style.transform = 'translateY(-1px)';
              }}
              onMouseLeave={e => {
                e.currentTarget.style.borderColor = 'var(--ag-border)';
                e.currentTarget.style.transform = 'translateY(0)';
              }}
            >
              {/* Game Icon */}
              <div
                style={{
                  width: 44,
                  height: 44,
                  borderRadius: 8,
                  background: 'rgba(204, 0, 0, 0.06)',
                  border: '1px solid rgba(255, 255, 255, 0.06)',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  flexShrink: 0,
                  overflow: 'hidden',
                }}
              >
                {game.iconBase64 ? (
                  <img
                    src={game.iconBase64}
                    alt={game.name}
                    style={{
                      width: '100%',
                      height: '100%',
                      objectFit: 'cover',
                      imageRendering: '-webkit-optimize-contrast',
                    }}
                  />
                ) : (
                  <span style={{ fontFamily: 'var(--ag-font-display)', fontWeight: 700, fontSize: 15, color: 'var(--ag-text-muted)' }}>
                    {game.name.substring(0, 2).toUpperCase()}
                  </span>
                )}
              </div>

              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{
                  fontSize: 13,
                  fontWeight: 700,
                  color: '#FFF',
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                  fontFamily: 'var(--ag-font-display)',
                  letterSpacing: '0.02em',
                }}>
                  {game.name}
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginTop: 3 }}>
                  <span style={{
                    fontSize: 9,
                    fontFamily: 'var(--ag-font-mono)',
                    color: 'var(--ag-text-dim)',
                  }}>
                    {game.api}
                  </span>
                  {game.compat === 'verified' && (
                    <span style={{ fontSize: 9, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent-success)' }}>
                      VERIFIED
                    </span>
                  )}
                </div>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
