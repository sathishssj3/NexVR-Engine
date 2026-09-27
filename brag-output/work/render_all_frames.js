const { chromium } = require('playwright');
const path = require('path');
const fs = require('fs');

const TOTAL_FRAMES = 600;
const FPS = 30.0;
const CONCURRENCY = 4;

async function renderSlice(browser, sliceId, startFrame, endFrame, outDir, htmlUri) {
  const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });
  await page.goto(htmlUri, { waitUntil: 'load' });
  await page.evaluate(() => document.fonts.ready);

  for (let f = startFrame; f < endFrame; f++) {
    const t = f / FPS;
    await page.evaluate((timestamp) => window.setTimestamp(timestamp), t);
    const filename = `frame_${String(f).padStart(4, '0')}.png`;
    const filepath = path.join(outDir, filename);
    await page.screenshot({ path: filepath, type: 'png' });

    if ((f - startFrame + 1) % 25 === 0 || f === endFrame - 1) {
      console.log(`[Worker ${sliceId}] Rendered frame ${f + 1}/${endFrame} (t=${t.toFixed(2)}s)`);
    }
  }

  await page.close();
}

async function main() {
  const outDir = path.resolve(__dirname, 'frames');
  if (!fs.existsSync(outDir)) {
    fs.mkdirSync(outDir, { recursive: true });
  }

  const htmlPath = path.resolve(__dirname, 'renderer.html');
  const htmlUri = 'file:///' + htmlPath.replace(/\\/g, '/');

  console.log(`Starting parallel rendering of ${TOTAL_FRAMES} frames across ${CONCURRENCY} workers...`);
  const startTime = Date.now();

  const browser = await chromium.launch({ headless: true });

  const framesPerWorker = Math.ceil(TOTAL_FRAMES / CONCURRENCY);
  const tasks = [];

  for (let w = 0; w < CONCURRENCY; w++) {
    const start = w * framesPerWorker;
    const end = Math.min(TOTAL_FRAMES, start + framesPerWorker);
    if (start < end) {
      tasks.push(renderSlice(browser, w, start, end, outDir, htmlUri));
    }
  }

  await Promise.all(tasks);
  await browser.close();

  const durationSec = (Date.now() - startTime) / 1000;
  console.log(`\nALL ${TOTAL_FRAMES} FRAMES RENDERED SUCCESSFULLY IN ${durationSec.toFixed(1)}s!`);
}

main().catch(err => {
  console.error('Fatal render error:', err);
  process.exit(1);
});
