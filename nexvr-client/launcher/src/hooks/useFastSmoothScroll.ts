import { useEffect, type RefObject } from 'react';

interface SmoothScrollOptions {
  speed?: number;       // Multiplier for scroll distance per wheel click (default: 1.65)
  smoothness?: number;  // Easing factor: 0.1 (floaty/slow) to 0.4 (snappy/fast), default: 0.24
}

export function useFastSmoothScroll(
  ref: RefObject<HTMLElement | null>,
  options: SmoothScrollOptions = {}
) {
  const { speed = 1.65, smoothness = 0.24 } = options;

  useEffect(() => {
    const el = ref.current;
    if (!el) return;

    let targetY = el.scrollTop;
    let currentY = el.scrollTop;
    let animationFrameId: number | null = null;
    let isWheeling = false;

    const onWheel = (e: WheelEvent) => {
      // Don't intercept horizontal scrolling or if ctrlKey (zooming)
      if (Math.abs(e.deltaX) > Math.abs(e.deltaY) || e.ctrlKey) return;

      const maxScroll = el.scrollHeight - el.clientHeight;
      if (maxScroll <= 0) return;

      e.preventDefault();

      // Sync if user dragged scrollbar
      if (!isWheeling) {
        targetY = el.scrollTop;
        currentY = el.scrollTop;
        isWheeling = true;
      }

      // Fast accelerated delta
      const delta = e.deltaY * speed;
      targetY = Math.max(0, Math.min(maxScroll, targetY + delta));

      if (animationFrameId === null) {
        animationFrameId = requestAnimationFrame(step);
      }
    };

    const step = () => {
      const diff = targetY - currentY;
      if (Math.abs(diff) < 0.5) {
        el.scrollTop = targetY;
        currentY = targetY;
        isWheeling = false;
        animationFrameId = null;
        return;
      }

      currentY += diff * smoothness;
      el.scrollTop = currentY;
      animationFrameId = requestAnimationFrame(step);
    };

    const onScroll = () => {
      // If user drags scrollbar with mouse, cancel animation and sync
      if (!isWheeling) {
        targetY = el.scrollTop;
        currentY = el.scrollTop;
      }
    };

    el.addEventListener('wheel', onWheel, { passive: false });
    el.addEventListener('scroll', onScroll, { passive: true });

    return () => {
      el.removeEventListener('wheel', onWheel);
      el.removeEventListener('scroll', onScroll);
      if (animationFrameId !== null) {
        cancelAnimationFrame(animationFrameId);
      }
    };
  }, [ref, speed, smoothness]);
}
