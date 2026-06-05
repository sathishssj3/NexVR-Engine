import { createServer } from 'vite';
import { spawn } from 'child_process';

async function start() {
  // Start Vite dev server
  const server = await createServer({
    configFile: 'vite.config.ts'
  });
  
  await server.listen();
  
  // server.config.server.port is automatically updated if 5173 is in use and strictPort is false
  const port = server.config.server.port || 5173;
  const url = `http://localhost:${port}`;
  
  console.log(`\x1b[36m[Vite]\x1b[0m Dev server is running at: \x1b[32m${url}\x1b[0m`);
  
  // Spawn Electron process
  console.log(`\x1b[35m[Electron]\x1b[0m Starting Electron...`);
  
  // Note: On Windows, using shell: true helps resolve 'electron' from node_modules/.bin
  const electronProcess = spawn('npx', ['electron', '.'], {
    env: { 
      ...process.env, 
      NODE_ENV: 'development',
      VITE_DEV_SERVER_URL: url
    },
    stdio: 'inherit',
    shell: true
  });
  
  // Kill Vite when Electron closes
  electronProcess.on('close', () => {
    console.log(`\x1b[35m[Electron]\x1b[0m Exited. Closing Vite server...`);
    server.close();
    process.exit(0);
  });
}

start().catch(console.error);
