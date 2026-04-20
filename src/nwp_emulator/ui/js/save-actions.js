import { dom } from "./dom.js";
import { uiState } from "./state.js";
import { renderStepMapOverlay, ensureCoastlinesAboveMarkers } from "./map/map-renderer-plotly.js";
import { ensureComputedWindSpeedFields } from "./derived-fields.js";

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
    .map((cb) => String(cb.value || "").trim())
    .filter(Boolean);
}

function pluginLayerCacheKey(pluginName, stepNumber) {
  const name = String(pluginName || "").trim();
  const step = Math.max(0, Math.floor(Number(stepNumber) || 0));
  if (!name || step <= 0) {
    return "";
  }
  return `${name}::${step}`;
}

async function fetchAvailablePluginLayers() {
  try {
    const response = await fetch("/api/run/plugins/available");
    if (!response.ok) {
      return {};
    }
    const payload = await response.json();
    const available = {};
    for (const entry of Array.isArray(payload?.plugins) ? payload.plugins : []) {
      const name = String(entry?.name || "").trim();
      if (name) {
        available[name] = entry;
      }
    }
    return available;
  } catch {
    return {};
  }
}

async function ensureSelectedPluginLayersLoadedForStep(stepNumber) {
  const step = Math.max(0, Math.floor(Number(stepNumber) || 0));
  if (step <= 0) {
    return;
  }

  const selectedPlugins = getSelectedPluginNames();
  if (selectedPlugins.length === 0) {
    return;
  }

  const available = Object.keys(uiState.pluginLayersAvailable || {}).length
    ? uiState.pluginLayersAvailable
    : await fetchAvailablePluginLayers();
  uiState.pluginLayersAvailable = available;

  const fetches = selectedPlugins.map(async (pluginName) => {
    const cacheKey = pluginLayerCacheKey(pluginName, step);
    if (!cacheKey) {
      return;
    }
    if (Object.prototype.hasOwnProperty.call(uiState.pluginLayers, cacheKey)) {
      return;
    }

    const entry = available[pluginName];
    const availableSteps = Array.isArray(entry?.available_steps) ? entry.available_steps : [];
    if (!availableSteps.includes(step)) {
      uiState.pluginLayers[cacheKey] = null;
      return;
    }

    try {
      const encoded = encodeURIComponent(pluginName);
      const response = await fetch(`/api/run/plugins/${encoded}/layer/step/${step}`);
      if (!response.ok) {
        uiState.pluginLayers[cacheKey] = null;
        return;
      }
      const payload = await response.json();
      uiState.pluginLayers[cacheKey] = payload;
    } catch {
      uiState.pluginLayers[cacheKey] = null;
    }
  });

  await Promise.all(fetches);
}

async function flushUiFrame() {
  await new Promise((resolve) => requestAnimationFrame(() => resolve()));
}

function getSelectedFieldKey() {
  const checked = dom.fieldToggleList?.querySelector('input[name="field-toggle"]:checked');
  return (checked?.value || "").trim();
}

function buildStepTitle() {
  const field = getSelectedFieldName();
  const plugins = getSelectedPluginNames();
  const step = uiState.stepCurrent;
  const parts = [`field_${field}`];
  if (plugins.length > 0) parts.push(`plugins_${plugins.join("-")}`);
  parts.push(`step_${step}`);
  return parts.join("_");
}

function buildGifTitle() {
  const field = getSelectedFieldName();
  const plugins = getSelectedPluginNames();
  const parts = [`field_${field}`];
  if (plugins.length > 0) parts.push(`plugins_${plugins.join("-")}`);
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
// Loading overlay helpers
// ---------------------------------------------------------------------------

function showLoadingOverlay(text) {
  if (dom.saveLoadingOverlay) {
    const textEl = dom.saveLoadingOverlay.querySelector(".save-loading-text");
    if (textEl) {
      textEl.textContent = text || "Processing...";
    }
    dom.saveLoadingOverlay.hidden = false;
  }
}

function hideLoadingOverlay() {
  if (dom.saveLoadingOverlay) {
    dom.saveLoadingOverlay.hidden = true;
  }
}

function updateLoadingOverlay(text) {
  if (dom.saveLoadingOverlay) {
    const textEl = dom.saveLoadingOverlay.querySelector(".save-loading-text");
    if (textEl) {
      textEl.textContent = text || "Processing...";
    }
  }
}

async function checkRunFolderExists() {
  try {
    const response = await fetch("/api/run/archive");
    if (response.status === 404) {
      return false;
    }
    return true;
  } catch {
    return false;
  }
}

function shouldSaveFullResolution() {
  return !!dom.saveFullResolution?.checked;
}

function snapshotForCurrentView() {
  const step = String(uiState.stepCurrent || 0);
  if (!step || step === "0") {
    return null;
  }
  if (uiState.selectedMapRank !== null && Number.isFinite(uiState.selectedMapRank)) {
    const snap = uiState.stepRankMapData?.[`${step}:${uiState.selectedMapRank}`] || null;
    return ensureComputedWindSpeedFields(snap);
  }
  const snap = uiState.stepMapData?.[step] || null;
  return ensureComputedWindSpeedFields(snap);
}

function snapshotForStepWithCurrentRank(stepNumber) {
  const key = String(Math.floor(Number(stepNumber) || 0));
  if (!key || key === "0") {
    return null;
  }
  if (uiState.selectedMapRank !== null && Number.isFinite(uiState.selectedMapRank)) {
    const rankSnap = uiState.stepRankMapData?.[`${key}:${uiState.selectedMapRank}`] || null;
    if (rankSnap) {
      return ensureComputedWindSpeedFields(rankSnap);
    }
  }
  const globalSnap = uiState.stepMapData?.[key] || null;
  return ensureComputedWindSpeedFields(globalSnap);
}

async function withSaveResolutionMode(work) {
  const prevEnabled = uiState.browserPointCapEnabled;
  const saveFull = shouldSaveFullResolution();
  if (saveFull) {
    uiState.browserPointCapEnabled = false;
  }
  try {
    return await work();
  } finally {
    uiState.browserPointCapEnabled = prevEnabled;
    const snap = snapshotForCurrentView();
    if (snap) {
      renderStepMapOverlay(snap);
    }
  }
}

function wrapLegendLabel(text, maxCharsPerLine = 22) {
  const words = String(text || "").split(/\s+/);
  if (words.length === 0) return "";
  
  const lines = [];
  let currentLine = "";
  
  for (const word of words) {
    if (!currentLine) {
      currentLine = word;
    } else if ((currentLine.length + 1 + word.length) <= maxCharsPerLine) {
      currentLine += " " + word;
    } else {
      lines.push(currentLine);
      currentLine = word;
    }
  }
  if (currentLine) {
    lines.push(currentLine);
  }
  
  return lines.join("<br>");
}

function polygonBoundaryLegendEntries() {
  const traces = Array.isArray(dom.mapDiv?.data) ? dom.mapDiv.data : [];
  const entries = [];
  const seen = new Set();

  for (let i = 0; i < traces.length; i += 1) {
    const trace = traces[i];
    const meta = trace?.meta;
    if (!meta?.regionHoverHighlight) {
      continue;
    }

    const label = String(meta.legendLabel || "region").trim() || "region";
    const color = String(meta.legendColor || trace?.line?.color || "#d62728");
    const key = `${label}::${color}`;
    if (seen.has(key)) {
      continue;
    }
    seen.add(key);
    entries.push({ traceIndex: i, label, color, key, wrappedLabel: wrapLegendLabel(label) });
  }

  return entries;
}

function buildTickValues(cmin, cmax, count = 5) {
  if (!Number.isFinite(cmin) || !Number.isFinite(cmax)) {
    return [];
  }
  if (cmax === cmin) {
    return [cmin];
  }
  const step = (cmax - cmin) / Math.max(1, count - 1);
  const ticks = [];
  for (let i = 0; i < count; i += 1) {
    ticks.push(cmin + (step * i));
  }
  return ticks;
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
  const prevAnnotations = dom.mapDiv.layout?.annotations ?? [];
  const prevLegend = dom.mapDiv.layout?.legend ?? undefined;
  const prevShowLegend = dom.mapDiv.layout?.showlegend ?? false;
  const prevMarker = dom.mapDiv.data?.[0]?.marker || {};
  const prevSize = Number(prevMarker.size);
  const exportSize = Number.isFinite(prevSize) ? prevSize * 1.5 : 6;

  const cmin = Number(prevMarker.cmin);
  const cmax = Number(prevMarker.cmax);
  const hasScale = Number.isFinite(cmin) && Number.isFinite(cmax);
  const tickvals = hasScale ? buildTickValues(cmin, cmax, 5) : [];
  const ticktext = tickvals.map((value) => Number(value).toFixed(3));

  const polygonLegendEntries = polygonBoundaryLegendEntries();

  if (polygonLegendEntries.length > 0) {
    const restoreIndices = polygonLegendEntries.map((entry) => entry.traceIndex);
    const restoreShowLegend = [];
    const restoreName = [];
    const restoreLegendGroup = [];
    const restoreLineColor = [];

    for (const entry of polygonLegendEntries) {
      const trace = dom.mapDiv.data?.[entry.traceIndex] || {};
      restoreShowLegend.push(Boolean(trace.showlegend));
      restoreName.push(Object.prototype.hasOwnProperty.call(trace, "name") ? trace.name : "");
      restoreLegendGroup.push(Object.prototype.hasOwnProperty.call(trace, "legendgroup") ? trace.legendgroup : "");
      restoreLineColor.push(String(trace?.line?.color || entry.color));
    }

    await Plotly.restyle(dom.mapDiv, {
      showlegend: polygonLegendEntries.map(() => true),
      name: polygonLegendEntries.map((entry) => entry.wrappedLabel),
      legendgroup: polygonLegendEntries.map((entry) => entry.key),
      "line.color": polygonLegendEntries.map((entry) => entry.color),
    }, restoreIndices);

    dom.mapDiv.__exportLegendRestore = {
      indices: restoreIndices,
      showlegend: restoreShowLegend,
      name: restoreName,
      legendgroup: restoreLegendGroup,
      lineColor: restoreLineColor,
    };
  } else {
    dom.mapDiv.__exportLegendRestore = null;
  }

  await Plotly.restyle(dom.mapDiv, {
    marker: [{
      ...prevMarker,
      size: exportSize,
      showscale: hasScale,
      colorbar: hasScale ? {
        orientation: "h",
        x: 0.5,
        xanchor: "center",
        y: -0.02,
        yanchor: "top",
        len: 0.72,
        thickness: 12,
        tickmode: "array",
        tickvals,
        ticktext,
        ticks: "outside",
      } : prevMarker.colorbar,
    }],
  }, [0]);

  await Plotly.relayout(dom.mapDiv, {
    title: { text: titleText || "", font: { size: 16, weight: "bold", color: "#223f54" } },
    margin: {
      l: 0,
      r: polygonLegendEntries.length > 0 ? 210 : 0,
      t: 28,
      b: hasScale ? 58 : 0,
    },
    showlegend: polygonLegendEntries.length > 0,
    legend: polygonLegendEntries.length > 0 ? {
      x: 1.02,
      xanchor: "left",
      y: 1.0,
      yanchor: "top",
      bgcolor: "rgba(255,255,255,0.92)",
      bordercolor: "#c9d8e4",
      borderwidth: 1,
      font: { size: 10, color: "#223f54" },
      traceorder: "normal",
    } : prevLegend,
    annotations: [],
  });

  // Ensure coastlines render on top for export
  ensureCoastlinesAboveMarkers();

  try {
    return await Plotly.toImage(dom.mapDiv, { format: "png", width: 1200, height: 700 });
  } finally {
    const legendRestore = dom.mapDiv.__exportLegendRestore;
    if (legendRestore && Array.isArray(legendRestore.indices) && legendRestore.indices.length > 0) {
      await Plotly.restyle(dom.mapDiv, {
        showlegend: legendRestore.showlegend,
        name: legendRestore.name,
        legendgroup: legendRestore.legendgroup,
        "line.color": legendRestore.lineColor,
      }, legendRestore.indices);
    }
    dom.mapDiv.__exportLegendRestore = null;

    await Plotly.restyle(dom.mapDiv, {
      marker: [{
        ...prevMarker,
        size: Number.isFinite(prevSize) ? prevSize : 4,
        showscale: false,
      }],
    }, [0]);
    await Plotly.relayout(dom.mapDiv, {
      title: prevTitle,
      margin: prevMargin,
      annotations: prevAnnotations,
      showlegend: prevShowLegend,
      legend: prevLegend,
    });
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
    showLoadingOverlay("Saving PNG...");
    // Show loader before async work begins
    await new Promise(resolve => setTimeout(resolve, 10));
    const dataUrl = await withSaveResolutionMode(async () => {
      const snap = snapshotForCurrentView();
      if (snap) {
        renderStepMapOverlay(snap);
      }
      return captureMapPng(title.replaceAll("_", " "));
    });
    triggerDownload(dataUrl, safeFilename(title) + ".png");
  } catch (err) {
    console.error("Save step failed:", err);
  } finally {
    hideLoadingOverlay();
  }
}

/**
 * Request an animated GIF from the backend using one captured frame per step.
 * Frames are captured automatically after each step advance and stored in
 * uiState.stepMapImages.
 */
export async function handleSaveGif() {
  const selectedFieldKey = getSelectedFieldKey();
  if (!selectedFieldKey) {
    console.warn("No field selected. Select a field before exporting GIF.");
    return;
  }

  showLoadingOverlay("Building GIF...");
  await new Promise(resolve => setTimeout(resolve, 10));
  const frames = [];
  const viewedStep = Number(uiState.stepCurrent || 0);
  const previousStepForExport = uiState.stepCurrent;
  let gifBuildStartTime = Date.now();
  const GIF_BUILD_TIMEOUT_MS = 5 * 60 * 1000;
  let errorMessage = null;

  await withSaveResolutionMode(async () => {
    for (let i = 1; i <= uiState.stepLatest; i++) {
      if (Date.now() - gifBuildStartTime > GIF_BUILD_TIMEOUT_MS) {
        errorMessage = "GIF building timed out. Run may have crashed or become unavailable.";
        break;
      }
      if (i % 5 === 0) {
        const folderExists = await checkRunFolderExists();
        if (!folderExists) {
          errorMessage = "Run folder no longer available. Run may have been cleaned up or crashed.";
          break;
        }
      }
      updateLoadingOverlay(`Building GIF... Processing step ${i}/${uiState.stepLatest}`);
      await flushUiFrame();
      const key = String(i);
      const rank = uiState.selectedMapRank;
      let snapshot = null;
      
      if (rank !== null && Number.isFinite(rank)) {
        const rankKey = `${key}:${rank}`;
        snapshot = uiState.stepRankMapData?.[rankKey] || null;
        if (!snapshot) {
          try {
            const response = await fetch(`/api/run/map/step/${key}/rank/${rank}`);
            if (response.ok) {
              snapshot = await response.json();
              ensureComputedWindSpeedFields(snapshot);
              uiState.stepRankMapData[rankKey] = snapshot;
            } else if (response.status === 404) {
              errorMessage = `Step ${i} data for rank ${rank} not found. Run may have crashed.`;
              break;
            }
          } catch (err) {
            errorMessage = `Failed to fetch step ${i} rank ${rank}: ${err.message}`;
            break;
          }
        }
      } else {
        snapshot = uiState.stepMapData?.[key] || null;
        if (!snapshot) {
          try {
            const response = await fetch(`/api/run/map/step/${key}`);
            if (response.ok) {
              snapshot = await response.json();
              ensureComputedWindSpeedFields(snapshot);
              uiState.stepMapData[key] = snapshot;
            } else if (response.status === 404) {
              errorMessage = `Step ${i} data not found. Run may have crashed.`;
              break;
            }
          } catch (err) {
            errorMessage = `Failed to fetch step ${i}: ${err.message}`;
            break;
          }
        }
      }
      ensureComputedWindSpeedFields(snapshot);
      if (!snapshot || !snapshot.fields || !snapshot.fields[selectedFieldKey]) {
        continue;
      }

      uiState.stepCurrent = i;
      await ensureSelectedPluginLayersLoadedForStep(i);
      renderStepMapOverlay(snapshot);
      await flushUiFrame();
      const title = `${buildGifTitle().replaceAll("_", " ")} step ${i}`;
      const dataUrl = await captureMapPng(title);
      frames.push({ step: i, png_base64: dataUrl.replace(/^data:image\/png;base64,/, "") });
    }
  });

  uiState.stepCurrent = previousStepForExport;

  if (errorMessage) {
    console.error("GIF build interrupted:", errorMessage);
    updateLoadingOverlay(`Error: ${errorMessage}`);
    await new Promise(resolve => setTimeout(resolve, 3000));
    hideLoadingOverlay();
    const restoreSnapshot = snapshotForStepWithCurrentRank(viewedStep);
    if (restoreSnapshot) {
      uiState.stepCurrent = viewedStep;
      renderStepMapOverlay(restoreSnapshot);
    }
    return;
  }

  const restoreSnapshot = snapshotForStepWithCurrentRank(viewedStep);
  if (restoreSnapshot) {
    uiState.stepCurrent = viewedStep;
    renderStepMapOverlay(restoreSnapshot);
  }

  if (frames.length === 0) {
    console.warn("No frames available for the selected field across executed steps.");
    updateLoadingOverlay("No frames available for this field.");
    await new Promise(resolve => setTimeout(resolve, 2000));
    hideLoadingOverlay();
    return;
  }

  const title = buildGifTitle();
  try {
    updateLoadingOverlay("Encoding GIF on server...");
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
    updateLoadingOverlay(`Error: ${err.message}`);
    await new Promise(resolve => setTimeout(resolve, 3000));
  } finally {
    hideLoadingOverlay();
  }
}

/**
 * Download the entire temporary run directory as a tar.gz archive.
 */
export async function handleSaveRun() {
  showLoadingOverlay("Archiving run...");
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
  } finally {
    hideLoadingOverlay();
  }
}
