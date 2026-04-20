import { dom } from "../dom.js";
import { uiState } from "../state.js";

const COOLWARM = [
  [0.0, "#3b4cc0"],
  [0.25, "#7b9ff9"],
  [0.5, "#dddddd"],
  [0.75, "#f4987a"],
  [1.0, "#b40426"],
];

function selectedFieldKey() {
  const checked = dom.fieldToggleList?.querySelector('input[name="field-toggle"]:checked');
  return String(checked?.value || "").trim();
}

function shiftLongitude(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) {
    return Number.NaN;
  }
  const runMode = String(uiState.runContext?.runMode || "config");
  if (runMode === "config") {
    return n - 180;
  }
  return n;
}

function finiteMinMax(values) {
  let minValue = Number.POSITIVE_INFINITY;
  let maxValue = Number.NEGATIVE_INFINITY;
  let found = false;
  for (let i = 0; i < values.length; i += 1) {
    const n = Number(values[i]);
    if (!Number.isFinite(n)) {
      continue;
    }
    found = true;
    if (n < minValue) {
      minValue = n;
    }
    if (n > maxValue) {
      maxValue = n;
    }
  }
  if (!found) {
    return { minValue: Number.NaN, maxValue: Number.NaN };
  }
  return { minValue, maxValue };
}

function maybeSampleTriples(lon, lat, values) {
  const maxPoints = Number(uiState.browserMaxPoints || 50000);
  if (!uiState.browserPointCapEnabled || !Number.isFinite(maxPoints) || maxPoints < 1) {
    return { lon, lat, values };
  }
  const total = Math.min(lon.length, lat.length, values.length);
  if (total <= maxPoints) {
    return { lon, lat, values };
  }

  let minLon = Number.POSITIVE_INFINITY;
  let maxLon = Number.NEGATIVE_INFINITY;
  let minLat = Number.POSITIVE_INFINITY;
  let maxLat = Number.NEGATIVE_INFINITY;
  for (let i = 0; i < total; i += 1) {
    const x = Number(lon[i]);
    const y = Number(lat[i]);
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      continue;
    }
    if (x < minLon) minLon = x;
    if (x > maxLon) maxLon = x;
    if (y < minLat) minLat = y;
    if (y > maxLat) maxLat = y;
  }

  if (!Number.isFinite(minLon) || !Number.isFinite(minLat)) {
    return { lon, lat, values };
  }

  const lonSpan = Math.max(1e-9, maxLon - minLon);
  const latSpan = Math.max(1e-9, maxLat - minLat);
  const aspect = lonSpan / latSpan;

  let nx = Math.max(1, Math.floor(Math.sqrt(maxPoints * aspect)));
  let ny = Math.max(1, Math.floor(maxPoints / nx));
  while ((nx * ny) > maxPoints && ny > 1) {
    ny -= 1;
  }

  const sampledLon = [];
  const sampledLat = [];
  const sampledValues = [];
  const occupied = new Set();

  for (let i = 0; i < total; i += 1) {
    const x = Number(lon[i]);
    const y = Number(lat[i]);
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      continue;
    }

    const ix = Math.min(nx - 1, Math.max(0, Math.floor(((x - minLon) / lonSpan) * nx)));
    const iy = Math.min(ny - 1, Math.max(0, Math.floor(((y - minLat) / latSpan) * ny)));
    const cellKey = `${ix}:${iy}`;
    if (occupied.has(cellKey)) {
      continue;
    }
    occupied.add(cellKey);
    sampledLon.push(x);
    sampledLat.push(y);
    sampledValues.push(Number(values[i]));
    if (sampledLon.length >= maxPoints) {
      break;
    }
  }

  if (sampledLon.length === 0) {
    return { lon, lat, values };
  }
  return { lon: sampledLon, lat: sampledLat, values: sampledValues };
}

function updateScaleLegend(fieldKey, minValue, maxValue) {
  if (dom.mapScaleColorbar) {
    dom.mapScaleColorbar.style.background = "linear-gradient(90deg, #3b4cc0 0%, #7b9ff9 25%, #dddddd 50%, #f4987a 75%, #b40426 100%)";
  }

  if (!dom.mapScaleNote) {
    return;
  }

  if (!fieldKey) {
    dom.mapScaleNote.textContent = "No field selected.";
    return;
  }

  if (!Number.isFinite(minValue) || !Number.isFinite(maxValue)) {
    dom.mapScaleNote.textContent = `${fieldKey} - no data available.`;
    return;
  }

  dom.mapScaleNote.textContent = `${fieldKey} - min ${minValue.toFixed(3)}, max ${maxValue.toFixed(3)}`;
}

function clearOverlay() {
  if (!window.Plotly || !dom.mapDiv) {
    return;
  }

  Plotly.restyle(dom.mapDiv, {
    lon: [[]],
    lat: [[]],
    marker: [{
      size: 4,
      opacity: 0.5,
      color: [],
      colorscale: COOLWARM,
      cmin: 0,
      cmax: 1,
      showscale: false,
      line: { width: 0 },
    }],
  }, [0]);
}

export function renderEqualEarthMap() {
  if (!window.Plotly || !dom.mapDiv) {
    if (dom.mapDiv) {
      dom.mapDiv.innerHTML = '<div class="map-label">Plotly could not be loaded.<br>Check network access for CDN.</div>';
    }
    return;
  }

  const mapData = [{
    type: "scattergeo",
    mode: "markers",
    lon: [],
    lat: [],
    marker: {
      size: 4,
      opacity: 0.5,
      color: [],
      colorscale: COOLWARM,
      cmin: 0,
      cmax: 1,
      showscale: false,
      line: { width: 0 },
    },
    hovertemplate: "lon=%{lon:.2f}<br>lat=%{lat:.2f}<br>value=%{marker.color:.4f}<extra></extra>",
    showlegend: false,
  }];

  const mapLayout = {
    margin: { l: 0, r: 0, t: 0, b: 0 },
    paper_bgcolor: "#fdfefe",
    plot_bgcolor: "#fdfefe",
    geo: {
      projection: {
        type: "equal earth",
        scale: uiState.mapProjectionScale,
      },
      center: uiState.mapCenter,
      showframe: true,
      framecolor: "#223f54",
      framewidth: 1,
      showland: false,
      showocean: false,
      showcountries: true,
      countrycolor: "#223f54",
      countrywidth: 1.0,
      showcoastlines: true,
      coastlinecolor: "#223f54",
      coastlinewidth: 1.2,
      bgcolor: "#fdfefe",
      lataxis: { range: [-90, 90] },
      lonaxis: { range: [-180, 180] },
    },
  };

  Plotly.newPlot(dom.mapDiv, mapData, mapLayout, {
    displayModeBar: false,
    staticPlot: false,
    scrollZoom: false,
    doubleClick: false,
    responsive: true,
  });

  dom.mapDiv.on("plotly_relayout", (eventData) => {
    if (typeof eventData["geo.center.lon"] === "number") {
      uiState.mapCenter.lon = eventData["geo.center.lon"];
    }
    if (typeof eventData["geo.center.lat"] === "number") {
      uiState.mapCenter.lat = eventData["geo.center.lat"];
    }
  });

  updateScaleLegend("", Number.NaN, Number.NaN);
}

export function renderStepMapOverlay(stepSnapshot) {
  if (!window.Plotly || !dom.mapDiv) {
    return;
  }

  const fieldKey = selectedFieldKey();
  uiState.selectedFieldKey = fieldKey;

  if (!fieldKey) {
    clearOverlay();
    updateScaleLegend("", Number.NaN, Number.NaN);
    return;
  }

  const fields = stepSnapshot?.fields;
  const selected = fields && typeof fields === "object" ? fields[fieldKey] : null;
  if (!selected) {
    clearOverlay();
    updateScaleLegend(fieldKey, Number.NaN, Number.NaN);
    return;
  }

  const lon = Array.isArray(selected.lon) ? selected.lon.map((l) => shiftLongitude(l)) : [];
  const lat = Array.isArray(selected.lat) ? selected.lat : [];
  const values = Array.isArray(selected.values) ? selected.values.map((value) => Number(value)) : [];
  const sampled = maybeSampleTriples(lon, lat, values);

  const currentStep = String(uiState.stepCurrent || 0);
  const globalFields = uiState.stepMapData[currentStep]?.fields;
  const globalSelected = globalFields && typeof globalFields === "object" ? globalFields[fieldKey] : null;
  const scaleValues = Array.isArray(globalSelected?.values)
    ? globalSelected.values
    : values;
  const { minValue, maxValue } = finiteMinMax(scaleValues);
  
  let cmin;
  let cmax;
  if (uiState.mapScaleLocked && uiState.mapScaleLockedField === fieldKey) {
    cmin = uiState.mapScaleLockedRange.cmin;
    cmax = uiState.mapScaleLockedRange.cmax;
  } else {
    cmin = Number.isFinite(minValue) ? minValue : 0;
    cmax = Number.isFinite(maxValue) && maxValue !== cmin ? maxValue : cmin + 1;
  }

  const updatedData = [{
    ...dom.mapDiv.data[0],
    lon: sampled.lon,
    lat: sampled.lat,
    marker: {
      size: 4,
      color: sampled.values,
      colorscale: COOLWARM,
      cmin,
      cmax,
      opacity: 0.5,
      showscale: false,
      line: { width: 0 },
    },
  }];

  Plotly.react(dom.mapDiv, updatedData, dom.mapDiv.layout, {
    displayModeBar: false,
    staticPlot: false,
    scrollZoom: false,
    doubleClick: false,
    responsive: true,
  });

  updateScaleLegend(fieldKey, minValue, maxValue);
}

export function ensureCoastlinesAboveMarkers() {
  if (window.Plotly && dom.mapDiv && dom.mapDiv.data && dom.mapDiv.layout) {
    Plotly.react(dom.mapDiv, dom.mapDiv.data, dom.mapDiv.layout, {
      displayModeBar: false,
      staticPlot: false,
      scrollZoom: false,
      doubleClick: false,
      responsive: true,
    });
  }
}

export function resizeMap() {
  if (window.Plotly && dom.mapDiv) {
    Plotly.Plots.resize(dom.mapDiv);
  }
}
