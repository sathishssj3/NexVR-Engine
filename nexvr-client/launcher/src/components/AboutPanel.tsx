import { useState } from 'react';
import mainLogo from '../assets/logo.png';

export function AboutPanel({ version }: { version?: string }) {
  const [sendingLogs, setSendingLogs] = useState(false);
  const [sendResult, setSendResult] = useState<'idle' | 'success' | 'error'>('idle');

  const handleLink = (url: string) => {
    if (window.ag && window.ag.shell) {
      window.ag.shell.openExternal(url);
    }
  };

  const handleUploadLogs = async () => {
    if (sendingLogs) return;
    setSendingLogs(true);
    try {
      const res = await window.ag?.telemetry?.sendReport({ userNote: 'Manual report from About Panel' });
      if (res?.success) {
        setSendResult('success');
        setTimeout(() => setSendResult('idle'), 4000);
      } else {
        setSendResult('error');
        setTimeout(() => setSendResult('idle'), 4000);
      }
    } catch {
      setSendResult('error');
      setTimeout(() => setSendResult('idle'), 4000);
    } finally {
      setSendingLogs(false);
    }
  };

  const techStack = [
    { name: 'C++20', color: 'var(--ag-accent)' },
    { name: 'DirectX 11 / 12', color: '#00ff88' },
    { name: 'Vulkan', color: '#ff4444' },
    { name: 'DirectML', color: '#a855f7' },
    { name: 'OpenXR', color: '#38bdf8' },
    { name: 'React 19', color: '#61dafb' },
    { name: 'TypeScript', color: '#3178c6' },
    { name: 'Electron', color: '#47848f' },
  ];

  return (
    <div style={{ flex: 1, padding: '40px', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', overflowY: 'auto', background: 'radial-gradient(circle at 50% 30%, rgba(0, 240, 255, 0.05), transparent 70%)' }}>
      <div className="glass-card fade-in-up" style={{ width: '100%', maxWidth: 620, padding: '44px 40px', textAlign: 'center', borderTop: '3px solid var(--ag-accent)' }}>
        {/* Main Official Logo */}
        <div style={{ display: 'flex', justifyContent: 'center', marginBottom: 18 }}>
          <img 
            src={mainLogo} 
            alt="NexVR Engine Logo" 
            style={{ 
              width: '100px', 
              height: 'auto', 
              filter: 'drop-shadow(0 0 16px rgba(0, 240, 255, 0.5))' 
            }} 
          />
        </div>

        <h1 style={{ 
          margin: '0 0 8px 0', fontSize: 26, letterSpacing: '2px', 
          fontFamily: 'var(--ag-font-display)', color: '#fff' 
        }}>
          NEXVR ENGINE
        </h1>
        <p style={{ 
          margin: '0 0 28px 0', color: 'var(--ag-accent)', fontSize: 13, 
          letterSpacing: '1.5px', fontFamily: 'var(--ag-font-mono)', opacity: 0.9 
        }}>
          Universal VR Injection Runtime
        </p>

        <p style={{ 
          color: 'var(--ag-text-muted)', fontSize: 13, lineHeight: '1.6', 
          margin: '0 auto 28px auto', maxWidth: 480 
        }}>
          Every game. Full depth.
        </p>

        {/* System Info Grid */}
        <div style={{ 
          display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 1, marginBottom: 32, 
          background: 'var(--ag-border)', borderRadius: 'var(--ag-radius-sm)', overflow: 'hidden' 
        }}>
          {[
            { label: 'VERSION', value: version ? (version.startsWith('v') ? version : `v${version}`) : 'v0.1.0' },
            { label: 'BUILD', value: import.meta.env.VITE_BUILD_DATE || 'dev' },
            { label: 'ELECTRON', value: window.ag?.versions?.electron || '—' },
            { label: 'NODE', value: window.ag?.versions?.node || '—' },
            { label: 'CHROMIUM', value: window.ag?.versions?.chrome || '—' },
            { label: 'ARCH', value: 'x64' },
          ].map((item) => (
            <div key={item.label} style={{ 
              background: 'rgba(0,0,0,0.4)', padding: '14px 12px', textAlign: 'left' 
            }}>
              <div style={{ 
                color: 'var(--ag-text-muted)', fontSize: 9, fontFamily: 'var(--ag-font-mono)', 
                letterSpacing: '2px', marginBottom: 4 
              }}>
                {item.label}
              </div>
              <div style={{ 
                color: '#fff', fontSize: 13, fontFamily: 'var(--ag-font-mono)', 
                fontWeight: 500, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' 
              }}>
                {item.value}
              </div>
            </div>
          ))}
        </div>

        {/* Action Buttons */}
        <div style={{ display: 'flex', justifyContent: 'center', gap: 10, marginBottom: 32, flexWrap: 'wrap' }}>
          <button onClick={() => handleLink('https://github.com/sathishssj3/NexVR-Engine')} className="btn-glow" style={{ 
            background: 'rgba(0,240,255,0.08)', border: '1px solid rgba(0,240,255,0.25)', 
            padding: '10px 20px', borderRadius: 'var(--ag-radius-sm)', color: 'var(--ag-accent)', 
            cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', fontSize: 11, letterSpacing: '1px' 
          }}>
            ◇ GITHUB
          </button>
          <button onClick={() => handleLink('https://github.com/sathishssj3/NexVR-Engine/issues')} className="btn-glow" style={{ 
            background: 'rgba(255,255,255,0.03)', border: '1px solid var(--ag-border)', 
            padding: '10px 20px', borderRadius: 'var(--ag-radius-sm)', color: 'var(--ag-text-primary)', 
            cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', fontSize: 11, letterSpacing: '1px' 
          }}>
            ⚑ REPORT BUG
          </button>
          <button 
            onClick={handleUploadLogs}
            disabled={sendingLogs}
            className="btn-glow" 
            title="Upload diagnostic session logs directly to developer Discord"
            style={{ 
              background: sendResult === 'success' 
                ? 'rgba(16, 185, 129, 0.15)' 
                : sendResult === 'error' 
                  ? 'rgba(239, 68, 68, 0.15)' 
                  : 'rgba(168, 85, 247, 0.12)', 
              border: sendResult === 'success' 
                ? '1px solid var(--ag-accent-success)' 
                : sendResult === 'error' 
                  ? '1px solid var(--ag-accent-danger)' 
                  : '1px solid rgba(168, 85, 247, 0.4)', 
              padding: '10px 20px', 
              borderRadius: 'var(--ag-radius-sm)', 
              color: sendResult === 'success' 
                ? 'var(--ag-accent-success)' 
                : sendResult === 'error' 
                  ? 'var(--ag-accent-danger)' 
                  : '#c084fc', 
              cursor: sendingLogs ? 'wait' : 'pointer', 
              fontFamily: 'var(--ag-font-mono)', 
              fontSize: 11, 
              letterSpacing: '1px' 
            }}
          >
            {sendingLogs 
              ? '◌ UPLOADING...' 
              : sendResult === 'success' 
                ? '✓ LOGS SENT TO CLOUD' 
                : sendResult === 'error' 
                  ? '✗ RETRY UPLOAD' 
                  : '📡 SEND LOGS TO CLOUD'}
          </button>
          <button onClick={() => handleLink('https://github.com/sathishssj3/NexVR-Engine/wiki')} className="btn-glow" style={{ 
            background: 'rgba(255,255,255,0.03)', border: '1px solid var(--ag-border)', 
            padding: '10px 20px', borderRadius: 'var(--ag-radius-sm)', color: 'var(--ag-text-primary)', 
            cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', fontSize: 11, letterSpacing: '1px' 
          }}>
            ◈ DOCS
          </button>
        </div>

        {/* Tech Stack */}
        <div style={{ 
          display: 'flex', flexWrap: 'wrap', justifyContent: 'center', gap: 8, marginBottom: 20 
        }}>
          {techStack.map(tech => (
            <span key={tech.name} style={{ 
              padding: '4px 12px', borderRadius: 20, 
              background: `${tech.color}10`, border: `1px solid ${tech.color}25`,
              color: tech.color, fontSize: 10, fontFamily: 'var(--ag-font-mono)', 
              letterSpacing: '0.5px' 
            }}>
              {tech.name}
            </span>
          ))}
        </div>

        {/* Copyright */}
        <div style={{ 
          color: 'var(--ag-text-muted)', fontSize: 10, fontFamily: 'var(--ag-font-mono)', 
          letterSpacing: '1px', opacity: 0.5 
        }}>
          © 2026 sathishssj3 // Proprietary — All Rights Reserved
        </div>
      </div>
    </div>
  );
}
