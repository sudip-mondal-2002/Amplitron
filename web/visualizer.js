/**
 * Amplitron - Real-Time Audio Visualization Panel
 *
 * Two modes:
 *   1. Linear Waveform – scrolling canvas rendering track peaks,
 *      color changes dynamically with playback progress.
 *   2. Circular Pulse – minimalist circular ring that pulses to bass
 *      frequencies.
 *
 * Uses HTML5 Canvas for high-performance rendering.
 */
(function () {
  "use strict";

  // ---- Constants ----
  const ANALYZER_FFT_SIZE = 2048;
  const WAVEFORM_HISTORY_LEN = 600; // ~10 seconds at 60fps

  // ---- State ----
  let vizMode = "waveform"; // "waveform" | "circular"
  let colorTheme = "amber"; // "amber" | "cyan" | "violet" | "green" | "sunset"
  let sensitivity = 1.0;   // 0.1 – 3.0
  let panelVisible = false;
  let waveformHistory = [];  // array of arrays (peak frames)
  let animationId = null;
  let lastSequence = 0;
  let currentInputSamples = new Float32Array(ANALYZER_FFT_SIZE);
  let currentOutputSamples = new Float32Array(ANALYZER_FFT_SIZE);
  let hasData = false;
  let bassEnergy = 0;
  let smoothedBass = 0;
  let playbackProgress = 0;
  let canvasWidth = 0;
  let canvasHeight = 0;

  // ---- DOM References (populated on init) ----
  let panelEl, canvasEl, ctx;
  let toggleBtn, modeBtn, themeSelect, sensitivitySlider, sensitivityValue;

  // ---- Color Themes ----
  const THEMES = {
    amber:   { primary: "#f0a030", secondary: "#c08020", bg: "rgba(240,160,48,0.08)", glow: "rgba(240,160,48,0.3)" },
    cyan:    { primary: "#30c0f0", secondary: "#2090c0", bg: "rgba(48,192,240,0.08)", glow: "rgba(48,192,240,0.3)" },
    violet:  { primary: "#a060f0", secondary: "#8040c0", bg: "rgba(160,96,240,0.08)", glow: "rgba(160,96,240,0.3)" },
    green:   { primary: "#40c060", secondary: "#30a050", bg: "rgba(64,192,96,0.08)", glow: "rgba(64,192,96,0.3)" },
    sunset:  { primary: "#f07050", secondary: "#d05040", bg: "rgba(240,112,80,0.08)", glow: "rgba(240,112,80,0.3)" },
  };

  function getTheme() { return THEMES[colorTheme] || THEMES.amber; }

  // ---- C++ Bridge ----
  function fetchAnalyzerData() {
    if (!Module || !Module.ccall) return false;

    try {
      const seq = Module.ccall("get_analyzer_sequence", "number", [], []);
      if (seq === lastSequence && hasData) return true;
      lastSequence = seq;

      const n = ANALYZER_FFT_SIZE;
      // Allocate temporary buffers on the WASM heap
      const inPtr = Module._malloc(n * 4);
      const outPtr = Module._malloc(n * 4);
      if (!inPtr || !outPtr) {
        if (inPtr) Module._free(inPtr);
        if (outPtr) Module._free(outPtr);
        return hasData;
      }

      const count = Module.ccall(
        "copy_analyzer_snapshot",
        "number",
        ["number", "number", "number"],
        [inPtr, outPtr, n]
      );

      if (count > 0) {
        const inView = new Float32Array(Module.HEAPF32.buffer, inPtr, n);
        const outView = new Float32Array(Module.HEAPF32.buffer, outPtr, n);
        currentInputSamples.set(inView);
        currentOutputSamples.set(outView);
        hasData = true;

        // Compute bass energy (first ~5% of FFT bins = low frequencies)
        // For time-domain, look at low-frequency content via downsampled RMS
        let sum = 0;
        const bassBins = Math.min(128, n);
        for (let i = 0; i < bassBins; i++) {
          sum += Math.abs(outView[i]);
        }
        bassEnergy = sum / bassBins;
        smoothedBass = smoothedBass * 0.8 + bassEnergy * 0.2;

        // Build waveform frame: downsample 2048 -> ~256 for display
        const frameLen = 256;
        const frame = new Float32Array(frameLen);
        const step = Math.max(1, Math.floor(n / frameLen));
        for (let i = 0; i < frameLen; i++) {
          let peak = 0;
          const start = i * step;
          const end = Math.min(start + step, n);
          for (let j = start; j < end; j++) {
            const abs = Math.abs(outView[j]);
            if (abs > peak) peak = abs;
          }
          frame[i] = peak;
        }
        waveformHistory.push(frame);
        if (waveformHistory.length > WAVEFORM_HISTORY_LEN) {
          waveformHistory.shift();
        }
        // Simple progress simulation (could be driven by transport position)
        playbackProgress = (playbackProgress + 0.001) % 1.0;
      }

      Module._free(inPtr);
      Module._free(outPtr);
      return count > 0;
    } catch (e) {
      return false;
    }
  }

  // ---- Canvas Rendering ----

  function renderWaveform() {
    const W = canvasWidth;
    const H = canvasHeight;
    if (W < 10 || H < 10) return;

    const theme = getTheme();
    const sens = sensitivity;

    ctx.clearRect(0, 0, W, H);

    // Background
    ctx.fillStyle = "#1a1816";
    ctx.fillRect(0, 0, W, H);

    // Grid lines
    ctx.strokeStyle = "rgba(255,255,255,0.04)";
    ctx.lineWidth = 1;
    for (let y = 0; y < H; y += H / 4) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(W, y);
      ctx.stroke();
    }

    if (!hasData || waveformHistory.length < 2) {
      // Fallback: draw flat line with "Waiting for audio..." text
      ctx.fillStyle = "#555";
      ctx.font = "12px monospace";
      ctx.textAlign = "center";
      ctx.fillText("Waiting for audio...", W / 2, H / 2 - 10);

      // Draw subtle placeholder waveform
      const centerY = H / 2;
      ctx.strokeStyle = "rgba(255,255,255,0.08)";
      ctx.lineWidth = 2;
      ctx.beginPath();
      for (let x = 0; x < W; x++) {
        const phase = (x / W) * Math.PI * 4;
        const y = centerY + Math.sin(phase) * 8;
        if (x === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
      return;
    }

    // Draw waveform from history
    const historyLen = waveformHistory.length;
    const centerY = H / 2;
    const ampScale = H * 0.4 * sens;

    // Draw full waveform track (dim)
    ctx.strokeStyle = "rgba(255,255,255,0.06)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let x = 0; x < W; x++) {
      const histIdx = Math.floor((x / W) * historyLen);
      if (histIdx >= historyLen) break;
      const frame = waveformHistory[histIdx];
      if (!frame) continue;
      // Find peak in frame
      let peak = 0;
      for (let i = 0; i < frame.length; i++) {
        if (frame[i] > peak) peak = frame[i];
      }
      const y = centerY - peak * ampScale;
      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    // Mirror bottom half
    ctx.beginPath();
    for (let x = 0; x < W; x++) {
      const histIdx = Math.floor((x / W) * historyLen);
      if (histIdx >= historyLen) break;
      const frame = waveformHistory[histIdx];
      if (!frame) continue;
      let peak = 0;
      for (let i = 0; i < frame.length; i++) {
        if (frame[i] > peak) peak = frame[i];
      }
      const y = centerY + peak * ampScale;
      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    // Draw progress-highlighted portion (playback position)
    const progressX = Math.floor(playbackProgress * W);
    const gradient = ctx.createLinearGradient(0, 0, W, 0);
    gradient.addColorStop(0, theme.primary);
    gradient.addColorStop(0.5, theme.secondary);
    gradient.addColorStop(1, theme.primary);

    ctx.strokeStyle = gradient;
    ctx.lineWidth = 2;
    ctx.shadowColor = theme.glow;
    ctx.shadowBlur = 6;

    // Draw only up to progress position (top)
    ctx.beginPath();
    for (let x = 0; x <= progressX && x < W; x++) {
      const histIdx = Math.floor((x / W) * historyLen);
      if (histIdx >= historyLen) break;
      const frame = waveformHistory[histIdx];
      if (!frame) continue;
      let peak = 0;
      for (let i = 0; i < frame.length; i++) {
        if (frame[i] > peak) peak = frame[i];
      }
      const y = centerY - peak * ampScale;
      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    // Draw only up to progress position (bottom)
    ctx.beginPath();
    for (let x = 0; x <= progressX && x < W; x++) {
      const histIdx = Math.floor((x / W) * historyLen);
      if (histIdx >= historyLen) break;
      const frame = waveformHistory[histIdx];
      if (!frame) continue;
      let peak = 0;
      for (let i = 0; i < frame.length; i++) {
        if (frame[i] > peak) peak = frame[i];
      }
      const y = centerY + peak * ampScale;
      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    ctx.shadowBlur = 0;

    // Playback position indicator
    if (progressX < W - 2 && progressX > 2) {
      ctx.strokeStyle = theme.primary;
      ctx.lineWidth = 1;
      ctx.globalAlpha = 0.5;
      ctx.beginPath();
      ctx.moveTo(progressX, 0);
      ctx.lineTo(progressX, H);
      ctx.stroke();
      ctx.globalAlpha = 1.0;
    }
  }

  function renderCircular() {
    const W = canvasWidth;
    const H = canvasHeight;
    if (W < 10 || H < 10) return;

    const theme = getTheme();
    const sens = sensitivity;
    const cx = W / 2;
    const cy = H / 2;
    const maxRadius = Math.min(W, H) * 0.35;

    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = "#1a1816";
    ctx.fillRect(0, 0, W, H);

    if (!hasData) {
      ctx.fillStyle = "#555";
      ctx.font = "12px monospace";
      ctx.textAlign = "center";
      ctx.fillText("Waiting for audio...", cx, cy);
      return;
    }

    // Smoothed bass-driven pulse radius
    const pulseRadius = maxRadius * 0.5 + smoothedBass * maxRadius * 0.8 * sens;
    const clampedRadius = Math.min(pulseRadius, maxRadius * 1.2);

    // Outer glow ring
    const gradient = ctx.createRadialGradient(cx, cy, clampedRadius * 0.3, cx, cy, clampedRadius * 1.6);
    gradient.addColorStop(0, theme.glow);
    gradient.addColorStop(0.4, theme.bg);
    gradient.addColorStop(1, "rgba(0,0,0,0)");
    ctx.fillStyle = gradient;
    ctx.beginPath();
    ctx.arc(cx, cy, clampedRadius * 1.6, 0, Math.PI * 2);
    ctx.fill();

    // Main ring
    ctx.strokeStyle = theme.primary;
    ctx.lineWidth = 3;
    ctx.shadowColor = theme.glow;
    ctx.shadowBlur = 12;
    ctx.beginPath();
    ctx.arc(cx, cy, clampedRadius, 0, Math.PI * 2);
    ctx.stroke();

    // Inner ring (secondary color)
    ctx.strokeStyle = theme.secondary;
    ctx.lineWidth = 1.5;
    ctx.shadowBlur = 6;
    ctx.beginPath();
    ctx.arc(cx, cy, clampedRadius * 0.7, 0, Math.PI * 2);
    ctx.stroke();

    // Center dot
    ctx.fillStyle = theme.primary;
    ctx.shadowBlur = 8;
    ctx.beginPath();
    ctx.arc(cx, cy, 3 + smoothedBass * 6 * sens, 0, Math.PI * 2);
    ctx.fill();

    ctx.shadowBlur = 0;

    // Frequency tick marks around the ring
    const numTicks = 32;
    for (let i = 0; i < numTicks; i++) {
      const angle = (i / numTicks) * Math.PI * 2 - Math.PI / 2;
      // Use input sample energy at various positions for variation
      const idx = Math.floor((i / numTicks) * currentOutputSamples.length);
      const energy = Math.abs(currentOutputSamples[idx] || 0) * sens;
      const tickLen = 4 + energy * 20;
      const innerR = clampedRadius - 2;
      const outerR = clampedRadius + Math.min(tickLen, 30);

      ctx.strokeStyle = i % 4 === 0 ? theme.primary : "rgba(255,255,255,0.2)";
      ctx.lineWidth = i % 4 === 0 ? 2 : 1;
      ctx.beginPath();
      ctx.moveTo(cx + Math.cos(angle) * innerR, cy + Math.sin(angle) * innerR);
      ctx.lineTo(cx + Math.cos(angle) * outerR, cy + Math.sin(angle) * outerR);
      ctx.stroke();
    }

    // Label
    ctx.fillStyle = "rgba(255,255,255,0.15)";
    ctx.font = "9px monospace";
    ctx.textAlign = "center";
    ctx.fillText("BASS RESPONSE", cx, cy + clampedRadius + 20);
  }

  function renderFrame() {
    fetchAnalyzerData();

    if (vizMode === "waveform") {
      renderWaveform();
    } else {
      renderCircular();
    }
  }

  // ---- Animation Loop ----
  function startAnimation() {
    if (animationId) return;
    function tick() {
      renderFrame();
      animationId = requestAnimationFrame(tick);
    }
    animationId = requestAnimationFrame(tick);
  }

  function stopAnimation() {
    if (animationId) {
      cancelAnimationFrame(animationId);
      animationId = null;
    }
  }

  // ---- Panel Controls ----
  function updateControls() {
    modeBtn.textContent = vizMode === "waveform" ? "Waveform" : "Circular";
    themeSelect.value = colorTheme;
    sensitivitySlider.value = sensitivity;
    sensitivityValue.textContent = sensitivity.toFixed(1);
  }

  function togglePanel() {
    panelVisible = !panelVisible;
    panelEl.style.display = panelVisible ? "flex" : "none";
    toggleBtn.textContent = panelVisible ? "▾ Visualizer" : "▸ Visualizer";
    toggleBtn.classList.toggle("active", panelVisible);
    if (panelVisible) {
      // Enable the C++ analyzer capture so we get data
      if (Module && Module.ccall) {
        try {
          Module.ccall("enable_analyzer", null, ["number"], [1]);
        } catch (e) {
          console.warn("[Visualizer] enable_analyzer not available yet:", e);
        }
      }
      resizeCanvas();
      startAnimation();
    } else {
      // Disable the C++ analyzer capture when panel is closed to save CPU
      if (Module && Module.ccall) {
        try {
          Module.ccall("enable_analyzer", null, ["number"], [0]);
        } catch (e) {
          // ignore
        }
      }
      stopAnimation();
    }
  }

  function setMode(mode) {
    vizMode = mode;
    updateControls();
  }

  function setTheme(theme) {
    colorTheme = theme;
    updateControls();
  }

  function setSensitivity(val) {
    sensitivity = Math.max(0.1, Math.min(3.0, parseFloat(val) || 1.0));
    updateControls();
  }

  function resizeCanvas() {
    if (!canvasEl || !panelVisible) return;
    const rect = panelEl.getBoundingClientRect();
    const container = canvasEl.parentElement;
    if (!container) return;
    const cw = container.clientWidth - 4;
    const ch = container.clientHeight - 50; // leave space for controls bar
    if (cw < 50 || ch < 50) return;
    const dpr = window.devicePixelRatio || 1;
    canvasWidth = cw;
    canvasHeight = ch;
    canvasEl.width = cw * dpr;
    canvasEl.height = ch * dpr;
    canvasEl.style.width = cw + "px";
    canvasEl.style.height = ch + "px";
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }

  // ---- Audio unlock / startup ----
  function onAudioUnlock() {
    // Reset waveform history on audio start
    waveformHistory = [];
    playbackProgress = 0;
    hasData = false;
  }

  // ---- Init ----
  function init() {
    // Create panel structure
    panelEl = document.createElement("div");
    panelEl.id = "viz-panel";
    panelEl.style.cssText = `
      position: fixed;
      bottom: 0;
      left: 0;
      right: 0;
      height: 240px;
      background: #141210;
      border-top: 1px solid rgba(240,160,48,0.2);
      display: none;
      flex-direction: column;
      z-index: 60;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    `;
    document.body.appendChild(panelEl);

    // Canvas container
    const canvasContainer = document.createElement("div");
    canvasContainer.style.cssText = `
      flex: 1;
      position: relative;
      overflow: hidden;
      padding: 2px;
    `;
    panelEl.appendChild(canvasContainer);

    canvasEl = document.createElement("canvas");
    canvasEl.id = "viz-canvas";
    canvasContainer.appendChild(canvasEl);
    ctx = canvasEl.getContext("2d");

    // Controls bar
    const controlsBar = document.createElement("div");
    controlsBar.style.cssText = `
      display: flex;
      align-items: center;
      gap: 12px;
      padding: 6px 12px;
      background: rgba(26,24,22,0.95);
      border-top: 1px solid rgba(255,255,255,0.06);
      flex-shrink: 0;
      flex-wrap: wrap;
    `;
    panelEl.appendChild(controlsBar);

    // Mode toggle
    modeBtn = document.createElement("button");
    modeBtn.style.cssText = `
      background: rgba(240,160,48,0.1);
      border: 1px solid rgba(240,160,48,0.3);
      color: #f0a030;
      padding: 4px 12px;
      border-radius: 4px;
      cursor: pointer;
      font-size: 0.75rem;
      font-family: monospace;
    `;
    modeBtn.addEventListener("click", function () {
      setMode(vizMode === "waveform" ? "circular" : "waveform");
    });
    controlsBar.appendChild(modeBtn);

    // Theme selector
    const themeLabel = document.createElement("label");
    themeLabel.style.cssText = "color: #888; font-size: 0.7rem;";
    themeLabel.textContent = "Theme:";
    controlsBar.appendChild(themeLabel);

    themeSelect = document.createElement("select");
    themeSelect.style.cssText = `
      background: #1a1816;
      color: #e0dcd4;
      border: 1px solid rgba(255,255,255,0.1);
      border-radius: 4px;
      padding: 3px 6px;
      font-size: 0.7rem;
      font-family: monospace;
    `;
    const themeOptions = ["amber", "cyan", "violet", "green", "sunset"];
    themeOptions.forEach(function (t) {
      const opt = document.createElement("option");
      opt.value = t;
      opt.textContent = t.charAt(0).toUpperCase() + t.slice(1);
      themeSelect.appendChild(opt);
    });
    themeSelect.addEventListener("change", function () {
      setTheme(themeSelect.value);
    });
    controlsBar.appendChild(themeSelect);

    // Sensitivity slider
    const sensLabel = document.createElement("label");
    sensLabel.style.cssText = "color: #888; font-size: 0.7rem;";
    sensLabel.textContent = "Sensitivity:";
    controlsBar.appendChild(sensLabel);

    sensitivitySlider = document.createElement("input");
    sensitivitySlider.type = "range";
    sensitivitySlider.min = "0.1";
    sensitivitySlider.max = "3.0";
    sensitivitySlider.step = "0.1";
    sensitivitySlider.style.cssText = `
      width: 80px;
      height: 4px;
      accent-color: #f0a030;
    `;
    sensitivitySlider.addEventListener("input", function () {
      setSensitivity(sensitivitySlider.value);
    });
    controlsBar.appendChild(sensitivitySlider);

    sensitivityValue = document.createElement("span");
    sensitivityValue.style.cssText = "color: #aaa; font-size: 0.7rem; min-width: 24px; font-family: monospace;";
    sensitivityValue.textContent = "1.0";
    controlsBar.appendChild(sensitivityValue);

    // Spacer
    const spacer = document.createElement("div");
    spacer.style.cssText = "flex: 1;";
    controlsBar.appendChild(spacer);

    // Close button
    const closeBtn = document.createElement("button");
    closeBtn.textContent = "✕";
    closeBtn.style.cssText = `
      background: transparent;
      border: none;
      color: #666;
      cursor: pointer;
      font-size: 0.85rem;
      padding: 2px 6px;
    `;
    closeBtn.addEventListener("click", togglePanel);
    controlsBar.appendChild(closeBtn);

    // Toggle button on main UI
    toggleBtn = document.createElement("button");
    toggleBtn.id = "viz-toggle";
    toggleBtn.textContent = "▸ Visualizer";
    toggleBtn.style.cssText = `
      position: fixed;
      bottom: 8px;
      left: 50%;
      transform: translateX(-50%);
      background: rgba(20,18,16,0.85);
      border: 1px solid rgba(240,160,48,0.25);
      color: #f0a030;
      padding: 5px 16px;
      border-radius: 4px;
      cursor: pointer;
      font-size: 0.75rem;
      font-family: monospace;
      z-index: 55;
      transition: background 0.2s;
    `;
    toggleBtn.addEventListener("mouseenter", function () {
      toggleBtn.style.background = "rgba(240,160,48,0.15)";
    });
    toggleBtn.addEventListener("mouseleave", function () {
      toggleBtn.style.background = "rgba(20,18,16,0.85)";
    });
    toggleBtn.addEventListener("click", togglePanel);
    document.body.appendChild(toggleBtn);

    // Resize observer
    const ro = new ResizeObserver(function () {
      if (panelVisible) resizeCanvas();
    });
    ro.observe(panelEl);

    window.addEventListener("resize", function () {
      if (panelVisible) resizeCanvas();
    });

    // Initial state
    updateControls();

    // Listen for audio unlock on the existing overlay
    const unlockEl = document.getElementById("audio-unlock");
    if (unlockEl) {
      unlockEl.addEventListener("click", onAudioUnlock);
    }

    console.log("[Visualizer] Panel initialized");
  }

  // Wait for DOM ready
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();