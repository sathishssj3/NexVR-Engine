import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [
    react(),
    {
      name: 'remove-crossorigin',
      transformIndexHtml(html) {
        return html.replace(/\s*crossorigin/g, '');
      },
    },
  ],
  base: './',
  define: {
    'process.env.VITE_BUILD_DATE': JSON.stringify(new Date().toISOString().split('T')[0])
  },
  server: {
    port: 5173,
    strictPort: false,
  },
  build: {
    outDir: 'frontend-dist',
    modulePreload: false,
  }
});
