import { dom } from "../dom.js";
import { uiState } from "../state.js";

export function updateMapZoom(delta) {
  uiState.mapProjectionScale = Math.max(1, Math.min(6, uiState.mapProjectionScale + delta));
  if (!window.Plotly || !dom.mapDiv) {
    return;
  }
  Plotly.relayout(dom.mapDiv, {
    "geo.projection.scale": uiState.mapProjectionScale,
    "geo.center": uiState.mapCenter,
  });
}

export function applyTranslateMode() {
  if (!window.Plotly || !dom.mapDiv) {
    return;
  }

  if (dom.mapPanToggleBtn) {
    dom.mapPanToggleBtn.classList.toggle("active", uiState.translateEnabled);
    dom.mapPanToggleBtn.textContent = uiState.translateEnabled ? "Translate On" : "Translate";
  }

  Plotly.relayout(dom.mapDiv, {
    dragmode: uiState.translateEnabled ? "pan" : false,
  });
}

export function toggleTranslateMode() {
  uiState.translateEnabled = !uiState.translateEnabled;
  applyTranslateMode();
}

export function initMapControls() {
  dom.mapZoomInBtn.addEventListener("click", () => updateMapZoom(0.22));
  dom.mapZoomOutBtn.addEventListener("click", () => updateMapZoom(-0.22));
  dom.mapPanToggleBtn.addEventListener("click", toggleTranslateMode);
}
