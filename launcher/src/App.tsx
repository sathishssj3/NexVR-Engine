import { useState, useEffect, useRef } from 'react';
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
      utils: {
        openConfig: (id: string) => Promise<void>,
        openLog: (id: string) => Promise<void>
      },
      inject: { deploy: (id: string) => Promise<{success: boolean, message: string, pid?: number, cancelled?: boolean}>, cancel: () => Promise<void>, monitor: (pid: number) => Promise<void> },
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
  const [logLines, setLogLines] = useState<string[]>([]);
  const [injectState, setInjectState] = useState<'default' | 'injecting' | 'success' | 'running' | 'error' | 'cancelled'>('default');
  const [currentTab, setCurrentTab] = useState<'library' | 'about'>('library');
  const [transitioning, setTransitioning] = useState(false);
  const injectTokenRef = useRef<number>(0);
  const mainContentRef = useRef<HTMLDivElement>(null);

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
    
    // Ensure we scroll to top AFTER the render completes
    requestAnimationFrame(() => {
      if (mainContentRef.current) {
        mainContentRef.current.scrollTop = 0;
      }
    });
  }, [selectedGame]);

  const handleSelectGame = (g: GameEntry) => {
    if (g.id === selectedGame?.id) return;
    setTransitioning(true);
    setTimeout(() => {
      setSelectedGame(g);
      setTransitioning(false);
      requestAnimationFrame(() => {
        if (mainContentRef.current) {
          mainContentRef.current.scrollTop = 0;
        }
      });
    }, 150);
  };

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
    if (injectState === 'injecting' || injectState === 'running') {
       injectTokenRef.current = 0;
       window.ag.inject.cancel();
       setInjectState('cancelled');
       setTimeout(() => {
         setInjectState('default');
       }, 1500);
       return;
    }
    
    const token = Date.now();
    injectTokenRef.current = token;
    
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
    
    if (injectTokenRef.current !== token) return;
    
    window.ag.log.offLine();
    if (res.success && res.pid) {
      setInjectState('success');
      setTimeout(async () => {
        if (injectTokenRef.current !== token) return;
        setInjectState('running');
        
        await window.ag.inject.monitor(res.pid);
        
        if (injectTokenRef.current === token) {
          setInjectState('default');
        }
      }, 3000);
    } else if (res.cancelled) {
      setInjectState('cancelled');
      setTimeout(() => {
        setInjectState('default');
      }, 1500);
    } else {
      setInjectState('error');
      setTimeout(() => {
        setInjectState('default');
      }, 3000);
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', position: 'relative', zIndex: 1 }}>
      {/* Title Bar */}
      <div className="glass-panel" style={{ 
        height: 46, WebkitAppRegion: 'drag', display: 'flex', alignItems: 'center', 
        justifyContent: 'space-between', padding: '0 0 0 22px', 
        borderBottom: '1px solid var(--ag-border)', 
        borderTop: 'none', borderLeft: 'none', borderRight: 'none', zIndex: 10, 
        boxShadow: '0 4px 20px rgba(0,0,0,0.3), inset 0 -1px 1px rgba(255,255,255,0.03)' 
      } as any}>
        <div style={{ display: 'flex', alignItems: 'center' }}>
          <strong style={{ 
            color: '#fff', letterSpacing: '3px', fontSize: 13,
            fontFamily: 'var(--ag-font-display)',
            textShadow: '0 0 10px rgba(255,255,255,0.2)' 
          }}>
            NEX/<span style={{ marginLeft: '-0.15em' }}>R</span> ENGINE
          </strong>
          <span style={{ 
            marginLeft: 10, marginRight: 24, color: 'var(--ag-accent)', fontSize: 10, 
            fontFamily: 'var(--ag-font-mono)', letterSpacing: '1px', 
            textShadow: '0 0 8px rgba(0,240,255,0.4)', opacity: 0.7 
          }}>
            v0.1.0
          </span>
          <div style={{ display: 'flex', gap: 2, WebkitAppRegion: 'no-drag' } as any}>
             {(['library', 'about'] as const).map(tab => (
               <button 
                 key={tab}
                 onClick={() => setCurrentTab(tab)} 
                 style={{ 
                   background: currentTab === tab ? 'rgba(0,240,255,0.08)' : 'transparent', 
                   border: 'none', 
                   borderBottom: currentTab === tab ? '2px solid var(--ag-accent)' : '2px solid transparent',
                   color: currentTab === tab ? '#fff' : 'var(--ag-text-muted)', 
                   fontFamily: 'var(--ag-font-mono)', fontSize: 11, letterSpacing: '1.5px', 
                   cursor: 'pointer', 
                   textShadow: currentTab === tab ? '0 0 8px rgba(255,255,255,0.4)' : 'none', 
                   padding: '0 14px', outline: 'none', height: 46,
                   transition: 'all 0.3s var(--ag-transition)'
                 }}
               >
                 {tab.toUpperCase()}
               </button>
             ))}
          </div>
        </div>
        
        {/* Window Controls */}
        <div style={{ display: 'flex', height: '100%', WebkitAppRegion: 'no-drag' } as any}>
          <button 
            onClick={() => window.ag.window.minimize()}
            style={{ width: 44, height: '100%', background: 'transparent', border: 'none', color: 'var(--ag-text-muted)', cursor: 'pointer', transition: 'all 0.2s', fontSize: 14, outline: 'none' }}
            onMouseEnter={e => { e.currentTarget.style.background = 'rgba(255,255,255,0.08)'; e.currentTarget.style.color = '#fff'; }}
            onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = 'var(--ag-text-muted)'; }}
          >
            &#x2014;
          </button>
          <button 
            onClick={() => window.ag.window.maximize()}
            style={{ width: 44, height: '100%', background: 'transparent', border: 'none', color: 'var(--ag-text-muted)', cursor: 'pointer', transition: 'all 0.2s', fontSize: 14, outline: 'none' }}
            onMouseEnter={e => { e.currentTarget.style.background = 'rgba(255,255,255,0.08)'; e.currentTarget.style.color = '#fff'; }}
            onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = 'var(--ag-text-muted)'; }}
          >
            &#x25A1;
          </button>
          <button 
            onClick={() => window.ag.window.close()}
            style={{ width: 44, height: '100%', background: 'transparent', border: 'none', color: 'var(--ag-text-muted)', cursor: 'pointer', transition: 'all 0.2s', fontSize: 14, outline: 'none' }}
            onMouseEnter={e => { e.currentTarget.style.background = '#e24b4a'; e.currentTarget.style.color = '#fff'; }}
            onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = 'var(--ag-text-muted)'; }}
          >
            &#x2715;
          </button>
        </div>
      </div>
      
      {/* Main Content */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {currentTab === 'library' ? (
          <>
            <Sidebar 
              games={games} 
              waitingGames={waitingGames}
              selectedId={selectedGame?.id} 
              onSelect={handleSelectGame} 
              onRescan={scanGames} 
              onRestore={async (id: string) => { await window.ag.library.restoreGame(id); scanGames(); }}
              onIgnore={async (id: string) => { await window.ag.library.ignoreGame(id); scanGames(); }}
              onRestoreIgnored={async () => { await window.ag.library.restoreIgnoredGames(); scanGames(); }}
            />
            <div 
              ref={mainContentRef}
              style={{ 
              flex: 1, padding: '28px 36px', overflowY: 'auto', overflowX: 'hidden',
              opacity: transitioning ? 0 : 1,
              transform: transitioning ? 'translateY(6px)' : 'translateY(0)',
              transition: 'opacity 0.2s ease, transform 0.2s ease'
            }}>
              {selectedGame && config ? (
                <GameDetail 
                  key={selectedGame.id}
                  game={selectedGame} 
                  config={config} 
                  onConfigChange={handleConfigChange}
                  logLines={logLines}
                  onRemoveGame={handleRemoveGame}
                />
              ) : (
                <div className="empty-state">
                  <div className="icon">⬡</div>
                  <div className="text">&gt; AWAITING SELECTION...</div>
                  <div className="hint">Select a game from the sidebar to configure VR settings</div>
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
        injectState={injectState}
        onInject={handleInject} 
      />
      <div className="crt-overlay" />
    </div>
  );
}
