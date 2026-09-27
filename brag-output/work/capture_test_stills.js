const { chromium } = require('playwright');
const path = require('path');
const fs = require('fs');

async function captureStills() {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({
    viewport: { width: 1920, height: 1080 }
  });

  const htmlPath = path.resolve(__dirname, 'renderer.html');
  await page.goto('file:///' + htmlPath.replace(/\\/g, '/'), { waitUntil: 'load' });
  await page.evaluate(() => document.fonts.ready);

  const testTimes = [
    { t: 1.5, name: 'still_scene1.png' },
    { t: 5.0, name: 'still_scene2.png' },
    { t: 9.5, name: 'still_scene3.png' },
    { t: 14.5, name: 'still_scene4.png' },
    { t: 18.0, name: 'still_scene5.png' }
  ];

  const outDir = path.resolve(__dirname, 'stills');
  if (!fs.existsSync(outDir)) {
    fs.mkdirSync(outDir, { recursive: true });
  }

  for (const item of testTimes) {
    await page.evaluate((time) => window.setTimestamp(time), item.t);
    const savePath = path.join(outDir, item.name);
    await page.screenshot({ path: savePath, type: 'png' });
    console.log(`Saved ${item.name} at t=${item.t}s`);
  }

  await browser.close();
}

captureStills().catch(err => {
  console.error('Error capturing stills:', err);
  process.exit(1);
});
