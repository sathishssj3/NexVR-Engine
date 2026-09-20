import { useState } from 'react';

export function AboutPanel({ version }: { version?: string }) {
  const [copied, setCopied] = useState(false);

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

  const handleCopyDiagnostics = async () => {
    const diagText = [
      `NexVR Engine ${currentVer}`,
      `Platform: Windows x64 (MSVC 2022+)`,
      `Hook Engine: MinHook Detours (DX11 / DX12 / Vulkan)`,
      `Spatial Compositor: OpenXR 1.0.34`,
      `AI Engine: DirectML 1.13.1 / ONNX Runtime 1.16.3`,
      `User Agent: ${navigator.userAgent}`,
      `Date: ${new Date().toISOString()}`
    ].join('\n');
    try {
      await navigator.clipboard.writeText(diagText);
      setCopied(true);
      setTimeout(() => setCopied(false), 3000);
    } catch {
      // fallback
    }
  };

  const [copiedExclusion, setCopiedExclusion] = useState(false);

  const handleCopyExclusion = async () => {
    try {
      await navigator.clipboard.writeText('Add-MpPreference -ExclusionPath "$env:LOCALAPPDATA\\Programs\\launcher"');
      setCopiedExclusion(true);
      setTimeout(() => setCopiedExclusion(false), 3000);
    } catch {}
  };

  const targetVer = '0.1.78';
  const effectiveVer = (!version || version === '0.1.77' || version === 'v0.1.77' || version === '0.1.29') 
    ? targetVer 
    : version.replace(/^v/i, '');
  const currentVer = `v${effectiveVer}`;

  return (
    <div 
      className="fast-smooth-scroll" 
      style={{ 
        flex: 1, 
        width: '100%',
        height: '100%',
        padding: '24px 20px 48px 20px', 
        display: 'flex', 
        flexDirection: 'column', 
        alignItems: 'center', 
        overflowY: 'auto', 
        background: '#000000' 
      }}
    >
      <div 
        className="settings-card settings-open-anim" 
        style={{ 
          width: '100%', 
          maxWidth: 660, 
          margin: '0 auto',
          padding: '24px 28px 24px 28px',
          display: 'flex', 
          flexDirection: 'column', 
          alignItems: 'center',
          textAlign: 'center'
        }}
      >
        {/* Centered Large Title */}
        <h1 style={{ 
          margin: '0 0 4px 0', 
          fontSize: 26, 
          fontWeight: 800, 
          letterSpacing: '-0.01em', 
          fontFamily: 'var(--ag-font-display)', 
          color: '#FFF',
          textAlign: 'center'
        }}>
          NEXVR ENGINE
        </h1>

        {/* Subtitle & Description */}
        <p style={{ 
          margin: '0 0 6px 0', 
          color: 'var(--ag-accent)', 
          fontSize: 12, 
          fontFamily: 'var(--ag-font-mono)', 
          letterSpacing: '0.04em',
          fontWeight: 700
        }}>
          Universal VR Injection Runtime
        </p>

        <p style={{ 
          margin: '0 0 16px 0', 
          color: 'var(--ag-text-muted)', 
          fontSize: 11.5, 
          lineHeight: '1.45', 
          maxWidth: 520,
          fontFamily: 'var(--ag-font-ui)'
        }}>
          Hardware-accelerated stereoscopic VR injector with 6DOF tracking and DirectML neural reprojection for standard PC games.
        </p>

        {/* 4-Cell Specifications Bento Grid */}
        <div style={{ 
          width: '100%', 
          display: 'grid', 
          gridTemplateColumns: '1fr 1fr', 
          gap: 9, 
          marginBottom: 10 
        }}>
          {[
            { 
              label: 'NATIVE RUNTIME', 
              title: currentVer, 
              desc: 'High-performance MSVC x64 binary hook engine' 
            },
            { 
              label: 'SPATIAL COMPOSITOR', 
              title: 'OpenXR 1.0.34', 
              desc: 'Hardware-agnostic 6DOF headset & controller sync' 
            },
            { 
              label: 'GRAPHICS DETOURS', 
              title: 'DX11 · DX12 · Vulkan', 
              desc: 'Low-latency VTable Swapchain & Queue Detours' 
            },
            { 
              label: 'NEURAL PIPELINE', 
              title: 'DirectML / ONNX', 
              desc: 'Direct3D 12 Tensor Inpainting & Stereo Reprojection' 
            },
          ].map((item) => (
            <div 
              key={item.label} 
              style={{ 
                background: 'linear-gradient(180deg, rgba(255, 255, 255, 0.025) 0%, rgba(255, 255, 255, 0.006) 100%), #060608', 
                border: '1px solid #2C2D35', 
                borderTop: '2px solid #CC0000',
                borderRadius: 6, 
                padding: '11px 13px', 
                textAlign: 'left',
                display: 'flex', 
                flexDirection: 'column', 
                gap: 3,
                boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.05), 0 4px 14px rgba(0, 0, 0, 0.35)',
              }}
            >
              <div style={{ 
                color: 'var(--ag-text-dim)', 
                fontSize: 9.5, 
                fontFamily: 'var(--ag-font-mono)', 
                letterSpacing: '0.08em', 
                fontWeight: 700 
              }}>
                {item.label}
              </div>
              <div style={{ 
                color: '#FFF', 
                fontSize: 14, 
                fontFamily: 'var(--ag-font-display)', 
                fontWeight: 800,
                letterSpacing: '0.02em',
                marginTop: 1
              }}>
                {item.title}
              </div>
              <div style={{ 
                color: 'var(--ag-text-muted)', 
                fontSize: 11, 
                fontFamily: 'var(--ag-font-ui)',
                lineHeight: '1.35',
                fontWeight: 500,
                marginTop: 2
              }}>
                {item.desc}
              </div>
            </div>
          ))}
        </div>

        {/* Diagnostics Utility Toolbar */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8, marginBottom: 8, width: '100%' }}>
          <button 
            type="button"
            onClick={() => { if (window.ag?.utils?.openLogFolder) window.ag.utils.openLogFolder(); }} 
            style={{ 
              padding: '8px 14px', 
              borderRadius: 'var(--ag-radius-sm)', 
              color: 'var(--ag-text-primary)', 
              cursor: 'pointer', 
              fontFamily: 'var(--ag-font-display)', 
              fontSize: 11, 
              letterSpacing: '0.06em', 
              fontWeight: 700, 
              border: '1px solid #383A44', 
              background: 'rgba(255, 255, 255, 0.03)', 
              transition: 'all 0.15s ease' 
            }}
            onMouseEnter={e => {
              e.currentTarget.style.borderColor = '#4A4D5C';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.06)';
            }}
            onMouseLeave={e => {
              e.currentTarget.style.borderColor = '#383A44';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.03)';
            }}
          >
            OPEN LOG FOLDER
          </button>
          <button 
            type="button"
            onClick={handleCopyDiagnostics} 
            style={{ 
              padding: '8px 14px', 
              borderRadius: 'var(--ag-radius-sm)', 
              color: copied ? 'var(--ag-accent-success)' : 'var(--ag-text-primary)', 
              cursor: 'pointer', 
              fontFamily: 'var(--ag-font-display)', 
              fontSize: 11, 
              letterSpacing: '0.06em', 
              fontWeight: 700, 
              border: copied ? '1px solid var(--ag-accent-success)' : '1px solid #383A44', 
              background: copied ? 'rgba(48, 209, 88, 0.08)' : 'rgba(255, 255, 255, 0.03)', 
              transition: 'all 0.15s ease' 
            }}
            onMouseEnter={e => {
              if (!copied) {
                e.currentTarget.style.borderColor = '#4A4D5C';
                e.currentTarget.style.background = 'rgba(255, 255, 255, 0.06)';
              }
            }}
            onMouseLeave={e => {
              if (!copied) {
                e.currentTarget.style.borderColor = '#383A44';
                e.currentTarget.style.background = 'rgba(255, 255, 255, 0.03)';
              }
            }}
          >
            {copied ? '✓ COPIED TO CLIPBOARD' : 'COPY SYSTEM SPECS'}
          </button>
        </div>

        {/* Action Buttons: 2 Utility Buttons */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8, marginBottom: 10, width: '100%' }}>
          <button 
            onClick={() => handleLink('https://discord.gg/FBeGjgK2fd')} 
            style={{ 
              padding: '8px 14px', 
              borderRadius: 'var(--ag-radius-sm)', 
              color: 'var(--ag-text-primary)', 
              cursor: 'pointer', 
              fontFamily: 'var(--ag-font-display)', 
              fontSize: 11, 
              letterSpacing: '0.08em', 
              fontWeight: 600, 
              border: '1px solid #383A44', 
              background: 'rgba(255, 255, 255, 0.035)', 
              boxShadow: 'none', 
              transition: 'all 0.2s ease' 
            }}
            onMouseEnter={e => {
              e.currentTarget.style.borderColor = '#4A4D5C';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.07)';
            }}
            onMouseLeave={e => {
              e.currentTarget.style.borderColor = '#383A44';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.035)';
            }}
          >
            COMMUNITY & GUIDES
          </button>
          <button 
            onClick={() => handleLink('https://discord.gg/FBeGjgK2fd')} 
            style={{ 
              padding: '8px 14px', 
              borderRadius: 'var(--ag-radius-sm)', 
              color: 'var(--ag-text-primary)', 
              cursor: 'pointer', 
              fontFamily: 'var(--ag-font-display)', 
              fontSize: 11, 
              letterSpacing: '0.08em', 
              fontWeight: 600, 
              border: '1px solid #383A44', 
              background: 'rgba(255, 255, 255, 0.035)', 
              boxShadow: 'none', 
              transition: 'all 0.2s ease' 
            }}
            onMouseEnter={e => {
              e.currentTarget.style.borderColor = '#4A4D5C';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.07)';
            }}
            onMouseLeave={e => {
              e.currentTarget.style.borderColor = '#383A44';
              e.currentTarget.style.background = 'rgba(255, 255, 255, 0.035)';
            }}
          >
            REPORT ISSUE
          </button>
        </div>

        {/* Antivirus & Windows Defender Whitelist Notice */}
        <div style={{
          width: '100%',
          background: 'rgba(255, 255, 255, 0.02)',
          border: '1px solid #2C2D35',
          borderLeft: '3px solid #CC0000',
          borderRadius: 6,
          padding: '10px 12px',
          marginBottom: 10,
          textAlign: 'left',
          display: 'flex',
          flexDirection: 'column',
          gap: 5
        }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
            <span style={{ color: '#F0F0F5', fontSize: 10.5, fontWeight: 700, fontFamily: 'var(--ag-font-display)', letterSpacing: '0.06em' }}>
              SECURITY & ANTIVIRUS NOTICE
            </span>
            <span style={{ color: 'var(--ag-text-muted)', fontSize: 9.5, fontFamily: 'var(--ag-font-mono)' }}>
              TIER 1 / PROTOTYPE
            </span>
          </div>
          <p style={{ color: 'var(--ag-text-dim)', fontSize: 10.5, lineHeight: 1.45, margin: 0 }}>
            Because NexVR attaches directly to game graphics swapchains via memory detours, Windows Defender may flag an injection false-positive. Add this folder to Defender exclusions for uninterrupted play.
          </p>
          <div style={{ display: 'flex', gap: 8, marginTop: 2 }}>
            <button
              type="button"
              onClick={handleCopyExclusion}
              style={{
                padding: '5px 10px',
                borderRadius: 'var(--ag-radius-sm)',
                color: copiedExclusion ? 'var(--ag-accent-success)' : 'var(--ag-text-primary)',
                cursor: 'pointer',
                fontFamily: 'var(--ag-font-mono)',
                fontSize: 10,
                fontWeight: 600,
                border: copiedExclusion ? '1px solid var(--ag-accent-success)' : '1px solid #383A44',
                background: copiedExclusion ? 'rgba(48, 209, 88, 0.08)' : 'rgba(255, 255, 255, 0.03)',
                transition: 'all 0.15s ease'
              }}
            >
              {copiedExclusion ? '✓ EXCLUSION COMMAND COPIED' : 'COPY DEFENDER EXCLUSION (POWERSHELL)'}
            </button>
          </div>
        </div>

        {/* Full-width Discord CTA Button placed INSIDE the card box */}
        <button 
          onClick={() => handleLink('https://discord.gg/FBeGjgK2fd')} 
          style={{ 
            width: '100%', 
            padding: '10px 18px', 
            borderRadius: 'var(--ag-radius-sm)', 
            color: '#FFFFFF', 
            cursor: 'pointer', 
            fontFamily: 'var(--ag-font-display)', 
            fontSize: 11.5, 
            letterSpacing: '0.08em', 
            fontWeight: 800, 
            border: '1px solid #CC0000', 
            background: '#CC0000', 
            boxShadow: 'none', 
            marginBottom: 0, 
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

      </div>

      {/* Clean Monospace Copyright placed OUTSIDE the card box (downside) with generous space */}
      <div style={{ 
        color: '#848884', 
        fontSize: 10.5, 
        fontFamily: 'var(--ag-font-mono)', 
        letterSpacing: '0.06em', 
        opacity: 0.75,
        marginTop: 20,
        marginBottom: 32,
        textAlign: 'center'
      }}>
        © 2026 sathishssj3 · NexVR Engine · All Rights Reserved
      </div>
    </div>
  );
}
