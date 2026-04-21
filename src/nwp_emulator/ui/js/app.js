import { dom } from "./dom.js";
import { initTabs } from "./tabs.js";
import { initUploadHandlers } from "./upload-handlers.js";
import { setStatus, unlockRunWorkspaceOptions, updateRunAvailability } from "./run-controls.js";
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

  if (dom.transportPause) {
    dom.transportPause.disabled = true;
  }
  setStatus("running");
  dom.output.textContent = "Launching emulator...";

  try {
    const response = await fetch("/launch", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ dev: dom.devToggle.checked }),
    });

    const result = await response.json();
    if (!response.ok) {
      if (result.status === "not-ready") {
        setStatus("not-ready");
      }
      throw new Error(result.error || "Launch failed");
    }

    const lines = [
      `Command: ${result.command.join(" ")}`,
      `Engine: ${result.engine || "unknown"}`,
      `Engine Requested: ${result.engine_requested || "unknown"}`,
      `Engine Fallback: ${result.engine_fallback ? "yes" : "no"}`,
      `Run ID: ${result.run_id || "unknown"}`,
      `Run Directory: ${result.run_dir || "unknown"}`,
      `Run Log: ${result.run_log || "unknown"}`,
      `Dev: ${result.dev ? "on" : "off"}`,
      `Return code: ${result.return_code}`,
      "",
      "--- stdout (tail) ---",
      result.stdout_tail || "(empty)",
      "",
      "--- stderr (tail) ---",
      result.stderr_tail || "(empty)",
    ];

    if (result.engine_fallback_reason) {
      lines.push("", `Fallback reason: ${result.engine_fallback_reason}`);
    }

    dom.output.textContent = lines.join("\n");
    const mappedStatus = result.status || (result.return_code === 0 ? "complete" : "failed");
    setStatus(mappedStatus);
  } catch (error) {
    if (!String(error).toLowerCase().includes("not-ready")) {
      setStatus("failed");
    }
    dom.output.textContent = String(error);
  } finally {
    if (dom.transportPause) {
      dom.transportPause.disabled = false;
    }
    updateRunAvailability();
  }
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

  window.addEventListener("resize", resizeMap);
  window.addEventListener("pagehide", notifySessionClose);
  window.addEventListener("beforeunload", notifySessionClose);
  sendSessionHeartbeat();
  window.setInterval(sendSessionHeartbeat, 30000);

  if (dom.transportPause) {
    dom.transportPause.addEventListener("click", handleLaunch);
  }

  try {
    await fetchSetupState();
  } catch (error) {
    dom.output.textContent = String(error);
    updateRunAvailability("idle");
  }
}

initApp();
