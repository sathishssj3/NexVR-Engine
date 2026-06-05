import { useEffect, useRef, useState } from 'react';

function TypewriterLine({ text, isLast }: { text: string, isLast: boolean }) {
  const [displayedText, setDisplayedText] = useState('');
  
  useEffect(() => {
    let i = 0;
    const interval = setInterval(() => {
      i += 3;
      setDisplayedText(text.slice(0, i));
      if (i >= text.length) clearInterval(interval);
    }, 5);
    return () => clearInterval(interval);
  }, [text]);

  return (
    <>
      {displayedText}
      {isLast && displayedText.length >= text.length && <span className="blink-cursor">█</span>}
      {isLast && displayedText.length < text.length && <span style={{opacity:0.5}}>█</span>}
    </>
  );
}

export function SessionLog({ logLines }: { logLines: string[] }) {
  const endRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    endRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [logLines]);

  const getColor = (line: string) => {
    if (line.includes('[OK]')) return 'var(--ag-accent-success)';
    if (line.includes('[--]')) return 'var(--ag-text-code)';
    if (line.includes('[!!]')) return 'var(--ag-accent-warn)';
    if (line.includes('[ERR]') || line.includes('[Injector CLI Error]')) return 'var(--ag-accent-danger)';
    return 'var(--ag-text-primary)';
  };

  const [exporting, setExporting] = useState(false);
  const [exportResult, setExportResult] = useState<'idle'|'success'|'error'>('idle');

  const handleExport = async () => {
    if (logLines.length === 0) return;
    setExporting(true);
    try {
      const result = await window.ag.log.export(logLines);
      if (result.success) {
        setExportResult('success');
        setTimeout(() => setExportResult('idle'), 3000);
      }
    } catch {
      setExportResult('error');
      setTimeout(() => setExportResult('idle'), 3000);
    } finally {
      setExporting(false);
    }
  };

  return (
    <div className="fade-in-up stagger-3" style={{ display: 'flex', flexDirection: 'column', flex: 1 }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 10 }}>
        <div className="section-header" style={{ marginBottom: 0, flex: 1 }}>
          SESSION LOG
          {logLines.length > 0 && (
            <span className="game-count" style={{ marginLeft: 8 }}>{logLines.length}</span>
          )}
        </div>
        <button
          onClick={handleExport}
          disabled={exporting || logLines.length === 0}
          className="btn-glow"
          style={{
            background: 'transparent',
            border: '1px solid var(--ag-border)',
            color: exportResult === 'success'
                     ? 'var(--ag-accent-success)'
                     : exportResult === 'error'
                       ? 'var(--ag-accent-danger)'
                       : 'var(--ag-text-muted)',
            fontFamily: 'var(--ag-font-mono)',
            fontSize: '10px',
            letterSpacing: '1px',
            padding: '5px 12px',
            borderRadius: 'var(--ag-radius-sm)',
            cursor: logLines.length === 0 ? 'not-allowed' : 'pointer',
            textTransform: 'uppercase' as const,
            transition: 'all 0.3s var(--ag-transition)',
            opacity: logLines.length === 0 ? 0.4 : 1,
          }}
        >
          {exporting          ? '◌ EXPORTING...'
           : exportResult === 'success' ? '✓ SAVED'
           : exportResult === 'error'   ? '✗ FAILED'
           :                              '↓ EXPORT'}
        </button>
      </div>
      
      <div style={{ 
        flex: 1, minHeight: 140, 
        background: 'rgba(2, 3, 6, 0.9)', 
        border: '1px solid var(--ag-border)', 
        borderRadius: 'var(--ag-radius-sm)', 
        padding: '16px 20px', 
        fontFamily: 'var(--ag-font-mono)', 
        fontSize: 11, lineHeight: 1.9, 
        overflowY: 'auto', 
        boxShadow: 'inset 0 0 30px rgba(0,0,0,0.8), 0 4px 10px rgba(0,0,0,0.2)', 
        position: 'relative', display: 'flex', flexDirection: 'column' 
      }}>
        {/* Scanline overlay */}
        <div style={{ 
          position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, 
          background: 'repeating-linear-gradient(0deg, rgba(0,0,0,0.12), rgba(0,0,0,0.12) 1px, transparent 1px, transparent 2px)', 
          pointerEvents: 'none', zIndex: 1 
        }} />
      
        <div style={{ position: 'relative', zIndex: 2 }}>
          {logLines.length === 0 ? (
            <div className="empty-state" style={{ minHeight: 100, gap: 8 }}>
              <div className="text" style={{ display: 'flex', alignItems: 'center' }}>
                {`> SYSTEM IDLE. AWAITING COMMAND...`} <span className="blink-cursor" style={{marginLeft: 8}}>█</span>
              </div>
              <div className="hint">Select a game and click INITIALIZE INJECTION to begin</div>
            </div>
          ) : (
            logLines.map((l, i) => {
              const isLast = i === logLines.length - 1;
              const lineColor = getColor(l);
              return (
                <div key={`${i}-${l.slice(0,20)}`} style={{ 
                  color: lineColor, 
                  textShadow: `0 0 5px ${lineColor}40`,
                  display: 'flex',
                  alignItems: 'flex-start'
                }}>
                  <span style={{ 
                    opacity: 0.25, marginRight: 12, minWidth: 24, textAlign: 'right' as const,
                    color: 'var(--ag-text-muted)', fontSize: 10, lineHeight: '1.9',
                    fontVariantNumeric: 'tabular-nums'
                  }}>
                    {String(i + 1).padStart(3, '0')}
                  </span>
                  <span style={{ flex: 1 }}>
                    <TypewriterLine text={l} isLast={isLast} />
                  </span>
                </div>
              );
            })
          )}
          <div ref={endRef} />
        </div>
      </div>
    </div>
  );
}
