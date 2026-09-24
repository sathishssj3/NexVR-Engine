import { useState, useEffect, useRef } from 'react';
import type { GameEntry, VRStatus, VRConfig, ScanResult, UpdateStatus } from './types';
import { Sidebar } from './components/Sidebar';
import { GameDetail } from './components/GameDetail';
import { VRStatusBar } from './components/VRStatusBar';
import { AboutPanel } from './components/AboutPanel';
import { SettingsView } from './components/SettingsView';
import { HeroCommandCenter } from './components/HeroCommandCenter';
import { ConfirmModal } from './components/ConfirmModal';
import { LegalModal } from './components/LegalModal';
import { useFastSmoothScroll } from './hooks/useFastSmoothScroll';
import './index.css';

declare global {
  interface Window {
    ag: {
      library: {
        scan: () => Promise<ScanResult>;
        scanCached: () => Promise<ScanResult & { fromCache: boolean }>;
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
        openLog: (id?: string) => Promise<boolean | void>,
        openLogFolder: () => Promise<boolean | void>,
      },
      inject: {
        deploy: (id: string) => Promise<{success: boolean, message: string, pid?: number, cancelled?: boolean}>,
        cancel: () => Promise<void>,
        monitor: (pid: number) => Promise<void>,
        uninstall: (id: string) => Promise<{success: boolean, message: string}>
      },
      update: {
        check: () => Promise<UpdateStatus>,
        getStatus: () => Promise<{ version: string, timestamp: number, changelog: string, features?: string[], fixes?: string[] }>,
        openFolder: () => Promise<void>
      },
      log: {
        onLine: (cb: (line: string) => void) => void,
        offLine: () => void,
        export: (lines: unknown) => Promise<{ success: boolean, path: string }>
      },
      telemetry: {
        sendReport: (options?: { gameId?: string; userNote?: string }) => Promise<{ success: boolean; message?: string }>
      },
      window: {
        minimize: () => void,
        maximize: () => void,
        close: () => void,
        isMaximized?: () => Promise<boolean>,
        onMaximizedChange?: (callback: (isMaximized: boolean) => void) => (() => void),
      },
      shell: {
        openExternal: (url: string) => Promise<void> | void
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
  const [vrStatus, setVrStatus] = useState<VRStatus>({ connected: false, runtime: 'Unknown', headset: 'No Headset Connected', refreshRate: 90 });
  const [config, setConfig] = useState<VRConfig | null>(null);
  const [logLines, setLogLines] = useState<string[]>([]);
  const [injectState, setInjectState] = useState<'default' | 'injecting' | 'success' | 'running' | 'error' | 'cancelled'>('default');
  const [currentTab, setCurrentTab] = useState<'library' | 'settings' | 'about'>('library');
  const [transitioning, setTransitioning] = useState(false);
  const [hasConsented, setHasConsented] = useState<boolean>(() => localStorage.getItem('ag_ac_consent') === 'true');
  const [updateStatus, setUpdateStatus] = useState<UpdateStatus | null>(null);
  const [isMaximized, setIsMaximized] = useState(true);
  const [tabAnimNonce, setTabAnimNonce] = useState(0);
  const [modalState, setModalState] = useState<{
    isOpen: boolean;
    title: string;
    description: string;
    confirmText?: string;
    cancelText?: string;
    variant?: 'danger' | 'warning' | 'info';
    onConfirm: () => void;
  }>({
    isOpen: false,
    title: '',
    description: '',
    onConfirm: () => {},
  });
  const closeModal = () => setModalState(prev => ({ ...prev, isOpen: false }));

  const [legalModalOpen, setLegalModalOpen] = useState<boolean>(() => {
    return localStorage.getItem('nexvr_beta_eula_accepted') !== 'true';
  });
  const [isLegalFirstRun, setIsLegalFirstRun] = useState<boolean>(() => {
    return localStorage.getItem('nexvr_beta_eula_accepted') !== 'true';
  });

  const handleAcceptLegal = () => {
    localStorage.setItem('nexvr_beta_eula_accepted', 'true');
    localStorage.setItem('nexvr_beta_eula_timestamp', Date.now().toString());
    localStorage.setItem('ag_ac_consent', 'true');
    setHasConsented(true);
    setLegalModalOpen(false);
  };
  const injectTokenRef = useRef<number>(0);
  const mainContentRef = useRef<HTMLDivElement>(null);
  const logQueueRef = useRef<string[]>([]);
  const rafIdRef = useRef<number | null>(null);

  // Fast & smooth accelerated scrolling
  useFastSmoothScroll(mainContentRef, { speed: 1.7, smoothness: 0.25 }, [currentTab, tabAnimNonce]);

  // Synchronize window maximized state with Electron main process
  useEffect(() => {
    if (window.ag?.window?.isMaximized) {
      window.ag.window.isMaximized().then(setIsMaximized).catch(() => {});
    }
    const unsub = window.ag?.window?.onMaximizedChange?.((max) => {
      setIsMaximized(max);
    });
    return () => unsub?.();
  }, []);

  const scanGames = async () => {
    if (window.ag && window.ag.library) {
      const res = await window.ag.library.scan();
      setGames(res.active);
      setWaitingGames(res.waiting);
    }
  };

  useEffect(() => {
    // === OPTIMIZED STARTUP SEQUENCE ===
    // 1. Load cached library data instantly (sub-5ms) so UI renders immediately
    // 2. Fetch VR status right away (don't wait for first 5s interval)
    // 3. Trigger full background scan to refresh data

    // Step 1: Instant cached library render
    if (window.ag?.library?.scanCached) {
      window.ag.library.scanCached().then((cached) => {
        if (cached.active.length > 0 || cached.waiting.length > 0) {
          setGames(cached.active);
          setWaitingGames(cached.waiting);
        }
        // Step 3: Background full scan (updates cache + refreshes UI)
        scanGames();
      }).catch(() => {
        scanGames();
      });
    } else {
      scanGames();
    }

    // Step 2: Immediate VR status fetch
    if (window.ag?.vr) {
      window.ag.vr.status().then(st => setVrStatus(st)).catch(() => {});
    }

    // VR status polling (every 5s)
    const interval = setInterval(async () => {
      if (window.ag?.vr) {
        const st = await window.ag.vr.status();
        setVrStatus(st);
      }
    }, 5000);

    // Query OTA hotfix status
    if (window.ag?.update) {
      window.ag.update.getStatus().then((st) => {
        if (st) {
          setUpdateStatus({
            checking: false,
            hasUpdate: false,
            updated: st.timestamp > 0,
            version: st.version || '0.1.29',
            changelog: st.changelog,
            features: st.features,
            fixes: st.fixes,
          });
        }
      }).catch(() => {});
    }

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
  
  const handleRemoveGame = () => {
    if (!selectedGame) return;
    setModalState({
      isOpen: true,
      title: 'REMOVE FROM LIBRARY',
      description: `Remove "${selectedGame.name}" from your active library?\n\nThis will not delete your game installation or files. You can restore hidden titles at any time in Settings > Library Utilities.`,
      confirmText: 'REMOVE TITLE',
      cancelText: 'CANCEL',
      variant: 'danger',
      onConfirm: async () => {
        closeModal();
        const res = await window.ag.library.removeGame(selectedGame.id);
        if (res.success) {
          setSelectedGame(null);
          scanGames();
        }
      }
    });
  };

  const handleUninstallMod = () => {
    if (!selectedGame) return;
    setModalState({
      isOpen: true,
      title: 'RESTORE FLAT SCREEN',
      description: `Restore "${selectedGame.name}" to standard flat screen mode?\n\nThis will safely remove vrinject.dll and compiled shaders from the game folder so it launches normally without VR hooks.`,
      confirmText: 'RESTORE FLAT',
      cancelText: 'KEEP VR MOD',
      variant: 'warning',
      onConfirm: async () => {
        closeModal();
        const res = await window.ag.inject.uninstall(selectedGame.id);
        if (res.success) {
          scanGames();
        } else {
          setModalState({
            isOpen: true,
            title: 'RESTORATION NOTICE',
            description: res.message,
            confirmText: 'OK',
            variant: 'danger',
            onConfirm: closeModal
          });
        }
      }
    });
  };

  const handleInject = () => {
    if (!selectedGame) return;

    if (selectedGame.hasAntiCheat) {
      setModalState({
        isOpen: true,
        title: 'INJECTION BLOCKED FOR SAFETY',
        description: `"${selectedGame.name}" is protected by ${selectedGame.antiCheatName || 'Anti-Cheat'}.\n\nInjecting custom DLLs into anti-cheat protected titles is strictly disabled to prevent multiplayer account bans.`,
        confirmText: 'I UNDERSTAND',
        cancelText: 'CLOSE',
        variant: 'danger',
        onConfirm: closeModal
      });
      return;
    }

    if (!vrStatus.connected) {
      setModalState({
        isOpen: true,
        title: 'HEADSET NOT DETECTED',
        description: `Your OpenXR runtime does not detect an active VR headset.\n\nPlease verify that your headset is powered on and SteamVR, Quest Link, or Virtual Desktop is running.\n\nDo you want to initialize VR injection anyway?`,
        confirmText: 'LAUNCH ANYWAY',
        cancelText: 'CANCEL',
        variant: 'warning',
        onConfirm: () => {
          closeModal();
          checkConsentAndInject();
        }
      });
      return;
    }

    checkConsentAndInject();
  };

  const checkConsentAndInject = () => {
    if (!hasConsented) {
      setModalState({
        isOpen: true,
        title: 'ANTI-CHEAT SAFETY NOTICE',
        description: `Injecting custom DLLs into multiplayer games protected by Anti-Cheat software (e.g., Easy Anti-Cheat, BattlEye, Vanguard) is strictly prohibited and can result in permanent account bans.\n\nNexVR Engine explicitly refuses to inject when these systems are detected, but the risk remains with online multiplayer titles.\n\nBy proceeding, you acknowledge this risk and agree to only use NexVR Engine with single-player or unprotected titles.`,
        confirmText: 'ACCEPT & CONTINUE',
        cancelText: 'CANCEL',
        variant: 'warning',
        onConfirm: () => {
          closeModal();
          localStorage.setItem('ag_ac_consent', 'true');
          setHasConsented(true);
          startInjectionSequence();
        }
      });
      return;
    }

    startInjectionSequence();
  };

  const startInjectionSequence = async () => {
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
    logQueueRef.current = [];
    if (rafIdRef.current !== null) {
      cancelAnimationFrame(rafIdRef.current);
      rafIdRef.current = null;
    }
    
    window.ag.log.onLine((line) => {
      const hex = '0x' + Math.floor(Math.random() * 65536).toString(16).toUpperCase().padStart(4, '0');
      const time = new Date().toLocaleTimeString('en-US', { hour12: false });
      const formattedLine = `[${hex}] ${time} // ${line}`;
      logQueueRef.current.push(formattedLine);

      if (rafIdRef.current === null) {
        rafIdRef.current = requestAnimationFrame(() => {
          if (logQueueRef.current.length > 0) {
            const batch = logQueueRef.current.splice(0, logQueueRef.current.length);
            setLogLines(prev => {
              const combined = [...prev, ...batch];
              return combined.length > 300 ? combined.slice(combined.length - 300) : combined;
            });
          }
          rafIdRef.current = null;
        });
      }
    });

    const res = await window.ag.inject.deploy(selectedGame.id);
    
    if (injectTokenRef.current !== token) {
      if (rafIdRef.current !== null) {
        cancelAnimationFrame(rafIdRef.current);
        rafIdRef.current = null;
      }
      logQueueRef.current = [];
      window.ag.log.offLine();
      return;
    }
    
    if (res.success && res.pid !== undefined) {
      setInjectState('success');
      setTimeout(async () => {
        if (injectTokenRef.current !== token) {
          if (rafIdRef.current !== null) {
            cancelAnimationFrame(rafIdRef.current);
            rafIdRef.current = null;
          }
          logQueueRef.current = [];
          window.ag.log.offLine();
          return;
        }
        setInjectState('running');
        
        if (res.pid !== undefined) {
          await window.ag.inject.monitor(res.pid);
        }
        
        // Game process terminated - detach log listener
        if (rafIdRef.current !== null) {
          cancelAnimationFrame(rafIdRef.current);
          rafIdRef.current = null;
        }
        logQueueRef.current = [];
        window.ag.log.offLine();
        if (injectTokenRef.current === token) {
          setInjectState('default');
        }
      }, 3000);
    } else if (res.cancelled) {
      if (rafIdRef.current !== null) {
        cancelAnimationFrame(rafIdRef.current);
        rafIdRef.current = null;
      }
      logQueueRef.current = [];
      window.ag.log.offLine();
      setInjectState('cancelled');
      setLogLines(prev => [...prev, `[ERROR] Injection Cancelled: ${res.message}`]);
      setTimeout(() => {
        setInjectState('default');
      }, 1500);
    } else {
      if (rafIdRef.current !== null) {
        cancelAnimationFrame(rafIdRef.current);
        rafIdRef.current = null;
      }
      logQueueRef.current = [];
      window.ag.log.offLine();
      setInjectState('error');
      setLogLines(prev => [...prev, `[ERROR] Injection Failed: ${res.message}`]);
      setTimeout(() => {
        setInjectState('default');
      }, 3000);
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', position: 'relative', zIndex: 1 }}>
      {/* Title Bar */}
      <div 
        className="glass-panel" 
        onDoubleClick={() => window.ag?.window?.maximize()}
        style={{ 
          height: 46, WebkitAppRegion: 'drag', display: 'flex', alignItems: 'center', 
          justifyContent: 'space-between', padding: '0 0 0 22px', 
          borderBottom: '1px solid var(--ag-border)', 
          borderTop: 'none', borderLeft: 'none', borderRight: 'none', zIndex: 10, 
          boxShadow: '0 4px 20px rgba(0,0,0,0.3), inset 0 -1px 1px rgba(255,255,255,0.03)' 
        } as any}
      >
        <div style={{ display: 'flex', alignItems: 'center' }}>
          <strong style={{ 
            color: 'var(--ag-text-primary)', letterSpacing: '0.06em', fontSize: 13,
            fontFamily: 'var(--ag-font-display)',
            fontWeight: 700
          }}>
            NEXVR ENGINE
          </strong>
          <span style={{ 
            marginLeft: 8, marginRight: 16, 
            color: updateStatus?.updated ? 'var(--ag-accent-success)' : 'var(--ag-accent)', 
            fontSize: 11.5, 
            fontFamily: 'var(--ag-font-mono)', letterSpacing: '0.05em', 
            fontWeight: 800
          }}>
            {updateStatus?.version ? updateStatus.version.replace(/^v/, '') : '0.1.90'}
          </span>
          {updateStatus?.updated && (
            <span style={{
              display: 'inline-flex', alignItems: 'center', gap: 5,
              padding: '2px 8px', borderRadius: 3, fontSize: 9,
              background: 'rgba(48, 209, 88, 0.12)', border: '1px solid rgba(48, 209, 88, 0.3)',
              color: 'var(--ag-accent-success)', fontFamily: 'var(--ag-font-mono)',
              marginRight: 16, letterSpacing: '0.05em'
            }}>
              <span style={{ width: 5, height: 5, borderRadius: '50%', background: 'var(--ag-accent-success)' }} />
              HOTFIX ACTIVE
            </span>
          )}
          <div style={{ display: 'flex', gap: 2, height: '100%', WebkitAppRegion: 'no-drag' } as any}>
             {(['library', 'settings', 'about'] as const).map(tab => {
               const isActive = currentTab === tab;
               return (
                 <button 
                   key={tab}
                   onClick={() => {
                     setCurrentTab(tab);
                     setTabAnimNonce(prev => prev + 1);
                   }} 
                   style={{ 
                     position: 'relative',
                     background: isActive ? 'linear-gradient(180deg, transparent 60%, rgba(204, 0, 0, 0.06) 100%)' : 'transparent', 
                     border: 'none', 
                     color: isActive ? 'var(--ag-text-primary)' : 'var(--ag-text-muted)', 
                     fontFamily: 'var(--ag-font-display)', fontSize: 11, letterSpacing: '0.1em', 
                     fontWeight: 600,
                     cursor: 'pointer', 
                     padding: '0 16px', outline: 'none', height: 46,
                     transition: 'all 0.2s var(--ag-transition)',
                     display: 'flex',
                     alignItems: 'center',
                     justifyContent: 'center'
                   }}
                 >
                   {tab.toUpperCase()}
                   {isActive && (
                     <span 
                       key={`${tab}-${tabAnimNonce}`}
                       className="tab-active-indicator"
                       style={{
                         position: 'absolute',
                         bottom: 0,
                         left: 0,
                         right: 0,
                         height: 2,
                         background: 'var(--ag-accent)',
                       }}
                     />
                   )}
                 </button>
               );
             })}
          </div>
        </div>
        
        {/* Custom Modern Clean Window Controls */}
        <div style={{ display: 'flex', height: '100%', WebkitAppRegion: 'no-drag' } as any}>
          <button 
            onClick={() => window.ag?.window?.minimize()}
            title="Minimize"
            style={{ 
              width: 46, height: '100%', 
              background: 'transparent', border: 'none', 
              color: 'var(--ag-text-muted)', cursor: 'pointer', 
              transition: 'all 0.15s ease', outline: 'none',
              display: 'flex', alignItems: 'center', justifyContent: 'center'
            }}
            onMouseEnter={e => { e.currentTarget.style.background = 'rgba(255,255,255,0.08)'; e.currentTarget.style.color = '#FFF'; }}
            onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = 'var(--ag-text-muted)'; }}
          >
            <svg width="10" height="1" viewBox="0 0 10 1" style={{ display: 'block' }}>
              <rect width="10" height="1" fill="currentColor" />
            </svg>
          </button>
          <button 
            onClick={() => window.ag?.window?.maximize()}
            title={isMaximized ? "Restore Down" : "Maximize"}
            style={{ 
              width: 46, height: '100%', 
              background: 'transparent', border: 'none', 
              color: 'var(--ag-text-muted)', cursor: 'pointer', 
              transition: 'all 0.15s ease', outline: 'none',
              display: 'flex', alignItems: 'center', justifyContent: 'center'
            }}
            onMouseEnter={e => { e.currentTarget.style.background = 'rgba(255,255,255,0.08)'; e.currentTarget.style.color = '#FFF'; }}
            onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = 'var(--ag-text-muted)'; }}
          >
            {isMaximized ? (
              <svg width="10" height="10" viewBox="0 0 10 10" style={{ display: 'block' }}>
                <path d="M2.5 2V0.75H9.25V7.5H8" fill="none" stroke="currentColor" strokeWidth="1.1" />
                <rect x="0.75" y="2.25" width="7" height="7" fill="none" stroke="currentColor" strokeWidth="1.1" rx="0.5" />
              </svg>
            ) : (
              <svg width="10" height="10" viewBox="0 0 10 10" style={{ display: 'block' }}>
                <rect x="0.75" y="0.75" width="8.5" height="8.5" fill="none" stroke="currentColor" strokeWidth="1.1" rx="0.5" />
              </svg>
            )}
          </button>
          <button 
            onClick={() => window.ag?.window?.close()}
            title="Close"
            style={{ 
              width: 46, height: '100%', 
              background: 'transparent', border: 'none', 
              color: 'var(--ag-text-muted)', cursor: 'pointer', 
              transition: 'all 0.15s ease', outline: 'none',
              display: 'flex', alignItems: 'center', justifyContent: 'center'
            }}
            onMouseEnter={e => { e.currentTarget.style.background = '#CC0000'; e.currentTarget.style.color = '#FFF'; }}
            onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = 'var(--ag-text-muted)'; }}
          >
            <svg width="10" height="10" viewBox="0 0 10 10" style={{ display: 'block' }}>
              <line x1="0.8" y1="0.8" x2="9.2" y2="9.2" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" />
              <line x1="9.2" y1="0.8" x2="0.8" y2="9.2" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" />
            </svg>
          </button>
        </div>
      </div>
      
      {/* Main Content */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {currentTab === 'library' ? (
          <div 
            key={`library-${tabAnimNonce}`}
            className="settings-open-anim"
            style={{ display: 'flex', flex: 1, width: '100%', height: '100%', overflow: 'hidden' }}
          >
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
              className="fast-smooth-scroll"
              style={{ 
                flex: 1, 
                padding: '28px 36px',
                overflowY: 'auto',
                background: '#000000',
                opacity: transitioning ? 0 : 1,
                transform: transitioning ? 'translateY(6px)' : 'translateY(0)',
                transition: 'opacity 0.2s ease, transform 0.2s ease',
                willChange: 'scroll-position',
              }}
            >
              {selectedGame && config ? (
                <GameDetail 
                  key={selectedGame.id}
                  game={selectedGame} 
                  config={config} 
                  onConfigChange={handleConfigChange}
                  logLines={logLines}
                  onRemoveGame={handleRemoveGame}
                  onUninstallMod={handleUninstallMod}
                />
              ) : (
                <HeroCommandCenter
                  games={games}
                  vrStatus={vrStatus}
                  onSelectGame={handleSelectGame}
                  onRescan={scanGames}
                  onAddCustom={async () => {
                    const res = await window.ag.library.addCustom();
                    if (res.success) scanGames();
                  }}
                />
              )}
            </div>
          </div>
        ) : currentTab === 'settings' ? (
          <SettingsView
            key={`settings-${tabAnimNonce}`}
            vrStatus={vrStatus}
            updateStatus={updateStatus}
            onUpdateStatusChange={st => setUpdateStatus(st)}
            onRescan={scanGames}
          />
        ) : (
          <AboutPanel 
            key={`about-${tabAnimNonce}`}
            version={updateStatus?.version} 
            onOpenLegal={() => {
              setIsLegalFirstRun(false);
              setLegalModalOpen(true);
            }}
          />
        )}
      </div>

      <VRStatusBar 
        status={vrStatus} 
        selectedGame={selectedGame} 
        injectState={injectState}
        onInject={handleInject} 
        onUninstallMod={handleUninstallMod}
      />

      <ConfirmModal
        {...modalState}
        onCancel={closeModal}
      />

      <LegalModal
        isOpen={legalModalOpen}
        isFirstRun={isLegalFirstRun}
        onAccept={handleAcceptLegal}
        onClose={() => setLegalModalOpen(false)}
      />
    </div>
  );
}
