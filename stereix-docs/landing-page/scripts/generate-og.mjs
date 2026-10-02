import { chromium } from 'playwright';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const htmlPath = path.resolve(__dirname, '../public/og-card.html');
const outPng = path.resolve(__dirname, '../public/og-preview.png');
const outJpg = path.resolve(__dirname, '../public/og-preview.jpg');

async function main() {
  const browser = await chromium.launch({ channel: 'msedge' });
  const page = await browser.newPage({
    viewport: { width: 1200, height: 630 },
    deviceScaleFactor: 1
  });

  await page.goto(`file://${htmlPath}`);
  await page.waitForTimeout(1000); // Wait for Google Fonts to render

  const card = await page.$('.card');
  if (card) {
    await card.screenshot({ path: outPng, type: 'png' });
    await card.screenshot({ path: outJpg, type: 'jpeg', quality: 85 });
    console.log('Successfully generated og-preview.png and og-preview.jpg!');
  } else {
    console.error('Could not find .card selector');
  }

  await browser.close();
}

main().catch(err => {
  console.error(err);
  process.exit(1);
});
