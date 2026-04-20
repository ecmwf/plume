import { dom } from "./dom.js";
import { uiState } from "./state.js";

// ---------------------------------------------------------------------------
// Title / filename helpers
// ---------------------------------------------------------------------------

function getSelectedFieldName() {
  const checked = dom.fieldToggleList?.querySelector('input[name="field-toggle"]:checked');
  return (checked?.value || "").trim() || "none";
}

function getSelectedPluginNames() {
  if (!dom.pluginToggleList) return [];
  return [...dom.pluginToggleList.querySelectorAll('input[type="checkbox"]:checked')]
    .map((cb) => cb.closest("label")?.textContent?.trim() || "")
    .filter(Boolean);
}

function buildStepTitle() {
  const field = getSelectedFieldName();
  const plugins = getSelectedPluginNames();
  const scale = Number(uiState.mapProjectionScale).toFixed(2);
  const step = uiState.stepCurrent;
  const parts = [`field_${field}`];
  if (plugins.length > 0) parts.push(`plugins_${plugins.join("-")}`);
  parts.push(`step_${step}`, `scale_${scale}x`);
  return parts.join("_");
}

function buildGifTitle() {
  const field = getSelectedFieldName();
  const plugins = getSelectedPluginNames();
  const scale = Number(uiState.mapProjectionScale).toFixed(2);
  const parts = [`field_${field}`];
  if (plugins.length > 0) parts.push(`plugins_${plugins.join("-")}`);
  parts.push(`scale_${scale}x`);
  return parts.join("_");
}

function safeFilename(str) {
  return str.replace(/[^a-zA-Z0-9._\-]/g, "_");
}

function triggerDownload(url, filename) {
  const a = document.createElement("a");
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
}

// ---------------------------------------------------------------------------
// Map capture
// ---------------------------------------------------------------------------

/**
 * Capture the current Plotly map as a PNG data URL, temporarily setting the
 * given title text as a chart title so it appears in the exported image.
 *
 * @param {string} titleText - Human-readable label embedded in the image.
 * @returns {Promise<string>} data URL (image/png;base64,…)
 */
export async function captureMapPng(titleText) {
  if (!window.Plotly || !dom.mapDiv) {
    throw new Error("Map is not available for capture");
  }

  const prevTitle = dom.mapDiv.layout?.title ?? "";
  const prevMargin = dom.mapDiv.layout?.margin ?? { l: 0, r: 0, t: 0, b: 0 };

  await Plotly.relayout(dom.mapDiv, {
    title: { text: titleText || "", font: { size: 11, color: "#223f54" } },
    margin: { l: 0, r: 0, t: 28, b: 0 },
  });

  try {
    return await Plotly.toImage(dom.mapDiv, { format: "png", width: 1200, height: 700 });
  } finally {
    await Plotly.relayout(dom.mapDiv, { title: prevTitle, margin: prevMargin });
  }
}

// ---------------------------------------------------------------------------
// Button handlers
// ---------------------------------------------------------------------------

/**
 * Download the current map as a PNG.
 * The filename encodes the selected field, plugin outputs, current step, and
 * map projection scale.
 */
export async function handleSaveStep() {
  try {
    const title = buildStepTitle();
    const dataUrl = await captureMapPng(title.replaceAll("_", " "));
    triggerDownload(dataUrl, safeFilename(title) + ".png");
  } catch (err) {
    console.error("Save step failed:", err);
  }
}

/**
 * Request an animated GIF from the backend using one captured frame per step.
 * Frames are captured automatically after each step advance and stored in
 * uiState.stepMapImages.
 */
export async function handleSaveGif() {
  const frames = [];
  for (let i = 1; i <= uiState.stepLatest; i++) {
    const dataUrl = uiState.stepMapImages?.[String(i)];
    if (dataUrl) {
      frames.push({
        step: i,
        png_base64: dataUrl.replace(/^data:image\/png;base64,/, ""),
      });
    }
  }

  if (frames.length === 0) {
    console.warn("No captured map frames available. Run at least one step first.");
    return;
  }

  const title = buildGifTitle();
  try {
    const response = await fetch("/api/run/save-gif", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ frames, title }),
    });

    if (!response.ok) {
      const detail = await response.json().catch(() => ({}));
      throw new Error(detail.error || `GIF creation failed: ${response.statusText}`);
    }

    const blob = await response.blob();
    const url = URL.createObjectURL(blob);
    triggerDownload(url, safeFilename(title) + ".gif");
    URL.revokeObjectURL(url);
  } catch (err) {
    console.error("Save GIF failed:", err);
  }
}

/**
 * Download the entire temporary run directory as a tar.gz archive.
 */
export async function handleSaveRun() {
  try {
    const response = await fetch("/api/run/archive");

    if (!response.ok) {
      const detail = await response.json().catch(() => ({}));
      throw new Error(detail.error || `Archive request failed: ${response.statusText}`);
    }

    const blob = await response.blob();
    const url = URL.createObjectURL(blob);
    const cd = response.headers.get("Content-Disposition");
    const match = cd?.match(/filename="?([^";]+)"?/);
    triggerDownload(url, match?.[1] || "run-archive.tar.gz");
    URL.revokeObjectURL(url);
  } catch (err) {
    console.error("Save run failed:", err);
  }
}
