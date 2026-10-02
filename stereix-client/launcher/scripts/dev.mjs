import { createServer } from 'vite';
import { spawn, execSync } from 'child_process';
import electronPath from 'electron';

async function start() {
  // Clean up any stale electron or packaged engine processes from prior crashes or dev sessions
  try {
    if (process.platform === 'win32') {
      execSync('taskkill /F /IM electron.exe /T 2>nul', { stdio: 'ignore' });
      execSync('taskkill /F /IM "NexVR Engine.exe" /T 2>nul', { stdio: 'ignore' });
    }
  } catch {}

  // Build Electron main process TypeScript
  console.log(`\x1b[35m[Electron]\x1b[0m Compiling Electron main process...`);
  try {
    execSync('npx tsc -p electron/tsconfig.json', { stdio: 'inherit' });
  } catch (e) {
    console.error('Failed to compile Electron main process:', e);
  }

  // Start Vite dev server
  const server = await createServer({
    configFile: 'vite.config.ts'
  });
  
  await server.listen();
  
  // server.config.server.port is automatically updated if 5173 is in use and strictPort is false
  const port = server.config.server.port || 5173;
  const url = `http://localhost:${port}`;
  
  console.log(`\x1b[36m[Vite]\x1b[0m Dev server is running at: \x1b[32m${url}\x1b[0m`);
  
  // Spawn Electron process directly using the native electron binary
  console.log(`\x1b[35m[Electron]\x1b[0m Starting Electron...`);
  
  const electronProcess = spawn(electronPath, ['.'], {
    env: { 
      ...process.env, 
      NODE_ENV: 'development',
      VITE_DEV_SERVER_URL: url
    },
    stdio: 'inherit',
    shell: false
  });
  
  // Kill Vite when Electron closes
  electronProcess.on('close', () => {
    console.log(`\x1b[35m[Electron]\x1b[0m Exited. Closing Vite server...`);
    server.close();
    process.exit(0);
  });
}

start().catch(console.error);
