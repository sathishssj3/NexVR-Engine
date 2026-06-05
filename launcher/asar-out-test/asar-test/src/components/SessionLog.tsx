import { useEffect, useRef } from 'react';

export function SessionLog({ logLines }: { logLines: string[] }) {
  const endRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    endRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [logLines]);

  const getColor = (line: string) => {
    if (line.startsWith('[OK]')) return 'var(--ag-accent)';
    if (line.startsWith('[--]')) return '#4898e0';
    if (line.startsWith('[!!]')) return '#e8a030';
    if (line.startsWith('[ERR]')) return '#e24b4a';
    return 'var(--ag-text-primary)';
  };

  return (
    <div style={{ flex: 1, background: 'var(--ag-bg-base)', border: '1px solid var(--ag-border)', borderRadius: 6, padding: 10, fontFamily: 'var(--ag-font-mono)', fontSize: 10, lineHeight: 1.8, overflowY: 'auto' }}>
      {logLines.length === 0 ? (
        <div style={{ color: 'var(--ag-text-muted)' }}>Waiting for session...</div>
      ) : (
        logLines.map((l, i) => (
          <div key={i} style={{ color: getColor(l) }}>{l}</div>
        ))
      )}
      <div ref={endRef} />
    </div>
  );
}
