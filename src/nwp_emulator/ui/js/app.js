import { dom } from "./dom.js";
import { initTabs } from "./tabs.js";
import { initUploadHandlers } from "./upload-handlers.js";
import { setStatus, updateRunAvailability } from "./run-controls.js";
import {
  fetchSetupState,
  initSetupControls,
  saveEmulatorConfig,
  saveGribSource,
  savePlumeConfig,
  setEmulatorUploadReference,
  setPlumeUploadReference,
} from "./setup-controls.js";
import { initMapControls, applyTranslateMode } from "./map/map-controls.js";
import { renderEqualEarthMap, resizeMap } from "./map/map-renderer-plotly.js";

async function handleLaunch() {
  dom.launchBtn.disabled = true;
  setStatus("Running", null);
  dom.output.textContent = "Launching emulator...";

  try {
    const response = await fetch("/launch", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ dev: dom.devToggle.checked }),
    });

    const result = await response.json();
    if (!response.ok) {
      throw new Error(result.error || "Launch failed");
    }

    const lines = [
      `Command: ${result.command.join(" ")}`,
      `Dev: ${result.dev ? "on" : "off"}`,
      `Return code: ${result.return_code}`,
      "",
      "--- stdout (tail) ---",
      result.stdout_tail || "(empty)",
      "",
      "--- stderr (tail) ---",
      result.stderr_tail || "(empty)",
    ];

    dom.output.textContent = lines.join("\n");
    if (result.return_code === 0) {
      setStatus("Completed", "ok");
    } else {
      setStatus("Failed", "err");
    }
  } catch (error) {
    setStatus("Launch error", "err");
    dom.output.textContent = String(error);
  } finally {
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
    onGribFilesSelected: async (paths) => {
      await saveGribSource("files", paths);
    },
    onGribFolderSelected: async (folderName, topLevelEntries) => {
      await saveGribSource("folder", topLevelEntries, `${folderName}/`);
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
  dom.launchBtn.addEventListener("click", handleLaunch);

  try {
    await fetchSetupState();
  } catch (error) {
    dom.output.textContent = String(error);
    updateRunAvailability("idle");
  }
}

initApp();
