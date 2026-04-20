import { dom } from "./dom.js";
import { initTabs } from "./tabs.js";
import { initUploadHandlers } from "./upload-handlers.js";
import {
  getStepHistory,
  recordStepHistory,
  resetStepProgress,
  setViewedStep,
  setExecutionMode,
  setStatus,
  syncOptionsBoxHeight,
  unlockRunWorkspaceOptions,
  updateRunAvailability,
  updateStepProgress,
} from "./run-controls.js";
import {
  fetchSetupState,
  initSetupControls,
  saveEmulatorConfig,
  savePlumeConfig,
  setEmulatorUploadReference,
  setPlumeUploadReference,
} from "./setup-controls.js";
import { initMapControls, applyTranslateMode } from "./map/map-controls.js";
import { renderEqualEarthMap, renderStepMapOverlay, resizeMap } from "./map/map-renderer-plotly.js";
import { captureMapPng, handleSaveStep, handleSaveGif, handleSaveRun } from "./save-actions.js";
import { uiState } from "./state.js";
import { ensureComputedWindSpeedFields } from "./derived-fields.js";

function buildRunOutput(result, fallbackExecutionMode = "full") {
  return [
    `Command: ${result.command.join(" ")}`,
    `Run ID: ${result.run_id || "unknown"}`,
    `Run Directory: ${result.run_dir || "unknown"}`,
    `Run Log: ${result.run_log || "unknown"}`,
    `Execution mode: ${result.execution_mode || fallbackExecutionMode}`,
    `Dev: ${result.dev ? "on" : "off"}`,
    `Return code: ${result.return_code}`,
    "",
    "--- stdout (tail) ---",
    result.stdout_tail || "(empty)",
    "",
    "--- stderr (tail) ---",
    result.stderr_tail || "(empty)",
  ].join("\n");
}

function startOutputProgressTicker(prefixText) {
  if (!dom.output) {
    return () => {};
  }

  const spinner = ["|", "/", "-", "\\"];
  let tick = 0;
  const startedAt = Date.now();

  const render = () => {
    const elapsedSec = Math.max(0, Math.floor((Date.now() - startedAt) / 1000));
    dom.output.textContent = [
      `${prefixText} ${spinner[tick % spinner.length]}`,
      `Elapsed: ${elapsedSec}s`,
      "",
      "Working... this may take a while for large MPI runs.",
    ].join("\n");
    tick += 1;
  };

  render();
  const timerId = window.setInterval(render, 1000);
  return () => window.clearInterval(timerId);
}

function statusForViewedStep(stepNumber) {
  if (stepNumber <= 0) {
    return uiState.runStatus;
  }
  const isLatest = stepNumber >= uiState.stepLatest;
  if (isLatest && !uiState.stepHasNext) {
    return "complete";
  }
  return "step-complete";
}

function selectedPluginNames() {
  if (!dom.pluginToggleList) {
    return [];
  }
  return [...dom.pluginToggleList.querySelectorAll('input[type="checkbox"]:checked')]
    .map((cb) => String(cb.value || "").trim())
    .filter(Boolean);
}

function pluginLayerCacheKey(pluginName, stepNumber = uiState.stepCurrent) {
  const name = String(pluginName || "").trim();
  const step = Math.max(0, Math.floor(Number(stepNumber) || 0));
  if (!name || step <= 0) {
    return "";
  }
  return `${name}::${step}`;
}

function pluginHasDataForStep(entry, stepNumber = uiState.stepCurrent) {
  if (!entry || typeof entry !== "object") {
    return false;
  }
  const step = Math.max(0, Math.floor(Number(stepNumber) || 0));
  if (step <= 0) {
    return false;
  }
  const availableSteps = Array.isArray(entry.available_steps) ? entry.available_steps : [];
  return availableSteps.includes(step);
}

function updatePluginToggleAvailabilityIndicators() {
  if (!dom.pluginToggleList) {
    return;
  }

  const available = uiState.pluginLayersAvailable || {};
  const inputs = dom.pluginToggleList.querySelectorAll('input[type="checkbox"]');
  inputs.forEach((input) => {
    const pluginName = String(input.value || "").trim();
    const item = input.closest("li");
    const label = input.closest("label");
    if (!item || !label || !pluginName) {
      return;
    }

    item.classList.remove("plugin-output-no-data");
    label.title = "";
    const existingStatus = label.querySelector(".plugin-output-status");
    if (existingStatus) {
      existingStatus.remove();
    }

    const hasAvailableLayer = pluginHasDataForStep(available[pluginName], uiState.stepCurrent);
    if (input.checked && !hasAvailableLayer) {
      item.classList.add("plugin-output-no-data");
      label.title = "Selected plugin output has no data for this step.";
      const badge = document.createElement("span");
      badge.className = "plugin-output-status";
      badge.textContent = "no data";
      label.appendChild(badge);
    }
  });
}

async function fetchAvailablePluginLayers() {
  try {
    const response = await fetch("/api/run/plugins/available");
    if (!response.ok) {
      uiState.pluginLayersAvailable = {};
      updatePluginToggleAvailabilityIndicators();
      return {};
    }
    const payload = await response.json();
    const available = {};
    for (const entry of Array.isArray(payload?.plugins) ? payload.plugins : []) {
      const name = String(entry?.name || "").trim();
      if (!name) {
        continue;
      }
      available[name] = entry;
    }
    uiState.pluginLayersAvailable = available;
    updatePluginToggleAvailabilityIndicators();
    return available;
  } catch {
    uiState.pluginLayersAvailable = {};
    updatePluginToggleAvailabilityIndicators();
    return {};
  }
}

async function fetchPluginLayer(pluginName, stepNumber = uiState.stepCurrent) {
  const name = String(pluginName || "").trim();
  if (!name) {
    return null;
  }
  const step = Math.max(0, Math.floor(Number(stepNumber) || 0));
  if (step <= 0) {
    return null;
  }
  const cacheKey = pluginLayerCacheKey(name, step);
  if (cacheKey && Object.prototype.hasOwnProperty.call(uiState.pluginLayers, cacheKey)) {
    return uiState.pluginLayers[cacheKey];
  }
  try {
    const encoded = encodeURIComponent(name);
    const endpoint = `/api/run/plugins/${encoded}/layer/step/${step}`;
    const response = await fetch(endpoint);
    if (!response.ok) {
      uiState.pluginLayers[cacheKey] = null;
      return null;
    }
    const payload = await response.json();
    uiState.pluginLayers[cacheKey] = payload;
    return payload;
  } catch {
    uiState.pluginLayers[cacheKey] = null;
    return null;
  }
}

async function ensureSelectedPluginLayersLoaded(stepNumber = uiState.stepCurrent) {
  const step = Math.max(0, Math.floor(Number(stepNumber) || 0));
  const selected = selectedPluginNames();
  if (selected.length === 0) {
    return;
  }
  const available = Object.keys(uiState.pluginLayersAvailable || {}).length
    ? uiState.pluginLayersAvailable
    : await fetchAvailablePluginLayers();
  const fetches = selected
    .filter((name) => pluginHasDataForStep(available[name], step))
    .map((name) => fetchPluginLayer(name, step));

  selected
    .filter((name) => !pluginHasDataForStep(available[name], step))
    .forEach((name) => {
      const cacheKey = pluginLayerCacheKey(name, step);
      uiState.pluginLayers[cacheKey] = null;
    });

  if (fetches.length > 0) {
    await Promise.all(fetches);
  }
}

async function fetchStepMapSnapshot(stepNumber) {
  const key = String(Math.floor(Number(stepNumber) || 0));
  const rank = uiState.selectedMapRank;

  if (rank !== null && Number.isFinite(rank)) {
    const rankKey = `${key}:${rank}`;
    if (uiState.stepRankMapData[rankKey]) {
      return ensureComputedWindSpeedFields(uiState.stepRankMapData[rankKey]);
    }
    try {
      const response = await fetch(`/api/run/map/step/${key}/rank/${rank}`);
      if (!response.ok) {
        return null;
      }
      const payload = await response.json();
      // Rank file has same fields structure; normalise to expected shape.
      if (!payload.field_keys) {
        payload.field_keys = Object.keys(payload.fields || {});
      }
      ensureComputedWindSpeedFields(payload);
      uiState.stepRankMapData[rankKey] = payload;
      return payload;
    } catch {
      return null;
    }
  }

  if (uiState.stepMapData[key]) {
    return ensureComputedWindSpeedFields(uiState.stepMapData[key]);
  }
  try {
    const response = await fetch(`/api/run/map/step/${key}`);
    if (!response.ok) {
      return null;
    }
    const payload = await response.json();
    ensureComputedWindSpeedFields(payload);
    uiState.stepMapData[key] = payload;
    return payload;
  } catch {
    return null;
  }
}

async function renderMapForViewedStep(stepNumber) {
  if (!uiState.stepSessionActive || stepNumber <= 0) {
    renderStepMapOverlay(null);
    return;
  }
  updatePluginToggleAvailabilityIndicators();
  // Always ensure global snapshot is available in stepMapData so that
  // renderStepMapOverlay can derive scale from global data even when a rank is selected.
  const key = String(Math.floor(Number(stepNumber) || 0));
  if (!uiState.stepMapData[key]) {
    try {
      const response = await fetch(`/api/run/map/step/${key}`);
      if (response.ok) {
        const payload = await response.json();
        ensureComputedWindSpeedFields(payload);
        uiState.stepMapData[key] = payload;
      }
    } catch { /* non-fatal */ }
  }
  await ensureSelectedPluginLayersLoaded(stepNumber);
  const snapshot = await fetchStepMapSnapshot(stepNumber);
  renderStepMapOverlay(snapshot);
}

async function showStoredStep(stepNumber) {
  let output = getStepHistory(stepNumber);
  if (!output) {
    if (uiState.executionMode === "full") {
      output = getStepHistory(uiState.stepCurrent) || dom.output.textContent || "";
      if (!output) {
        return false;
      }
      recordStepHistory(stepNumber, output);
    } else {
      return false;
    }
  }
  if (uiState.executionMode === "step") {
    dom.output.textContent = output;
  }
  setViewedStep(stepNumber);
  setStatus(statusForViewedStep(stepNumber));
  await renderMapForViewedStep(stepNumber);
  return true;
}

function notifySessionClose() {
  const endpoint = "/api/session/close";
  try {
    if (navigator.sendBeacon) {
      const payload = new Blob(["{}"], { type: "application/json" });
      navigator.sendBeacon(endpoint, payload);
      return;
    }
  } catch (error) {
    // Ignore close notification failures during shutdown.
  }

  fetch(endpoint, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: "{}",
    keepalive: true,
  }).catch(() => {
    // Ignore close notification failures during shutdown.
  });
}

function sendSessionHeartbeat() {
  fetch("/api/session/heartbeat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: "{}",
    keepalive: true,
  }).catch(() => {
    // Ignore heartbeat failures; reaper fallback will handle stale sessions.
  });
}

function populateRankSelector(mpiNp) {
  uiState.runMpiNp = Number(mpiNp) || 1;
  if (!dom.mapRankSelector || !dom.mapRankSelectorGroup) {
    return;
  }
  if (uiState.runMpiNp <= 1) {
    dom.mapRankSelectorGroup.hidden = true;
    dom.mapRankSelector.value = "global";
    uiState.selectedMapRank = null;
    return;
  }
  dom.mapRankSelector.innerHTML = '<option value="global">Global (all ranks)</option>';
  for (let r = 0; r < uiState.runMpiNp; r++) {
    const opt = document.createElement("option");
    opt.value = String(r);
    opt.textContent = `Rank ${r}`;
    dom.mapRankSelector.appendChild(opt);
  }
  dom.mapRankSelector.value = "global";
  uiState.selectedMapRank = null;
  dom.mapRankSelectorGroup.hidden = false;
}

function syncBrowserPointCapFromSetup() {
  if (!dom.mapPointLimitPreset) {
    return;
  }
  const preset = String(dom.mapPointLimitPreset.value || "balanced");
  uiState.mapPointLimitPreset = preset;
  if (preset === "full") {
    uiState.browserPointCapEnabled = false;
    uiState.browserMaxPoints = 0;
    return;
  }
  uiState.browserPointCapEnabled = true;
  uiState.browserMaxPoints = preset === "fast" ? 10000 : 50000;
}

async function handleLaunch() {
  if (dom.runControlPanel.classList.contains("locked")) {
    setStatus("not-ready");
    return;
  }

  unlockRunWorkspaceOptions();

  // Disable play button to prevent double-clicks; replay re-enables it.
  if (dom.transportPlay) {
    dom.transportPlay.disabled = true;
  }
  setStatus("running");
  let stopProgressTicker = startOutputProgressTicker("Launching emulator");

  try {
    syncBrowserPointCapFromSetup();
    uiState.pluginLayers = {};
    uiState.pluginLayersAvailable = {};
    updatePluginToggleAvailabilityIndicators();
    const executionMode = dom.execModeStep && dom.execModeStep.checked ? "step" : "full";
    setExecutionMode(executionMode);
    if (executionMode !== "step") {
      resetStepProgress();
      renderStepMapOverlay(null);
    }
    const response = await fetch("/launch", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ dev: dom.devToggle.checked, execution_mode: executionMode }),
    });

    const result = await response.json();
    if (stopProgressTicker) {
      stopProgressTicker();
      stopProgressTicker = null;
    }
    if (!response.ok) {
      if (result.status === "not-ready") {
        setStatus("not-ready");
      }
      throw new Error(result.error || "Launch failed");
    }

    const outputText = buildRunOutput(result, executionMode);
    dom.output.textContent = outputText;

    if (executionMode === "step") {
      const currentStep = Number(result.current_step ?? result.last_step_run ?? 0);
      const totalSteps = Number(result.total_steps);
      recordStepHistory(currentStep, outputText);
      populateRankSelector(result.mpi_np ?? 1);
      if (currentStep > 0) {
        setViewedStep(currentStep);
        await fetchAvailablePluginLayers();
        await ensureSelectedPluginLayersLoaded(currentStep);
        await fetchStepMapSnapshot(currentStep);
        await renderMapForViewedStep(currentStep);
      }
      // Fire-and-forget map capture so GIF assembly has a frame for this step.
      captureMapPng(`Step ${currentStep}`).then((dataUrl) => {
        uiState.stepMapImages[String(currentStep)] = dataUrl;
      }).catch(() => {});
      updateStepProgress({
        active: true,
        current: Number.isFinite(currentStep) ? currentStep : 0,
        total: Number.isFinite(totalSteps) ? totalSteps : null,
        hasNext: !!result.has_next,
        requestInFlight: false,
      });
    } else {
      const lastStep = Number(result.last_step_run ?? result.current_step ?? result.total_steps ?? 0);
      const totalSteps = Number(result.total_steps);
      const inferredTotal = Number.isFinite(totalSteps) && totalSteps > 0
        ? Math.floor(totalSteps)
        : (Number.isFinite(lastStep) && lastStep > 0 ? Math.floor(lastStep) : 0);
      const initialStep = inferredTotal > 0 ? 1 : 0;

      populateRankSelector(result.mpi_np ?? 1);
      if (inferredTotal > 0) {
        for (let step = 1; step <= inferredTotal; step += 1) {
          recordStepHistory(step, outputText);
        }
        await fetchAvailablePluginLayers();
        await ensureSelectedPluginLayersLoaded(initialStep);
        await fetchStepMapSnapshot(initialStep);
      }
      updateStepProgress({
        active: inferredTotal > 0,
        current: initialStep,
        total: inferredTotal > 0 ? inferredTotal : null,
        hasNext: false,
        requestInFlight: false,
      });
      if (initialStep > 0) {
        setViewedStep(initialStep);
        await renderMapForViewedStep(initialStep);
      }
    }

    const mappedStatus = result.status || (result.return_code === 0 ? "complete" : "failed");
    setStatus(mappedStatus);
  } catch (error) {
    if (stopProgressTicker) {
      stopProgressTicker();
      stopProgressTicker = null;
    }
    if (!String(error).toLowerCase().includes("not-ready")) {
      setStatus("failed");
    }
    dom.output.textContent = String(error);
  } finally {
    if (stopProgressTicker) {
      stopProgressTicker();
      stopProgressTicker = null;
    }
    updateRunAvailability();
  }
}

async function handleStepNext() {
  if (!uiState.stepSessionActive) {
    return;
  }

  // Full mode: purely local navigation across already generated steps.
  if (uiState.executionMode !== "step") {
    if (uiState.stepCurrent < uiState.stepLatest) {
      await showStoredStep(uiState.stepCurrent + 1);
    }
    return;
  }

  if (uiState.stepCurrent < uiState.stepLatest) {
    await showStoredStep(uiState.stepCurrent + 1);
    return;
  }

  updateStepProgress({ requestInFlight: true });
  setStatus("running");
  let stopProgressTicker = startOutputProgressTicker("Advancing step");

  try {
    const response = await fetch("/api/session/step/next", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: "{}",
    });

    const result = await response.json();
    if (stopProgressTicker) {
      stopProgressTicker();
      stopProgressTicker = null;
    }
    if (!response.ok) {
      if (result.status === "not-ready") {
        setStatus("not-ready");
      }
      throw new Error(result.error || "Step advance failed");
    }

    const outputText = buildRunOutput(result, "step");
    dom.output.textContent = outputText;

    const currentStep = Number(result.current_step ?? result.last_step_run ?? 0);
    const totalSteps = Number(result.total_steps);
    recordStepHistory(currentStep, outputText);
    if (currentStep > 0) {
      setViewedStep(currentStep);
      await fetchAvailablePluginLayers();
      await ensureSelectedPluginLayersLoaded(currentStep);
      await fetchStepMapSnapshot(currentStep);
      await renderMapForViewedStep(currentStep);
    }
    // Fire-and-forget map capture for GIF assembly.
    captureMapPng(`Step ${currentStep}`).then((dataUrl) => {
      uiState.stepMapImages[String(currentStep)] = dataUrl;
    }).catch(() => {});
    updateStepProgress({
      active: true,
      current: Number.isFinite(currentStep) ? currentStep : 0,
      total: Number.isFinite(totalSteps) ? totalSteps : null,
      hasNext: !!result.has_next,
      requestInFlight: false,
    });
    setStatus(result.status || (result.step_finished ? "complete" : "ready"));
  } catch (error) {
    if (stopProgressTicker) {
      stopProgressTicker();
      stopProgressTicker = null;
    }
    updateStepProgress({ requestInFlight: false });
    if (!String(error).toLowerCase().includes("not-ready")) {
      setStatus("failed");
    }
    dom.output.textContent = String(error);
  } finally {
    if (stopProgressTicker) {
      stopProgressTicker();
      stopProgressTicker = null;
    }
  }
}

async function handleReplay() {
  // Clean up current run directory then re-enable the play button.
  try {
    await fetch("/api/session/close", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: "{}",
    });
  } catch (_) {
    // Non-fatal — continue regardless of cleanup result.
  }

  dom.output.textContent = "No run yet.";
  resetStepProgress();
  uiState.pluginLayers = {};
  uiState.pluginLayersAvailable = {};
  updatePluginToggleAvailabilityIndicators();
  renderStepMapOverlay(null);
  setStatus(dom.runControlPanel.classList.contains("locked") ? "not-ready" : "ready");
}

async function handleStepPrev() {
  if (!uiState.stepSessionActive || uiState.stepCurrent <= 1) {
    return;
  }
  await showStoredStep(uiState.stepCurrent - 1);
}

async function initApp() {
  initTabs();
  initSetupControls();
  initUploadHandlers(dom.uploadButtons, {
    onPlumeYamlUploaded: async (text, fileName) => {
      setPlumeUploadReference(text);
      await savePlumeConfig(text, fileName);
    },
    onEmulatorYamlUploaded: async (text, fileName) => {
      setEmulatorUploadReference(text);
      await saveEmulatorConfig(text, fileName);
    },
  });
  initMapControls();

  dom.plumeEditor.addEventListener("change", async () => {
    try {
      await savePlumeConfig(dom.plumeEditor.value, dom.plumeUploadPath.value);
    } catch (error) {
      dom.output.textContent = String(error);
    }
  });

  dom.emulatorEditor.addEventListener("change", async () => {
    try {
      await saveEmulatorConfig(dom.emulatorEditor.value, dom.emulatorUploadPath.value);
    } catch (error) {
      dom.output.textContent = String(error);
    }
  });

  renderEqualEarthMap();
  applyTranslateMode();
  renderStepMapOverlay(null);

  if (dom.fieldToggleList) {
    dom.fieldToggleList.addEventListener("change", async () => {
      // Reset scale lock when field changes
      uiState.mapScaleLocked = false;
      if (dom.mapScaleLockCheckbox) {
        dom.mapScaleLockCheckbox.checked = false;
      }
      if (uiState.stepSessionActive && uiState.stepCurrent > 0) {
        await renderMapForViewedStep(uiState.stepCurrent);
      } else {
        renderStepMapOverlay(null);
      }
    });
  }

  if (dom.pluginToggleList) {
    dom.pluginToggleList.addEventListener("change", async () => {
      if (uiState.stepSessionActive && uiState.stepCurrent > 0) {
        await fetchAvailablePluginLayers();
        await ensureSelectedPluginLayersLoaded(uiState.stepCurrent);
        updatePluginToggleAvailabilityIndicators();
        await renderMapForViewedStep(uiState.stepCurrent);
      }
    });
  }

  if (dom.mapRankSelector) {
    dom.mapRankSelector.addEventListener("change", async () => {
      const value = dom.mapRankSelector.value;
      uiState.selectedMapRank = value === "global" ? null : Number(value);
      if (uiState.stepSessionActive && uiState.stepCurrent > 0) {
        await renderMapForViewedStep(uiState.stepCurrent);
      } else {
        renderStepMapOverlay(null);
      }
    });
  }

  if (dom.mapPointLimitPreset) {
    dom.mapPointLimitPreset.addEventListener("change", async () => {
      syncBrowserPointCapFromSetup();
      if (uiState.stepSessionActive && uiState.stepCurrent > 0) {
        await renderMapForViewedStep(uiState.stepCurrent);
      }
    });
  }

  window.addEventListener("resize", () => {
    resizeMap();
    syncOptionsBoxHeight();
  });
  window.addEventListener("pagehide", notifySessionClose);
  window.addEventListener("beforeunload", notifySessionClose);
  sendSessionHeartbeat();
  window.setInterval(sendSessionHeartbeat, 30000);

  if (dom.transportPlay) {
    dom.transportPlay.addEventListener("click", handleLaunch);
  }

  if (dom.transportReplay) {
    dom.transportReplay.addEventListener("click", handleReplay);
  }

  if (dom.transportNext) {
    dom.transportNext.addEventListener("click", handleStepNext);
  }

  if (dom.transportPrev) {
    dom.transportPrev.addEventListener("click", handleStepPrev);
  }

  if (dom.saveStepBtn) {
    dom.saveStepBtn.addEventListener("click", handleSaveStep);
  }

  if (dom.saveGifBtn) {
    dom.saveGifBtn.addEventListener("click", handleSaveGif);
  }

  if (dom.saveRunBtn) {
    dom.saveRunBtn.addEventListener("click", handleSaveRun);
  }

  if (dom.mapScaleLockCheckbox) {
    dom.mapScaleLockCheckbox.addEventListener("change", async () => {
      const fieldKey = (dom.fieldToggleList?.querySelector('input[name="field-toggle"]:checked')?.value || "").trim();
      if (!fieldKey) {
        uiState.mapScaleLocked = false;
        uiState.mapScaleLockedField = "";
        dom.mapScaleLockCheckbox.checked = false;
        return;
      }

      if (dom.mapScaleLockCheckbox.checked) {
        // Prefer the currently rendered scale to lock exactly what the user sees.
        const marker = dom.mapDiv?.data?.[0]?.marker;
        const currentCmin = Number(marker?.cmin);
        const currentCmax = Number(marker?.cmax);
        if (Number.isFinite(currentCmin) && Number.isFinite(currentCmax)) {
          uiState.mapScaleLockedRange = {
            cmin: currentCmin,
            cmax: currentCmax !== currentCmin ? currentCmax : currentCmin + 1,
          };
        }

        // Fallback: capture from global aggregate snapshot so locking while a
        // rank is selected still results in the same scale.
        if (uiState.stepSessionActive && uiState.stepCurrent > 0) {
          const key = String(uiState.stepCurrent);
          let globalSnapshot = uiState.stepMapData[key] || null;
          if (!globalSnapshot) {
            try {
              const resp = await fetch(`/api/run/map/step/${key}`);
              if (resp.ok) {
                globalSnapshot = await resp.json();
                ensureComputedWindSpeedFields(globalSnapshot);
                uiState.stepMapData[key] = globalSnapshot;
              }
            } catch { /* non-fatal */ }
          }
          if (globalSnapshot) {
            ensureComputedWindSpeedFields(globalSnapshot);
          }
          if (globalSnapshot?.fields?.[fieldKey]) {
            const values = globalSnapshot.fields[fieldKey].values || [];
            const finiteValues = values.filter(v => Number.isFinite(Number(v))).map(v => Number(v));
            if (finiteValues.length > 0) {
              const cmin = Math.min(...finiteValues);
              const cmax = Math.max(...finiteValues);
              uiState.mapScaleLockedRange = { cmin, cmax: cmax !== cmin ? cmax : cmin + 1 };
            }
          }
        }
        uiState.mapScaleLocked = true;
        uiState.mapScaleLockedField = fieldKey;
      } else {
        uiState.mapScaleLocked = false;
        uiState.mapScaleLockedField = "";
      }

      if (uiState.stepSessionActive && uiState.stepCurrent > 0) {
        await renderMapForViewedStep(uiState.stepCurrent);
      }
    });
  }

  try {
    await fetchSetupState();
    syncBrowserPointCapFromSetup();
    syncOptionsBoxHeight();
  } catch (error) {
    dom.output.textContent = String(error);
    updateRunAvailability("idle");
  }
}

initApp();
