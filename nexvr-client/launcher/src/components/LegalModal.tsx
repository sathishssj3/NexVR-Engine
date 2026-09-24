import { useState, useEffect } from 'react';

export interface LegalModalProps {
  isOpen: boolean;
  isFirstRun?: boolean;
  onAccept: () => void;
  onClose?: () => void;
}

export function LegalModal({
  isOpen,
  isFirstRun = false,
  onAccept,
  onClose
}: LegalModalProps) {
  const [agreedAntiCheat, setAgreedAntiCheat] = useState(false);
  const [agreedHealth, setAgreedHealth] = useState(false);

  useEffect(() => {
    if (!isOpen) {
      setAgreedAntiCheat(false);
      setAgreedHealth(false);
    }
  }, [isOpen]);

  useEffect(() => {
    if (!isOpen) return;
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape' && !isFirstRun && onClose) {
        onClose();
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [isOpen, isFirstRun, onClose]);

  if (!isOpen) return null;

  const canAccept = !isFirstRun || (agreedAntiCheat && agreedHealth);

  return (
    <div
      onClick={() => {
        if (!isFirstRun && onClose) onClose();
      }}
      style={{
        position: 'fixed',
        inset: 0,
        zIndex: 2000,
        background: 'rgba(0, 0, 0, 0.82)',
        backdropFilter: 'blur(20px)',
        WebkitBackdropFilter: 'blur(20px)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        padding: '24px 16px',
        animation: 'modalBackdropFade 0.2s cubic-bezier(0.16, 1, 0.3, 1) both'
      }}
    >
      <div
        onClick={e => e.stopPropagation()}
        style={{
          width: '100%',
          maxWidth: 620,
          maxHeight: '90vh',
          background: 'rgba(10, 10, 14, 0.98)',
          border: '1px solid #2C2D35',
          borderTop: '2px solid #CC0000',
          borderRadius: 10,
          boxShadow: '0 28px 70px rgba(0, 0, 0, 0.85), inset 0 1px 0 rgba(255, 255, 255, 0.08)',
          padding: '28px 32px',
          animation: 'modalContentScale 0.22s cubic-bezier(0.16, 1, 0.3, 1) both',
          display: 'flex',
          flexDirection: 'column',
          gap: 16
        }}
      >
        {/* Header */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
            <span
              style={{
                width: 4,
                height: 20,
                borderRadius: 2,
                background: '#CC0000',
                flexShrink: 0
              }}
            />
            <h2
              style={{
                margin: 0,
                fontSize: 16,
                fontWeight: 800,
                letterSpacing: '0.06em',
                fontFamily: 'var(--ag-font-display)',
                color: '#FFFFFF',
                textTransform: 'uppercase'
              }}
            >
              {isFirstRun ? 'CLOSED BETA TERMS & SAFETY ADVISORY' : 'TERMS, LEGAL & SAFETY DISCLAIMERS'}
            </h2>
          </div>
          <p
            style={{
              margin: '0 0 0 14px',
              fontSize: 11.5,
              color: 'var(--ag-accent)',
              fontFamily: 'var(--ag-font-mono)',
              fontWeight: 700,
              letterSpacing: '0.04em'
            }}
          >
            NexVR Engine v0.1.90-beta · Confidential Testing Guidelines
          </p>
        </div>

        {/* Scrollable Policy Content */}
        <div
          className="fast-smooth-scroll"
          style={{
            flex: 1,
            overflowY: 'auto',
            maxHeight: '52vh',
            display: 'flex',
            flexDirection: 'column',
            gap: 12,
            paddingRight: 6
          }}
        >
          {/* Card 1: Anti-Cheat & Ban Liability */}
          <div
            style={{
              background: 'rgba(255, 255, 255, 0.02)',
              border: '1px solid #282932',
              borderRadius: 6,
              padding: '14px 16px'
            }}
          >
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: 8,
                color: '#FF4D4D',
                fontFamily: 'var(--ag-font-display)',
                fontSize: 12,
                fontWeight: 800,
                letterSpacing: '0.04em',
                marginBottom: 6
              }}
            >
              <span>🛡️</span> 1. SINGLE-PLAYER ONLY & ZERO ANTI-CHEAT LIABILITY
            </div>
            <div style={{ fontSize: 12, lineHeight: 1.55, color: '#A0A3B1', fontFamily: 'var(--ag-font-ui)' }}>
              NexVR Engine is intended strictly for single-player, mod-friendly, and offline gameplay. Injecting memory detours into competitive multiplayer games protected by Anti-Cheat systems (such as Easy Anti-Cheat, BattlEye, Riot Vanguard, or Ricochet) is strictly prohibited. Although the launcher contains automatic tripwires that refuse injection when anti-cheat signatures are detected, the user assumes sole responsibility for complying with each game's End User License Agreement.
              <br /><br />
              <strong style={{ color: '#FFF' }}>
                Under no circumstances shall the developers of NexVR Engine be held liable for game account bans, suspensions, matchmaking restrictions, or loss of digital purchases.
              </strong>
            </div>
          </div>

          {/* Card 2: VR Health & Motion Sickness */}
          <div
            style={{
              background: 'rgba(255, 255, 255, 0.02)',
              border: '1px solid #282932',
              borderRadius: 6,
              padding: '14px 16px'
            }}
          >
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: 8,
                color: '#FFB340',
                fontFamily: 'var(--ag-font-display)',
                fontSize: 12,
                fontWeight: 800,
                letterSpacing: '0.04em',
                marginBottom: 6
              }}
            >
              <span>⚠️</span> 2. VR HEALTH, EPILEPSY & MOTION SICKNESS ADVISORY
            </div>
            <div style={{ fontSize: 12, lineHeight: 1.55, color: '#A0A3B1', fontFamily: 'var(--ag-font-ui)' }}>
              Stereoscopic 3D rendering and 6DOF head tracking can induce vestibular mismatch, motion sickness, disorientation, eye fatigue, or nausea. Users with a history of photosensitive seizures, epilepsy, or balance disorders should consult a physician before using VR injection software.
              <br /><br />
              Take regular 15-minute breaks every hour. If you experience dizziness, disorientation, or discomfort, immediately remove your headset and rest until symptoms fully subside.
            </div>
          </div>

          {/* Card 3: Trademark & Non-Affiliation */}
          <div
            style={{
              background: 'rgba(255, 255, 255, 0.02)',
              border: '1px solid #282932',
              borderRadius: 6,
              padding: '14px 16px'
            }}
          >
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: 8,
                color: '#4DA6FF',
                fontFamily: 'var(--ag-font-display)',
                fontSize: 12,
                fontWeight: 800,
                letterSpacing: '0.04em',
                marginBottom: 6
              }}
            >
              <span>⚖️</span> 3. TRADEMARKS & NON-AFFILIATION DISCLAIMER
            </div>
            <div style={{ fontSize: 12, lineHeight: 1.55, color: '#A0A3B1', fontFamily: 'var(--ag-font-ui)' }}>
              NexVR Engine is an independent, community-driven spatial modding runtime. All trademarks, registered game titles, and publisher logos (including but not limited to <em>Sekiro: Shadows Die Twice, Elden Ring, Mortal Shell, Cyberpunk 2077, Hogwarts Legacy, FromSoftware, Bandai Namco, Epic Games, Unreal Engine, Valve, SteamVR, Meta Quest</em>) remain the exclusive property of their respective copyright holders. Mention of any game or trademark does not imply affiliation, sponsorship, or endorsement.
            </div>
          </div>

          {/* Card 4: As-Is Beta Warranty & Save Safety */}
          <div
            style={{
              background: 'rgba(255, 255, 255, 0.02)',
              border: '1px solid #282932',
              borderRadius: 6,
              padding: '14px 16px'
            }}
          >
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: 8,
                color: '#30D158',
                fontFamily: 'var(--ag-font-display)',
                fontSize: 12,
                fontWeight: 800,
                letterSpacing: '0.04em',
                marginBottom: 6
              }}
            >
              <span>🧪</span> 4. "AS-IS" BETA SOFTWARE & SAVE DATA BACKUPS
            </div>
            <div style={{ fontSize: 12, lineHeight: 1.55, color: '#A0A3B1', fontFamily: 'var(--ag-font-ui)' }}>
              This software is provided "AS-IS" without warranty of any kind. As an active Closed Beta release, graphics driver anomalies or application crashes may occur. Users are advised to back up their game save files before launching with VR injection active.
            </div>
          </div>
        </div>

        {/* Checkboxes (Required on First-Run) */}
        {isFirstRun && (
          <div
            style={{
              display: 'flex',
              flexDirection: 'column',
              gap: 8,
              padding: '12px 14px',
              background: 'rgba(204, 0, 0, 0.05)',
              border: '1px solid rgba(204, 0, 0, 0.25)',
              borderRadius: 6
            }}
          >
            <label
              style={{
                display: 'flex',
                alignItems: 'flex-start',
                gap: 10,
                fontSize: 11.5,
                color: '#E0E2EC',
                fontFamily: 'var(--ag-font-ui)',
                cursor: 'pointer',
                lineHeight: 1.45
              }}
            >
              <input
                type="checkbox"
                checked={agreedAntiCheat}
                onChange={e => setAgreedAntiCheat(e.target.checked)}
                style={{
                  marginTop: 2,
                  accentColor: '#CC0000',
                  cursor: 'pointer'
                }}
              />
              <span>
                I agree to use NexVR only with single-player/offline titles and acknowledge developers bear zero liability for online account bans.
              </span>
            </label>

            <label
              style={{
                display: 'flex',
                alignItems: 'flex-start',
                gap: 10,
                fontSize: 11.5,
                color: '#E0E2EC',
                fontFamily: 'var(--ag-font-ui)',
                cursor: 'pointer',
                lineHeight: 1.45
              }}
            >
              <input
                type="checkbox"
                checked={agreedHealth}
                onChange={e => setAgreedHealth(e.target.checked)}
                style={{
                  marginTop: 2,
                  accentColor: '#CC0000',
                  cursor: 'pointer'
                }}
              />
              <span>
                I have read the VR health & motion safety guidelines and agree to take necessary precautions during testing.
              </span>
            </label>
          </div>
        )}

        {/* Footer Actions */}
        <div
          style={{
            display: 'flex',
            justifyContent: 'flex-end',
            gap: 10,
            paddingTop: 12,
            borderTop: '1px solid rgba(255, 255, 255, 0.06)'
          }}
        >
          {isFirstRun ? (
            <button
              type="button"
              disabled={!canAccept}
              onClick={onAccept}
              style={{
                padding: '11px 24px',
                borderRadius: 'var(--ag-radius-sm)',
                color: '#FFFFFF',
                cursor: canAccept ? 'pointer' : 'not-allowed',
                fontFamily: 'var(--ag-font-display)',
                fontSize: 12,
                letterSpacing: '0.08em',
                fontWeight: 800,
                border: canAccept ? '1px solid #FF4D4D' : '1px solid #3A3B45',
                background: canAccept
                  ? 'linear-gradient(135deg, #FF1A1A 0%, #CC0000 55%, #990000 100%)'
                  : 'rgba(255, 255, 255, 0.05)',
                opacity: canAccept ? 1 : 0.45,
                boxShadow: canAccept ? '0 4px 18px rgba(204, 0, 0, 0.4)' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              ACCEPT & ENTER CLOSED BETA
            </button>
          ) : (
            <button
              type="button"
              onClick={onClose}
              style={{
                padding: '9px 20px',
                borderRadius: 'var(--ag-radius-sm)',
                color: 'var(--ag-text-primary)',
                cursor: 'pointer',
                fontFamily: 'var(--ag-font-display)',
                fontSize: 11.5,
                letterSpacing: '0.06em',
                fontWeight: 700,
                border: '1px solid #383A44',
                background: 'rgba(255, 255, 255, 0.04)',
                transition: 'all 0.15s ease'
              }}
            >
              CLOSE
            </button>
          )}
        </div>
      </div>
    </div>
  );
}
