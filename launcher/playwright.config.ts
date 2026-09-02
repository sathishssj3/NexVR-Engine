import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: './e2e',
  timeout: 30000,
  expect: {
    timeout: 5000
  },
  reporter: 'list',
  use: {
    trace: 'on-first-retry',
  },
  /* Build electron-dist/ before running E2E tests so launcher.spec.ts can find main.js */
  globalSetup: './e2e/global-setup.ts',
});
