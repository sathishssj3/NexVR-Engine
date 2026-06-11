import { _electron as electron } from '@playwright/test';
import { test, expect } from '@playwright/test';
import * as path from 'path';

test.describe('Launcher E2E Tests', () => {
  let electronApp: any;
  let window: any;

  test.beforeAll(async () => {
    // Launch electron passing the main file path
    electronApp = await electron.launch({
      args: [path.join(__dirname, '../electron-dist/electron/main.js')]
    });
    window = await electronApp.firstWindow();
    window.on('console', (msg: any) => console.log('PAGE LOG:', msg.text()));
    window.on('pageerror', (err: any) => console.log('PAGE ERROR:', err.message));
  });

  test.afterAll(async () => {
    if (electronApp) {
      await electronApp.close();
    }
  });

  test('should render the top bar with NEX/R ENGINE title', async () => {
    const title = window.locator('strong:has-text("NEX/R ENGINE")');
    await expect(title).toBeVisible();
    
    const version = window.locator('span:has-text("v0.1.0")');
    await expect(version).toBeVisible();
  });

  test('should show VR status bar with default or detected status', async () => {
    // wait for VR status polling (5s) or use initial
    const hmdElement = window.locator('span:has-text("Unknown HMD")').or(window.locator('span:has-text("Meta Quest")')).or(window.locator('span:has-text("SteamVR HMD")')).or(window.locator('span:has-text("WMR Headset")')).first();
    await expect(hmdElement).toBeVisible();
  });

  test('should display library games in sidebar or empty state', async () => {
    // Give it a moment to scan the registry
    await window.waitForTimeout(2000);
    
    // It should either have games or say 'Select a game from the library'
    const emptyState = window.locator('text=Select a game from the library');
    
    if (await emptyState.isVisible()) {
      console.log('No Steam games found (expected in mock environment).');
    } else {
      console.log('Steam games found!');
      // Assuming at least one game was found, we can select it
      const firstGame = window.locator('.sidebar-item').first();
      if (await firstGame.isVisible()) {
         await firstGame.click();
         await expect(window.locator('h1')).toBeVisible(); // Game Title
      }
    }
  });

});
