import { defineConfig, devices } from '@playwright/test';
import * as path from 'path';

// Where the web build artefacts live.
// • CI:    set WEB_BUILD_DIR to the downloaded artifact path (e.g. $GITHUB_WORKSPACE/web-build)
// • Local: build with Emscripten first, then point to the output directory
const webBuildDir = process.env.WEB_BUILD_DIR
  ? path.resolve(process.env.WEB_BUILD_DIR)
  : path.resolve(__dirname, '../../build-web');

const serverScript = path.resolve(__dirname, 'server.js');

export default defineConfig({
  testDir: __dirname,
  testMatch: '**/*.spec.ts',

  // Global timeout per test (WASM download + init can take tens of seconds)
  timeout: 90_000,

  // One retry in CI to handle intermittent network/startup races
  retries: process.env.CI ? 1 : 0,

  // Run tests sequentially to avoid port conflicts with the single dev server
  workers: 1,

  reporter: process.env.CI
    ? [['github'], ['html', { open: 'never', outputFolder: 'playwright-report' }]]
    : [['list'], ['html', { open: 'on-failure', outputFolder: 'playwright-report' }]],

  use: {
    baseURL: 'http://127.0.0.1:8080',

    // Headless Chromium is the most compatible browser for WASM + SharedArrayBuffer
    ...devices['Desktop Chrome'],

    launchOptions: {
      args: [
        // Allow SharedArrayBuffer without the COI service worker reload on first load
        '--enable-features=SharedArrayBuffer',
        // Prevent the browser from blocking autoplay (needed for AudioContext.resume())
        '--autoplay-policy=no-user-gesture-required',
        // Fake audio device so AudioContext doesn't fail in a headless runner
        '--use-fake-audio-capture',
        '--use-fake-device-for-media-stream',
        '--use-fake-ui-for-media-stream',
        ...(process.env.CI ? ['--use-gl=swiftshader', '--disable-gpu'] : ['--use-gl=angle']),
        '--no-sandbox',
      ],
    },

    viewport: { width: 1280, height: 720 },
    // Capture screenshots & traces on failure for easier debugging
    screenshot: 'only-on-failure',
    trace: 'on-first-retry',
  },

  projects: [
    {
      name: 'chromium',
      use: { ...devices['Desktop Chrome'] },
    },
  ],

  webServer: {
    command: `node "${serverScript}"`,
    env: { ...process.env, WEB_BUILD_DIR: webBuildDir },
    url: 'http://127.0.0.1:8080',
    reuseExistingServer: !process.env.CI,
    // Allow up to 30 s for the server process to become ready
    timeout: 30_000,
    // Pipe server stdout/stderr to the test runner output
    stdout: 'pipe',
    stderr: 'pipe',
  },
});
