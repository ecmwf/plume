import { dom } from "../dom.js";
import { uiState } from "../state.js";

const MISSING_VALUE = 9999;

const COOLWARM = [
  [0.0, "#3b4cc0"],
  [0.25, "#7b9ff9"],
  [0.5, "#dddddd"],
  [0.75, "#f4987a"],
  [1.0, "#b40426"],
];

let highlightedRegionTrace = null;

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
    if (!Number.isFinite(n) || n === MISSING_VALUE) {
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

function selectedPluginNames() {
  if (!dom.pluginToggleList) {
    return [];
  }
  return [...dom.pluginToggleList.querySelectorAll('input[type="checkbox"]:checked')]
    .map((cb) => String(cb.value || "").trim())
    .filter(Boolean);
}

function pluginPayloadForStep(pluginName, stepNumber = uiState.stepCurrent) {
  const name = String(pluginName || "").trim();
  const step = Math.max(0, Math.floor(Number(stepNumber) || 0));
  if (!name || step <= 0) {
    return null;
  }
  const stepKey = `${name}::${step}`;
  return Object.prototype.hasOwnProperty.call(uiState.pluginLayers || {}, stepKey)
    ? uiState.pluginLayers[stepKey]
    : null;
}

function normalizeCoords(coordList) {
  if (!Array.isArray(coordList)) {
    return { lon: [], lat: [] };
  }
  const lon = [];
  const lat = [];
  for (const pair of coordList) {
    if (!Array.isArray(pair) || pair.length < 2) {
      continue;
    }
    const x = shiftLongitude(pair[0]);
    const y = Number(pair[1]);
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      continue;
    }
    lon.push(x);
    lat.push(y);
  }
  return { lon, lat };
}

function wrapLon180(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) {
    return Number.NaN;
  }
  let wrapped = ((n + 180) % 360 + 360) % 360 - 180;
  // Keep +180 instead of -180 for cleaner seam rendering.
  if (wrapped === -180) {
    wrapped = 180;
  }
  return wrapped;
}

function shortestWrappedDelta(x1, x2) {
  let delta = Number(x2) - Number(x1);
  if (!Number.isFinite(delta)) {
    return Number.NaN;
  }
  if (delta > 180) {
    delta -= 360;
  } else if (delta < -180) {
    delta += 360;
  }
  return delta;
}

function densifyPolyline(lon, lat, maxStepDeg = 2) {
  if (!Array.isArray(lon) || !Array.isArray(lat) || lon.length !== lat.length || lon.length < 2) {
    return { lon, lat };
  }

  const outLon = [];
  const outLat = [];
  for (let i = 0; i < lon.length - 1; i += 1) {
    const x1 = Number(lon[i]);
    const y1 = Number(lat[i]);
    const x2 = Number(lon[i + 1]);
    const y2 = Number(lat[i + 1]);
    if (!Number.isFinite(x1) || !Number.isFinite(y1) || !Number.isFinite(x2) || !Number.isFinite(y2)) {
      continue;
    }

    const deltaLon = shortestWrappedDelta(x1, x2);
    if (!Number.isFinite(deltaLon)) {
      continue;
    }

    outLon.push(wrapLon180(x1));
    outLat.push(y1);

    const span = Math.max(Math.abs(deltaLon), Math.abs(y2 - y1));
    const segments = Math.max(1, Math.ceil(span / Math.max(0.25, maxStepDeg)));
    for (let s = 1; s < segments; s += 1) {
      const t = s / segments;
      outLon.push(wrapLon180(x1 + (deltaLon * t)));
      outLat.push(y1 + ((y2 - y1) * t));
    }
  }

  outLon.push(wrapLon180(Number(lon[lon.length - 1])));
  outLat.push(Number(lat[lat.length - 1]));
  return { lon: outLon, lat: outLat };
}

function normalizeLayerList(payload) {
  if (!payload || typeof payload !== "object") {
    return [];
  }
  if (Array.isArray(payload.layers)) {
    return payload.layers.filter((layer) => layer && typeof layer === "object");
  }
  return [payload];
}

function polygonBoundaryTraces(layer, pluginName) {
  const traces = [];
  if (!Array.isArray(layer?.regions)) {
    return traces;
  }

  for (const region of layer.regions) {
    const { lon, lat } = normalizeCoords(region?.coordinates);
    if (lon.length < 3 || lat.length < 3) {
      continue;
    }
    const strokeColor = String(region?.style?.stroke_color || "#d62728");
    const strokeWidth = Number(region?.style?.stroke_width);
    const baseLineWidth = Number.isFinite(strokeWidth) ? strokeWidth : 2;
    const densified = densifyPolyline(lon, lat, 1.5);
    const hoverText = `${pluginName}: ${String(region?.name || "region")}`;
    const description = String(region?.description || region?.name || "region").trim();

    // Use outline-only rendering for polygon_boundary to avoid geo fill
    // artifacts that can draw a full-globe outline on some projections.
    traces.push({
      type: "scattergeo",
      mode: "lines+markers",
      lon: densified.lon,
      lat: densified.lat,
      line: {
        color: strokeColor,
        width: baseLineWidth,
      },
      marker: {
        size: 4,
        opacity: 0,
      },
      meta: {
        regionHoverHighlight: true,
        baseLineWidth,
        baseLineColor: strokeColor,
        hoverLineWidth: Math.max(baseLineWidth + 3, baseLineWidth * 2),
        hoverLineColor: "#111827",
        baseMarkerOpacity: 0,
        legendLabel: description,
        legendColor: strokeColor,
      },
      text: densified.lon.map(() => hoverText),
      hovertemplate: "%{text}<extra></extra>",
      showlegend: false,
    });
  }
  return traces;
}

function clearHighlightedRegionTrace() {
  if (!window.Plotly || !dom.mapDiv || !highlightedRegionTrace) {
    highlightedRegionTrace = null;
    return;
  }

  const { curveNumber, baseLineWidth, baseLineColor, baseMarkerOpacity } = highlightedRegionTrace;
  highlightedRegionTrace = null;
  if (!dom.mapDiv.data?.[curveNumber]) {
    return;
  }

  Plotly.restyle(dom.mapDiv, {
    "line.width": [baseLineWidth],
    "line.color": [baseLineColor],
    "marker.opacity": [baseMarkerOpacity],
  }, [curveNumber]);
}

function highlightRegionTrace(curveNumber) {
  if (!window.Plotly || !dom.mapDiv) {
    return;
  }

  const trace = dom.mapDiv.data?.[curveNumber];
  const meta = trace?.meta;
  if (!meta?.regionHoverHighlight) {
    clearHighlightedRegionTrace();
    return;
  }

  if (highlightedRegionTrace?.curveNumber === curveNumber) {
    return;
  }

  clearHighlightedRegionTrace();
  highlightedRegionTrace = {
    curveNumber,
    baseLineWidth: Number(meta.baseLineWidth) || 2,
    baseLineColor: String(meta.baseLineColor || trace?.line?.color || "#d62728"),
    baseMarkerOpacity: Number(meta.baseMarkerOpacity) || 0,
  };

  Plotly.restyle(dom.mapDiv, {
    "line.width": [Number(meta.hoverLineWidth) || 4],
    "line.color": [String(meta.hoverLineColor || "#111827")],
    "marker.opacity": [0.08],
  }, [curveNumber]);
}

function bindMapHoverEffects() {
  if (!window.Plotly || !dom.mapDiv || dom.mapDiv.__regionHoverEffectsBound) {
    return;
  }

  dom.mapDiv.on("plotly_hover", (eventData) => {
    const curveNumber = eventData?.points?.[0]?.curveNumber;
    if (!Number.isInteger(curveNumber)) {
      clearHighlightedRegionTrace();
      return;
    }
    highlightRegionTrace(curveNumber);
  });

  dom.mapDiv.on("plotly_unhover", () => {
    clearHighlightedRegionTrace();
  });

  dom.mapDiv.__regionHoverEffectsBound = true;
}

function pointCollectionTraces(layer, pluginName) {
  const points = Array.isArray(layer?.points) ? layer.points : [];
  if (points.length === 0) {
    return [];
  }

  const lon = [];
  const lat = [];
  const text = [];
  const markerColors = [];
  const markerSizes = [];

  for (const point of points) {
    const x = shiftLongitude(point?.lon);
    const y = Number(point?.lat);
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      continue;
    }
    lon.push(x);
    lat.push(y);

    const label = String(point?.label || point?.name || "point");
    const value = point?.value;
    const info = String(point?.info || point?.description || "");
    const valueLine = Number.isFinite(Number(value)) ? `<br>value=${Number(value).toFixed(3)}` : "";
    const infoLine = info ? `<br>${info}` : "";
    text.push(`${pluginName}: ${label}${valueLine}${infoLine}`);

    markerColors.push(String(point?.style?.color || layer?.style?.color || "#111827"));
    const size = Number(point?.style?.size ?? layer?.style?.size ?? 9);
    markerSizes.push(Number.isFinite(size) ? size : 9);
  }

  if (lon.length === 0) {
    return [];
  }

  return [{
    type: "scattergeo",
    mode: "markers",
    lon,
    lat,
    marker: {
      color: markerColors,
      size: markerSizes,
      opacity: 0.95,
      line: { width: 1, color: "#ffffff" },
      symbol: String(layer?.style?.symbol || "circle"),
    },
    text,
    hovertemplate: "%{text}<extra></extra>",
    showlegend: false,
  }];
}

function boundaryOutlineTrace(boundary, pluginName, label) {
  const { lon, lat } = normalizeCoords(boundary);
  if (lon.length < 3 || lat.length < 3) {
    return null;
  }
  return {
    type: "scattergeo",
    mode: "lines",
    lon,
    lat,
    fill: "none",
    line: {
      color: "#111827",
      width: 2,
      dash: "dot",
    },
    hovertemplate: `${pluginName}: ${label}<extra></extra>`,
    showlegend: false,
  };
}

function scalarFieldTraces(layer, pluginName, sharedScale) {
  const lonRaw = Array.isArray(layer?.lon) ? layer.lon : [];
  const latRaw = Array.isArray(layer?.lat) ? layer.lat : [];
  const valuesRaw = Array.isArray(layer?.values) ? layer.values : [];
  const total = Math.min(lonRaw.length, latRaw.length, valuesRaw.length);
  if (total === 0) {
    return [];
  }

  const lon = [];
  const lat = [];
  const values = [];
  for (let i = 0; i < total; i += 1) {
    const x = shiftLongitude(lonRaw[i]);
    const y = Number(latRaw[i]);
    const v = Number(valuesRaw[i]);
    if (!Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(v) || v === MISSING_VALUE) {
      continue;
    }
    lon.push(x);
    lat.push(y);
    values.push(v);
  }

  if (lon.length === 0) {
    return [];
  }

  const sampled = maybeSampleTriples(lon, lat, values);
  const { minValue, maxValue } = finiteMinMax(values);
  const dataMin = Number.isFinite(minValue) ? minValue : 0;
  const dataMax = Number.isFinite(maxValue) && maxValue !== dataMin ? maxValue : dataMin + 1;
  const cmin = Number.isFinite(Number(sharedScale?.cmin)) ? Number(sharedScale.cmin) : dataMin;
  const cmax = Number.isFinite(Number(sharedScale?.cmax)) ? Number(sharedScale.cmax) : dataMax;
  const name = String(layer?.name || layer?.field_name || "scalar_field");

  const traces = [{
    type: "scattergeo",
    mode: "markers",
    lon: sampled.lon,
    lat: sampled.lat,
    marker: {
      size: Number.isFinite(Number(layer?.style?.size)) ? Number(layer.style.size) : 4,
      color: sampled.values,
      colorscale: COOLWARM,
      cmin,
      cmax,
      opacity: Number.isFinite(Number(layer?.style?.opacity)) ? Number(layer.style.opacity) : 0.6,
      showscale: false,
      line: { width: 0 },
    },
    hovertemplate: `${pluginName}: ${name}<br>lon=%{lon:.2f}<br>lat=%{lat:.2f}<br>value=%{marker.color:.4f}<extra></extra>`,
    showlegend: false,
  }];

  // For local fields, boundary traces make the area limits clearly visible.
  if (Array.isArray(layer?.local_areas)) {
    for (const area of layer.local_areas) {
      const outline = boundaryOutlineTrace(area?.boundary, pluginName, String(area?.name || "local area"));
      if (outline) {
        traces.push(outline);
      }
    }
  }
  if (Array.isArray(layer?.area_boundary)) {
    const outline = boundaryOutlineTrace(layer.area_boundary, pluginName, `${name} boundary`);
    if (outline) {
      traces.push(outline);
    }
  }

  return {
    traces,
    hasData: true,
    minValue: dataMin,
    maxValue: dataMax,
  };
}

function pluginLayerTraces(options = {}) {
  const traces = [];
  const scalarStats = {
    hasData: false,
    minValue: Number.POSITIVE_INFINITY,
    maxValue: Number.NEGATIVE_INFINITY,
  };
  const activePlugins = selectedPluginNames();

  for (const pluginName of activePlugins) {
    const payload = pluginPayloadForStep(pluginName, uiState.stepCurrent);
    if (!payload || typeof payload !== "object") {
      continue;
    }

    for (const layer of normalizeLayerList(payload)) {
      const layerType = String(layer?.type || "").trim();
      if (layerType === "polygon_boundary" || layerType === "polygon_bounday") {
        traces.push(...polygonBoundaryTraces(layer, pluginName));
      } else if (layerType === "point_collection") {
        traces.push(...pointCollectionTraces(layer, pluginName));
      } else if (layerType === "scalar_field") {
        const scalar = scalarFieldTraces(layer, pluginName, options.scalarScale);
        traces.push(...scalar.traces);
        if (scalar.hasData) {
          scalarStats.hasData = true;
          scalarStats.minValue = Math.min(scalarStats.minValue, Number(scalar.minValue));
          scalarStats.maxValue = Math.max(scalarStats.maxValue, Number(scalar.maxValue));
        }
      }
    }
  }

  if (!scalarStats.hasData) {
    scalarStats.minValue = Number.NaN;
    scalarStats.maxValue = Number.NaN;
  }

  return { traces, scalarStats };
}

function clearOverlay() {
  if (!window.Plotly || !dom.mapDiv) {
    return;
  }

  const plugin = pluginLayerTraces();
  const pluginTraces = plugin.traces;
  const baseTrace = {
    ...(dom.mapDiv.data?.[0] || {}),
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
  };

  Plotly.react(dom.mapDiv, [baseTrace, ...pluginTraces], dom.mapDiv.layout, {
    displayModeBar: false,
    staticPlot: false,
    scrollZoom: false,
    doubleClick: false,
    responsive: true,
  });

  highlightedRegionTrace = null;
  bindMapHoverEffects();

  return plugin.scalarStats;
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

  highlightedRegionTrace = null;
  bindMapHoverEffects();

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
    const scalarStats = clearOverlay();
    if (scalarStats?.hasData) {
      updateScaleLegend("plugin scalar", scalarStats.minValue, scalarStats.maxValue);
    } else {
      updateScaleLegend("", Number.NaN, Number.NaN);
    }
    return;
  }

  const fields = stepSnapshot?.fields;
  const selected = fields && typeof fields === "object" ? fields[fieldKey] : null;
  if (!selected) {
    clearOverlay();
    updateScaleLegend(fieldKey, Number.NaN, Number.NaN);
    return;
  }

  const rawLon = Array.isArray(selected.lon) ? selected.lon : [];
  const rawLat = Array.isArray(selected.lat) ? selected.lat : [];
  const rawValues = Array.isArray(selected.values) ? selected.values : [];
  const lon = [];
  const lat = [];
  const values = [];
  for (let i = 0; i < Math.min(rawLon.length, rawLat.length, rawValues.length); i += 1) {
    const v = Number(rawValues[i]);
    if (v === MISSING_VALUE) continue;
    lon.push(shiftLongitude(rawLon[i]));
    lat.push(Number(rawLat[i]));
    values.push(v);
  }
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

  const plugin = pluginLayerTraces({ scalarScale: { cmin, cmax } });
  const pluginTraces = plugin.traces;

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
  }, ...pluginTraces];

  Plotly.react(dom.mapDiv, updatedData, dom.mapDiv.layout, {
    displayModeBar: false,
    staticPlot: false,
    scrollZoom: false,
    doubleClick: false,
    responsive: true,
  });

  highlightedRegionTrace = null;
  bindMapHoverEffects();

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
