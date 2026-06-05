
export function AboutPanel() {
  const handleLink = (url: string) => {
    if (window.ag && window.ag.shell) {
      window.ag.shell.openExternal(url);
    }
  };

  return (
    <div style={{ flex: 1, padding: '40px', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', overflowY: 'auto', background: 'radial-gradient(circle at 50% 30%, rgba(0, 240, 255, 0.05), transparent 70%)' }}>
      <div className="glass-card" style={{ width: '100%', maxWidth: 600, padding: 40, textAlign: 'center', borderTop: '4px solid var(--ag-accent)' }}>
        <h1 style={{ fontSize: 48, fontWeight: 700, letterSpacing: '8px', color: 'var(--ag-accent)', margin: '0 0 10px 0', fontFamily: 'var(--ag-font-mono)', textShadow: '0 0 20px rgba(0,240,255,0.4)' }}>
          NEXVR ENGINE
        </h1>
        <p style={{ fontSize: 16, color: 'var(--ag-text-primary)', letterSpacing: '2px', marginBottom: 40, opacity: 0.8 }}>
          Every game. Full depth.
        </p>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 20, marginBottom: 40, textAlign: 'left', background: 'rgba(0,0,0,0.3)', padding: 20, borderRadius: 8, border: '1px solid var(--ag-border)' }}>
          <div>
            <div style={{ color: 'var(--ag-text-muted)', fontSize: 11, fontFamily: 'var(--ag-font-mono)', letterSpacing: '1px', marginBottom: 4 }}>VERSION</div>
            <div style={{ color: '#fff', fontSize: 14, fontFamily: 'var(--ag-font-mono)' }}>v0.1.0</div>
          </div>
          <div>
            <div style={{ color: 'var(--ag-text-muted)', fontSize: 11, fontFamily: 'var(--ag-font-mono)', letterSpacing: '1px', marginBottom: 4 }}>BUILD</div>
            <div style={{ color: '#fff', fontSize: 14, fontFamily: 'var(--ag-font-mono)' }}>{import.meta.env.VITE_BUILD_DATE || 'development'}</div>
          </div>
          <div>
            <div style={{ color: 'var(--ag-text-muted)', fontSize: 11, fontFamily: 'var(--ag-font-mono)', letterSpacing: '1px', marginBottom: 4 }}>RUNTIME</div>
            <div style={{ color: '#fff', fontSize: 14, fontFamily: 'var(--ag-font-mono)' }}>Electron {window.ag?.versions?.electron || 'Unknown'}</div>
          </div>
          <div>
            <div style={{ color: 'var(--ag-text-muted)', fontSize: 11, fontFamily: 'var(--ag-font-mono)', letterSpacing: '1px', marginBottom: 4 }}>NODE</div>
            <div style={{ color: '#fff', fontSize: 14, fontFamily: 'var(--ag-font-mono)' }}>{window.ag?.versions?.node || 'Unknown'}</div>
          </div>
          <div>
            <div style={{ color: 'var(--ag-text-muted)', fontSize: 11, fontFamily: 'var(--ag-font-mono)', letterSpacing: '1px', marginBottom: 4 }}>CHROMIUM</div>
            <div style={{ color: '#fff', fontSize: 14, fontFamily: 'var(--ag-font-mono)' }}>{window.ag?.versions?.chrome || 'Unknown'}</div>
          </div>
        </div>

        <div style={{ display: 'flex', justifyContent: 'center', gap: 15, marginBottom: 40 }}>
          <button onClick={() => handleLink('https://voxira.vr/docs')} className="btn-glow" style={{ background: 'rgba(255,255,255,0.05)', border: '1px solid var(--ag-border)', padding: '10px 20px', borderRadius: 6, color: 'var(--ag-text-primary)', cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', fontSize: 12, letterSpacing: '1px' }}>DOCUMENTATION</button>
          <button onClick={() => handleLink('https://github.com/nexvr/nexvr-engine/issues')} className="btn-glow" style={{ background: 'rgba(255,255,255,0.05)', border: '1px solid var(--ag-border)', padding: '10px 20px', borderRadius: 6, color: 'var(--ag-text-primary)', cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', fontSize: 12, letterSpacing: '1px' }}>REPORT A BUG</button>
          <button onClick={() => handleLink('https://discord.gg/voxisvr')} className="btn-glow" style={{ background: 'rgba(0,240,255,0.1)', border: '1px solid rgba(0,240,255,0.3)', padding: '10px 20px', borderRadius: 6, color: 'var(--ag-accent)', cursor: 'pointer', fontFamily: 'var(--ag-font-mono)', fontSize: 12, letterSpacing: '1px' }}>DISCORD</button>
        </div>

        <div style={{ color: 'var(--ag-text-muted)', fontSize: 11, fontFamily: 'var(--ag-font-mono)', letterSpacing: '0.5px' }}>
          Built on: OpenXR · MinHook · DirectX 12 · Vulkan · Electron
        </div>
      </div>
    </div>
  );
}
