import { dom } from "./dom.js";

export function setActiveTab(tabName) {
  const isRun = tabName === "run";
  dom.runTabBtn.classList.toggle("active", isRun);
  dom.setupTabBtn.classList.toggle("active", !isRun);
  dom.runTabBtn.setAttribute("aria-selected", isRun ? "true" : "false");
  dom.setupTabBtn.setAttribute("aria-selected", isRun ? "false" : "true");
  dom.runPanel.hidden = !isRun;
  dom.setupPanel.hidden = isRun;
}

export function initTabs() {
  dom.runTabBtn.addEventListener("click", () => setActiveTab("run"));
  dom.setupTabBtn.addEventListener("click", () => setActiveTab("setup"));
}
