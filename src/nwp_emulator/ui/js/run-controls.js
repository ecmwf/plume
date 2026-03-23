import { dom } from "./dom.js";
import { uiState } from "./state.js";

export function setRunControlsDisabled(disabled) {
  dom.runControlPanel.classList.toggle("locked", disabled);
  const controls = dom.runControlPanel.querySelectorAll("button, input, textarea, select");
  controls.forEach((control) => {
    control.disabled = disabled;
  });
}

export function updateRunAvailability() {
  setRunControlsDisabled(uiState.checksState !== "passed");
}

export function setStatus(text, level) {
  dom.status.textContent = text;
  dom.status.classList.remove("ok", "err");
  dom.runTabDot.classList.remove("ok", "err");
  if (level) {
    dom.status.classList.add(level);
    dom.runTabDot.classList.add(level);
  }
}
