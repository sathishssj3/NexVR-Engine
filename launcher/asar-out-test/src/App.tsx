import { useState, useEffect } from 'react';
import type { GameEntry, VRStatus, VRConfig, ScanResult } from './types';
import { Sidebar } from './components/Sidebar';
import { GameDetail } from './components/GameDetail';
import { VRStatusBar } from './components/VRStatusBar';
import { AboutPanel } from './components/AboutPanel';
import './index.css';

declare global {
  interface Window {
    ag: {
      library: {
        scan: () => Promise<ScanResult>;
        addCustom: () => Promise<{ success: boolean }>;
        removeGame: (id: string) => Promise<{ success: boolean }>;
        restoreGame: (id: string) => Promise<{ success: boolean }>;
        ignoreGame: (id: string) => Promise<{ success: boolean }>;
        restoreIgnoredGames: () => Promise<{ success: boolean }>;
      };
      vr: { status: () => Promise<VRStatus> },
      config: {
        read: (id: string) => Promise<VRConfig>,
        write: (id: string, cfg: VRConfig) => Promise<{success: boolean, error?: string}>
      },
      inject: { deploy: (id: string) => Promise<{success: boolean, message: string, pid?: number, cancelled?: boolean}>, cancel: () => Promise<void> },
      log: {
        onLine: (cb: (line: string) => void) => void,
        offLine: () => void,
        export: (lines: unknown) => Promise<{ success: boolean, path: string }>
      },
      window: {
        minimize: () => void,
        maximize: () => void,
        close: () => void
      },
      shell: {
        openExternal: (url: string) => void
      },
      versions: {
        electron: string,
        node: string,
        chrome: string
      }
    }
  }
}

export default function App() {
  const [games, setGames] = useState<GameEntry[]>([]);
  const [waitingGames, setWaitingGames] = useState<GameEntry[]>([]);
  const [selectedGame, setSelectedGame] = useState<GameEntry | null>(null);
  const [vrStatus, setVrStatus] = useState<VRStatus>({ connected: false, runtime: 'Unknown', headset: 'Unknown HMD', refreshRate: 90 });
  const [config, setConfig] = useState<VRConfig | null>(null);
  const [isInjecting, setIsInjecting] = useState(false);
  const [logLines, setLogLines] = useState<string[]>([]);
  const [injectState, setInjectState] = useState<'default' | 'injecting' | 'success' | 'error' | 'cancelled'>('default');
  const [currentTab, setCurrentTab] = useState<'library' | 'about'>('library');

  const scanGames = async () => {
    if (window.ag && window.ag.library) {
      const res = await window.ag.library.scan();
      setGames(res.active);
      setWaitingGames(res.waiting);
    }
  };

  useEffect(() => {
    scanGames();
    const interval = setInterval(async () => {
      const st = await window.ag.vr.status();
      setVrStatus(st);
    }, 5000);
    return () => clearInterval(interval);
  }, []);

  useEffect(() => {
    if (selectedGame) {
      window.ag.config.read(selectedGame.id).then(c => setConfig(c));
    } else {
      setConfig(null);
    }
  }, [selectedGame]);

  const handleConfigChange = async (newConfig: Partial<VRConfig>) => {
    if (!selectedGame) return;
    const merged = { ...config, ...newConfig } as VRConfig;
    setConfig(merged);
    await window.ag.config.write(selectedGame.id, merged);
  };
  
  const handleRemoveGame = async () => {
    if (!selectedGame) return;
    const res = await window.ag.library.removeGame(selectedGame.id);
    if (res.success) {
      setSelectedGame(null);
      scanGames();
    }
  };

  const handleInject = async () => {
    if (!selectedGame) return;
    if (isInjecting) {
       window.ag.inject.cancel();
       return;
    }
    
    setIsInjecting(true);
    setInjectState('injecting');
    setLogLines([]);
    
    window.ag.log.onLine((line) => {
      const hex = '0x' + Math.floor(Math.random() * 65536).toString(16).toUpperCase().padStart(4, '0');
      const time = new Date().toLocaleTimeString('en-US', { hour12: false });
      const formattedLine = `[${hex}] ${time} // ${line}`;
      setLogLines(prev => {
        const next = [...prev, formattedLine];
        if (next.length > 100) next.shift();
        return next;
      });
    });

    const res = await window.ag.inject.deploy(selectedGame.id);
    window.ag.log.offLine();
    if (res.success) {
      setInjectState('success');
    } else if (res.cancelled) {
      setInjectState('cancelled');
    } else {
      setInjectState('error');
    }
    
    setTimeout(() => {
      setInjectState('default');
      setIsInjecting(false);
    }, res.cancelled ? 1500 : 3000);
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', position: 'relative', zIndex: 1 }}>
      <div className="glass-panel" style={{ height: 48, WebkitAppRegion: 'drag', display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '0 0 0 24px', borderBottom: '1px solid var(--ag-border)', borderTop: 'none', borderLeft: 'none', borderRight: 'none', zIndex: 10, boxShadow: '0 4px 20px rgba(0,0,0,0.3), inset 0 -1px 1px rgba(255,255,255,0.03)' } as any}>
        <div style={{ display: 'flex', alignItems: 'center' }}>
          <strong style={{ color: '#fff', letterSpacing: '2.5px', textShadow: '0 0 10px rgba(255,255,255,0.3)' }}>ANTIGRAVITY</strong>
          <span style={{ marginLeft: 12, marginRight: 24, color: 'var(--ag-accent)', fontSize: 11, fontFamily: 'var(--ag-font-mono)', letterSpacing: '1px', textShadow: '0 0 8px rgba(0,240,255,0.4)' }}>v1.0.0 // SYS.CORE</span>
          <div style={{ display: 'flex', gap: 15, WebkitAppRegion: 'no-drag' } as any}>
             <button onClick={() => setCurrentTab('library')} style={{ background: 'transparent', border: 'none', color: currentTab === 'library' ? '#fff' : 'var(--ag-text-muted)', fontFamily: 'var(--ag-font-mono)', fontSize: 12, letterSpacing: '1px', cursor: 'pointer', textShadow: currentTab === 'library' ? '0 0 8px rgba(255,255,255,0.5)' : 'none', padding: 0, outline: 'none' }}>LIBRARY</button>
             <button onClick={() => setCurrentTab('about')} style={{ background: 'transparent', border: 'none', color: currentTab === 'about' ? '#fff' : 'var(--ag-text-muted)', fontFamily: 'var(--ag-font-mono)', fontSize: 12, letterSpacing: '1px', cursor: 'pointer', textShadow: currentTab === 'about' ? '0 0 8px rgba(255,255,255,0.5)' : 'none', padding: 0, outline: 'none' }}>ABOUT</button>
          </div>
        </div>
        
        <div style={{ display: 'flex', height: '100%', WebkitAppRegion: 'no-drag' } as any}>
          <button 
            onClick={() => window.ag.window.minimize()}
            style={{ width: 46, height: '100%', background: 'transparent', border: 'none', color: 'var(--ag-text-muted)', cursor: 'pointer', transition: 'background 0.2s', fontSize: 16, outline: 'none' }}
            onMouseEnter={e => e.currentTarget.style.background = 'rgba(255,255,255,0.1)'}
            onMouseLeave={e => e.currentTarget.style.background = 'transparent'}
          >
            &#x2014;
          </button>
          <button 
            onClick={() => window.ag.window.maximize()}
            style={{ width: 46, height: '100%', background: 'transparent', border: 'none', color: 'var(--ag-text-muted)', cursor: 'pointer', transition: 'background 0.2s', fontSize: 16, outline: 'none' }}
            onMouseEnter={e => e.currentTarget.style.background = 'rgba(255,255,255,0.1)'}
            onMouseLeave={e => e.currentTarget.style.background = 'transparent'}
          >
            &#x25A1;
          </button>
          <button 
            onClick={() => window.ag.window.close()}
            style={{ width: 46, height: '100%', background: 'transparent', border: 'none', color: 'var(--ag-text-muted)', cursor: 'pointer', transition: 'all 0.2s', fontSize: 16, outline: 'none' }}
            onMouseEnter={e => { e.currentTarget.style.background = '#e24b4a'; e.currentTarget.style.color = '#fff'; }}
            onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = 'var(--ag-text-muted)'; }}
          >
            &#x2715;
          </button>
        </div>
      </div>
      
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {currentTab === 'library' ? (
          <>
            <Sidebar 
              games={games} 
              waitingGames={waitingGames}
              selectedId={selectedGame?.id} 
              onSelect={(g: GameEntry) => setSelectedGame(g)} 
              onRescan={scanGames} 
              onRestore={async (id: string) => { await window.ag.library.restoreGame(id); scanGames(); }}
              onIgnore={async (id: string) => { await window.ag.library.ignoreGame(id); scanGames(); }}
              onRestoreIgnored={async () => { await window.ag.library.restoreIgnoredGames(); scanGames(); }}
            />
            <div style={{ flex: 1, padding: '30px 40px', overflowY: 'auto', overflowX: 'hidden' }}>
              {selectedGame && config ? (
                <GameDetail 
                  game={selectedGame} 
                  config={config} 
                  onConfigChange={handleConfigChange}
                  logLines={logLines}
                  onRemoveGame={handleRemoveGame}
                />
              ) : (
                <div style={{ color: 'var(--ag-text-muted)', display: 'flex', height: '100%', alignItems: 'center', justifyContent: 'center', fontFamily: 'var(--ag-font-mono)', letterSpacing: '1px', opacity: 0.5 }}>
                  &gt; AWAITING SELECTION...
                </div>
              )}
            </div>
          </>
        ) : (
          <AboutPanel />
        )}
      </div>

      <VRStatusBar 
        status={vrStatus} 
        selectedGame={selectedGame} 
        isInjecting={isInjecting} 
        injectState={injectState}
        onInject={handleInject} 
      />
      <div className="crt-overlay" />
    </div>
  );
}
