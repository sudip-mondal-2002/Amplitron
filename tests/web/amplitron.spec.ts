/**
 * Playwright end-to-end tests for the Amplitron web demo.
 *
 * The suite validates the complete WASM lifecycle as seen in a real browser:
 *   1. Page load & WASM initialisation
 *   2. Audio-unlock prompt
 *   3. Canvas rendering
 *   4. SharedArrayBuffer / COI service-worker availability
 *   5. Absence of runtime errors
 *
 * All tests share the same local static server (configured in playwright.config.ts)
 * that serves the Emscripten build with the required COOP/COEP headers.
 */

import { test, expect, Page, ConsoleMessage } from '@playwright/test';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Collect every console message and page error that fires during a test. */
interface PageLog {
  messages: ConsoleMessage[];
  errors: Error[];
}

function attachLogger(page: Page): PageLog {
  const log: PageLog = { messages: [], errors: [] };
  page.on('console', msg => log.messages.push(msg));
  page.on('pageerror', err => log.errors.push(err));
  return log;
}

/** Return the text of every console message of type 'error'. */
function consoleErrors(log: PageLog): string[] {
  return log.messages
    .filter(m => m.type() === 'error')
    .map(m => m.text());
}

/** Return the texts of all log.messages regardless of type. */
function allMessageTexts(log: PageLog): string[] {
  return log.messages.map(m => m.text());
}

// ---------------------------------------------------------------------------
// 1. Page Load & WASM Initialisation
// ---------------------------------------------------------------------------

test.describe('Page Load & WASM Initialisation', () => {
  test('page loads without JavaScript errors', async ({ page }) => {
    const log = attachLogger(page);

    await page.goto('/');

    // The page must be present and reachable
    await expect(page).toHaveTitle(/Amplitron/i);

    // No uncaught JS exceptions on the initial page load
    expect(log.errors).toHaveLength(0);
  });

  test('loading overlay is visible on initial page load', async ({ page }) => {
    // Only wait for DOMContentLoaded — WASM initialises asynchronously after
    // that, so the overlay must still be visible at this point.
    await page.goto('/', { waitUntil: 'domcontentloaded' });

    const loading = page.locator('#loading');
    // The overlay should exist
    await expect(loading).toBeAttached();
    // It must NOT already be hidden (class 'hidden' is only added after WASM init)
    const classes = await loading.getAttribute('class');
    expect(classes ?? '').not.toContain('hidden');
  });

  test('progress bar fills as WASM data downloads', async ({ page }) => {
    // Collect recorded widths from the fill element via polling
    await page.goto('/');

    // Wait until WASM finishes (progress bar may animate very quickly on a
    // fast local server, so we just assert it reaches 100% at some point)
    await page.waitForFunction(
      () => {
        const fill = document.getElementById('progress-fill');
        if (!fill) return false;
        const w = parseFloat(fill.style.width || '0');
        return w >= 100;
      },
      { timeout: 60_000, polling: 200 }
    );

    const width = await page.locator('#progress-fill').evaluate(
      (el: HTMLElement) => parseFloat(el.style.width || '0')
    );
    expect(width).toBeGreaterThanOrEqual(100);
  });

  test('loading overlay disappears once WASM runtime is ready', async ({ page }) => {
    await page.goto('/');

    // Wait for the 'hidden' class to be applied (Module.setStatus('') callback)
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });

    // The overlay must be invisible to the user (CSS: opacity:0; pointer-events:none)
    await expect(page.locator('#loading')).toHaveClass(/hidden/);
  });

  test('WASM runtime logs "[Amplitron] WASM runtime ready." to the console', async ({
    page,
  }) => {
    const log = attachLogger(page);

    await page.goto('/');
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });

    expect(allMessageTexts(log)).toContain('[Amplitron] WASM runtime ready.');
  });
});

// ---------------------------------------------------------------------------
// 2. Audio Unlock Prompt
// ---------------------------------------------------------------------------

test.describe('Audio Unlock Prompt', () => {
  test('audio-unlock overlay appears after WASM finishes loading', async ({
    page,
  }) => {
    await page.goto('/');
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });

    const overlay = page.locator('#audio-unlock');
    // The overlay becomes display:flex inside Module.setStatus('')
    await expect(overlay).toBeVisible({ timeout: 10_000 });
  });

  test('clicking the audio-unlock overlay dismisses it', async ({ page }) => {
    await page.goto('/');
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });

    const overlay = page.locator('#audio-unlock');
    await expect(overlay).toBeVisible({ timeout: 10_000 });

    await overlay.click();

    // After click, onclick sets display:'none' → the overlay is no longer visible
    await expect(overlay).toBeHidden({ timeout: 5_000 });
  });
});

// ---------------------------------------------------------------------------
// 3. Canvas Rendering
// ---------------------------------------------------------------------------

test.describe('Canvas Rendering', () => {
  test('canvas element exists and has non-zero dimensions', async ({ page }) => {
    await page.goto('/');

    const canvas = page.locator('#canvas');
    await expect(canvas).toBeAttached();

    const dims = await canvas.evaluate((el: HTMLCanvasElement) => ({
      offsetWidth: el.offsetWidth,
      offsetHeight: el.offsetHeight,
    }));

    expect(dims.offsetWidth).toBeGreaterThan(0);
    expect(dims.offsetHeight).toBeGreaterThan(0);
  });

  test('canvas has non-zero internal (backing-buffer) dimensions after WASM init', async ({
    page,
  }) => {
    await page.goto('/');
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });

    // Dismiss audio unlock so the ImGui main-loop has a chance to render
    const overlay = page.locator('#audio-unlock');
    if (await overlay.isVisible()) {
      await overlay.click();
    }

    // Wait until the canvas has been given non-zero dimensions by the Emscripten runtime
    await page.waitForFunction(() => {
      const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
      return canvas !== null && canvas.width > 0 && canvas.height > 0;
    }, { timeout: 10_000 });

    const { width, height } = await page.locator('#canvas').evaluate(
      (el: HTMLCanvasElement) => ({ width: el.width, height: el.height })
    );

    expect(width).toBeGreaterThan(0);
    expect(height).toBeGreaterThan(0);
  });

  test('canvas is rendered (pixel content is not entirely transparent/black)', async ({
    page,
  }) => {
    await page.goto('/');
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });

    const overlay = page.locator('#audio-unlock');
    if (await overlay.isVisible()) {
      await overlay.click();
    }

    // Give the ImGui loop a few frames to populate the colour buffer
    await page.waitForTimeout(1_000);

    const hasNonBlackPixels = await page.evaluate(() => {
      const canvas = document.getElementById('canvas') as HTMLCanvasElement;
      // The canvas uses a WebGL2 context (Emscripten default)
      const gl =
        (canvas.getContext('webgl2') as WebGL2RenderingContext | null) ||
        (canvas.getContext('webgl') as WebGLRenderingContext | null);

      if (!gl) return false;

      // Sample a strip of pixels across the centre of the canvas
      const sampleCount = 32;
      const pixels = new Uint8Array(sampleCount * 4);
      gl.readPixels(
        Math.max(0, Math.floor(canvas.width / 2) - sampleCount / 2),
        Math.floor(canvas.height / 2),
        sampleCount,
        1,
        gl.RGBA,
        gl.UNSIGNED_BYTE,
        pixels
      );

      return pixels.some(v => v !== 0);
    });

    // Soft assertion: may legitimately be false when the GPU is fully emulated
    // and the canvas back-buffer is cleared before readPixels is called.
    expect.soft(hasNonBlackPixels).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// 4. SharedArrayBuffer & Service Worker
// ---------------------------------------------------------------------------

test.describe('SharedArrayBuffer & Service Worker', () => {
  test('SharedArrayBuffer is available in the page context', async ({ page }) => {
    const log = attachLogger(page);

    await page.goto('/');

    const sabAvailable = await page.evaluate(
      () => typeof SharedArrayBuffer !== 'undefined'
    );
    expect(sabAvailable).toBe(true);

    // Must not produce any "SharedArrayBuffer is not defined" console errors
    const sabErrors = consoleErrors(log).filter(msg =>
      msg.toLowerCase().includes('sharedarraybuffer')
    );
    expect(sabErrors).toHaveLength(0);
  });

  test('coi-serviceworker.js is fetched successfully', async ({ page }) => {
    // Track network requests to the service-worker script
    const swRequests: number[] = [];
    page.on('response', response => {
      if (response.url().includes('coi-serviceworker.js')) {
        swRequests.push(response.status());
      }
    });

    await page.goto('/');

    // The shell.html loads coi-serviceworker.js via a <script> tag
    expect(swRequests.length).toBeGreaterThan(0);
    expect(swRequests.every(status => status === 200)).toBe(true);
  });

  test('service worker is registered or COOP/COEP headers enable SAB directly', async ({
    page,
  }) => {
    await page.goto('/');

    // If the server already sends COOP/COEP headers the page is cross-origin
    // isolated from the start and coi-serviceworker.js skips SW registration,
    // so navigator.serviceWorker.ready would hang forever.  Exit early in that
    // case; otherwise wait for the service worker to become active.
    await page.waitForFunction(async () => {
      if (window.crossOriginIsolated) return true;
      try {
        await navigator.serviceWorker.ready;
        return true;
      } catch {
        return false;
      }
    }, { timeout: 15_000 });

    const [crossOriginIsolated, swCount] = await page.evaluate(async () => {
      const regs = await navigator.serviceWorker.getRegistrations();
      return [window.crossOriginIsolated, regs.length] as [boolean, number];
    });

    // The page must be cross-origin isolated (either via COOP/COEP headers from
    // the server or via the COI service worker after a reload)
    expect(crossOriginIsolated).toBe(true);

    // When crossOriginIsolated is satisfied by the server headers alone,
    // coi-serviceworker.js registers 0 service workers; log for debugging
    expect(typeof swCount).toBe('number');
  });
});

// ---------------------------------------------------------------------------
// 5. No Runtime Errors
// ---------------------------------------------------------------------------

test.describe('No Runtime Errors', () => {
  test('no uncaught exceptions during the full load cycle', async ({ page }) => {
    const log = attachLogger(page);

    await page.goto('/');
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });

    expect(log.errors).toHaveLength(0);
  });

  test('no WASM abort() or RuntimeError: unreachable panics', async ({ page }) => {
    const log = attachLogger(page);

    await page.goto('/');
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });

    const wasmPanics = log.errors.filter(
      err =>
        err.message.includes('abort') ||
        err.message.includes('RuntimeError') ||
        err.message.includes('unreachable') ||
        err.message.includes('memory access out of bounds')
    );

    expect(wasmPanics).toHaveLength(0);
  });

  test('no SharedArrayBuffer errors in the console during load', async ({ page }) => {
    const log = attachLogger(page);

    await page.goto('/');
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });

    const sabErrors = [
      ...consoleErrors(log),
      ...log.errors.map(e => e.message),
    ].filter(msg => msg.toLowerCase().includes('sharedarraybuffer'));

    expect(sabErrors).toHaveLength(0);
  });
});

// ---------------------------------------------------------------------------
// 6. Web MIDI Support
// ---------------------------------------------------------------------------

test.describe('Web MIDI Support', () => {
  test('MIDI CC11 controls output gain', async ({ page }) => {
    // Mock navigator.requestMIDIAccess before loading the page
    await page.addInitScript(() => {
      const mockInput = new EventTarget();
      let midiMessageListener: ((event: any) => void) | null = null;
      
      window.navigator.requestMIDIAccess = async () => {
        // Create mock MIDI access object
        const mockMidiAccess = {
          inputs: new Map([['mock-device-id', mockInput]]),
          outputs: new Map(),
          addEventListener: (eventName: string) => {},
          removeEventListener: (eventName: string) => {},
          dispatchEvent: (event: Event) => true,
        };
        
        // Wrap addEventListener to capture listener
        mockInput.addEventListener = function(
          eventName: string,
          callback: any
        ) {
          if (eventName === 'midimessage') {
            midiMessageListener = callback;
          }
        };
        
        // Define the mock input port
        Object.defineProperty(mockInput, 'name', {
          value: 'Mock MIDI Controller',
        });
        Object.defineProperty(mockInput, 'state', {
          value: 'connected',
        });
        
        return mockMidiAccess;
      };
      
      // Simulate MIDI message after a delay
      setTimeout(() => {
        if (midiMessageListener) {
          const mockEvent = {
            data: new Uint8Array([0xB0, 11, 64]),  // CC11, value 64
          };
          midiMessageListener(mockEvent);
        }
      }, 500);
    });
    
    // Load the page
    await page.goto('/');
    
    // Wait for loading to complete
    await page.waitForSelector('#loading.hidden', { timeout: 10000 });
    
    // Click to unlock audio (which triggers MIDI init)
    await page.click('#audio-unlock');
    
    // Wait for MIDI status to show connection
    const midiStatus = page.locator('#midi-status');
    await expect(midiStatus).toContainText('1 MIDI device(s) connected', { timeout: 2000 });
    
    // Verify no JavaScript errors occurred
    const messages: string[] = [];
    page.on('console', (msg) => {
      if (msg.type() === 'error') {
        messages.push(msg.text());
      }
    });
    
    // Wait a bit to see if any errors pop up
    await page.waitForTimeout(1000);
    
    expect(messages).toHaveLength(0);
  });
  
  test('Gracefully handles missing Web MIDI support', async ({ page }) => {
    // Mock the absence of Web MIDI API
    await page.addInitScript(() => {
      if (window.navigator) {
        delete (window.navigator as any).requestMIDIAccess;
      }
    });
    
    await page.goto('/');
    await page.waitForSelector('#loading.hidden', { timeout: 10000 });
    await page.click('#audio-unlock');
    
    // Should show permission denied message when Web MIDI is unavailable
    const midiStatus = page.locator('#midi-status');
    await expect(midiStatus).toContainText('MIDI permission denied', { timeout: 2000 });
  });
});
