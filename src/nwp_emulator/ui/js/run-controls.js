import { dom } from "./dom.js";
import { uiState } from "./state.js";

const STATUS_LABELS = {
  "not-ready": "Not Ready",
  ready: "Ready",
  running: "Running",
  "step-complete": "Step Complete",
  complete: "Complete",
  failed: "Failed",
};

function parseEmulatorNSteps(yamlText) {
  if (typeof yamlText !== "string" || !yamlText.trim()) {
    return null;
  }

  const match = yamlText.match(/^\s*n_steps\s*:\s*(\d+)\s*$/m);
  if (!match) {
    return null;
  }

  const value = Number(match[1]);
  return Number.isFinite(value) && value > 0 ? value : null;
}

function deriveTotalLabelFromRunContext(runContext) {
  if (runContext?.runMode === "grib") {
    const gribSteps = Number(runContext?.gribFileCount);
    return Number.isFinite(gribSteps) && gribSteps > 0 ? String(gribSteps) : "# steps";
  }

  const nSteps = parseEmulatorNSteps(runContext?.emulatorConfigText);
  return nSteps ? String(nSteps) : "?";
}

function renderRunWorkspaceOptions(locked) {
  if (!dom.fieldToggleList || !dom.pluginToggleList) {
    return;
  }

  if (locked) {
    dom.fieldToggleList.innerHTML = "";
    dom.pluginToggleList.innerHTML = "";
    if (dom.fieldToggleLockHint) {
      dom.fieldToggleLockHint.style.display = "block";
    }
    if (dom.pluginToggleLockHint) {
      dom.pluginToggleLockHint.style.display = "block";
    }
    return;
  }

  if (dom.fieldToggleLockHint) {
    dom.fieldToggleLockHint.style.display = "none";
  }
  if (dom.pluginToggleLockHint) {
    dom.pluginToggleLockHint.style.display = "none";
  }
}

function updateCurrentStepDisplay(checksState, runContext) {
  if (!dom.currentStepValue) {
    return;
  }

  if (uiState.stepSessionActive) {
    const totalLabel = Number.isFinite(uiState.stepTotal) && uiState.stepTotal > 0
      ? String(uiState.stepTotal)
      : deriveTotalLabelFromRunContext(runContext);
    dom.currentStepValue.textContent = `${uiState.stepCurrent} / ${totalLabel}`;
    return;
  }

  let totalLabel = "?";

  if (checksState === "passed" || checksState === "running") {
    totalLabel = deriveTotalLabelFromRunContext(runContext);
  }

  dom.currentStepValue.textContent = `0 / ${totalLabel}`;
}

function statusClass(state) {
  if (state === "step-complete") {
    return "status-complete";
  }
  return `status-${state}`;
}

function syncTransportButtons() {
  const hasLocalStepNavigation = uiState.stepSessionActive && uiState.stepLatest > 0;

  if (dom.transportPrev) {
    dom.transportPrev.disabled = !(hasLocalStepNavigation && uiState.stepCurrent > 1);
  }

  if (uiState.executionMode === "step" && uiState.stepSessionActive) {
    if (dom.transportPlay) {
      dom.transportPlay.disabled = true;
    }
    if (dom.transportReplay) {
      dom.transportReplay.disabled = false;
    }
    if (dom.transportNext) {
      const canMoveForwardLocally = uiState.stepCurrent < uiState.stepLatest;
      dom.transportNext.disabled = canMoveForwardLocally
        ? false
        : (
          !uiState.stepHasNext ||
          uiState.stepRequestInFlight ||
          uiState.runStatus === "running"
        );
    }
    return;
  }

  if (dom.transportNext) {
    dom.transportNext.disabled = !(hasLocalStepNavigation && uiState.stepCurrent < uiState.stepLatest);
  }

  if (dom.transportPlay) {
    dom.transportPlay.disabled = uiState.runStatus !== "ready";
  }
  if (dom.transportReplay) {
    dom.transportReplay.disabled = !(
      uiState.runStatus === "failed" ||
      uiState.runStatus === "complete" ||
      uiState.runStatus === "step-complete"
    );
  }
}

export function setExecutionMode(mode) {
  uiState.executionMode = mode === "step" ? "step" : "full";
  if (uiState.executionMode !== "step") {
    resetStepProgress();
  }
  syncTransportButtons();
}

export function updateStepProgress(progress = {}) {
  if (typeof progress.active === "boolean") {
    uiState.stepSessionActive = progress.active;
  }
  if (typeof progress.current === "number" && Number.isFinite(progress.current)) {
    const current = Math.max(0, Math.floor(progress.current));
    uiState.stepCurrent = current;
    uiState.stepLatest = Math.max(uiState.stepLatest, current);
  }
  if (progress.total === null || progress.total === undefined) {
    uiState.stepTotal = null;
  } else if (typeof progress.total === "number" && Number.isFinite(progress.total) && progress.total > 0) {
    uiState.stepTotal = Math.floor(progress.total);
  }
  if (typeof progress.hasNext === "boolean") {
    uiState.stepHasNext = progress.hasNext;
  }
  if (typeof progress.requestInFlight === "boolean") {
    uiState.stepRequestInFlight = progress.requestInFlight;
  }

  updateCurrentStepDisplay(uiState.checksState, uiState.runContext);
  syncTransportButtons();
}

export function resetStepProgress() {
  uiState.stepSessionActive = false;
  uiState.stepCurrent = 0;
  uiState.stepLatest = 0;
  uiState.stepTotal = null;
  uiState.stepHasNext = false;
  uiState.stepRequestInFlight = false;
  uiState.stepHistory = {};
  uiState.stepMapImages = {};
  uiState.stepMapData = {};
  uiState.stepRankMapData = {};
  uiState.selectedFieldKey = "";
  uiState.selectedMapRank = null;
  uiState.runMpiNp = 1;
  uiState.mapScaleLocked = false;
  uiState.mapScaleLockedField = "";
  if (dom.mapRankSelector) {
    dom.mapRankSelector.innerHTML = '<option value="global">Global (all ranks)</option>';
  }
  if (dom.mapRankSelectorGroup) {
    dom.mapRankSelectorGroup.hidden = true;
  }
  if (dom.mapScaleLockCheckbox) {
    dom.mapScaleLockCheckbox.checked = false;
  }
  updateCurrentStepDisplay(uiState.checksState, uiState.runContext);
  syncTransportButtons();
}

export function recordStepHistory(step, outputText) {
  const stepNumber = Number(step);
  if (!Number.isFinite(stepNumber) || stepNumber <= 0) {
    return;
  }
  uiState.stepHistory[String(Math.floor(stepNumber))] = String(outputText || "");
  uiState.stepLatest = Math.max(uiState.stepLatest, Math.floor(stepNumber));
}

export function getStepHistory(step) {
  const key = String(Math.floor(Number(step) || 0));
  return uiState.stepHistory[key] || "";
}

export function setViewedStep(step) {
  const stepNumber = Math.max(0, Math.floor(Number(step) || 0));
  uiState.stepCurrent = stepNumber;
  updateCurrentStepDisplay(uiState.checksState, uiState.runContext);
  syncTransportButtons();
}

export function syncOptionsBoxHeight() {
  if (!dom.optionsBox || !dom.mapColumn) {
    return;
  }

  // When the run tab is hidden, map column has no measurable height.
  if (dom.runPanel?.hidden) {
    return;
  }

  const mapHeight = Math.round(dom.mapColumn.getBoundingClientRect().height);
  if (mapHeight > 0) {
    dom.optionsBox.style.height = `${mapHeight}px`;
    dom.optionsBox.style.maxHeight = `${mapHeight}px`;
  }
}

export function setStatus(state) {
  uiState.runStatus = state;

  syncTransportButtons();

  if (!dom.status || !dom.runTabDot) {
    return;
  }

  const classes = ["status-not-ready", "status-ready", "status-running", "status-complete", "status-failed"];
  dom.status.classList.remove(...classes);
  dom.runTabDot.classList.remove(...classes);

  const cssClass = statusClass(state);
  dom.status.classList.add(cssClass);
  dom.runTabDot.classList.add(cssClass);
  dom.status.textContent = STATUS_LABELS[state] || STATUS_LABELS["not-ready"];

  // Disable execution mode switch when run is not ready
  if (dom.execModeStep) {
    dom.execModeStep.disabled = state !== "ready";
  }
}

export function unlockRunWorkspaceOptions() {
  uiState.optionsUnlocked = true;
  // Toggles are now populated via fetchParams() when checks pass, not here.
}

export function setRunControlsDisabled(disabled) {
  dom.runControlPanel.classList.toggle("locked", disabled);
  const controls = dom.runControlPanel.querySelectorAll("button, input, textarea, select");
  controls.forEach((control) => {
    control.disabled = disabled;
  });

  // Keep execution mode locked unless the run status is explicitly ready.
  if (dom.execModeStep) {
    dom.execModeStep.disabled = disabled || uiState.runStatus !== "ready";
  }

  renderRunWorkspaceOptions(disabled);
  if (!disabled) {
    syncTransportButtons();
  }
}

export function updateRunAvailability(checksState = uiState.checksState, runContext = uiState.runContext) {
  uiState.checksState = checksState;
  uiState.runContext = runContext;

  if (uiState.checksState !== "passed") {
    uiState.optionsUnlocked = false;
  }

  updateCurrentStepDisplay(uiState.checksState, uiState.runContext);
  setRunControlsDisabled(uiState.checksState !== "passed");
  requestAnimationFrame(syncOptionsBoxHeight);

  if (uiState.runStatus === "running") {
    return;
  }

  if (uiState.checksState === "passed") {
    if (
      uiState.runStatus === "complete" ||
      uiState.runStatus === "failed" ||
      uiState.runStatus === "step-complete"
    ) {
      return;
    }
    setStatus("ready");
  } else {
    setStatus("not-ready");
  }
}
