import { defineConfig } from 'astro/config';
import tailwind from '@astrojs/tailwind';

export default defineConfig({
  site: 'https://openvoxtuner.eiffelbs.ovh',
  base: '/',
  integrations: [tailwind()],
  output: 'static',
  build: {
    assets: 'assets',
  },
  i18n: {
    defaultLocale: 'en',
    locales: ['en', 'fr', 'de', 'es', 'ja', 'zh'],
    routing: {
      prefixDefaultLocale: false,
    },
  },
  vite: {
    optimizeDeps: {
      include: ['gsap', 'lenis'],
    },
  },
});