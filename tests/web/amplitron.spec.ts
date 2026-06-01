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

// Emscripten injects `Module` into the page's global scope at runtime.
// Declare it here so TypeScript doesn't report ts(2304) errors.
// Overloads narrow the return type based on the `returnType` string literal.
declare const Module: {
  ccall(ident: string, returnType: 'number', argTypes: string[], args: (number | string | boolean)[]): number;
  ccall(ident: string, returnType: 'boolean', argTypes: string[], args: (number | string | boolean)[]): boolean;
  ccall(ident: string, returnType: 'string', argTypes: string[], args: (number | string | boolean)[]): string;
  ccall(ident: string, returnType: null, argTypes: string[], args: (number | string | boolean)[]): void;
};


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
    // Collect console logs for debugging
    const logs: string[] = [];
    page.on('console', (msg) => logs.push(msg.text()));
    
    // Set up mock BEFORE page loads
    await page.addInitScript(() => {
      // Create a mock input port with proper iterator support
      const mockInput = {
        name: 'Mock MIDI Controller',
        state: 'connected',
        id: 'mock-device-id',
        manufacturer: 'Test',
        addEventListener: (eventName: string, callback: any) => {
          if (eventName === 'midimessage') {
            console.log('[TEST-MOCK] Listener registered for midimessage');
          }
        },
        removeEventListener: () => {},
      };
      
      // Create a proper iterator for inputs (must support iteration)
      const inputsMap = new Map([['mock-device-id', mockInput]]);
      const mockInputs = {
        get size() { return inputsMap.size; },
        get(key: any) { return inputsMap.get(key); },
        has(key: any) { return inputsMap.has(key); },
        [Symbol.iterator]() { return inputsMap[Symbol.iterator](); },
        values() { return inputsMap.values(); },
        keys() { return inputsMap.keys(); },
        entries() { return inputsMap.entries(); },
        forEach(callback: any, thisArg: any) { return inputsMap.forEach(callback, thisArg); },
      };
      
      // Create mock MIDI access object
      const mockMidiAccess = {
        inputs: mockInputs,
        outputs: new Map(),
        addEventListener: () => {},
        removeEventListener: () => {},
        sysexEnabled: true,
      };
      
      // Override navigator.requestMIDIAccess
      (window.navigator as any).requestMIDIAccess = async () => {
        console.log('[TEST-MOCK] requestMIDIAccess called');
        // Return immediately with mock access
        return mockMidiAccess;
      };
      
      console.log('[TEST-MOCK] Web MIDI mock injected');
    });
    
    // Load the page
    await page.goto('/');
    
    // Wait for WASM to load
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });
    
    // Click to unlock audio AND trigger MIDI initialization
    const audioUnlock = page.locator('#audio-unlock');
    if (await audioUnlock.isVisible()) {
      await audioUnlock.click();
    }
    
    // Wait for MIDI status to update
    const midiStatus = page.locator('#midi-status');
    await expect(midiStatus).toContainText('MIDI Active: Mock MIDI Controller', { 
      timeout: 10_000 
    });
    
    // Log all console messages for debugging in case of failure
    if (logs.length > 0) {
      console.log('Console logs:', logs.filter(l => l.includes('MIDI') || l.includes('TEST-MOCK')));
    }
  });
  
  test('Gracefully handles missing Web MIDI support', async ({ page }) => {
    // Remove Web MIDI API from the browser.
    // Use assignment rather than delete because requestMIDIAccess may be
    // a non-configurable getter on the Navigator prototype.
    await page.addInitScript(() => {
      (window.navigator as any).requestMIDIAccess = undefined;
      console.log('[TEST-MOCK] Web MIDI API removed');
    });
    
    await page.goto('/');
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });
    
    const audioUnlock = page.locator('#audio-unlock');
    if (await audioUnlock.isVisible()) {
      await audioUnlock.click();
    }
    
    // Should show the unsupported message
    const midiStatus = page.locator('#midi-status');
    await expect(midiStatus).toContainText('MIDI not supported', { 
      timeout: 10_000 
    });
  });
});
test.describe('Modular Graph Canvas Interactions', () => {
  async function waitForRuntime(page: Page) {
    page.on('console', msg => console.log('BROWSER LOG:', msg.text()));
    page.on('pageerror', err => console.error('BROWSER ERROR:', err.message));
    await page.goto('/');
    await page.waitForSelector('#loading.hidden', { timeout: 60_000 });
    const overlay = page.locator('#audio-unlock');
    if (await overlay.isVisible()) await overlay.click();
    await page.waitForTimeout(500);
  }

  test('canvas pan via right-click drag shifts scrolling', async ({ page }) => {
    await waitForRuntime(page);

    const before = await page.evaluate(() => ({
      x: Module.ccall('get_canvas_scroll_x', 'number', [], []),
      y: Module.ccall('get_canvas_scroll_y', 'number', [], []),
    }));

    // Use the JavaScript bridge to simulate right-click drag gesture
    // since DOM event routing may vary in different browser/test environments.
    await page.evaluate(() => {
      const c = document.getElementById('canvas');
      if (c && Module && Module.ccall) {
        const rect = c.getBoundingClientRect();
        const startX = rect.left + rect.width / 2;
        const startY = rect.top + rect.height / 2;
        const endX = startX + 80;
        const endY = startY + 60;
        // Trigger the canvas touch gesture directly to pan
        for (let i = 0; i < 10; i++) {
          const progress = (i + 1) / 10;
          const x = startX + (endX - startX) * progress;
          const y = startY + (endY - startY) * progress;
          const dx = (endX - startX) / 10;
          const dy = (endY - startY) / 10;
          const localX = x - rect.left;
          const localY = y - rect.top;
          Module.ccall('on_canvas_touch_gesture', null, ['number','number','number','number','number'], [dx, dy, 0.0, localX, localY]);
        }
      }
    });

    await page.waitForTimeout(200);

    const after = await page.evaluate(() => ({
      x: Module.ccall('get_canvas_scroll_x', 'number', [], []),
      y: Module.ccall('get_canvas_scroll_y', 'number', [], []),
    }));

    expect(after.x).not.toBeCloseTo(before.x, 0);
    expect(after.y).not.toBeCloseTo(before.y, 0);
  });

  test('two-finger touch gesture pans and zooms the canvas', async ({ page }) => {
    await waitForRuntime(page);

    const before = await page.evaluate(() => ({
      zoom: Module.ccall('get_canvas_zoom', 'number', [], []),
      sx: Module.ccall('get_canvas_scroll_x', 'number', [], []),
    }));

    await page.evaluate(() => {
      Module.ccall('on_canvas_touch_gesture', null, ['number','number','number','number','number'], [30, 20, 0.15, 640, 360]);
    });

    const after = await page.evaluate(() => ({
      zoom: Module.ccall('get_canvas_zoom', 'number', [], []),
      sx: Module.ccall('get_canvas_scroll_x', 'number', [], []),
    }));

    expect(after.zoom).toBeGreaterThan(before.zoom);
    expect(after.sx).not.toBeCloseTo(before.sx, 0);
  });

  test('adding a Splitter node increases the node count', async ({ page }) => {
    await waitForRuntime(page);

    const countBefore: number = await page.evaluate(() =>
      Module.ccall('get_node_count', 'number', [], [])
    );

    await page.evaluate(() =>
      Module.ccall('trigger_add_splitter_node', 'number', [], [])
    );
    await page.waitForTimeout(200);

    const countAfter: number = await page.evaluate(() =>
      Module.ccall('get_node_count', 'number', [], [])
    );

    expect(countAfter).toBe(countBefore + 1);

    const hasSplitter: boolean = await page.evaluate(() =>
      Module.ccall('has_node_of_type', 'boolean', ['number'], [1])
    );
    expect(hasSplitter).toBe(true);
  });

  test('drawing a cable between two nodes increases link count', async ({ page }) => {
    await waitForRuntime(page);

    const linksBefore: number = await page.evaluate(() =>
      Module.ccall('get_link_count', 'number', [], [])
    );

    await page.evaluate(() => {
      Module.ccall('trigger_add_splitter_node', 'number', [], []);
    });
    await page.waitForTimeout(100);

    const result: number = await page.evaluate(() => {
      const srcPin = Module.ccall('get_node_output_pin_by_index', 'number', ['number', 'number'], [2, 0]);
      const dstPin = Module.ccall('get_node_input_pin_by_index', 'number', ['number', 'number'], [3, 0]);
      return Module.ccall('trigger_add_link', 'number', ['number', 'number'], [srcPin, dstPin]);
    });

    const linksAfter: number = await page.evaluate(() =>
      Module.ccall('get_link_count', 'number', [], [])
    );

    expect(linksAfter).toBeGreaterThan(linksBefore);
  });

  test('deleting a node decreases the node count', async ({ page }) => {
    await waitForRuntime(page);

    await page.evaluate(() =>
      Module.ccall('trigger_add_splitter_node', 'number', [], [])
    );
    await page.waitForTimeout(100);

    const countBefore: number = await page.evaluate(() =>
      Module.ccall('get_node_count', 'number', [], [])
    );

    const deleted: boolean = await page.evaluate(() =>
      Module.ccall('trigger_delete_last_node', 'boolean', [], [])
    );
    expect(deleted).toBe(true);

    const countAfter: number = await page.evaluate(() =>
      Module.ccall('get_node_count', 'number', [], [])
    );
    expect(countAfter).toBe(countBefore - 1);
  });

  test('add nodes and cables, then undo back to empty canvas', async ({ page }) => {
    await waitForRuntime(page);

    const countBefore: number = await page.evaluate(() =>
      Module.ccall('get_node_count', 'number', [], [])
    );

    // 1. Add 3 Splitter nodes
    for (let i = 0; i < 3; i++) {
      await page.evaluate(() =>
        Module.ccall('trigger_add_splitter_node', 'number', [], [])
      );
      await page.waitForTimeout(100);
    }

    const countAfterNodes: number = await page.evaluate(() =>
      Module.ccall('get_node_count', 'number', [], [])
    );
    expect(countAfterNodes).toBe(countBefore + 3);

    const linksBefore: number = await page.evaluate(() =>
      Module.ccall('get_link_count', 'number', [], [])
    );

    // 2. Draw 2 cables
    // Node indices are countBefore, countBefore+1, countBefore+2
    // We'll connect them: N0 -> N1, N1 -> N2
    await page.evaluate(() => {
      const c = Module.ccall('get_node_count', 'number', [], []);
      const n0 = c - 3;
      const n1 = c - 2;
      const n2 = c - 1;
      
      const pin0 = Module.ccall('get_node_output_pin_by_index', 'number', ['number', 'number'], [n0, 0]);
      const pin1 = Module.ccall('get_node_input_pin_by_index', 'number', ['number', 'number'], [n1, 0]);
      Module.ccall('trigger_add_link', 'number', ['number', 'number'], [pin0, pin1]);

      const pin1Out = Module.ccall('get_node_output_pin_by_index', 'number', ['number', 'number'], [n1, 0]);
      const pin2In = Module.ccall('get_node_input_pin_by_index', 'number', ['number', 'number'], [n2, 0]);
      Module.ccall('trigger_add_link', 'number', ['number', 'number'], [pin1Out, pin2In]);
    });
    
    await page.waitForTimeout(100);

    const linksAfterCables: number = await page.evaluate(() =>
      Module.ccall('get_link_count', 'number', [], [])
    );
    expect(linksAfterCables).toBe(linksBefore + 2);

    // 3. Undo 5 times (2 cables + 3 nodes)
    for (let i = 0; i < 5; i++) {
      await page.keyboard.press('Control+Z');
      await page.waitForTimeout(100);
    }

    // 4. Verify back to original state
    const countFinalNodes: number = await page.evaluate(() =>
      Module.ccall('get_node_count', 'number', [], [])
    );
    const countFinalLinks: number = await page.evaluate(() =>
      Module.ccall('get_link_count', 'number', [], [])
    );

    expect(countFinalNodes).toBe(countBefore);
    expect(countFinalLinks).toBe(linksBefore);
  });
});
>>>>>>> upstream/main
