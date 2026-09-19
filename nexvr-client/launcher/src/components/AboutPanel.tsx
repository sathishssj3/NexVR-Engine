export function AboutPanel({ version }: { version?: string }) {
  const handleLink = async (url: string) => {
    try {
      if (window.ag && window.ag.shell) {
        await window.ag.shell.openExternal(url);
      } else {
        window.open(url, '_blank', 'noopener,noreferrer');
      }
    } catch (err) {
      console.warn('shell.openExternal failed, falling back to window.open:', err);
      window.open(url, '_blank', 'noopener,noreferrer');
    }
  };

  const currentVer = version ? (version.startsWith('v') ? version : `v${version}`) : 'v0.1.61';

  return (
    <div className="fast-smooth-scroll" style={{ flex: 1, padding: '36px 24px', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', overflowY: 'auto', background: '#000000' }}>
      <div 
        className="settings-card settings-open-anim" 
        style={{ 
          width: '100%', 
          maxWidth: 680, 
          padding: '44px 48px',
          display: 'flex', 
          flexDirection: 'column', 
          alignItems: 'center',
          textAlign: 'center'
        }}
      >
        {/* Centered Large Title */}
        <h1 style={{ 
          margin: '0 0 8px 0', 
          fontSize: 32, 
          fontWeight: 800, 
          letterSpacing: '0.08em', 
          fontFamily: 'var(--ag-font-display)', 
          color: '#FFF',
          textAlign: 'center'
        }}>
          NEXVR ENGINE
        </h1>

        {/* Subtitle & Description */}
        <p style={{ 
          margin: '0 0 8px 0', 
          color: 'var(--ag-accent)', 
          fontSize: 13.5, 
          fontFamily: 'var(--ag-font-mono)', 
          letterSpacing: '0.04em',
          fontWeight: 700
        }}>
          Universal VR Injection Runtime
        </p>

        <p style={{ 
          margin: '0 0 30px 0', 
          color: '#848884', 
          fontSize: 12.5, 
          lineHeight: '1.6', 
          maxWidth: 540,
          fontFamily: 'var(--ag-font-ui)'
        }}>
          Hardware-accelerated stereoscopic VR injector with 6DOF tracking and DirectML neural reprojection for standard PC games.
        </p>

        {/* 4-Cell Specifications Bento Grid */}
        <div style={{ 
          width: '100%', 
          display: 'grid', 
          gridTemplateColumns: '1fr 1fr', 
          gap: 12, 
          marginBottom: 26 
        }}>
          {[
            { 
              label: 'RUNTIME ARCHITECTURE', 
              title: currentVer, 
              desc: 'Native MSVC x64 Hook Runtime' 
            },
            { 
              label: 'OPENXR ENVIRONMENT', 
              title: 'OpenXR 1.0', 
              desc: 'SteamVR · Quest Link · Virtual Desktop' 
            },
            { 
              label: 'GRAPHICS BACKENDS', 
              title: 'DX11 · DX12 · Vulkan', 
              desc: 'Low-latency VTable Swapchain Hooks' 
            },
            { 
              label: 'NEURAL ACCELERATION', 
              title: 'DirectML / ONNX', 
              desc: 'AI Inpainting & Stereo Reprojection' 
            },
          ].map((item) => (
            <div 
              key={item.label} 
              style={{ 
                background: 'linear-gradient(180deg, rgba(255, 255, 255, 0.025) 0%, rgba(255, 255, 255, 0.006) 100%), #060608', 
                border: '1px solid rgba(255, 255, 255, 0.07)', 
                borderTop: '2px solid #CC0000',
                borderRadius: 6, 
                padding: '16px 18px', 
                textAlign: 'left',
                display: 'flex', 
                flexDirection: 'column', 
                gap: 4,
                boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.05), 0 4px 14px rgba(0, 0, 0, 0.35)',
              }}
            >
              <div style={{ 
                color: '#848884', 
                fontSize: 10, 
                fontFamily: 'var(--ag-font-mono)', 
                letterSpacing: '0.08em', 
                fontWeight: 700 
              }}>
                {item.label}
              </div>
              <div style={{ 
                color: '#FFF', 
                fontSize: 15.5, 
                fontFamily: 'var(--ag-font-display)', 
                fontWeight: 800,
                letterSpacing: '0.03em',
                marginTop: 1
              }}>
                {item.title}
              </div>
              <div style={{ 
                color: '#848884', 
                fontSize: 12, 
                fontFamily: 'var(--ag-font-ui)',
                lineHeight: '1.4',
                fontWeight: 600,
                marginTop: 2
              }}>
                {item.desc}
              </div>
            </div>
          ))}
        </div>

        {/* Action Buttons: 3 Utility Buttons */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 10, marginBottom: 12, width: '100%' }}>
          <button 
            onClick={() => handleLink('https://github.com/sathishssj3/NexVR-Engine')} 
            style={{ 
              padding: '11px 16px', 
              borderRadius: 'var(--ag-radius-sm)', 
              color: 'var(--ag-text-primary)', 
              cursor: 'pointer', 
              fontFamily: 'var(--ag-font-display)', 
              fontSize: 11.5, 
              letterSpacing: '0.08em', 
              fontWeight: 600, 
              border: '1px solid rgba(255, 255, 255, 0.12)', 
              background: 'rgba(255, 255, 255, 0.035)', 
              boxShadow: 'none', 
              transition: 'all 0.2s ease' 
            }}
            onMouseEnter={e => {
              e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.3)';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.07)';
            }}
            onMouseLeave={e => {
              e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.12)';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.035)';
            }}
          >
            GITHUB
          </button>
          <button 
            onClick={() => handleLink('https://nexvr.org/docs')} 
            style={{ 
              padding: '11px 16px', 
              borderRadius: 'var(--ag-radius-sm)', 
              color: 'var(--ag-text-primary)', 
              cursor: 'pointer', 
              fontFamily: 'var(--ag-font-display)', 
              fontSize: 11.5, 
              letterSpacing: '0.08em', 
              fontWeight: 600, 
              border: '1px solid rgba(255, 255, 255, 0.12)', 
              background: 'rgba(255, 255, 255, 0.035)', 
              boxShadow: 'none', 
              transition: 'all 0.2s ease' 
            }}
            onMouseEnter={e => {
              e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.3)';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.07)';
            }}
            onMouseLeave={e => {
              e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.12)';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.035)';
            }}
          >
            DOCUMENTATION
          </button>
          <button 
            onClick={() => handleLink('https://github.com/sathishssj3/NexVR-Engine/issues/new')} 
            style={{ 
              padding: '11px 16px', 
              borderRadius: 'var(--ag-radius-sm)', 
              color: 'var(--ag-text-primary)', 
              cursor: 'pointer', 
              fontFamily: 'var(--ag-font-display)', 
              fontSize: 11.5, 
              letterSpacing: '0.08em', 
              fontWeight: 600, 
              border: '1px solid rgba(255, 255, 255, 0.12)', 
              background: 'rgba(255, 255, 255, 0.035)', 
              boxShadow: 'none', 
              transition: 'all 0.2s ease' 
            }}
            onMouseEnter={e => {
              e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.3)';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.07)';
            }}
            onMouseLeave={e => {
              e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.12)';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.035)';
            }}
          >
            REPORT ISSUE
          </button>
        </div>

        {/* Full-width Discord CTA Button in Solid Pure Red with Bold White Text (No Glow) */}
        <button 
          onClick={() => handleLink('https://discord.gg/FBeGjgK2fd')} 
          style={{ 
            width: '100%', 
            padding: '13px 20px', 
            borderRadius: 'var(--ag-radius-sm)', 
            color: '#FFFFFF', 
            cursor: 'pointer', 
            fontFamily: 'var(--ag-font-display)', 
            fontSize: 12.5, 
            letterSpacing: '0.1em', 
            fontWeight: 800, 
            border: '1px solid #CC0000', 
            background: '#CC0000', 
            boxShadow: 'none', 
            marginBottom: 26, 
            transition: 'all 0.2s ease' 
          }}
          onMouseEnter={e => {
            e.currentTarget.style.background = '#E60000';
            e.currentTarget.style.borderColor = '#FF1A1A';
            e.currentTarget.style.boxShadow = 'none';
          }}
          onMouseLeave={e => {
            e.currentTarget.style.background = '#CC0000';
            e.currentTarget.style.borderColor = '#CC0000';
            e.currentTarget.style.boxShadow = 'none';
          }}
        >
          JOIN OUR DISCORD SERVER
        </button>

        {/* Clean Monospace Copyright in Smoke #848884 */}
        <div style={{ 
          color: '#848884', 
          fontSize: 11, 
          fontFamily: 'var(--ag-font-mono)', 
          letterSpacing: '0.06em', 
          opacity: 0.8 
        }}>
          © 2026 sathishssj3 · NexVR Engine · Open Source VR Injector
        </div>

      </div>
    </div>
  );
}
