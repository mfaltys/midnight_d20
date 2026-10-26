import { defineConfig } from 'vite';

export default defineConfig({
  root: '.',
  // Relative base: the site is served from /midnight_d20/ on GitHub Pages, so
  // every asset reference must stay relative to index.html.
  base: './',
  // One real page and no client-side routing: unknown paths 404 instead of
  // being served the flasher as a fallback.
  appType: 'mpa',
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    sourcemap: false,
    minify: false
  },
  server: {
    open: true,
    port: 3000
  }
});
