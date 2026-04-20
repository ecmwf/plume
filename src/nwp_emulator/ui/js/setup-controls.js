import { dom } from "./dom.js";
import { resetStepProgress, updateRunAvailability } from "./run-controls.js";
import { renderStepMapOverlay } from "./map/map-renderer-plotly.js";
import { getComputedWindSpeedFieldNames } from "./derived-fields.js";

// Start with undefined; only set these when user explicitly uploads in THIS session
let plumeUploadedReference = undefined;
let emulatorUploadedReference = undefined;
let latestGribPathDisplay = "";
let latestGribSelectedPaths = [];
let latestGribMetadata = {};
let latestChecksResults = {};
let latestChecksMessages = {};

const CHECK_IDS = {
  plume_plugins_template: {
    item: () => dom.checkItemPlumeTemplate,
    icon: () => dom.checkIconPlumeTemplate,
    message: () => dom.checkMsgPlumeTemplate,
  },
  emulator_yaml_basic: {
    item: () => dom.checkItemEmulatorYaml,
    icon: () => dom.checkIconEmulatorYaml,
    message: () => dom.checkMsgEmulatorYaml,
  },
  grib_source_selection: {
    item: () => dom.checkItemGribSource,
    icon: () => dom.checkIconGribSource,
    message: () => dom.checkMsgGribSource,
  },
  mpi_np_config: {
    item: () => dom.checkItemMpiNp,
    icon: () => dom.checkIconMpiNp,
    message: () => dom.checkMsgMpiNp,
  },
};

function setPaneDisabled(pane, disabled) {
  pane.classList.toggle("disabled", disabled);
  const controls = pane.querySelectorAll("button, input, textarea, select");
  controls.forEach((control) => {
    control.disabled = disabled;
  });
}

function checksStateFromBackend(rawState) {
  if (rawState === "passed") {
    return "passed";
  }
  if (rawState === "failed") {
    return "failed";
  }
  if (rawState === "running") {
    return "running";
  }
  return "idle";
}

function checksLabelFromState(state) {
  if (state === "passed") {
    return "Passed";
  }
  if (state === "failed") {
    return "Failed";
  }
  if (state === "running") {
    return "Running";
  }
  return "Not Run";
}

async function fetchJson(url, options) {
  const response = await fetch(url, options);
  const payload = await response.json();
  if (!response.ok) {
    throw new Error(payload.error || "Setup API request failed");
  }
  return payload;
}

function setCheckVisual(checkKey, status, disabled) {
  const refs = CHECK_IDS[checkKey];
  if (!refs) {
    return;
  }

  const item = refs.item();
  const icon = refs.icon();
  const msg = refs.message();
  if (!item || !icon || !msg) {
    return;
  }

  item.classList.remove("pass", "fail", "disabled");
  msg.textContent = "";

  if (disabled) {
    item.classList.add("disabled");
    icon.textContent = "☐";
    return;
  }

  if (status === "passed") {
    item.classList.add("pass");
    icon.textContent = "✓";
    return;
  }

  if (status === "failed") {
    item.classList.add("fail");
    icon.textContent = "✗";
    return;
  }

  icon.textContent = "☐";
}

function setCheckMessage(checkKey, messages, disabled) {
  const refs = CHECK_IDS[checkKey];
  if (!refs) {
    return;
  }

  const msg = refs.message();
  if (!msg) {
    return;
  }

  if (disabled) {
    msg.textContent = "";
    return;
  }

  if (!Array.isArray(messages) || messages.length === 0) {
    msg.textContent = "";
    return;
  }

  msg.textContent = messages.join("\n");
}

function renderChecksList(results, resultMessages, dryRun, emulatorPaneDisabled, gribPaneDisabled) {
  const safeResults = results || {};
  const safeMessages = resultMessages || {};
  setCheckVisual("plume_plugins_template", safeResults.plume_plugins_template || "not-run", dryRun);
  setCheckMessage("plume_plugins_template", safeMessages.plume_plugins_template || [], dryRun);

  setCheckVisual("emulator_yaml_basic", safeResults.emulator_yaml_basic || "not-run", emulatorPaneDisabled);
  setCheckMessage("emulator_yaml_basic", safeMessages.emulator_yaml_basic || [], emulatorPaneDisabled);

  setCheckVisual("grib_source_selection", safeResults.grib_source_selection || "not-run", gribPaneDisabled);
  setCheckMessage("grib_source_selection", safeMessages.grib_source_selection || [], gribPaneDisabled);

  setCheckVisual("mpi_np_config", safeResults.mpi_np_config || "not-run", false);
  setCheckMessage("mpi_np_config", safeMessages.mpi_np_config || [], false);
}

function updateGribInfoBox() {
  if (!dom.gribInfoBox) {
    return;
  }

  const gribPaneDisabled = dom.gribPane.classList.contains("disabled");
  const gribStatus = latestChecksResults.grib_source_selection;
  const meta = latestGribMetadata || {};
  const params = Array.isArray(meta.params) && meta.params.length > 0 ? meta.params : ["unknown"];

  dom.gribInfoBox.classList.remove("invalid");

  if (gribPaneDisabled || !gribStatus || gribStatus === "idle") {
    dom.gribInfoBox.textContent = "Run checks to get info about the selected data.";
    return;
  }

  if (gribStatus === "failed") {
    dom.gribInfoBox.classList.add("invalid");
    dom.gribInfoBox.textContent = [
      "# steps: error",
      "grid identifier: error",
      "parameters: error",
    ].join("\n");
    return;
  }

  const lines = [];
  lines.push(`# steps: ${meta.grib_file_count ?? 0}`);
  lines.push(`grid identifier: ${meta.grid_identifier ?? "unknown"}`);
  lines.push(`parameters:`);
  params.forEach((param) => {
    lines.push(`${param}`);
  });
  dom.gribInfoBox.textContent = lines.join("\n");
}

function updatePlumeEditorHighlights() {
  const dryRun = dom.dryRunToggle.checked;
  const plumeCheckFailed = latestChecksResults.plume_plugins_template === "failed";
  const edited =
    !dryRun &&
    plumeUploadedReference !== undefined &&
    dom.plumeEditor.value !== plumeUploadedReference;

  dom.plumeEditor.classList.remove("modified", "invalid");
  dom.plumeEditorState.classList.remove("modified", "failed");
  dom.plumeEditorState.textContent = "";

  if (!dryRun && plumeCheckFailed) {
    dom.plumeEditor.classList.add("invalid");
    dom.plumeEditorState.classList.add("failed");
    dom.plumeEditorState.textContent = "failed checks";
    return;
  }

  if (edited) {
    dom.plumeEditor.classList.add("modified");
    dom.plumeEditorState.classList.add("modified");
    dom.plumeEditorState.textContent = "modified";
  }
}

function updateEmulatorEditorHighlights() {
  const emulatorCheckFailed = latestChecksResults.emulator_yaml_basic === "failed";
  const edited =
    emulatorUploadedReference !== undefined &&
    dom.emulatorEditor.value !== emulatorUploadedReference;

  dom.emulatorEditor.classList.remove("modified", "invalid");
  dom.emulatorEditorState.classList.remove("modified", "failed");
  dom.emulatorEditorState.textContent = "";

  if (emulatorCheckFailed) {
    dom.emulatorEditor.classList.add("invalid");
    dom.emulatorEditorState.classList.add("failed");
    dom.emulatorEditorState.textContent = "failed checks";
    return;
  }

  if (edited) {
    dom.emulatorEditor.classList.add("modified");
    dom.emulatorEditorState.classList.add("modified");
    dom.emulatorEditorState.textContent = "modified";
  }
}

export function setPlumeUploadReference(text) {
  plumeUploadedReference = text;
  if (text === null) {
    sessionStorage.removeItem("plumeUploadedRef");
  } else {
    sessionStorage.setItem("plumeUploadedRef", text);
  }
  updatePlumeEditorHighlights();
}

export function setEmulatorUploadReference(text) {
  emulatorUploadedReference = text;
  if (text === null) {
    sessionStorage.removeItem("emulatorUploadedRef");
  } else {
    sessionStorage.setItem("emulatorUploadedRef", text);
  }
  updateEmulatorEditorHighlights();
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

  const emulatorPaneDisabled = dom.emulatorPane.classList.contains("disabled");
  const gribPaneDisabled = dom.gribPane.classList.contains("disabled");
  renderChecksList(
    latestChecksResults,
    latestChecksMessages,
    dom.dryRunToggle.checked,
    emulatorPaneDisabled,
    gribPaneDisabled,
  );
  updatePlumeEditorHighlights();
  updateEmulatorEditorHighlights();
  updateGribInfoBox();
}

function clearToggleNames() {
  if (!dom.fieldToggleList || !dom.pluginToggleList) {
    return;
  }
  dom.fieldToggleList.innerHTML = '<li class="options-placeholder">No fields available yet.</li>';
  dom.pluginToggleList.innerHTML = '<li class="options-placeholder">No plugin outputs available yet.</li>';
  dom.fieldToggleList.closest(".option-group")?.classList.add("empty");
  dom.pluginToggleList.closest(".option-group")?.classList.add("empty");
  requestAnimationFrame(updateAllToggleScrollbars);
}

function updateToggleScrollbar(listEl) {
  const shell = listEl?.closest(".toggle-scroll-shell");
  const track = shell?.querySelector(".toggle-scrollbar");
  const thumb = track?.querySelector(".toggle-scrollbar-thumb");
  if (!shell || !track || !thumb) {
    return;
  }

  const scrollHeight = listEl.scrollHeight;
  const clientHeight = listEl.clientHeight;
  const maxScroll = Math.max(0, scrollHeight - clientHeight);

  if (maxScroll <= 0 || clientHeight <= 0) {
    track.classList.add("hidden");
    thumb.style.height = "0px";
    thumb.style.transform = "translateY(0)";
    return;
  }

  track.classList.remove("hidden");

  const trackHeight = track.clientHeight;
  const thumbHeight = Math.max(20, Math.round((clientHeight / scrollHeight) * trackHeight));
  const maxThumbTop = Math.max(0, trackHeight - thumbHeight);
  const thumbTop = Math.round((listEl.scrollTop / maxScroll) * maxThumbTop);

  thumb.style.height = `${thumbHeight}px`;
  thumb.style.transform = `translateY(${thumbTop}px)`;
}

function updateAllToggleScrollbars() {
  if (dom.fieldToggleList) {
    updateToggleScrollbar(dom.fieldToggleList);
  }
  if (dom.pluginToggleList) {
    updateToggleScrollbar(dom.pluginToggleList);
  }
}

export function refreshToggleScrollbars() {
  requestAnimationFrame(updateAllToggleScrollbars);
}

let _paramsFetchGen = 0;

async function fetchAndRenderParams() {
  const gen = ++_paramsFetchGen;
  try {
    const data = await fetchJson("/api/setup/params", undefined);
    if (gen !== _paramsFetchGen) {
      return;
    }
    renderToggleNames(data.field_names || [], data.plugin_output_names || []);
  } catch (_) {
    // Non-fatal: toggles stay empty until params are available.
  }
}

function renderToggleNames(fieldNames, pluginOutputNames) {
  if (!dom.fieldToggleList || !dom.pluginToggleList) {
    return;
  }

  const nativeFieldNames = Array.isArray(fieldNames)
    ? fieldNames.map((name) => String(name || "").trim()).filter(Boolean)
    : [];
  const computedFieldNames = getComputedWindSpeedFieldNames(nativeFieldNames);
  const specialNames = new Set(computedFieldNames);
  const allFieldNames = [...nativeFieldNames, ...computedFieldNames];

  const escapeHtml = (value) => String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");

  if (dom.fieldToggleLockHint) {
    dom.fieldToggleLockHint.style.display = "none";
  }
  if (dom.pluginToggleLockHint) {
    dom.pluginToggleLockHint.style.display = "none";
  }

  dom.fieldToggleList.innerHTML = allFieldNames.length
    ? ["None", ...allFieldNames]
        .map((name, i) => {
          const value = i === 0 ? "" : name;
          const liClass = i !== 0 && specialNames.has(name) ? ' class="special-field-toggle"' : "";
          return `<li${liClass}><label><input type="radio" name="field-toggle" value="${escapeHtml(value)}"${i === 0 ? " checked" : ""}> ${escapeHtml(name)}</label></li>`;
        })
        .join("")
    : '<li class="options-placeholder">No fields available yet.</li>';

  dom.pluginToggleList.innerHTML = pluginOutputNames.length
    ? pluginOutputNames
        .map((name) => `<li><label><input type="checkbox"> ${escapeHtml(name)}</label></li>`)
        .join("")
    : '<li class="options-placeholder">No plugin outputs available yet.</li>';

  dom.fieldToggleList.closest(".option-group")?.classList.toggle("empty", allFieldNames.length === 0);
  dom.pluginToggleList.closest(".option-group")?.classList.toggle("empty", pluginOutputNames.length === 0);
  requestAnimationFrame(updateAllToggleScrollbars);
}

export function setChecksStatus(state, label) {
  dom.checksStatus.textContent = label;
  dom.checksStatus.classList.remove("pending", "ok", "err");
  dom.setupTabDot.classList.remove("pending", "ok", "err");

  if (state === "running") {
    dom.checksStatus.classList.add("pending");
    dom.setupTabDot.classList.add("pending");
    if (dom.output) {
      dom.output.textContent = "No run yet.";
      renderStepMapOverlay(null);
    }
  }
  if (state === "passed") {
    dom.checksStatus.classList.add("ok");
    dom.setupTabDot.classList.add("ok");
    fetchAndRenderParams();
  }
  if (state === "failed") {
    dom.checksStatus.classList.add("err");
    dom.setupTabDot.classList.add("err");
    clearToggleNames();
    resetStepProgress();
    if (dom.output) {
      dom.output.textContent = "No run yet.";
      renderStepMapOverlay(null);
    }
  }
  if (state === "idle" || state === "running") {
    clearToggleNames();
    resetStepProgress();
    if (dom.output) {
      dom.output.textContent = "No run yet.";
      renderStepMapOverlay(null);
    }
  }

  updateRunAvailability(state, {
    runMode: dom.modeGrib.checked ? "grib" : "config",
    gribFileCount: Number(latestGribMetadata?.grib_file_count ?? NaN),
    emulatorConfigText: dom.emulatorEditor?.value || "",
  });
}

export function applySetupStateToDom(state) {
  dom.dryRunToggle.checked = !!state.options?.dry_run;

  const runMode = state.options?.run_mode === "grib" ? "grib" : "config";
  dom.modeGrib.checked = runMode === "grib";
  dom.modeConfig.checked = runMode === "config";

  dom.mpiInput.value = String(state.options?.mpi_np ?? 3);

  if (typeof state.plume_config?.path_display === "string" && state.plume_config.path_display) {
    dom.plumeUploadPath.value = state.plume_config.path_display;
  }
  if (typeof state.emulator_config?.path_display === "string" && state.emulator_config.path_display) {
    dom.emulatorUploadPath.value = state.emulator_config.path_display;
  }
  if (state.grib_source?.source_type === "folder") {
    if (state.grib_source.path_display) {
      dom.gribUploadPath.value = state.grib_source.path_display;
    } else if (typeof state.grib_source?.selected_paths?.length === "number" && state.grib_source.selected_paths.length > 0) {
      dom.gribUploadPath.value = "selected-folder/";
    }
  } else if (typeof state.grib_source?.selected_paths?.length === "number" && state.grib_source.selected_paths.length > 0) {
    dom.gribUploadPath.value =
      state.grib_source.selected_paths.length > 1
        ? `${state.grib_source.selected_paths.length} grib files selected`
        : state.grib_source.selected_paths[0];
  }

  if (typeof state.plume_config?.text === "string") {
    dom.plumeEditor.value = state.plume_config.text;
    if (plumeUploadedReference === null && state.plume_config.path_display && state.plume_config.path_display !== "...............") {
      plumeUploadedReference = state.plume_config.text;
    }
  }
  if (typeof state.emulator_config?.text === "string") {
    dom.emulatorEditor.value = state.emulator_config.text;
    if (emulatorUploadedReference === null && state.emulator_config.path_display && state.emulator_config.path_display !== "...............") {
      emulatorUploadedReference = state.emulator_config.text;
    }
  }

  latestChecksResults = state.checks?.results || {};
  latestChecksMessages = state.checks?.result_messages || {};
  latestGribPathDisplay = state.grib_source?.path_display || "";
  latestGribSelectedPaths = state.grib_source?.selected_paths || [];
  latestGribMetadata = state.grib_source?.metadata || {};
  const checksState = checksStateFromBackend(state.checks?.status);
  setChecksStatus(checksState, checksLabelFromState(checksState));
  const emulatorPaneDisabled = runMode === "grib";
  const gribPaneDisabled = runMode !== "grib";
  renderChecksList(
    latestChecksResults,
    latestChecksMessages,
    dom.dryRunToggle.checked,
    emulatorPaneDisabled,
    gribPaneDisabled,
  );
  updatePlumeEditorHighlights();
  updateEmulatorEditorHighlights();
  updateGribInfoBox();
  updateSetupAvailability();
  requestAnimationFrame(() => requestAnimationFrame(updateAllToggleScrollbars));
}

export async function fetchSetupState() {
  const state = await fetchJson("/api/setup/state", undefined);
  applySetupStateToDom(state);
}

export async function runSetupChecks() {
  const state = await fetchJson("/api/setup/checks/run", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({}),
  });
  applySetupStateToDom(state);
  return state;
}

async function postOptions() {
  const payload = {
    dry_run: dom.dryRunToggle.checked,
    run_mode: dom.modeGrib.checked ? "grib" : "config",
    mpi_np: Number(dom.mpiInput.value || 1),
  };

  const state = await fetchJson("/api/setup/options", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
  applySetupStateToDom(state);
}

export async function savePlumeConfig(text, pathDisplay) {
  const state = await fetchJson("/api/setup/source/plume-config", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ text, path_display: pathDisplay || dom.plumeUploadPath.value }),
  });
  applySetupStateToDom(state);
}

export async function saveEmulatorConfig(text, pathDisplay) {
  const state = await fetchJson("/api/setup/source/emulator-config", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ text, path_display: pathDisplay || dom.emulatorUploadPath.value }),
  });
  applySetupStateToDom(state);
}

export async function saveGribSource(sourceType, selectedPaths, pathDisplay) {
  const state = await fetchJson("/api/setup/source/grib", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      source_type: sourceType,
      selected_paths: selectedPaths,
      path_display: pathDisplay || latestGribPathDisplay || dom.gribUploadPath.value,
    }),
  });
  applySetupStateToDom(state);
}

export function initSetupControls() {
  const handleOptionsChange = async () => {
    ++_paramsFetchGen;
    clearToggleNames();
    resetStepProgress();
    try {
      await postOptions();
    } catch (error) {
      dom.output.textContent = String(error);
    }
  };

  // Validate mpi_np input to prevent values above 256 and minus sign
  dom.mpiInput.addEventListener("input", () => {
    let value = dom.mpiInput.value;
    // Remove minus signs
    value = value.replace(/-/g, "");
    // Clamp to max 256
    const numValue = Number(value);
    if (numValue > 256) {
      dom.mpiInput.value = "256";
    } else {
      dom.mpiInput.value = value;
    }
  });

  dom.dryRunToggle.addEventListener("change", handleOptionsChange);
  dom.runModeRadios.forEach((radio) => {
    radio.addEventListener("change", handleOptionsChange);
  });
  dom.mpiInput.addEventListener("change", handleOptionsChange);
  dom.plumeEditor.addEventListener("input", updatePlumeEditorHighlights);
  dom.emulatorEditor.addEventListener("input", updateEmulatorEditorHighlights);
  dom.fieldToggleList?.addEventListener("scroll", () => updateToggleScrollbar(dom.fieldToggleList));
  dom.pluginToggleList?.addEventListener("scroll", () => updateToggleScrollbar(dom.pluginToggleList));
  window.addEventListener("resize", updateAllToggleScrollbars);

  dom.gribUploadPath.addEventListener("change", async () => {
    try {
      await saveGribSource("folder", latestGribSelectedPaths, dom.gribUploadPath.value);
    } catch (error) {
      dom.output.textContent = String(error);
    }
  });

  dom.runChecksBtn.addEventListener("click", async () => {
    dom.runChecksBtn.disabled = true;
    setChecksStatus("running", "Running");

    try {
      await runSetupChecks();
    } catch (error) {
      setChecksStatus("failed", "Failed");
      dom.output.textContent = String(error);
    } finally {
      dom.runChecksBtn.disabled = false;
    }
  });
}
