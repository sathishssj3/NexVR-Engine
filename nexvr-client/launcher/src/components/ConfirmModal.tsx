import { useEffect } from 'react';

export interface ConfirmModalProps {
  isOpen: boolean;
  title: string;
  description: string;
  confirmText?: string;
  cancelText?: string;
  variant?: 'danger' | 'warning' | 'info';
  onConfirm: () => void;
  onCancel: () => void;
}

export function ConfirmModal({
  isOpen,
  title,
  description,
  confirmText = 'CONFIRM',
  cancelText = 'CANCEL',
  variant = 'danger',
  onConfirm,
  onCancel
}: ConfirmModalProps) {
  useEffect(() => {
    if (!isOpen) return;
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        onCancel();
      } else if (e.key === 'Enter') {
        onConfirm();
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [isOpen, onConfirm, onCancel]);

  if (!isOpen) return null;

  const accentColor = variant === 'danger'
    ? '#CC0000'
    : variant === 'warning'
      ? '#FF9F0A'
      : '#30D158';

  const confirmBg = variant === 'danger'
    ? 'linear-gradient(135deg, #FF1A1A 0%, #CC0000 55%, #990000 100%)'
    : variant === 'warning'
      ? 'linear-gradient(135deg, #FFB340 0%, #FF9F0A 55%, #D97706 100%)'
      : 'linear-gradient(135deg, #34D399 0%, #10B981 55%, #059669 100%)';

  const confirmBorder = variant === 'danger'
    ? '#FF4D4D'
    : variant === 'warning'
      ? '#FFB340'
      : '#6EE7B7';

  return (
    <div
      onClick={onCancel}
      style={{
        position: 'fixed',
        inset: 0,
        zIndex: 1000,
        background: 'rgba(0, 0, 0, 0.72)',
        backdropFilter: 'blur(16px)',
        WebkitBackdropFilter: 'blur(16px)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        padding: 24,
        animation: 'modalBackdropFade 0.2s cubic-bezier(0.16, 1, 0.3, 1) both'
      }}
    >
      <div
        onClick={e => e.stopPropagation()}
        style={{
          width: '100%',
          maxWidth: 440,
          background: 'rgba(10, 10, 14, 0.96)',
          border: '1px solid #2C2D35',
          borderTop: `2px solid ${accentColor}`,
          borderRadius: 8,
          boxShadow: '0 24px 60px rgba(0, 0, 0, 0.8), inset 0 1px 0 rgba(255, 255, 255, 0.08)',
          padding: '28px 30px',
          animation: 'modalContentScale 0.22s cubic-bezier(0.16, 1, 0.3, 1) both',
          display: 'flex',
          flexDirection: 'column',
          gap: 16
        }}
      >
        {/* Header with Indicator */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
          <span
            style={{
              width: 4,
              height: 18,
              borderRadius: 2,
              background: accentColor,
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
            {title}
          </h2>
        </div>

        {/* Description Body */}
        <div
          style={{
            fontSize: 12.5,
            lineHeight: 1.6,
            fontFamily: 'var(--ag-font-ui)',
            color: '#A0A3B1',
            whiteSpace: 'pre-line'
          }}
        >
          {description}
        </div>

        {/* Action Buttons */}
        <div
          style={{
            display: 'flex',
            justifyContent: 'flex-end',
            gap: 10,
            marginTop: 8,
            paddingTop: 16,
            borderTop: '1px solid rgba(255, 255, 255, 0.06)'
          }}
        >
          <button
            type="button"
            onClick={onCancel}
            style={{
              background: 'rgba(255, 255, 255, 0.035)',
              border: '1px solid rgba(255, 255, 255, 0.12)',
              color: '#C0C0C8',
              borderRadius: 'var(--ag-radius-sm)',
              padding: '9px 18px',
              fontFamily: 'var(--ag-font-display)',
              fontSize: 11.5,
              fontWeight: 700,
              letterSpacing: '0.06em',
              cursor: 'pointer',
              transition: 'all 0.15s ease',
              boxShadow: 'inset 0 1px 0 rgba(255, 255, 255, 0.04)'
            }}
            onMouseEnter={e => {
              e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.3)';
              e.currentTarget.style.color = '#FFFFFF';
            }}
            onMouseLeave={e => {
              e.currentTarget.style.borderColor = 'rgba(255, 255, 255, 0.12)';
              e.currentTarget.style.color = '#C0C0C8';
            }}
          >
            {cancelText}
          </button>

          <button
            type="button"
            onClick={onConfirm}
            style={{
              background: confirmBg,
              border: `1px solid ${confirmBorder}`,
              color: '#FFFFFF',
              borderRadius: 'var(--ag-radius-sm)',
              padding: '9px 22px',
              fontFamily: 'var(--ag-font-display)',
              fontSize: 11.5,
              fontWeight: 800,
              letterSpacing: '0.08em',
              cursor: 'pointer',
              transition: 'all 0.15s ease',
              boxShadow: variant === 'danger'
                ? '0 0 16px rgba(204, 0, 0, 0.4), inset 0 1px 0 rgba(255, 255, 255, 0.3)'
                : '0 0 16px rgba(255, 159, 10, 0.35), inset 0 1px 0 rgba(255, 255, 255, 0.3)'
            }}
            onMouseEnter={e => {
              e.currentTarget.style.opacity = '0.92';
            }}
            onMouseLeave={e => {
              e.currentTarget.style.opacity = '1';
            }}
          >
            {confirmText}
          </button>
        </div>
      </div>
    </div>
  );
}
