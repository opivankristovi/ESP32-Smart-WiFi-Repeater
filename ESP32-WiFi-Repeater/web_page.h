#ifndef WEB_PAGE_H
#define WEB_PAGE_H

#include <Arduino.h>  // for the PROGMEM macro (header may be included first)

// Static shell for the setup portal. The app bar, tab navigation and the
// dynamic forms (WiFi, AP, MQTT, sensors, relays, time) are built in
// web_portal.cpp and inserted between PAGE_HEAD and PAGE_FOOT. PAGE_FOOT
// carries the tab switcher, the live-readings poller and small form helpers.

const char PAGE_HEAD[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#0b1220">
  <title>ESP32 Smart Repeater</title>
  <link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64' fill='none' stroke='%2360a5fa' stroke-width='4' stroke-linejoin='round'><circle cx='32' cy='32' r='24'/><polygon points='32,8 52.8,44 11.2,44'/><polygon points='52.8,20 32,56 11.2,20'/></svg>">
  <style>
    :root {
      --bg: #0b1220; --card: #151e2e; --card2: #1c2740; --border: #27344d;
      --text: #e2e8f0; --muted: #8b9bb4; --accent: #3b82f6; --accent2: #60a5fa;
      --danger: #ef4444; --ok: #34d399;
    }
    * { box-sizing: border-box; }
    html { color-scheme: dark; }
    body { font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
           margin: 0; padding-bottom: 2.5rem; background: var(--bg);
           color: var(--text); -webkit-font-smoothing: antialiased; }
    /* Sticky app bar + tab nav */
    .hdr { position: sticky; top: 0; z-index: 30;
           background: rgba(11,18,32,.94); backdrop-filter: blur(10px);
           border-bottom: 1px solid var(--border); }
    .appbar { display: flex; align-items: center; gap: .9rem;
              padding: .8rem 1rem .55rem; max-width: 560px; margin: 0 auto; }
    .brand { display: flex; align-items: center; gap: .5rem; flex: 1;
             min-width: 0; }
    .brand svg { width: 24px; height: 24px; flex: 0 0 auto;
                 color: var(--accent2); }
    .appbar h1 { font-size: 1.02rem; margin: 0; font-weight: 700;
                 letter-spacing: .2px; white-space: nowrap; }
    .dotlbl { display: flex; align-items: center; gap: .35rem;
              font-size: .72rem; color: var(--muted); font-weight: 600; }
    .dot { width: 9px; height: 9px; border-radius: 50%;
           background: var(--danger); box-shadow: 0 0 6px rgba(239,68,68,.6); }
    .dot.ok { background: var(--ok); box-shadow: 0 0 6px rgba(52,211,153,.6); }
    .nav { display: flex; gap: .35rem; overflow-x: auto; padding: 0 1rem .6rem;
           max-width: 560px; margin: 0 auto; scrollbar-width: none; }
    .nav::-webkit-scrollbar { display: none; }
    .nav button { flex: 0 0 auto; width: auto; margin: 0; padding: .42rem .85rem;
                  border: 1px solid transparent; border-radius: 999px;
                  background: transparent; color: var(--muted);
                  font-size: .84rem; font-weight: 600; cursor: pointer;
                  transition: background .15s, color .15s; }
    .nav button:hover { color: var(--text); background: var(--card2); }
    .nav button.active { background: var(--accent); color: #fff; }
    /* Tab sections and cards */
    section.tab { display: none; max-width: 560px; margin: 1rem auto 0;
                  padding: 0 1rem; }
    section.tab.active { display: block; animation: fadein .18s ease; }
    @keyframes fadein { from { opacity: 0; transform: translateY(4px); }
                        to { opacity: 1; transform: none; } }
    .card { background: var(--card); border: 1px solid var(--border);
            border-radius: 14px; padding: 1.05rem 1.2rem; margin: 0 0 1rem;
            box-shadow: 0 4px 14px rgba(0,0,0,.25); }
    body > .card { max-width: 520px; margin: 1.5rem auto;
                   width: calc(100% - 2rem); }
    h1 { font-size: 1.3rem; margin: 0 0 .2rem; }
    h2 { font-size: 1rem; margin: 0 0 .7rem; font-weight: 700;
         letter-spacing: .2px; }
    label { display: block; font-size: .8rem; margin: .6rem 0 .2rem;
            font-weight: 600; color: var(--muted); }
    select, input { width: 100%; padding: .55rem .65rem; font-size: 1rem;
                    color: var(--text); background: var(--card2);
                    border: 1px solid var(--border); border-radius: 8px;
                    transition: border-color .15s; }
    select:focus, input:focus { outline: none; border-color: var(--accent);
                                box-shadow: 0 0 0 2px rgba(59,130,246,.25); }
    .row { display: flex; gap: .6rem; }
    .row > div { flex: 1; }
    .inline { display: flex; align-items: center; gap: .55rem; margin: .6rem 0; }
    .inline input { width: auto; }
    .inline input[type="number"] { flex: 1; }
    .inline label { margin: 0; color: var(--text); font-weight: 500;
                    font-size: .88rem; }
    /* Checkboxes as toggle switches (day chips opt out via .chip) */
    .inline input[type="checkbox"] { appearance: none; -webkit-appearance: none;
        flex: 0 0 40px; width: 40px; height: 22px; margin: 0; padding: 0;
        border-radius: 999px; background: #2a3650;
        border: 1px solid var(--border); position: relative; cursor: pointer;
        transition: background .15s, border-color .15s; }
    .inline input[type="checkbox"]::before { content: ""; position: absolute;
        top: 2px; left: 2px; width: 16px; height: 16px; border-radius: 50%;
        background: #94a3b8; transition: transform .15s, background .15s; }
    .inline input[type="checkbox"]:checked { background: var(--accent);
        border-color: var(--accent); }
    .inline input[type="checkbox"]:checked::before {
        transform: translateX(18px); background: #fff; }
    button { margin-top: 1rem; width: 100%; padding: .65rem; border: 0;
             border-radius: 9px; background: var(--accent); color: #fff;
             font-size: 1rem; font-weight: 600; cursor: pointer;
             transition: background .15s, transform .05s; }
    button:hover { background: #2563eb; }
    button:active { transform: scale(.99); }
    button.danger { background: var(--danger); }
    button.danger:hover { background: #dc2626; }
    fieldset { border: 1px solid var(--border); border-radius: 10px;
               margin: .7rem 0; padding: .55rem .85rem .8rem;
               background: rgba(28,39,64,.35); }
    legend { font-weight: 700; font-size: .86rem; padding: 0 .35rem;
             color: var(--accent2); }
    .status { font-size: .9rem; padding: .6rem .8rem; border-radius: 9px;
              background: var(--card2); border: 1px solid var(--border);
              margin-bottom: .5rem; line-height: 1.5; }
    .pill { display: inline-block; padding: .08rem .55rem; border-radius: 999px;
            font-size: .78rem; font-weight: 700; vertical-align: 1px; }
    .pill.ok { background: rgba(52,211,153,.15); color: var(--ok); }
    .pill.bad { background: rgba(239,68,68,.15); color: #f87171; }
    .pill.idle { background: rgba(139,155,180,.15); color: var(--muted); }
    .warning { background: rgba(239,68,68,.1); border: 1px solid var(--danger);
               color: #fca5a5; padding: .8rem 1rem; border-radius: 12px;
               font-size: .9rem; line-height: 1.45; margin: 0 0 1rem; }
    .warning strong { color: #f87171; }
    .muted { color: var(--muted); font-size: .8rem; margin-top: .35rem; }
    .reading { display: flex; justify-content: space-between;
               padding: .42rem .1rem; border-bottom: 1px solid var(--border);
               font-size: .94rem; }
    .reading:last-child { border-bottom: 0; }
    .reading b { font-variant-numeric: tabular-nums; }
    .unit { font-size: .82rem; color: var(--muted); white-space: nowrap; }
    /* Mode-specific relay sections (shown/hidden by modeChanged()) */
    .msec { border-left: 2px solid var(--border); padding-left: .8rem;
            margin: .5rem 0; }
    /* Day-of-week chips (checkboxes styled as pills) */
    .days { display: flex; gap: .3rem; margin: .45rem 0 .15rem; }
    .chip { flex: 1; margin: 0; }
    .chip input { display: none; }
    .chip span { display: block; text-align: center; padding: .32rem 0;
                 border-radius: 7px; border: 1px solid var(--border);
                 background: var(--card2); color: var(--muted);
                 font-size: .74rem; font-weight: 700; cursor: pointer;
                 user-select: none; transition: background .12s, color .12s; }
    .chip input:checked + span { background: var(--accent);
                                 border-color: var(--accent); color: #fff; }
    /* Small secondary buttons (e.g. analog calibration). */
    .cal-btns { display: flex; gap: .4rem; margin: .3rem 0 .2rem; }
    button.btn-sm { width: auto; margin: 0; flex: 1; padding: .42rem .5rem;
                    font-size: .82rem; font-weight: 600; background: #334155; }
    button.btn-sm:hover { background: #475569; }
    /* CSS-only info tooltip. Works on hover and on tap (tabindex focus). */
    .info { display: inline-flex; align-items: center; justify-content: center;
            width: 16px; height: 16px; margin-left: .35rem; border-radius: 50%;
            background: var(--card2); border: 1px solid var(--accent);
            color: var(--accent2); font-size: 10px; font-weight: 700;
            font-style: normal; cursor: help; position: relative;
            vertical-align: middle; }
    .info::after { content: attr(data-tip); position: absolute; left: 50%;
                   bottom: 140%; transform: translateX(-50%);
                   width: max-content; max-width: 230px; white-space: normal;
                   background: #0f1729; border: 1px solid var(--border);
                   color: var(--text); text-align: left;
                   font-size: .76rem; font-weight: 400; line-height: 1.35;
                   padding: .45rem .6rem; border-radius: 8px;
                   box-shadow: 0 4px 14px rgba(0,0,0,.5);
                   opacity: 0; pointer-events: none; transition: opacity .12s;
                   z-index: 40; }
    .info:hover::after, .info:focus::after { opacity: 1; }
  </style>
</head>
<body>
)HTML";

const char PAGE_FOOT[] PROGMEM = R"HTML(
<script>
// Tab navigation. The active tab is kept in the URL hash so the page comes
// back to the same tab after a save/reboot.
function showTab(id) {
  let found = false;
  document.querySelectorAll('section.tab').forEach(s => {
    const on = (s.id === 'tab-' + id);
    s.classList.toggle('active', on);
    if (on) found = true;
  });
  if (!found) {
    const first = document.querySelector('section.tab');
    if (first) { first.classList.add('active'); id = first.id.slice(4); }
  }
  document.querySelectorAll('.nav button').forEach(b =>
    b.classList.toggle('active', b.dataset.tab === id));
  history.replaceState(null, '', '#' + id);
  window.scrollTo(0, 0);
}

async function refresh() {
  try {
    const r = await fetch('/readings');
    const d = await r.json();
    const box = document.getElementById('live');
    if (!box) return;
    let h = '';
    for (const k in d) {
      h += '<div class="reading"><span>' + k + '</span><b>' + d[k] + '</b></div>';
    }
    box.innerHTML = h || '<div class="muted">No sensors enabled.</div>';
  } catch (e) { /* offline; ignore */ }
}
setInterval(refresh, 5000);
refresh();

// Collapse a settings block when its enable checkbox is off.
function toggleBox(cbId, boxId) {
  const cb = document.getElementById(cbId), b = document.getElementById(boxId);
  if (cb && b) b.style.display = cb.checked ? '' : 'none';
}

// I2C chip select: BMP180 has a fixed address (hide the picker); only BME280
// has humidity (show its threshold).
function toggleI2c() {
  const sel = document.getElementById('bme_type');
  if (!sel) return;
  const addr = document.getElementById('bme_addr_row');
  const hum  = document.getElementById('bme_hum_row');
  if (addr) addr.style.display = (sel.value === '2') ? 'none' : '';   // 2=BMP180
  if (hum)  hum.style.display  = (sel.value === '0') ? '' : 'none';   // 0=BME280
}

// Probe bus select: single-wire (DHT) reveals the model picker + humidity
// threshold; 1-wire (DS18B20) hides them.
function toggleProbe() {
  const sel = document.getElementById('ds_bus');
  if (!sel) return;
  const dht = (sel.value === 'dht');
  const model = document.getElementById('ds_model_row');
  const hum   = document.getElementById('ds_hum_row');
  if (model) model.style.display = dht ? '' : 'none';
  if (hum)   hum.style.display   = dht ? '' : 'none';
}

// Input type select: show the digital (active-low) row or the touch
// (live value + threshold) row.
function toggleInput(n) {
  const sel = document.getElementById('in' + n + '_type');
  if (!sel) return;
  const touch = (sel.value === '1');
  const dig = document.getElementById('in' + n + '_dig');
  const tch = document.getElementById('in' + n + '_touch');
  if (dig) dig.style.display = touch ? 'none' : '';
  if (tch) tch.style.display = touch ? '' : 'none';
}

// Poll the live capacitive-touch value for any input currently in touch mode.
async function pollTouch() {
  for (let n = 1; n <= 2; n++) {
    const sel = document.getElementById('in' + n + '_type');
    const out = document.getElementById('in' + n + '_val');
    if (!sel || !out || sel.value !== '1') continue;
    try {
      const r = await fetch('/touch?ch=' + (n - 1));
      out.textContent = (await r.text()).trim();
    } catch (e) { /* offline; ignore */ }
  }
}
setInterval(pollTouch, 1200);

// Show only the option blocks that belong to the selected relay mode, and
// collapse the whole relay body when the mode is Off.
function modeChanged(n) {
  const sel = document.getElementById('r' + n + '_mode');
  if (!sel) return;
  document.querySelectorAll('.msec[data-r="' + n + '"]').forEach(d => {
    d.style.display = (d.dataset.m === sel.value) ? '' : 'none';
  });
  const body = document.getElementById('r' + n + '_body');
  if (body) body.style.display = (sel.value === '0') ? 'none' : '';  // 0=Off
}

// Show the raw min/max calibration block only when "Percent" scaling is chosen.
function toggleAnalog(n) {
  const sel = document.getElementById('a' + n + '_scale');
  const cal = document.getElementById('a' + n + '_cal');
  if (sel && cal) cal.style.display = (sel.value === '1') ? 'block' : 'none';
}

// Grab the live raw ADC reading into the min or max calibration field.
async function setCal(ch, field) {
  try {
    const r = await fetch('/raw?ch=' + ch);
    const v = (await r.text()).trim();
    const el = document.getElementById('a' + (ch + 1) + '_' + field);
    if (el) el.value = v;
  } catch (e) { /* ignore */ }
}

document.addEventListener('DOMContentLoaded', function () {
  if (document.querySelector('section.tab')) {
    showTab((location.hash || '#home').slice(1));
  }
  // Collapse each I/O block whose enable toggle is off (and keep it in sync).
  [['bme_en','bme_body'], ['ds_en','ds_body'], ['a1_en','a1_body'],
   ['a2_en','a2_body'], ['in1_en','in1_body'], ['in2_en','in2_body']]
    .forEach(([cb, box]) => {
      const el = document.getElementById(cb);
      if (el) el.addEventListener('change', () => toggleBox(cb, box));
      toggleBox(cb, box);
    });
  toggleI2c();
  toggleProbe();
  toggleInput(1);
  toggleInput(2);
  modeChanged(1);
  modeChanged(2);
  toggleAnalog(1);
  toggleAnalog(2);
});
</script>
</body>
</html>
)HTML";

#endif  // WEB_PAGE_H
