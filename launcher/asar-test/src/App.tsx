import { useState, useEffect } from 'react';
import { GameEntry, VRStatus, VRConfig } from './types';
import { Sidebar } from './components/Sidebar';
import { GameDetail } from './components/GameDetail';
import { VRStatusBar } from './components/VRStatusBar';
import './index.css';

declare global {
  interface Window {
    ag: {
      steam: { scan: () => Promise<GameEntry[]> },
      vr: { status: () => Promise<VRStatus> },
      config: {
        read: (id: string) => Promise<VRConfig>,
        write: (id: string, cfg: VRConfig) => Promise<{success: boolean, error?: string}>
      },
      inject: { deploy: (id: string) => Promise<{success: boolean, message: string, pid?: number}> },
      log: {
        onLine: (cb: (line: string) => void) => void,
        offLine: () => void
      }
    }
  }
}

export default function App() {
  const [games, setGames] = useState<GameEntry[]>([]);
  const [selectedGame, setSelectedGame] = useState<GameEntry | null>(null);
  const [vrStatus, setVrStatus] = useState<VRStatus>({ connected: false, runtime: 'Unknown', headset: 'Unknown HMD', refreshRate: 90 });
  const [config, setConfig] = useState<VRConfig | null>(null);
  const [isInjecting, setIsInjecting] = useState(false);
  const [logLines, setLogLines] = useState<string[]>([]);
  const [injectState, setInjectState] = useState<'default' | 'injecting' | 'success' | 'error'>('default');

  const scanGames = async () => {
    const g = await window.ag.steam.scan();
    setGames(g);
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

  const handleConfigChange = (newConfig: VRConfig) => {
    setConfig(newConfig);
    if (selectedGame) {
      // Basic debounce would be better here, but we will write immediately for mockup
      window.ag.config.write(selectedGame.id, newConfig);
    }
  };

  const handleInject = async () => {
    if (!selectedGame || isInjecting) return;
    setIsInjecting(true);
    setInjectState('injecting');
    setLogLines([]);
    
    window.ag.log.onLine((line) => {
      setLogLines(prev => {
        const next = [...prev, line];
        if (next.length > 100) next.shift();
        return next;
      });
    });

    const res = await window.ag.inject.deploy(selectedGame.id);
    if (res.success) {
      setInjectState('success');
    } else {
      setInjectState('error');
    }
    
    setTimeout(() => {
      setInjectState('default');
      setIsInjecting(false);
    }, 3000);
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div style={{ height: 40, WebkitAppRegion: 'drag', display: 'flex', alignItems: 'center', padding: '0 20px', borderBottom: '1px solid var(--ag-border)' } as any}>
        <strong style={{ color: 'var(--ag-accent)' }}>ANTIGRAVITY</strong>
        <span style={{ marginLeft: 10, color: 'var(--ag-text-muted)', fontSize: 12 }}>v1.0</span>
      </div>
      
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        <Sidebar 
          games={games} 
          selectedId={selectedGame?.id} 
          onSelect={g => setSelectedGame(g)} 
          onRescan={scanGames} 
        />
        <div style={{ flex: 1, padding: 20, overflowY: 'auto' }}>
          {selectedGame && config ? (
            <GameDetail 
              game={selectedGame} 
              config={config} 
              onConfigChange={handleConfigChange}
              logLines={logLines}
            />
          ) : (
            <div style={{ color: 'var(--ag-text-muted)', display: 'flex', height: '100%', alignItems: 'center', justifyContent: 'center' }}>
              Select a game from the library
            </div>
          )}
        </div>
      </div>

      <VRStatusBar 
        status={vrStatus} 
        selectedGame={selectedGame} 
        isInjecting={isInjecting} 
        injectState={injectState}
        onInject={handleInject} 
      />
    </div>
  );
}
