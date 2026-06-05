import { useEffect, useRef, useState } from 'react';

function TypewriterLine({ text, isLast }: { text: string, isLast: boolean }) {
  const [displayedText, setDisplayedText] = useState('');
  
  useEffect(() => {
    let i = 0;
    const interval = setInterval(() => {
      i += 3; // fast typing
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
    if (line.includes('[!!]')) return '#ffaa00';
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
    <div style={{ display: 'flex', flexDirection: 'column', flex: 1 }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 10 }}>
        <div style={{ color: 'var(--ag-text-muted)', fontFamily: 'var(--ag-font-mono)', fontSize: 12, letterSpacing: '1px' }}>SESSION LOG</div>
        <button
          onClick={handleExport}
          disabled={exporting || logLines.length === 0}
          style={{
            background:    'transparent',
            border:        '0.5px solid var(--ag-border)',
            color:         exportResult === 'success'
                             ? 'var(--ag-accent)'
                             : exportResult === 'error'
                               ? '#e24b4a'
                               : 'var(--ag-text-muted)',
            fontFamily:    'var(--ag-font-mono)',
            fontSize:      '10px',
            letterSpacing: '1px',
            padding:       '4px 10px',
            borderRadius:  '4px',
            cursor:        logLines.length === 0 ? 'not-allowed' : 'pointer',
            textTransform: 'uppercase',
            transition:    'color 0.2s, border-color 0.2s',
          }}
        >
          {exporting          ? '◌ EXPORTING...'
           : exportResult === 'success' ? '✓ SAVED TO DESKTOP'
           : exportResult === 'error'   ? '✗ EXPORT FAILED'
           :                              '↓ EXPORT LOGS'}
        </button>
      </div>
      <div style={{ flex: 1, minHeight: 120, background: '#030408', border: '1px solid var(--ag-border)', borderRadius: 6, padding: '15px 20px', fontFamily: 'var(--ag-font-mono)', fontSize: 11, lineHeight: 1.8, overflowY: 'auto', boxShadow: 'inset 0 0 20px rgba(0,0,0,0.8), 0 4px 10px rgba(0,0,0,0.2)', position: 'relative', display: 'flex', flexDirection: 'column' }}>
        {/* Scanline overlay effect */}
        <div style={{ position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, background: 'repeating-linear-gradient(0deg, rgba(0,0,0,0.15), rgba(0,0,0,0.15) 1px, transparent 1px, transparent 2px)', pointerEvents: 'none', zIndex: 1 }} />
      
      <div style={{ position: 'relative', zIndex: 2 }}>
        {logLines.length === 0 ? (
          <div style={{ color: 'var(--ag-text-muted)', display: 'flex', alignItems: 'center' }}>
            {`> SYSTEM IDLE. AWAITING COMMAND...`} <span className="blink-cursor" style={{marginLeft: 8}}>█</span>
          </div>
        ) : (
          logLines.map((l, i) => {
            const isLast = i === logLines.length - 1;
            return (
              <div key={l} style={{ color: getColor(l), textShadow: `0 0 5px ${getColor(l)}40` }}>
                <span style={{ opacity: 0.5, marginRight: 10 }}>{`>`}</span>
                <TypewriterLine text={l} isLast={isLast} />
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
