#pragma once

// Single-page phone-friendly control UI, served as one self-contained
// response (no external fonts/scripts/stylesheets - the phone may only
// have the pumpkin's own Wi-Fi at setup time, and there's no reason to
// need anything else afterward either). Talks to this bridge's own
// /api/status and /api/command endpoints, which forward to the RP2040
// over UART - see main.cpp and docs/esp32-network-bridge.md.
static const char INDEX_HTML[] = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>Jabberin' Jack</title>
<style>
  :root {
    --bg: #14100d;
    --panel: #221a15;
    --border: #3a2c22;
    --text: #f3e9df;
    --muted: #b9a89a;
    --accent: #ff8a1e;
    --accent-dim: #7a4c1a;
    --danger: #d9534f;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    padding: 16px 16px 48px;
    background: var(--bg);
    color: var(--text);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    font-size: 16px;
  }
  h1 {
    font-size: 1.4em;
    margin: 0 0 4px;
    color: var(--accent);
  }
  .sub { color: var(--muted); margin: 0 0 16px; font-size: 0.9em; }
  section {
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 14px 16px;
    margin-bottom: 14px;
  }
  section h2 {
    font-size: 0.85em;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: var(--muted);
    margin: 0 0 10px;
  }
  label { display: block; margin: 10px 0 4px; font-size: 0.95em; }
  input[type=number], input[type=text], select {
    width: 100%;
    padding: 10px;
    border-radius: 8px;
    border: 1px solid var(--border);
    background: #171310;
    color: var(--text);
    font-size: 1em;
  }
  input[type=range] { width: 100%; accent-color: var(--accent); }
  button {
    appearance: none;
    border: none;
    border-radius: 10px;
    padding: 12px 16px;
    font-size: 1em;
    font-weight: 600;
    color: #1a1310;
    background: var(--accent);
    cursor: pointer;
    touch-action: manipulation;
  }
  button.secondary {
    background: transparent;
    color: var(--text);
    border: 1px solid var(--border);
  }
  button.danger { background: var(--danger); color: #fff; }
  .row { display: flex; gap: 8px; flex-wrap: wrap; }
  .row > * { flex: 1 1 auto; }
  .switch-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
    padding: 6px 0;
  }
  .switch {
    position: relative;
    width: 46px;
    height: 26px;
    flex: none;
  }
  .switch input { opacity: 0; width: 0; height: 0; }
  .slider {
    position: absolute;
    inset: 0;
    background: var(--accent-dim);
    border-radius: 999px;
    transition: 0.15s;
  }
  .slider::before {
    content: "";
    position: absolute;
    width: 20px; height: 20px;
    left: 3px; top: 3px;
    background: #fff;
    border-radius: 50%;
    transition: 0.15s;
  }
  .switch input:checked + .slider { background: var(--accent); }
  .switch input:checked + .slider::before { transform: translateX(20px); }
  .face-list { max-height: 260px; overflow-y: auto; }
  .face-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 8px 0;
    border-bottom: 1px solid var(--border);
  }
  .face-row:last-child { border-bottom: none; }
  .face-row.current { color: var(--accent); font-weight: 600; }
  #statusBanner {
    font-size: 0.85em;
    color: var(--muted);
    margin-bottom: 10px;
    min-height: 1.2em;
  }
  #statusBanner.error { color: var(--danger); }
  .value-readout { color: var(--muted); font-size: 0.85em; float: right; }
</style>
</head>
<body>
  <h1>Jabberin' Jack</h1>
  <p class="sub">Pumpkin control panel</p>
  <div id="statusBanner">Loading&hellip;</div>

  <section>
    <h2>Now Showing</h2>
    <div class="switch-row">
      <div><strong id="currentFaceName">&mdash;</strong></div>
      <button id="nextFaceBtn" class="secondary">Next Face &rarr;</button>
    </div>
  </section>

  <section>
    <h2>Auto-Rotate</h2>
    <div class="switch-row">
      <label for="rotateEnabled" style="margin:0;">Enabled</label>
      <label class="switch">
        <input type="checkbox" id="rotateEnabled">
        <span class="slider"></span>
      </label>
    </div>
    <label for="rotateMs">Interval (ms)</label>
    <input type="number" id="rotateMs" min="0" step="500">
    <div class="row" style="margin-top:10px;">
      <button id="rotateApplyBtn">Apply</button>
    </div>

    <label for="orderSelect">Face order</label>
    <select id="orderSelect">
      <option value="in-order">In order</option>
      <option value="random">Random</option>
    </select>
  </section>

  <section>
    <h2>Brightness</h2>
    <label for="brightness">Candle brightness<span class="value-readout" id="brightnessReadout"></span></label>
    <input type="range" id="brightness" min="0" max="200" step="5">
  </section>

  <section>
    <h2>Faces</h2>
    <div class="face-list" id="faceList"></div>
  </section>

  <section>
    <h2>Configurable Text</h2>
    <label for="textLine1">Line 1</label>
    <input type="text" id="textLine1" maxlength="47">
    <label for="textLine2">Line 2</label>
    <input type="text" id="textLine2" maxlength="47">
    <label for="textLine3">Line 3</label>
    <input type="text" id="textLine3" maxlength="47">
    <label for="textLine4">Line 4</label>
    <input type="text" id="textLine4" maxlength="47">
    <div class="row" style="margin-top:10px;">
      <button id="textApplyBtn">Show Text</button>
    </div>

    <label for="fontSelect">Font</label>
    <select id="fontSelect"></select>
  </section>

  <section>
    <h2>Diagnostics</h2>
    <div class="switch-row">
      <label for="debugLogging" style="margin:0;">Eye-look motion debug logging</label>
      <label class="switch">
        <input type="checkbox" id="debugLogging">
        <span class="slider"></span>
      </label>
    </div>
  </section>

  <section>
    <h2>Bridge Link</h2>
    <p style="color:var(--muted);font-size:0.85em;margin:0 0 10px;">
      Wiring between this board and the RP2040 - see
      docs/esp32-network-bridge.md. Changes apply immediately (not just
      after Save) since a mismatch here can only be fixed by talking to
      whichever side is still reachable.
    </p>

    <div id="espLinkPinsBlock">
      <label style="margin-top:0;">This board's pins</label>
      <div class="row">
        <div>
          <label for="espRxPin" style="margin-top:0;">RX (from RP2040 TX)</label>
          <input type="number" id="espRxPin" min="0" max="39">
        </div>
        <div>
          <label for="espTxPin" style="margin-top:0;">TX (to RP2040 RX)</label>
          <input type="number" id="espTxPin" min="0" max="39">
        </div>
      </div>
      <div class="row" style="margin-top:10px;">
        <button id="espLinkPinsApplyBtn">Apply This Board's Pins</button>
      </div>
    </div>
    <p id="espLinkPinsUnsupported" style="display:none;color:var(--muted);font-size:0.85em;">
      This build talks to the RP2040 over USB host mode - no GPIO pins to configure.
    </p>

    <label>RP2040's pins</label>
    <div class="row">
      <div>
        <label for="jackTxPin" style="margin-top:0;">TX (to this board's RX)</label>
        <input type="number" id="jackTxPin" min="0" max="28">
      </div>
      <div>
        <label for="jackRxPin" style="margin-top:0;">RX (from this board's TX)</label>
        <input type="number" id="jackRxPin" min="0" max="28">
      </div>
    </div>
    <div class="row" style="margin-top:10px;">
      <button id="jackLinkPinsApplyBtn" class="secondary">Apply RP2040's Pins</button>
    </div>
  </section>

  <section>
    <h2>Saved Configuration</h2>
    <div class="row">
      <button id="saveBtn">Save to Flash</button>
      <button id="loadBtn" class="secondary">Load from Flash</button>
      <button id="resetBtn" class="danger">Reset Defaults</button>
    </div>
  </section>

<script>
"use strict";

var lastNonZeroRotateMs = 6000;
var pollTimer = null;

function isEditing(el) {
  return el === document.activeElement;
}

function setBanner(text, isError) {
  var el = document.getElementById("statusBanner");
  el.textContent = text;
  el.className = isError ? "error" : "";
}

function sendCommand(cmd) {
  return fetch("/api/command", {
    method: "POST",
    headers: { "Content-Type": "text/plain" },
    body: cmd
  }).then(function (res) {
    if (!res.ok) { throw new Error("Command failed: " + cmd); }
    return res.json();
  });
}

function refreshStatus() {
  return fetch("/api/status").then(function (res) {
    if (!res.ok) { throw new Error("status " + res.status); }
    return res.json();
  }).then(function (status) {
    setBanner("Connected", false);
    applyStatus(status);
  }).catch(function (err) {
    // Two very different failure modes land here: fetch() itself
    // rejecting (this bridge is genuinely unreachable) vs. a bad/partial
    // response or a bug in applyStatus() (it answered, but something after
    // that broke) - logged so either is actually diagnosable from the
    // browser console instead of just "can't reach", which isn't always
    // true and was itself once the symptom of a real bug elsewhere.
    console.error("refreshStatus failed:", err);
    setBanner("Problem talking to the pumpkin controller (see console)", true);
  });
}

function applyStatus(status) {
  var faceList = document.getElementById("faceList");
  faceList.innerHTML = "";
  status.faces.forEach(function (face) {
    var row = document.createElement("div");
    row.className = "face-row" + (face.i === status.currentFace ? " current" : "");

    var label = document.createElement("label");
    label.className = "switch-row";
    label.style.width = "100%";
    label.style.padding = "0";
    label.style.border = "none";

    var name = document.createElement("span");
    name.textContent = face.name;
    label.appendChild(name);

    var sw = document.createElement("label");
    sw.className = "switch";
    var cb = document.createElement("input");
    cb.type = "checkbox";
    cb.checked = face.enabled;
    cb.addEventListener("change", function () {
      sendCommand(String(face.i)).then(refreshStatus);
    });
    var slider = document.createElement("span");
    slider.className = "slider";
    sw.appendChild(cb);
    sw.appendChild(slider);
    label.appendChild(sw);

    row.appendChild(label);
    faceList.appendChild(row);
  });

  var currentFace = status.faces.find(function (f) { return f.i === status.currentFace; });
  document.getElementById("currentFaceName").textContent = currentFace ? currentFace.name : ("#" + status.currentFace);

  if (!isEditing(document.getElementById("rotateMs"))) {
    document.getElementById("rotateMs").value = status.rotateMs > 0 ? status.rotateMs : lastNonZeroRotateMs;
  }
  if (status.rotateMs > 0) {
    lastNonZeroRotateMs = status.rotateMs;
  }
  var rotateEnabled = document.getElementById("rotateEnabled");
  if (!isEditing(rotateEnabled)) {
    rotateEnabled.checked = status.rotateMs > 0;
  }

  var orderSelect = document.getElementById("orderSelect");
  if (!isEditing(orderSelect)) {
    orderSelect.value = status.order;
  }

  var brightness = document.getElementById("brightness");
  if (!isEditing(brightness)) {
    brightness.value = status.brightnessPercent;
  }
  document.getElementById("brightnessReadout").textContent = status.brightnessPercent + "%";

  var fontSelect = document.getElementById("fontSelect");
  if (fontSelect.options.length === 0) {
    status.fonts.forEach(function (name) {
      var opt = document.createElement("option");
      opt.value = name;
      opt.textContent = name;
      fontSelect.appendChild(opt);
    });
  }
  if (!isEditing(fontSelect)) {
    fontSelect.value = status.font;
  }

  var debugLogging = document.getElementById("debugLogging");
  if (!isEditing(debugLogging)) {
    debugLogging.checked = status.debugLogging;
  }

  var jackTxPin = document.getElementById("jackTxPin");
  var jackRxPin = document.getElementById("jackRxPin");
  if (!isEditing(jackTxPin)) { jackTxPin.value = status.esp32TxPin; }
  if (!isEditing(jackRxPin)) { jackRxPin.value = status.esp32RxPin; }
}

function refreshLinkPins() {
  return fetch("/api/link-pins").then(function (res) {
    return res.json();
  }).then(function (data) {
    var block = document.getElementById("espLinkPinsBlock");
    var unsupported = document.getElementById("espLinkPinsUnsupported");
    if (!data.supported) {
      block.style.display = "none";
      unsupported.style.display = "block";
      return;
    }
    block.style.display = "block";
    unsupported.style.display = "none";
    var rxPin = document.getElementById("espRxPin");
    var txPin = document.getElementById("espTxPin");
    if (!isEditing(rxPin)) { rxPin.value = data.rxPin; }
    if (!isEditing(txPin)) { txPin.value = data.txPin; }
  });
}

document.getElementById("nextFaceBtn").addEventListener("click", function () {
  sendCommand("").then(refreshStatus);
});

document.getElementById("rotateApplyBtn").addEventListener("click", function () {
  var enabled = document.getElementById("rotateEnabled").checked;
  var ms = enabled ? (parseInt(document.getElementById("rotateMs").value, 10) || lastNonZeroRotateMs) : 0;
  sendCommand("rotate " + ms).then(refreshStatus);
});

document.getElementById("rotateEnabled").addEventListener("change", function (e) {
  if (!e.target.checked) {
    sendCommand("rotate 0").then(refreshStatus);
  }
});

document.getElementById("orderSelect").addEventListener("change", function (e) {
  sendCommand("order " + e.target.value).then(refreshStatus);
});

document.getElementById("brightness").addEventListener("change", function (e) {
  sendCommand("brightness " + e.target.value).then(refreshStatus);
});

document.getElementById("textApplyBtn").addEventListener("click", function () {
  var lines = [1, 2, 3, 4].map(function (n) {
    return document.getElementById("textLine" + n).value.replace(/\|/g, "");
  });
  sendCommand("text " + lines.join("|")).then(refreshStatus);
});

document.getElementById("fontSelect").addEventListener("change", function (e) {
  sendCommand("font " + e.target.value).then(refreshStatus);
});

document.getElementById("debugLogging").addEventListener("change", function () {
  sendCommand("debug").then(refreshStatus);
});

// Polls fn (a function returning a fetch-backed promise) every intervalMs
// until it resolves, up to maxAttempts times - used to wait out this
// board's own reboot after changing its link pins (see
// espLinkPinsApplyBtn below), since the very next request or two will
// find nothing listening yet.
function pollUntilBack(fn, maxAttempts, intervalMs) {
  return fn().catch(function () {
    if (maxAttempts <= 1) { throw new Error("still unreachable"); }
    return new Promise(function (resolve) {
      setTimeout(resolve, intervalMs);
    }).then(function () {
      return pollUntilBack(fn, maxAttempts - 1, intervalMs);
    });
  });
}

document.getElementById("espLinkPinsApplyBtn").addEventListener("click", function () {
  var rxPin = parseInt(document.getElementById("espRxPin").value, 10);
  var txPin = parseInt(document.getElementById("espTxPin").value, 10);
  setBanner("Applying and restarting this board...", false);
  fetch("/api/link-pins", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: "rxPin=" + rxPin + "&txPin=" + txPin
  }).then(function (res) {
    return res.json();
  }).then(function (data) {
    if (!data.ok) {
      alert("Failed: " + (data.error || "unknown error"));
      setBanner("Connected", false);
      return;
    }
    return pollUntilBack(refreshLinkPins, 10, 2000).then(function () {
      setBanner("Connected", false);
    });
  }).catch(function () {
    setBanner("This board hasn't come back yet - reload the page shortly", true);
  });
});

document.getElementById("jackLinkPinsApplyBtn").addEventListener("click", function () {
  var txPin = parseInt(document.getElementById("jackTxPin").value, 10);
  var rxPin = parseInt(document.getElementById("jackRxPin").value, 10);
  if (!confirm("This changes which GPIOs the RP2040 uses for this link, " +
              "immediately. If it doesn't match the actual wiring, this " +
              "bridge will lose contact with the pumpkin until it's " +
              "fixed. Continue?")) {
    return;
  }
  sendCommand("esp32link " + txPin + " " + rxPin).then(refreshStatus);
});

document.getElementById("saveBtn").addEventListener("click", function () {
  sendCommand("save").then(refreshStatus);
});
document.getElementById("loadBtn").addEventListener("click", function () {
  sendCommand("load").then(refreshStatus);
});
document.getElementById("resetBtn").addEventListener("click", function () {
  if (confirm("Restore compiled-in defaults? (Flash is untouched until you Save.)")) {
    sendCommand("reset").then(refreshStatus);
  }
});

refreshStatus();
refreshLinkPins();
pollTimer = setInterval(refreshStatus, 4000);
</script>
</body>
</html>
)HTML";
