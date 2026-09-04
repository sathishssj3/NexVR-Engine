const { _electron: electron } = require('@playwright/test');
const path = require('path');

(async () => {
  const electronApp = await electron.launch({
    args: [path.join(__dirname, 'electron-dist/electron/main.js')]
  });
  const window = await electronApp.firstWindow();
  
  window.on('console', msg => console.log('LOG:', msg.text()));
  
  const title = await window.title();
  console.log('WINDOW TITLE:', title);
  
  await window.waitForTimeout(2000);
  const content = await window.content();
  console.log('WINDOW CONTENT:\n', content);
  
  await electronApp.close();
})();
