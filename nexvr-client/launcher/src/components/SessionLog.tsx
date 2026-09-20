import { useEffect, useRef, useState } from 'react';

export function SessionLog({ logLines }: { logLines: string[] }) {
  const scrollContainerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (scrollContainerRef.current) {
      scrollContainerRef.current.scrollTop = scrollContainerRef.current.scrollHeight;
    }
  }, [logLines]);

  const getColor = (line: string) => {
    // 1. Errors -> RED
    if (line.includes('[ERR]') || line.includes('[Injector CLI Error]') || line.includes('ERROR') || line.includes('FAILED')) {
      return 'var(--ag-accent-danger)';
    }
    // 2. Warnings -> AMBER / ORANGE
    if (line.includes('[!!]') || line.includes('WARN')) {
      return 'var(--ag-accent-warn)';
    }
    // 3. All [ OK ] lines (with or without spaces) -> VIBRANT GREEN
    if (/\[\s*OK\s*\]/i.test(line)) {
      return 'var(--ag-accent-success)';
    }
    // 4. Normal reading (INFO, [--], DIAGNOSTICS, STATUS, headers, default) -> CRISP WHITE
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

  const [sendingDev, setSendingDev] = useState(false);
  const [sendDevResult, setSendDevResult] = useState<'idle'|'success'|'error'>('idle');

  const handleSendToDev = async () => {
    setSendingDev(true);
    try {
      const res = await window.ag.telemetry.sendReport();
      if (res && res.success) {
        setSendDevResult('success');
        setTimeout(() => setSendDevResult('idle'), 3500);
      } else {
        setSendDevResult('error');
        setTimeout(() => setSendDevResult('idle'), 3500);
      }
    } catch {
      setSendDevResult('error');
      setTimeout(() => setSendDevResult('idle'), 3500);
    } finally {
      setSendingDev(false);
    }
  };

  const sectionLabel = (title: string, subtitle?: string) => (
    <div>
      <div style={{
        fontSize: 13,
        fontFamily: 'var(--ag-font-display)',
        color: '#FFFFFF',
        letterSpacing: '0.08em',
        fontWeight: 700,
        textTransform: 'uppercase',
        display: 'flex',
        alignItems: 'center',
        gap: 8,
      }}>
        <span style={{ 
          width: 3, 
          height: 13, 
          background: 'var(--ag-accent)', 
          borderRadius: 2,
        }} />
        {title}
        {logLines.length > 0 && (
          <span style={{ 
            marginLeft: 8, 
            color: 'var(--ag-accent)', 
            fontSize: 14, 
            fontWeight: 800, 
            fontFamily: 'var(--ag-font-mono)' 
          }}>
            {logLines.length}
          </span>
        )}
      </div>
      {subtitle && (
        <div style={{
          fontSize: 11.5,
          fontFamily: 'var(--ag-font-ui)',
          color: '#848884',
          marginTop: 4,
          paddingLeft: 11,
          lineHeight: 1.4,
        }}>
          {subtitle}
        </div>
      )}
    </div>
  );

  return (
    <div className="settings-item-enter stagger-3" style={{ display: 'flex', flexDirection: 'column', flex: 1 }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: 14, flexWrap: 'wrap', gap: 10 }}>
        {sectionLabel(
          'SESSION TELEMETRY & ENGINE LOG',
          'Live diagnostic pipe output, hook status, and OpenXR swapchain metrics'
        )}
        <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
          <button
            onClick={() => window.ag.utils.openLog()}
            className="btn-outline-laser"
            title="Open active engine & VR log in default text editor"
            style={{
              fontSize: '10px',
              padding: '6px 12px',
              borderColor: '#383A44',
              color: '#FFF'
            }}
          >
            ↗ OPEN LOG
          </button>
          <button
            onClick={() => window.ag.utils.openLogFolder()}
            className="btn-outline-laser"
            title="Open logs folder in Windows Explorer"
            style={{
              fontSize: '10px',
              padding: '6px 12px',
            }}
          >
            FOLDER
          </button>
          <button
            onClick={handleSendToDev}
            disabled={sendingDev}
            className="btn-outline-laser"
            title="Upload session log and diagnostic telemetry directly to developer Discord"
            style={{
              borderColor: sendDevResult === 'success'
                ? 'var(--ag-accent-success)'
                : sendDevResult === 'error'
                  ? 'var(--ag-accent-danger)'
                  : 'rgba(204, 0, 0, 0.4)',
              color: sendDevResult === 'success'
                ? 'var(--ag-accent-success)'
                : sendDevResult === 'error'
                  ? 'var(--ag-accent-danger)'
                  : 'var(--ag-accent)',
              fontSize: '10px',
              padding: '6px 12px',
              cursor: sendingDev ? 'wait' : 'pointer',
            }}
          >
            {sendingDev
              ? 'SENDING...'
              : sendDevResult === 'success'
                ? 'SENT TO DEV'
                : sendDevResult === 'error'
                  ? 'RETRY DEV'
                  : 'SEND TO DEV'}
          </button>
          <button
            onClick={handleExport}
            disabled={exporting || logLines.length === 0}
            className="btn-outline-laser"
            style={{
              color: exportResult === 'success'
                       ? 'var(--ag-accent-success)'
                       : exportResult === 'error'
                         ? 'var(--ag-accent-danger)'
                         : '#848884',
              borderColor: exportResult === 'success'
                       ? 'var(--ag-accent-success)'
                       : exportResult === 'error'
                         ? 'var(--ag-accent-danger)'
                         : '#383A44',
              fontSize: '10px',
              padding: '6px 12px',
              cursor: logLines.length === 0 ? 'not-allowed' : 'pointer',
              opacity: logLines.length === 0 ? 0.4 : 1,
            }}
          >
            {exporting          ? 'EXPORTING...'
             : exportResult === 'success' ? 'SAVED'
             : exportResult === 'error'   ? 'FAILED'
             :                              'EXPORT'}
          </button>
        </div>
      </div>
      
      <div 
        ref={scrollContainerRef}
        className="settings-card fast-smooth-scroll"
        style={{ 
        flex: 1, minHeight: 150, 
        background: '#020305', 
        border: '1px solid #2C2D35', 
        borderRadius: 6, 
        padding: '16px 20px', 
        fontFamily: 'var(--ag-font-mono)', 
        fontSize: 12, lineHeight: 1.8, 
        overflowY: 'auto', 
        boxShadow: 'inset 0 0 40px rgba(0,0,0,0.9), 0 5px 15px rgba(0,0,0,0.4)', 
        position: 'relative', display: 'flex', flexDirection: 'column' 
      }}>
        <div style={{ position: 'relative', zIndex: 2 }}>
          {logLines.length === 0 ? (
            <div className="empty-state" style={{ minHeight: 110, gap: 10, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center' }}>
              <div style={{ display: 'flex', alignItems: 'center', fontFamily: 'var(--ag-font-mono)', fontSize: 13, fontWeight: 700, color: '#FFFFFF', letterSpacing: '0.04em' }}>
                <span>{`> SYSTEM IDLE. AWAITING COMMAND `}</span>
                <span className="dot-pulse dot-1" style={{ color: 'var(--ag-accent)', fontWeight: 900 }}>.</span>
                <span className="dot-pulse dot-2" style={{ color: 'var(--ag-accent)', fontWeight: 900, marginLeft: 2 }}>.</span>
                <span className="dot-pulse dot-3" style={{ color: 'var(--ag-accent)', fontWeight: 900, marginLeft: 2 }}>.</span>
                <span className="blink-cursor" style={{ marginLeft: 8, color: '#FFFFFF', fontWeight: 900 }}>█</span>
              </div>
              <div className="log-hint-pulse" style={{ color: '#848884', fontFamily: 'var(--ag-font-mono)', fontSize: 11.5, letterSpacing: '0.04em', marginTop: 2 }}>
                Select a game and click INITIALIZE INJECTION to begin
              </div>
            </div>
          ) : (
            logLines.map((l, i) => {
              const isLast = i === logLines.length - 1;
              const lineColor = getColor(l);
              return (
                <div key={`${i}-${l.slice(0,20)}`} style={{ 
                  color: lineColor, 
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
                  <span style={{ flex: 1, wordBreak: 'break-all' }}>
                    {l}
                    {isLast && <span className="blink-cursor" style={{ marginLeft: 6 }}>█</span>}
                  </span>
                </div>
              );
            })
          )}
        </div>
      </div>
    </div>
  );
}
