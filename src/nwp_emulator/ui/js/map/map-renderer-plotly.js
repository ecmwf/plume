import { dom } from "../dom.js";
import { uiState } from "../state.js";

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
    hoverinfo: "skip",
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
      countrycolor: "#516a7e",
      countrywidth: 0.6,
      showcoastlines: true,
      coastlinecolor: "#223f54",
      coastlinewidth: 0.9,
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

}

export function resizeMap() {
  if (window.Plotly && dom.mapDiv) {
    Plotly.Plots.resize(dom.mapDiv);
  }
}
