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
import { renderEqualEarthMap, resizeMap } from "./map/map-renderer-plotly.js";
import { captureMapPng, handleSaveStep, handleSaveGif, handleSaveRun } from "./save-actions.js";
import { uiState } from "./state.js";

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

function showStoredStep(stepNumber) {
  const output = getStepHistory(stepNumber);
  if (!output) {
    return false;
  }
  dom.output.textContent = output;
  setViewedStep(stepNumber);
  setStatus(statusForViewedStep(stepNumber));
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
  dom.output.textContent = "Launching emulator...";

  try {
    const executionMode = dom.execModeStep && dom.execModeStep.checked ? "step" : "full";
    setExecutionMode(executionMode);
    if (executionMode !== "step") {
      resetStepProgress();
    }
    const response = await fetch("/launch", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ dev: dom.devToggle.checked, execution_mode: executionMode }),
    });

    const result = await response.json();
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
    }

    const mappedStatus = result.status || (result.return_code === 0 ? "complete" : "failed");
    setStatus(mappedStatus);
  } catch (error) {
    if (!String(error).toLowerCase().includes("not-ready")) {
      setStatus("failed");
    }
    dom.output.textContent = String(error);
  } finally {
    updateRunAvailability();
  }
}

async function handleStepNext() {
  if (!dom.execModeStep?.checked) {
    return;
  }

  if (uiState.stepCurrent < uiState.stepLatest) {
    showStoredStep(uiState.stepCurrent + 1);
    return;
  }

  updateStepProgress({ requestInFlight: true });
  setStatus("running");

  try {
    const response = await fetch("/api/session/step/next", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: "{}",
    });

    const result = await response.json();
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
    updateStepProgress({ requestInFlight: false });
    if (!String(error).toLowerCase().includes("not-ready")) {
      setStatus("failed");
    }
    dom.output.textContent = String(error);
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
  setStatus(dom.runControlPanel.classList.contains("locked") ? "not-ready" : "ready");
}

function handleStepPrev() {
  if (!dom.execModeStep?.checked || uiState.stepCurrent <= 1) {
    return;
  }
  showStoredStep(uiState.stepCurrent - 1);
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

  try {
    await fetchSetupState();
    syncOptionsBoxHeight();
  } catch (error) {
    dom.output.textContent = String(error);
    updateRunAvailability("idle");
  }
}

initApp();
