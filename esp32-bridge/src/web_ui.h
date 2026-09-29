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
  .page-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    flex-wrap: wrap;
    gap: 8px;
    margin-bottom: 16px;
  }
  #statusBanner {
    font-size: 0.85em;
    color: var(--muted);
    min-height: 1.2em;
  }
  #statusBanner.error { color: var(--danger); }
  .value-readout { color: var(--muted); font-size: 0.85em; float: right; }
  input:disabled, select:disabled, button:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }
</style>
</head>
<body>
  <div class="page-header">
    <h1 style="margin:0;">Jabberin' Jack</h1>
    <div id="statusBanner">Loading&hellip;</div>
  </div>

  <section>
    <h2>Now Showing</h2>
    <div class="switch-row">
      <div><strong id="currentFaceName">&mdash;</strong></div>
      <button id="nextFaceBtn" class="secondary">Next Face &rarr;</button>
    </div>
  </section>

  <section>
    <h2 class="switch-row" style="margin:0 0 10px;padding:0;">
      <span>Auto-Rotate</span>
      <label class="switch">
        <input type="checkbox" id="rotateEnabled">
        <span class="slider"></span>
      </label>
    </h2>
    <label for="rotateMs">Interval (ms)</label>
    <div class="row">
      <input type="number" id="rotateMs" min="0" step="500">
      <button id="rotateApplyBtn" style="flex:0 0 auto;">Apply</button>
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
    <h2>Countdown</h2>
    <p style="color:var(--muted);font-size:0.85em;margin:0 0 10px;">
      Shows "&lt;N&gt; days until &lt;holiday&gt;" on the pumpkin. The
      RP2040 has no clock of its own, so this board feeds it today's date
      over NTP once connected - the readout below shows whether that's
      happened yet.
    </p>
    <div id="countdownReadout" style="margin-bottom:10px;">&mdash;</div>
    <label for="countdownName">Holiday name</label>
    <input type="text" id="countdownName" maxlength="23">
    <label for="countdownName2">Holiday name, line 2 (optional)</label>
    <input type="text" id="countdownName2" maxlength="23">
    <div class="row">
      <div>
        <label for="countdownMonth">Month</label>
        <input type="number" id="countdownMonth" min="1" max="12">
      </div>
      <div>
        <label for="countdownDay">Day</label>
        <input type="number" id="countdownDay" min="1" max="31">
      </div>
    </div>
    <div class="row" style="margin-top:10px;">
      <button id="countdownApplyBtn">Set &amp; Show</button>
    </div>
  </section>

  <section>
    <h2>Audio</h2>
    <p style="color:var(--muted);font-size:0.85em;margin:0 0 10px;">
      Plays on the RP2040's own I2S speaker (see
      docs/audio-i2s-wiring.md).
    </p>
    <div class="row">
      <button id="muteBtn" class="secondary">Mute</button>
    </div>

    <label for="volume">Volume<span class="value-readout" id="volumeReadout"></span></label>
    <input type="range" id="volume" min="0" max="100" step="5">

    <div class="row" style="margin-top:10px;">
      <button id="audioToneBtn" class="secondary">Tone</button>
      <button id="audioVoiceBtn" class="secondary">Voice</button>
      <button id="audioPacmanBtn" class="secondary">Pac-Man</button>
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

    <p style="margin:10px 0 0;">RP2040's pins: <span id="jackPinsReadout">&mdash;</span></p>
    <p style="color:var(--muted);font-size:0.85em;margin:4px 0 0;">
      Read-only here - changing these only works if this link is already
      correctly configured, which defeats the point of fixing it remotely.
      Set them directly at the RP2040's own serial console instead
      ("esp32link" command).
    </p>
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

// The Interval field, order select, and Apply button only mean anything
// while auto-rotate is actually on - disabled rather than just left
// interactive-but-pointless while it's off.
function updateRotateControlsEnabled() {
  var enabled = document.getElementById("rotateEnabled").checked;
  document.getElementById("rotateMs").disabled = !enabled;
  document.getElementById("rotateApplyBtn").disabled = !enabled;
  document.getElementById("orderSelect").disabled = !enabled;
}

// Volume and the Tone/Voice/Pac-Man test buttons don't mean anything
// while muted - disabled rather than left interactive-but-pointless,
// same idea as updateRotateControlsEnabled() above. Reads muted state off
// the Mute button's own label (set from status.volumePercent in
// applyStatus(), or optimistically the instant it's clicked) rather than
// taking a separate parameter, so every caller stays in sync with
// whatever's actually currently displayed there.
function updateAudioControlsEnabled() {
  var muted = document.getElementById("muteBtn").textContent === "Unmute";
  document.getElementById("volume").disabled = muted;
  document.getElementById("audioToneBtn").disabled = muted;
  document.getElementById("audioVoiceBtn").disabled = muted;
  document.getElementById("audioPacmanBtn").disabled = muted;
}

function setBanner(text, isError) {
  var el = document.getElementById("statusBanner");
  el.textContent = text;
  el.className = isError ? "error" : "";
}

// Nothing on this page can do anything useful while the bridge itself is
// unreachable (every button/input here just calls sendCommand()/
// refreshStatus(), both of which would only fail) - disabling everything
// makes that obvious instead of leaving controls that silently do nothing.
// applyStatus() re-enables on every successful poll, layering its own
// narrower rotate-controls-only disabling (see updateRotateControlsEnabled)
// back on top afterward.
function setPageDisabled(disabled) {
  document.querySelectorAll("input, select, button").forEach(function (el) {
    el.disabled = disabled;
  });
}

// Serializes every request to this bridge through one queue - the ESP32
// side can only actually have one exchange with the RP2040 in flight at
// a time anyway (see its own sendCommandInFlight guard); an overlapping
// one just comes back empty/504 instead of running concurrently. Without
// this, the periodic poll (see pollTimer below) could fire in the middle
// of a user-initiated sendCommand().then(refreshStatus) - the loser of
// that race either errors out (briefly disabling the whole page, having
// done nothing wrong) or, worse, its response can land and get applied
// *after* the other one's, showing stale state right after the fresh one
// ("it refreshes, then refreshes again [to something older]").
var actionChain = Promise.resolve();
function enqueue(fn) {
  var result = actionChain.then(fn, fn);
  // What future enqueue() calls actually chain onto - deliberately
  // swallows this entry's own outcome so one failed request doesn't wedge
  // every later one behind a permanently-rejected chain. The caller of
  // *this* enqueue() still sees the real result/rejection via `result`.
  actionChain = result.then(function () {}, function () {});
  return result;
}

// Bumped every time the user actually changes something, so a status
// fetch can tell whether it's still the freshest thing asked for by the
// time it comes back - see refreshStatus() below. enqueue() alone only
// stops requests from overlapping *on the wire*; it doesn't stop an
// older one that was already queued (e.g. the periodic poll, queued the
// instant before a click) from being *applied* after a newer action,
// since FIFO queueing still runs it first. That's what actually caused
// "press Set & Show, see the old value again, then the new one a moment
// later" - not a flaw in polling as a concept, just this queue alone
// wasn't enough to also detect staleness.
var actionSeq = 0;

function sendCommand(cmd) {
  actionSeq++;
  return enqueue(function () {
    return fetch("/api/command", {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body: cmd
    }).then(function (res) {
      if (!res.ok) { throw new Error("Command failed: " + cmd); }
      return res.json();
    });
  });
}

function refreshStatus() {
  var seqAtStart = actionSeq;
  return enqueue(function () {
    return fetch("/api/status").then(function (res) {
      if (!res.ok) { throw new Error("status " + res.status); }
      return res.json();
    }).then(function (status) {
      setBanner("Connected", false);
      if (actionSeq !== seqAtStart) {
        // A command was sent after this particular fetch started (it was
        // still queued behind an earlier one, most likely) - this status
        // predates that command's own effect. Applying it now would
        // flash the pre-command state right as the command's effect is
        // about to land; skip it and let the fresher refresh already
        // coming (that command's own .then(refreshStatus), or the next
        // periodic tick) update the page instead.
        return;
      }
      applyStatus(status);
    }).catch(function (err) {
      // Two very different failure modes land here: fetch() itself
      // rejecting (this bridge is genuinely unreachable) vs. a bad/partial
      // response or a bug in applyStatus() (it answered, but something
      // after that broke) - logged so either is actually diagnosable from
      // the browser console instead of just "can't reach", which isn't
      // always true and was itself once the symptom of a real bug
      // elsewhere.
      console.error("refreshStatus failed:", err);
      setBanner("Problem talking to the pumpkin controller (see console)", true);
      setPageDisabled(true);
    });
  });
}

function applyStatus(status) {
  setPageDisabled(false);

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
  updateRotateControlsEnabled();

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

  var textLines = status.text || [];
  for (var n = 1; n <= 4; n++) {
    var lineInput = document.getElementById("textLine" + n);
    if (!isEditing(lineInput)) {
      lineInput.value = textLines[n - 1] || "";
    }
  }

  var volume = document.getElementById("volume");
  if (!isEditing(volume)) {
    volume.value = status.volumePercent;
  }
  document.getElementById("volumeReadout").textContent = status.volumePercent + "%";
  document.getElementById("muteBtn").textContent = status.volumePercent === 0 ? "Unmute" : "Mute";
  updateAudioControlsEnabled();

  document.getElementById("jackPinsReadout").textContent =
      "TX=" + status.esp32TxPin + " RX=" + status.esp32RxPin;

  var countdown = status.countdown || {};
  // holidayName may itself contain a '|' splitting it across 2 lines (see
  // the RP2040's "countdown" command) - split back apart for the two
  // separate input fields, and join with a space for the plain-text
  // readout below.
  var nameParts = (countdown.holidayName || "").split("|");
  var holidayDisplayName = nameParts.join(" ");
  var countdownName = document.getElementById("countdownName");
  var countdownName2 = document.getElementById("countdownName2");
  var countdownMonth = document.getElementById("countdownMonth");
  var countdownDay = document.getElementById("countdownDay");
  if (!isEditing(countdownName)) { countdownName.value = nameParts[0] || ""; }
  if (!isEditing(countdownName2)) { countdownName2.value = nameParts[1] || ""; }
  if (!isEditing(countdownMonth)) { countdownMonth.value = countdown.holidayMonth || ""; }
  if (!isEditing(countdownDay)) { countdownDay.value = countdown.holidayDay || ""; }
  var countdownReadout = document.getElementById("countdownReadout");
  if (!countdown.synced) {
    countdownReadout.textContent = "Not synced yet - waiting on this board's NTP sync";
  } else if (countdown.daysUntil === 0) {
    countdownReadout.textContent = "Today is " + holidayDisplayName + "!";
  } else if (countdown.daysUntil === 1) {
    countdownReadout.textContent = "Tomorrow is " + holidayDisplayName + "!";
  } else {
    countdownReadout.textContent = countdown.daysUntil + " days to " + holidayDisplayName;
  }
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
  updateRotateControlsEnabled();
  // Symmetric with unchecking: applies immediately either way, rather
  // than only turning rotation off immediately and leaving turning it
  // back on to require a separate Apply click - which did nothing
  // visible and then got silently undone by the next poll reading back
  // the still-zero rotateMs it never actually changed.
  var ms = e.target.checked
      ? (parseInt(document.getElementById("rotateMs").value, 10) || lastNonZeroRotateMs)
      : 0;
  sendCommand("rotate " + ms).then(refreshStatus);
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

document.getElementById("countdownApplyBtn").addEventListener("click", function () {
  var month = parseInt(document.getElementById("countdownMonth").value, 10);
  var day = parseInt(document.getElementById("countdownDay").value, 10);
  var name = document.getElementById("countdownName").value.trim();
  var name2 = document.getElementById("countdownName2").value.trim();
  if (!name || !month || !day) {
    alert("Holiday name, month, and day are all required.");
    return;
  }
  var fullName = name2 ? (name + "|" + name2) : name;
  sendCommand("countdown " + month + " " + day + " " + fullName).then(refreshStatus);
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

document.getElementById("audioToneBtn").addEventListener("click", function () {
  sendCommand("audio tone");
});
document.getElementById("audioVoiceBtn").addEventListener("click", function () {
  sendCommand("audio voice");
});
document.getElementById("audioPacmanBtn").addEventListener("click", function () {
  sendCommand("audio pacman");
});

document.getElementById("volume").addEventListener("change", function (e) {
  sendCommand("volume " + e.target.value).then(refreshStatus);
});

document.getElementById("muteBtn").addEventListener("click", function (e) {
  // Instant feedback, same pattern as rotateEnabled's change listener -
  // corrected by the refreshStatus() below regardless of what it guessed.
  e.target.textContent = e.target.textContent === "Unmute" ? "Mute" : "Unmute";
  updateAudioControlsEnabled();
  sendCommand("mute").then(refreshStatus);
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

setPageDisabled(true);
refreshStatus();
refreshLinkPins();
pollTimer = setInterval(refreshStatus, 4000);
</script>
</body>
</html>
)HTML";
