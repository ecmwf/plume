import { dom } from "./dom.js";
import { uiState } from "./state.js";
import { updateRunAvailability } from "./run-controls.js";

function setPaneDisabled(pane, disabled) {
  pane.classList.toggle("disabled", disabled);
  const controls = pane.querySelectorAll("button, input, textarea, select");
  controls.forEach((control) => {
    control.disabled = disabled;
  });
}

export function updateSetupAvailability() {
  if (dom.dryRunToggle.checked) {
    setPaneDisabled(dom.plumePane, true);
  } else {
    setPaneDisabled(dom.plumePane, false);
  }

  const selectedMode = document.querySelector("input[name='run-mode']:checked")?.value;
  if (selectedMode === "grib") {
    setPaneDisabled(dom.emulatorPane, true);
    setPaneDisabled(dom.gribPane, false);
  } else {
    setPaneDisabled(dom.emulatorPane, false);
    setPaneDisabled(dom.gribPane, true);
  }
}

export function setChecksStatus(state, label) {
  uiState.checksState = state;
  dom.checksStatus.textContent = label;
  dom.checksStatus.classList.remove("pending", "ok", "err");
  dom.setupTabDot.classList.remove("pending", "ok", "err");

  if (state === "running") {
    dom.checksStatus.classList.add("pending");
    dom.setupTabDot.classList.add("pending");
  }
  if (state === "passed") {
    dom.checksStatus.classList.add("ok");
    dom.setupTabDot.classList.add("ok");
  }
  if (state === "failed") {
    dom.checksStatus.classList.add("err");
    dom.setupTabDot.classList.add("err");
  }

  updateRunAvailability();
}

export function initSetupControls() {
  dom.dryRunToggle.addEventListener("change", updateSetupAvailability);
  dom.runModeRadios.forEach((radio) => {
    radio.addEventListener("change", updateSetupAvailability);
  });

  dom.runChecksBtn.addEventListener("click", () => {
    dom.runChecksBtn.disabled = true;
    setChecksStatus("running", "Running");

    window.setTimeout(() => {
      uiState.checksRunCount += 1;
      const passed = uiState.checksRunCount % 2 === 1;
      if (passed) {
        setChecksStatus("passed", "Passed");
      } else {
        setChecksStatus("failed", "Failed");
      }
      dom.runChecksBtn.disabled = false;
    }, 700);
  });
}
