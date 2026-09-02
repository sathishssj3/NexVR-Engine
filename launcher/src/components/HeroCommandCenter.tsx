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
    <div className="fade-in-up" style={{ display: 'flex', flexDirection: 'column', gap: 24, paddingBottom: 20 }}>
      {/* Hero Welcome Header */}
      <div
        style={{
          position: 'relative',
          padding: '32px 36px',
          background: 'linear-gradient(135deg, rgba(20, 26, 40, 0.85) 0%, rgba(10, 14, 22, 0.75) 100%)',
          borderRadius: 'var(--ag-radius-lg)',
          border: '1px solid rgba(0, 240, 255, 0.2)',
          boxShadow: '0 20px 50px rgba(0,0,0,0.5), inset 0 1px 2px rgba(255,255,255,0.08), 0 0 30px rgba(0,240,255,0.08)',
          overflow: 'hidden',
        }}
      >
        {/* Glow ambient shapes */}
        <div style={{ position: 'absolute', top: '-40%', right: '-15%', width: '60%', height: '160%', background: 'radial-gradient(ellipse at center, rgba(0,240,255,0.12), transparent 70%)', pointerEvents: 'none' }} />
        <div style={{ position: 'absolute', bottom: '-40%', left: '-15%', width: '50%', height: '140%', background: 'radial-gradient(ellipse at center, rgba(112,0,255,0.1), transparent 70%)', pointerEvents: 'none' }} />

        <div style={{ position: 'relative', zIndex: 1 }}>
          <div style={{ display: 'inline-flex', alignItems: 'center', gap: 8, padding: '4px 12px', borderRadius: 20, background: 'rgba(0,240,255,0.08)', border: '1px solid rgba(0,240,255,0.25)', marginBottom: 14 }}>
            <span style={{ width: 6, height: 6, borderRadius: '50%', background: vrStatus.connected ? 'var(--ag-accent-success)' : 'var(--ag-accent-warn)', boxShadow: vrStatus.connected ? '0 0 8px var(--ag-accent-success)' : '0 0 8px var(--ag-accent-warn)' }} />
            <span style={{ fontSize: 11, fontFamily: 'var(--ag-font-mono)', color: vrStatus.connected ? 'var(--ag-accent-success)' : 'var(--ag-accent-warn)', letterSpacing: '1px', fontWeight: 600 }}>
              {vrStatus.connected ? `SYSTEM READY // ${vrStatus.headset.toUpperCase()}` : 'SYSTEM STANDBY // VR RUNTIME READY'}
            </span>
          </div>

          <h1 style={{ margin: '0 0 8px 0', fontSize: 32, fontWeight: 800, fontFamily: 'var(--ag-font-display)', letterSpacing: '1px', color: '#fff', textShadow: '0 2px 20px rgba(0,0,0,0.8), 0 0 30px rgba(0,240,255,0.2)' }}>
            NEXV<span style={{ color: 'var(--ag-accent)' }}>R</span> COMMAND CENTER
          </h1>
          <p style={{ margin: 0, fontSize: 13, color: 'var(--ag-text-muted)', maxWidth: 640, lineHeight: 1.6, fontFamily: 'var(--ag-font-mono)' }}>
            Universal Direct-to-Eye XR Pipeline with DirectML neural depth reconstruction and sub-millisecond input passthrough. Select any game below to initialize your VR session.
          </p>
        </div>
      </div>

      {/* System Telemetry HUD (3 Cards) */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 16 }}>
        {/* Card 1: VR Runtime */}
        <div
          className="glass-card"
          style={{
            padding: '20px 22px',
            borderRadius: 'var(--ag-radius-md)',
            border: '1px solid rgba(255,255,255,0.08)',
            background: 'linear-gradient(135deg, rgba(16, 22, 34, 0.7) 0%, rgba(8, 12, 18, 0.85) 100%)',
            boxShadow: '0 10px 30px rgba(0,0,0,0.3)',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 12 }}>
            <span style={{ fontSize: 10, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-text-muted)', letterSpacing: '1.5px' }}>VR HARDWARE</span>
            <span style={{ fontSize: 18 }}>🥽</span>
          </div>
          <div style={{ fontSize: 16, fontWeight: 700, color: '#fff', marginBottom: 4, fontFamily: 'var(--ag-font-display)' }}>
            {vrStatus.headset}
          </div>
          <div style={{ fontSize: 11, color: 'var(--ag-accent)', fontFamily: 'var(--ag-font-mono)' }}>
            {vrStatus.refreshRate} Hz Low-Persistence
          </div>
          <div style={{ marginTop: 12, paddingTop: 10, borderTop: '1px solid rgba(255,255,255,0.06)', fontSize: 10, color: 'var(--ag-text-muted)', fontFamily: 'var(--ag-font-mono)' }}>
            Runtime: {vrStatus.runtime}
          </div>
        </div>

        {/* Card 2: Neural XR Engine */}
        <div
          className="glass-card"
          style={{
            padding: '20px 22px',
            borderRadius: 'var(--ag-radius-md)',
            border: '1px solid rgba(255,255,255,0.08)',
            background: 'linear-gradient(135deg, rgba(16, 22, 34, 0.7) 0%, rgba(8, 12, 18, 0.85) 100%)',
            boxShadow: '0 10px 30px rgba(0,0,0,0.3)',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 12 }}>
            <span style={{ fontSize: 10, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-text-muted)', letterSpacing: '1.5px' }}>NEURAL XR ENGINE</span>
            <span style={{ fontSize: 18 }}>⚡</span>
          </div>
          <div style={{ fontSize: 16, fontWeight: 700, color: '#fff', marginBottom: 4, fontFamily: 'var(--ag-font-display)' }}>
            DirectML Hardware AI
          </div>
          <div style={{ fontSize: 11, color: 'var(--ag-accent-success)', fontFamily: 'var(--ag-font-mono)' }}>
            Stereo Reprojection Active
          </div>
          <div style={{ marginTop: 12, paddingTop: 10, borderTop: '1px solid rgba(255,255,255,0.06)', fontSize: 10, color: 'var(--ag-text-muted)', fontFamily: 'var(--ag-font-mono)' }}>
            APIs: DX11 // DX12 // Vulkan
          </div>
        </div>

        {/* Card 3: Library & Security */}
        <div
          className="glass-card"
          style={{
            padding: '20px 22px',
            borderRadius: 'var(--ag-radius-md)',
            border: '1px solid rgba(255,255,255,0.08)',
            background: 'linear-gradient(135deg, rgba(16, 22, 34, 0.7) 0%, rgba(8, 12, 18, 0.85) 100%)',
            boxShadow: '0 10px 30px rgba(0,0,0,0.3)',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 12 }}>
            <span style={{ fontSize: 10, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-text-muted)', letterSpacing: '1.5px' }}>SAFETY & LIBRARY</span>
            <span style={{ fontSize: 18 }}>🛡️</span>
          </div>
          <div style={{ fontSize: 16, fontWeight: 700, color: '#fff', marginBottom: 4, fontFamily: 'var(--ag-font-display)' }}>
            {games.length} Titles Ready
          </div>
          <div style={{ fontSize: 11, color: 'var(--ag-accent-success)', fontFamily: 'var(--ag-font-mono)' }}>
            Anti-Cheat Guard Active
          </div>
          <div style={{ marginTop: 12, paddingTop: 10, borderTop: '1px solid rgba(255,255,255,0.06)', fontSize: 10, color: 'var(--ag-text-muted)', fontFamily: 'var(--ag-font-mono)' }}>
            1-Click Flat Screen Restore
          </div>
        </div>
      </div>

      {/* Quick Launch Showcase */}
      <div>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 14 }}>
          <h2 style={{ margin: 0, fontSize: 14, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-text-primary)', letterSpacing: '1.5px' }}>
            DETECTED GAMES ({games.length})
          </h2>
          <div style={{ display: 'flex', gap: 10 }}>
            <button
              onClick={onRescan}
              className="btn-glow"
              style={{
                padding: '6px 14px', borderRadius: 6,
                background: 'rgba(255,255,255,0.05)', border: '1px solid var(--ag-border)',
                color: 'var(--ag-text-muted)', fontSize: 11, cursor: 'pointer', fontFamily: 'var(--ag-font-mono)'
              }}
            >
              ⟳ RESCAN
            </button>
            <button
              onClick={onAddCustom}
              className="btn-glow"
              style={{
                padding: '6px 14px', borderRadius: 6,
                background: 'rgba(0,240,255,0.1)', border: '1px solid var(--ag-accent)',
                color: 'var(--ag-accent)', fontSize: 11, cursor: 'pointer', fontFamily: 'var(--ag-font-mono)'
              }}
            >
              + ADD GAME
            </button>
          </div>
        </div>

        {games.length === 0 ? (
          <div className="glass-card" style={{ padding: '40px 20px', textAlign: 'center', borderRadius: 'var(--ag-radius-md)' }}>
            <div style={{ fontSize: 36, marginBottom: 12 }}>🎮</div>
            <div style={{ fontSize: 16, color: '#fff', fontWeight: 600, marginBottom: 6 }}>No games detected yet</div>
            <div style={{ fontSize: 12, color: 'var(--ag-text-muted)', marginBottom: 20 }}>
              Click "Add Game" above to select any custom game .exe, or click "Rescan" to detect Steam & Epic Games.
            </div>
            <button
              onClick={onAddCustom}
              className="btn-glow pulse-btn"
              style={{
                padding: '10px 24px', borderRadius: 8,
                background: 'linear-gradient(135deg, rgba(0,240,255,0.2), rgba(0,240,255,0.05))',
                border: '1px solid var(--ag-accent)', color: 'var(--ag-accent)',
                fontSize: 12, cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', fontWeight: 700
              }}
            >
              BROWSE GAME EXECUTABLE (.EXE)
            </button>
          </div>
        ) : (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: 14 }}>
            {games.map(game => (
              <div
                key={game.id}
                onClick={() => onSelectGame(game)}
                className="glass-card"
                style={{
                  padding: '16px 18px',
                  borderRadius: 12,
                  border: '1px solid rgba(255,255,255,0.08)',
                  background: 'linear-gradient(135deg, rgba(18, 24, 36, 0.75) 0%, rgba(10, 14, 22, 0.9) 100%)',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: 16,
                  transition: 'all 0.3s ease',
                  boxShadow: '0 8px 24px rgba(0,0,0,0.3)',
                }}
                onMouseEnter={e => {
                  e.currentTarget.style.borderColor = 'rgba(0, 240, 255, 0.4)';
                  e.currentTarget.style.transform = 'translateY(-2px)';
                  e.currentTarget.style.boxShadow = '0 12px 30px rgba(0,240,255,0.15)';
                }}
                onMouseLeave={e => {
                  e.currentTarget.style.borderColor = 'rgba(255,255,255,0.08)';
                  e.currentTarget.style.transform = 'translateY(0)';
                  e.currentTarget.style.boxShadow = '0 8px 24px rgba(0,0,0,0.3)';
                }}
              >
                {/* Game Icon with perfect squircle radius */}
                <div
                  style={{
                    width: 54,
                    height: 54,
                    borderRadius: 12,
                    background: 'linear-gradient(135deg, rgba(0,240,255,0.15), rgba(15,20,30,0.9))',
                    border: '1px solid rgba(0,240,255,0.3)',
                    boxShadow: '0 4px 12px rgba(0,0,0,0.5)',
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
                    <span style={{ fontFamily: 'var(--ag-font-display)', fontWeight: 800, fontSize: 18, color: 'var(--ag-accent)' }}>
                      {game.name.substring(0, 2).toUpperCase()}
                    </span>
                  )}
                </div>

                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{ fontSize: 15, fontWeight: 700, color: '#fff', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis', fontFamily: 'var(--ag-font-display)' }}>
                    {game.name}
                  </div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginTop: 4 }}>
                    <span
                      style={{
                        fontSize: 9,
                        fontFamily: 'var(--ag-font-mono)',
                        padding: '1px 6px',
                        borderRadius: 4,
                        fontWeight: 700,
                        background: game.api === 'DX12' ? 'rgba(0, 230, 118, 0.15)' : 'rgba(0, 240, 255, 0.15)',
                        color: game.api === 'DX12' ? 'var(--ag-accent-success)' : 'var(--ag-accent)',
                        border: game.api === 'DX12' ? '1px solid rgba(0, 230, 118, 0.3)' : '1px solid rgba(0, 240, 255, 0.3)',
                      }}
                    >
                      {game.api}
                    </span>
                    {game.compat === 'verified' && (
                      <span style={{ fontSize: 9, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-accent-success)', fontWeight: 600 }}>
                        ✓ VERIFIED
                      </span>
                    )}
                  </div>
                </div>

                <span style={{ color: 'var(--ag-accent)', fontSize: 18, opacity: 0.8 }}>›</span>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Getting Started Guide */}
      <div
        style={{
          padding: '20px 24px',
          borderRadius: 'var(--ag-radius-md)',
          background: 'rgba(5, 8, 14, 0.5)',
          border: '1px solid rgba(255,255,255,0.05)',
        }}
      >
        <div style={{ fontSize: 11, fontFamily: 'var(--ag-font-mono)', color: 'var(--ag-text-muted)', letterSpacing: '1.5px', marginBottom: 12 }}>
          QUICK SETUP GUIDE
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 16 }}>
          <div style={{ display: 'flex', gap: 12 }}>
            <div style={{ width: 24, height: 24, borderRadius: '50%', background: 'rgba(0,240,255,0.15)', color: 'var(--ag-accent)', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: 12, fontWeight: 700, flexShrink: 0 }}>
              1
            </div>
            <div style={{ fontSize: 12, color: 'var(--ag-text-muted)', lineHeight: 1.5 }}>
              <strong style={{ color: '#fff' }}>Power on VR Headset</strong>
              <div>Ensure SteamVR, Quest Link, or Virtual Desktop is running.</div>
            </div>
          </div>

          <div style={{ display: 'flex', gap: 12 }}>
            <div style={{ width: 24, height: 24, borderRadius: '50%', background: 'rgba(0,240,255,0.15)', color: 'var(--ag-accent)', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: 12, fontWeight: 700, flexShrink: 0 }}>
              2
            </div>
            <div style={{ fontSize: 12, color: 'var(--ag-text-muted)', lineHeight: 1.5 }}>
              <strong style={{ color: '#fff' }}>Select Your Game</strong>
              <div>Pick a game from the sidebar or dashboard to configure VR settings.</div>
            </div>
          </div>

          <div style={{ display: 'flex', gap: 12 }}>
            <div style={{ width: 24, height: 24, borderRadius: '50%', background: 'rgba(0,240,255,0.15)', color: 'var(--ag-accent)', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: 12, fontWeight: 700, flexShrink: 0 }}>
              3
            </div>
            <div style={{ fontSize: 12, color: 'var(--ag-text-muted)', lineHeight: 1.5 }}>
              <strong style={{ color: '#fff' }}>Initialize Injection</strong>
              <div>Click the glowing injection button to launch into full stereoscopic 3D!</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
