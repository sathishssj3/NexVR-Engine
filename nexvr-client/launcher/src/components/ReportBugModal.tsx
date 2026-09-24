import { useState, useEffect } from 'react';

export interface ReportBugModalProps {
  isOpen: boolean;
  onClose: () => void;
  activeGameId?: string;
  activeGameName?: string;
}

export function ReportBugModal({
  isOpen,
  onClose,
  activeGameId,
  activeGameName
}: ReportBugModalProps) {
  const [note, setNote] = useState('');
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [submitResult, setSubmitResult] = useState<{
    success: boolean;
    reportId?: string;
    message?: string;
  } | null>(null);
  const [copiedId, setCopiedId] = useState(false);
  const [copiedSummary, setCopiedSummary] = useState(false);

  useEffect(() => {
    if (!isOpen) {
      setNote('');
      setIsSubmitting(false);
      setSubmitResult(null);
      setCopiedId(false);
      setCopiedSummary(false);
    }
  }, [isOpen]);

  useEffect(() => {
    if (!isOpen) return;
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape' && !isSubmitting) {
        onClose();
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [isOpen, isSubmitting, onClose]);

  if (!isOpen) return null;

  const handleSubmit = async () => {
    setIsSubmitting(true);
    setSubmitResult(null);
    try {
      if (window.ag?.telemetry?.sendReport) {
        const res = await window.ag.telemetry.sendReport({
          gameId: activeGameId || 'manual_session',
          userNote: note.trim() || 'General beta feedback / telemetry submission.'
        });
        if (res && res.success) {
          setSubmitResult({
            success: true,
            reportId: (res as any).reportId,
            message: res.message || 'Report received and recorded in engineering telemetry.'
          });
        } else {
          setSubmitResult({
            success: false,
            message: res?.message || 'Failed to submit report. Please check connection.'
          });
        }
      } else {
        setSubmitResult({
          success: false,
          message: 'Telemetry bridge unavailable in current environment.'
        });
      }
    } catch (err: any) {
      setSubmitResult({
        success: false,
        message: err?.message || 'Error transmitting report to telemetry API.'
      });
    } finally {
      setIsSubmitting(false);
    }
  };

  const handleCopyReportId = async () => {
    if (!submitResult?.reportId) return;
    try {
      await navigator.clipboard.writeText(submitResult.reportId);
      setCopiedId(true);
      setTimeout(() => setCopiedId(false), 2500);
    } catch {}
  };

  const handleCopyDiscordFormat = async () => {
    const summary = [
      `### NexVR Beta Report [v0.1.90]`,
      `- **Game**: ${activeGameName || activeGameId || 'General Launcher'}`,
      `- **Report ID**: ${submitResult?.reportId || 'Manual'}`,
      `- **Note**: ${note.trim() || 'No note provided'}`,
      `- **Status**: Ready for developer triage`
    ].join('\n');
    try {
      await navigator.clipboard.writeText(summary);
      setCopiedSummary(true);
      setTimeout(() => setCopiedSummary(false), 2500);
    } catch {}
  };

  const handleOpenDiscord = async () => {
    const url = 'https://discord.gg/FBeGjgK2fd';
    try {
      if (window.ag?.shell) {
        await window.ag.shell.openExternal(url);
      } else {
        window.open(url, '_blank', 'noopener,noreferrer');
      }
    } catch {
      window.open(url, '_blank', 'noopener,noreferrer');
    }
  };

  return (
    <div
      onClick={() => {
        if (!isSubmitting) onClose();
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
          maxWidth: 580,
          background: 'rgba(10, 10, 14, 0.98)',
          border: '1px solid #2C2D35',
          borderTop: '2px solid var(--ag-accent)',
          borderRadius: 10,
          boxShadow: '0 28px 70px rgba(0, 0, 0, 0.85), inset 0 1px 0 rgba(255, 255, 255, 0.08)',
          padding: '28px 30px',
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
                background: 'var(--ag-accent)',
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
              SUBMIT BETA FEEDBACK & BUG REPORT
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
            Direct Dual Telemetry · Cloudflare Edge & Discord
          </p>
        </div>

        {submitResult?.success ? (
          /* Success Screen */
          <div style={{ display: 'flex', flexDirection: 'column', gap: 14, padding: '12px 0' }}>
            <div
              style={{
                background: 'rgba(48, 209, 88, 0.08)',
                border: '1px solid rgba(48, 209, 88, 0.3)',
                borderRadius: 8,
                padding: '16px 18px',
                display: 'flex',
                flexDirection: 'column',
                gap: 8
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: 8, color: '#30D158', fontWeight: 800, fontFamily: 'var(--ag-font-display)', fontSize: 13 }}>
                <span>✓</span> REPORT DELIVERED TO ENGINEERING
              </div>
              <p style={{ margin: 0, color: '#D0D2DC', fontSize: 12, lineHeight: 1.5, fontFamily: 'var(--ag-font-ui)' }}>
                Your sanitized logs, graphics state, and feedback have been stored in Cloudflare telemetry and forwarded to the Discord developer channel.
              </p>
              {submitResult.reportId && (
                <div style={{ marginTop: 4, display: 'flex', alignItems: 'center', justifyContent: 'space-between', background: 'rgba(0,0,0,0.4)', padding: '8px 12px', borderRadius: 4, border: '1px solid #333' }}>
                  <code style={{ color: 'var(--ag-accent)', fontFamily: 'var(--ag-font-mono)', fontSize: 11 }}>
                    {submitResult.reportId}
                  </code>
                  <button
                    type="button"
                    onClick={handleCopyReportId}
                    style={{
                      padding: '4px 10px',
                      fontSize: 10.5,
                      fontFamily: 'var(--ag-font-mono)',
                      background: copiedId ? 'var(--ag-accent-success)' : 'rgba(255,255,255,0.06)',
                      color: copiedId ? '#000' : '#FFF',
                      border: '1px solid #444',
                      borderRadius: 4,
                      cursor: 'pointer'
                    }}
                  >
                    {copiedId ? 'COPIED!' : 'COPY ID'}
                  </button>
                </div>
              )}
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 10 }}>
              <button
                type="button"
                onClick={handleCopyDiscordFormat}
                style={{
                  padding: '10px 14px',
                  borderRadius: 'var(--ag-radius-sm)',
                  color: copiedSummary ? 'var(--ag-accent-success)' : 'var(--ag-text-primary)',
                  cursor: 'pointer',
                  fontFamily: 'var(--ag-font-display)',
                  fontSize: 11.5,
                  letterSpacing: '0.04em',
                  fontWeight: 700,
                  border: '1px solid #383A44',
                  background: 'rgba(255, 255, 255, 0.035)',
                  transition: 'all 0.15s ease'
                }}
              >
                {copiedSummary ? '✓ COPIED SUMMARY' : 'COPY DISCORD MSG'}
              </button>

              <button
                type="button"
                onClick={handleOpenDiscord}
                style={{
                  padding: '10px 14px',
                  borderRadius: 'var(--ag-radius-sm)',
                  color: '#FFFFFF',
                  cursor: 'pointer',
                  fontFamily: 'var(--ag-font-display)',
                  fontSize: 11.5,
                  letterSpacing: '0.04em',
                  fontWeight: 800,
                  border: '1px solid #5865F2',
                  background: '#5865F2',
                  transition: 'all 0.15s ease'
                }}
              >
                DISCUSS ON DISCORD
              </button>
            </div>

            <div style={{ display: 'flex', justifyContent: 'flex-end', marginTop: 4 }}>
              <button
                type="button"
                onClick={onClose}
                style={{
                  padding: '8px 22px',
                  borderRadius: 'var(--ag-radius-sm)',
                  color: 'var(--ag-text-primary)',
                  cursor: 'pointer',
                  fontFamily: 'var(--ag-font-display)',
                  fontSize: 11.5,
                  fontWeight: 700,
                  border: '1px solid #383A44',
                  background: 'rgba(255, 255, 255, 0.04)'
                }}
              >
                DONE
              </button>
            </div>
          </div>
        ) : (
          /* Submission Form */
          <div style={{ display: 'flex', flexDirection: 'column', gap: 14 }}>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
              <label
                style={{
                  fontSize: 11.5,
                  fontFamily: 'var(--ag-font-mono)',
                  color: 'var(--ag-text-dim)',
                  letterSpacing: '0.06em',
                  textTransform: 'uppercase'
                }}
              >
                Target Context / Title
              </label>
              <div
                style={{
                  padding: '10px 14px',
                  background: 'rgba(255, 255, 255, 0.025)',
                  border: '1px solid #282932',
                  borderRadius: 6,
                  color: '#FFF',
                  fontSize: 12.5,
                  fontFamily: 'var(--ag-font-ui)'
                }}
              >
                {activeGameName ? `${activeGameName} (${activeGameId})` : 'General Launcher & Engine Session'}
              </div>
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
              <label
                style={{
                  fontSize: 11.5,
                  fontFamily: 'var(--ag-font-mono)',
                  color: 'var(--ag-text-dim)',
                  letterSpacing: '0.06em',
                  textTransform: 'uppercase'
                }}
              >
                Describe the Issue or Experience
              </label>
              <textarea
                value={note}
                onChange={e => setNote(e.target.value)}
                placeholder="What happened? (e.g. Frame jitter during boss fight, black screen on launch, VR view skewed, etc.)"
                rows={4}
                style={{
                  width: '100%',
                  background: 'rgba(255, 255, 255, 0.025)',
                  border: '1px solid #282932',
                  borderRadius: 6,
                  color: '#FFF',
                  fontSize: 12.5,
                  fontFamily: 'var(--ag-font-ui)',
                  padding: '10px 14px',
                  outline: 'none',
                  resize: 'vertical',
                  lineHeight: 1.45
                }}
              />
            </div>

            {/* Privacy Notice */}
            <div
              style={{
                fontSize: 11,
                color: 'var(--ag-text-muted)',
                lineHeight: 1.45,
                background: 'rgba(255, 255, 255, 0.015)',
                padding: '10px 12px',
                borderRadius: 6,
                border: '1px dashed #2E303A'
              }}
            >
              🔒 <strong>Automatic PII Redaction:</strong> Windows user home directories (e.g. <code>C:\Users\[USER]</code>) and local private IP addresses are automatically scrubbed from attached logs before transmission.
            </div>

            {submitResult && !submitResult.success && (
              <div
                style={{
                  padding: '10px 12px',
                  borderRadius: 6,
                  background: 'rgba(255, 68, 68, 0.1)',
                  border: '1px solid rgba(255, 68, 68, 0.3)',
                  color: '#FF6B6B',
                  fontSize: 11.5,
                  fontFamily: 'var(--ag-font-ui)'
                }}
              >
                ⚠️ {submitResult.message}
              </div>
            )}

            {/* Footer Buttons */}
            <div
              style={{
                display: 'flex',
                justifyContent: 'flex-end',
                gap: 10,
                paddingTop: 10,
                borderTop: '1px solid rgba(255, 255, 255, 0.06)'
              }}
            >
              <button
                type="button"
                onClick={onClose}
                disabled={isSubmitting}
                style={{
                  padding: '9px 18px',
                  borderRadius: 'var(--ag-radius-sm)',
                  color: 'var(--ag-text-primary)',
                  cursor: isSubmitting ? 'not-allowed' : 'pointer',
                  fontFamily: 'var(--ag-font-display)',
                  fontSize: 11.5,
                  fontWeight: 700,
                  border: '1px solid #383A44',
                  background: 'rgba(255, 255, 255, 0.04)',
                  transition: 'all 0.15s ease'
                }}
              >
                CANCEL
              </button>

              <button
                type="button"
                onClick={handleSubmit}
                disabled={isSubmitting}
                style={{
                  padding: '9px 24px',
                  borderRadius: 'var(--ag-radius-sm)',
                  color: '#FFFFFF',
                  cursor: isSubmitting ? 'not-allowed' : 'pointer',
                  fontFamily: 'var(--ag-font-display)',
                  fontSize: 11.5,
                  letterSpacing: '0.06em',
                  fontWeight: 800,
                  border: '1px solid var(--ag-accent)',
                  background: 'var(--ag-accent)',
                  opacity: isSubmitting ? 0.6 : 1,
                  boxShadow: '0 4px 14px rgba(204, 0, 0, 0.35)',
                  transition: 'all 0.15s ease'
                }}
              >
                {isSubmitting ? 'TRANSMITTING...' : 'TRANSMIT REPORT'}
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
