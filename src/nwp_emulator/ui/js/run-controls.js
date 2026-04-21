import { dom } from "./dom.js";
import { uiState } from "./state.js";

const STATUS_LABELS = {
  "not-ready": "Not Ready",
  ready: "Ready",
  running: "Running",
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

  dom.fieldToggleList.innerHTML = [
    '<li><label><input type="radio" name="field-toggle" checked> a</label></li>',
    '<li><label><input type="radio" name="field-toggle"> b</label></li>',
  ].join("");

  dom.pluginToggleList.innerHTML = [
    '<li><label><input type="checkbox"> Loaded lugin 1</label></li>',
    '<li><label><input type="checkbox"> Loaded Plugin 2</label></li>',
  ].join("");
}

function updateCurrentStepDisplay(checksState, runContext) {
  if (!dom.currentStepValue) {
    return;
  }

  let totalLabel = "?";

  if (checksState === "passed") {
    if (runContext?.runMode === "grib") {
      const gribSteps = Number(runContext?.gribFileCount);
      totalLabel = Number.isFinite(gribSteps) && gribSteps > 0 ? String(gribSteps) : "# steps";
    } else {
      const nSteps = parseEmulatorNSteps(runContext?.emulatorConfigText);
      totalLabel = nSteps ? String(nSteps) : "?";
    }
  }

  dom.currentStepValue.textContent = `0 / ${totalLabel}`;
}

function statusClass(state) {
  return `status-${state}`;
}

export function setStatus(state) {
  uiState.runStatus = state;

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
}

export function unlockRunWorkspaceOptions() {
  uiState.optionsUnlocked = true;
  const optionsLocked = uiState.checksState !== "passed" || !uiState.optionsUnlocked;
  renderRunWorkspaceOptions(optionsLocked);
}

export function setRunControlsDisabled(disabled) {
  dom.runControlPanel.classList.toggle("locked", disabled);
  const controls = dom.runControlPanel.querySelectorAll("button, input, textarea, select");
  controls.forEach((control) => {
    control.disabled = disabled;
  });

  const optionsLocked = disabled || !uiState.optionsUnlocked;
  renderRunWorkspaceOptions(optionsLocked);
}

export function updateRunAvailability(checksState = uiState.checksState, runContext = uiState.runContext) {
  uiState.checksState = checksState;
  uiState.runContext = runContext;

  if (uiState.checksState !== "passed") {
    uiState.optionsUnlocked = false;
  }

  updateCurrentStepDisplay(uiState.checksState, uiState.runContext);
  setRunControlsDisabled(uiState.checksState !== "passed");

  if (uiState.runStatus === "running") {
    return;
  }

  if (uiState.checksState === "passed") {
    if (uiState.runStatus === "complete" || uiState.runStatus === "failed") {
      return;
    }
    setStatus("ready");
  } else {
    setStatus("not-ready");
  }
}
