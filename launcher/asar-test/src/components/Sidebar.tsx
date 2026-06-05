import { useState } from 'react';
import { GameEntry } from '../types';

export function Sidebar({ games, selectedId, onSelect, onRescan }: any) {
  const [search, setSearch] = useState('');
  
  const filtered = games.filter((g: GameEntry) => g.name.toLowerCase().includes(search.toLowerCase()));

  return (
    <div style={{ width: 250, borderRight: '1px solid var(--ag-border)', display: 'flex', flexDirection: 'column' }}>
      <div style={{ padding: 10 }}>
        <input 
          placeholder="Search Library..." 
          value={search}
          onChange={e => setSearch(e.target.value)}
          style={{ width: '100%', padding: '8px', background: 'var(--ag-bg-surface)', border: '1px solid var(--ag-border)', color: 'var(--ag-text-primary)', outline: 'none' }}
        />
      </div>
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {filtered.map((g: GameEntry) => (
          <div 
            key={g.id} 
            onClick={() => onSelect(g)}
            style={{ 
              padding: '10px 15px', 
              cursor: 'pointer', 
              display: 'flex', 
              alignItems: 'center',
              borderLeft: selectedId === g.id ? '3px solid var(--ag-accent)' : '3px solid transparent',
              background: selectedId === g.id ? 'var(--ag-bg-surface)' : 'transparent'
            }}
          >
            <div style={{ width: 24, height: 24, borderRadius: '50%', background: 'var(--ag-border)', display: 'flex', alignItems: 'center', justifyContent: 'center', marginRight: 10, fontSize: 12 }}>
              {g.name.charAt(0)}
            </div>
            <div style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
              {g.name}
            </div>
          </div>
        ))}
      </div>
      <div style={{ padding: 10, borderTop: '1px solid var(--ag-border)' }}>
        <button onClick={onRescan} style={{ width: '100%', padding: '8px', background: 'transparent', border: '1px solid var(--ag-border)', color: 'var(--ag-text-primary)', cursor: 'pointer' }}>
          Rescan Library
        </button>
      </div>
    </div>
  );
}
