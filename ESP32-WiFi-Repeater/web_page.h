#ifndef WEB_PAGE_H
#define WEB_PAGE_H

#include <Arduino.h>  // for the PROGMEM macro (header may be included first)

// Static shell for the setup portal. The dynamic forms (WiFi, AP, MQTT,
// sensors, relays) are built in web_portal.cpp and inserted between PAGE_HEAD
// and PAGE_FOOT. PAGE_FOOT carries the small live-readings poller.

const char PAGE_HEAD[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Repeater</title>
  <style>
    body { font-family: system-ui, sans-serif; margin: 0; padding: 1rem;
           background: #f4f5f7; color: #1f2933; }
    .card { max-width: 480px; margin: 0 auto 1rem; background: #fff;
            border-radius: 10px; padding: 1rem 1.2rem;
            box-shadow: 0 1px 4px rgba(0,0,0,.12); }
    h1 { font-size: 1.3rem; margin: 0 0 .2rem; }
    h2 { font-size: 1.05rem; margin: 0 0 .6rem; }
    label { display: block; font-size: .82rem; margin: .55rem 0 .15rem;
            font-weight: 600; }
    select, input { width: 100%; box-sizing: border-box; padding: .5rem;
                    border: 1px solid #cbd2d9; border-radius: 6px;
                    font-size: 1rem; }
    .row { display: flex; gap: .5rem; }
    .row > div { flex: 1; }
    .inline { display: flex; align-items: center; gap: .4rem; margin: .5rem 0; }
    .inline input { width: auto; }
    .inline label { margin: 0; }
    button { margin-top: .9rem; width: 100%; padding: .6rem; border: 0;
             border-radius: 6px; background: #2563eb; color: #fff;
             font-size: 1rem; font-weight: 600; cursor: pointer; }
    button:hover { background: #1d4ed8; }
    button.danger { background: #dc2626; }
    button.danger:hover { background: #b91c1c; }
    fieldset { border: 1px solid #e2e8f0; border-radius: 8px; margin: .6rem 0;
               padding: .5rem .8rem; }
    legend { font-weight: 600; font-size: .9rem; padding: 0 .3rem; }
    .status { font-size: .9rem; padding: .55rem .8rem; border-radius: 6px;
              background: #eef2ff; margin-bottom: .4rem; }
    .warning { background: #fef2f2; border: 2px solid #dc2626; color: #991b1b;
               padding: .8rem 1rem; border-radius: 8px; font-size: .92rem;
               line-height: 1.4; margin: 0 auto 1rem; max-width: 480px; }
    .warning strong { color: #dc2626; }
    .muted { color: #6b7280; font-size: .8rem; margin-top: .3rem; }
    .reading { display: flex; justify-content: space-between;
               padding: .3rem 0; border-bottom: 1px solid #f1f5f9;
               font-size: .95rem; }
    .reading b { font-variant-numeric: tabular-nums; }
    .unit { font-size: .82rem; color: #6b7280; white-space: nowrap; }
    /* Small secondary buttons (e.g. analog calibration). */
    .cal-btns { display: flex; gap: .4rem; margin: .3rem 0 .2rem; }
    button.btn-sm { width: auto; margin: 0; flex: 1; padding: .4rem .5rem;
                    font-size: .82rem; font-weight: 600; background: #475569; }
    button.btn-sm:hover { background: #334155; }
    /* CSS-only info tooltip. Works on hover and on tap (tabindex focus). */
    .info { display: inline-flex; align-items: center; justify-content: center;
            width: 16px; height: 16px; margin-left: .35rem; border-radius: 50%;
            background: #2563eb; color: #fff; font-size: 11px; font-weight: 700;
            font-style: normal; cursor: help; position: relative;
            vertical-align: middle; }
    .info::after { content: attr(data-tip); position: absolute; left: 50%;
                   bottom: 140%; transform: translateX(-50%);
                   width: max-content; max-width: 230px; white-space: normal;
                   background: #1f2933; color: #fff; text-align: left;
                   font-size: .76rem; font-weight: 400; line-height: 1.35;
                   padding: .45rem .6rem; border-radius: 6px;
                   box-shadow: 0 2px 8px rgba(0,0,0,.25);
                   opacity: 0; pointer-events: none; transition: opacity .12s;
                   z-index: 20; }
    .info:hover::after, .info:focus::after { opacity: 1; }
  </style>
</head>
<body>
)HTML";

const char PAGE_FOOT[] PROGMEM = R"HTML(
<script>
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
  toggleAnalog(1);
  toggleAnalog(2);
});
</script>
</body>
</html>
)HTML";

#endif  // WEB_PAGE_H
