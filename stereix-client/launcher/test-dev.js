const { _electron: electron } = require('@playwright/test');
const path = require('path');

(async () => {
  // Set NODE_ENV to development so it loads localhost:5173
  process.env.NODE_ENV = 'development';
  
  const electronApp = await electron.launch({
    args: [path.join(__dirname, 'electron-dist/electron/main.js')]
  });
  const window = await electronApp.firstWindow();
  
  window.on('console', msg => console.log('LOG:', msg.text()));
  window.on('pageerror', err => console.log('PAGE ERROR:', err.message));
  
  // Wait for 5 seconds to gather logs
  await window.waitForTimeout(5000);
  
  await electronApp.close();
})();
