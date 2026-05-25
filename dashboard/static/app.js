const DEFAULT_CENTER = [41.9028, 12.4964];
const STATE_REFRESH_MS = 15000;
const DETAIL_REFRESH_MS = 10000;

let nodes = [];
let selectedId = null;
let map = null;
let markers = {};
let chart = null;
let firstBoundsFit = false;
let lastDetailFetch = 0;
let selectedMetric = 'temperature';
let currentTrendPoints = [];
let currentEvents = [];
let eventFilter = 'ALL';
let sampleLimit = 'all';

const METRICS = [
  { key: 'temperature', label: 'Temperature', unit: ' C', digits: 1, color: '#a86e00' },
  { key: 'humidity', label: 'Humidity', unit: ' %', digits: 1, color: '#58641d' },
  { key: 'pressure', label: 'Pressure', unit: ' hPa', digits: 1, color: '#4d7c2e' },
  { key: 'gas_resistance', label: 'Gas resistance', unit: '', digits: 0, color: '#273b09' },
  { key: 'battery_level', label: 'Battery', unit: ' %', digits: 1, color: '#7b904b' },
  { key: 'anomaly_score', label: 'Anomaly score', unit: '', digits: 2, color: '#b32636' },
  { key: 'risk_score', label: 'Risk score', unit: '', digits: 2, color: '#b32636' }
];

function isAlert(n) { return Number(n.anomaly_state) === 1 || Number(n.risk_state) === 1; }
function fmt(v, d = 1) { return Number.isFinite(Number(v)) ? Number(v).toFixed(d) : '-'; }
function score(v) { return fmt(v, 2); }
function timeLabel(s) {
  const n = Number(s);
  if (!Number.isFinite(n) || n <= 0) return '-';
  return n > 1704067200 ? new Date(n * 1000).toLocaleString() : `${n}s`;
}
function statusLabel(n) { return isAlert(n) ? 'ALERT' : 'NORMAL'; }
function badge(n) { return `<span class="badge ${isAlert(n) ? 'alert' : 'ok'}">${statusLabel(n)}</span>`; }
function metricDef(key) { return METRICS.find(m => m.key === key) || METRICS[0]; }
function setError(msg) {
  const el = document.getElementById('apiError');
  el.textContent = msg || '';
  el.style.display = msg ? 'block' : 'none';
}

function initMap() {
  if (!window.L) {
    document.getElementById('map').style.display = 'none';
    document.getElementById('mapFallback').style.display = 'flex';
    return;
  }
  map = L.map('map', { zoomControl: true }).setView(DEFAULT_CENTER, 12);
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 19,
    attribution: '&copy; OpenStreetMap'
  }).addTo(map);
}

async function fetchState() {
  try {
    const r = await fetch('/api/state/all');
    if (!r.ok) throw new Error('state fetch failed');
    nodes = await r.json();
    setError('');
    renderHeader();
    renderMap();
    renderNodeCards();
    if (selectedId) {
      renderDrawer(currentNode());
      if (Date.now() - lastDetailFetch > DETAIL_REFRESH_MS) fetchNodeDetails(selectedId);
    }
  } catch (e) {
    setError('API error: ' + e.message);
  }
}

function renderHeader() {
  const total = nodes.length;
  const alerts = nodes.filter(isAlert).length;
  document.getElementById('headTotal').textContent = total;
  document.getElementById('headAlert').textContent = alerts;
  document.getElementById('headRefresh').textContent = new Date().toLocaleTimeString();
  const unlocated = nodes.filter(n => !n.location_known).length;
  const avg = total ? nodes.reduce((s, n) => s + Number(n.risk_score || 0), 0) / total : 0;
  document.getElementById('nodeSummary').innerHTML =
    `Showing <b>${total}</b> nodes &nbsp; Unlocated: <b>${unlocated}</b> &nbsp; Average risk: <b>${avg.toFixed(2)}</b>`;
}

function markerIcon(alert) {
  return L.divIcon({
    className: '',
    html: `<div class="marker-node ${alert ? 'marker-red' : 'marker-green'}"></div>`,
    iconSize: [24, 24],
    iconAnchor: [12, 12]
  });
}

function renderMap() {
  const located = nodes.filter(n => n.location_known);
  document.getElementById('mapHint').textContent = located.length ? `${located.length} located nodes` : 'No located nodes yet';
  document.getElementById('fitMapBtn').disabled = !located.length || !map;
  if (!map) return;

  const active = {};
  located.forEach(n => {
    active[n.node_id] = true;
    const html =
      `<b>Node ${n.node_id}</b><br>` +
      `Status: ${statusLabel(n)}<br>` +
      `Anomaly: ${score(n.anomaly_score)}<br>` +
      `Risk: ${score(n.risk_score)}<br>` +
      `Battery: ${fmt(n.battery_level)}%<br>` +
      `<button class="popup-btn" onclick="selectNode(${n.node_id})">Open Details</button>`;
    if (!markers[n.node_id]) {
      markers[n.node_id] = L.marker([n.latitude, n.longitude], { icon: markerIcon(isAlert(n)) })
        .addTo(map)
        .on('click', () => selectNode(n.node_id));
    }
    markers[n.node_id].setLatLng([n.latitude, n.longitude]);
    markers[n.node_id].setIcon(markerIcon(isAlert(n)));
    markers[n.node_id].bindPopup(html);
  });
  Object.keys(markers).forEach(id => {
    if (!active[id]) {
      map.removeLayer(markers[id]);
      delete markers[id];
    }
  });
  if (located.length && !firstBoundsFit) {
    fitMapToNodes();
    firstBoundsFit = true;
  }
}

function fitMapToNodes() {
  if (!map) return;
  const located = nodes.filter(n => n.location_known);
  if (!located.length) return;
  map.fitBounds(located.map(n => [n.latitude, n.longitude]), { padding: [12, 12], maxZoom: 18 });
}

function renderNodeCards() {
  const grid = document.getElementById('nodeGrid');
  const empty = document.getElementById('emptyState');
  grid.innerHTML = '';
  empty.style.display = nodes.length ? 'none' : 'block';
  nodes.forEach(n => {
    const card = document.createElement('button');
    card.className = `node-card ${isAlert(n) ? 'alert-card' : ''} ${String(selectedId) === String(n.node_id) ? 'selected' : ''}`;
    card.onclick = () => selectNode(n.node_id);
    card.innerHTML =
      `<div class="card-top"><div class="node-id">Node ${n.node_id}</div>${badge(n)}</div>` +
      `<div class="card-metrics">` +
      `<div class="mini"><span>Anomaly</span><b>${score(n.anomaly_score)}</b></div>` +
      `<div class="mini"><span>Risk</span><b>${score(n.risk_score)}</b></div>` +
      `<div class="mini"><span>Battery</span><b>${fmt(n.battery_level)}%</b></div>` +
      `</div>` +
      `<div class="flags"><span class="flag">${n.location_known ? 'Located' : 'No location'}</span>` +
      `${n.pending_alert_mode ? '<span class="flag pending">Pending command</span>' : ''}` +
      `${n.streak_open ? '<span class="flag hot">Active event</span>' : ''}</div>`;
    grid.appendChild(card);
  });
}

function currentNode() { return nodes.find(x => String(x.node_id) === String(selectedId)); }
function selectNode(id) {
  selectedId = id;
  renderNodeCards();
  openDrawer();
  renderDrawer(currentNode());
  fetchNodeDetails(id);
}
function openDrawer() {
  document.getElementById('drawer').classList.add('open');
  document.getElementById('drawer').setAttribute('aria-hidden', 'false');
  document.getElementById('drawerBackdrop').classList.add('open');
}
function closeDrawer() {
  selectedId = null;
  document.getElementById('drawer').classList.remove('open');
  document.getElementById('drawer').setAttribute('aria-hidden', 'true');
  document.getElementById('drawerBackdrop').classList.remove('open');
  renderNodeCards();
}

function renderDrawer(n) {
  if (!n) return;
  document.getElementById('drawerTitle').textContent = 'Node ' + n.node_id;
  document.getElementById('drawerStatus').innerHTML = badge(n);
  document.getElementById('drawerCoords').textContent = n.location_known
    ? `Location: ${fmt(n.latitude, 6)}, ${fmt(n.longitude, 6)}`
    : 'Location: not configured';
  document.getElementById('drawerMeta').innerHTML =
    `<span class="drawer-chip">${n.event_count || 0} events</span>` +
    `<span class="drawer-chip">${n.trend_count || 0} trend points</span>` +
    `${n.pending_alert_mode ? '<span class="drawer-chip">Pending command</span>' : ''}`;
  document.getElementById('telemetrySeen').textContent = 'Last seen ' + timeLabel(n.last_seen_timestamp_s);
  const riskClass = isAlert(n) ? 'risk' : 'risk normal';
  document.getElementById('metricGrid').innerHTML = METRICS.map(m => {
    const riskStyle = m.key === 'risk_score' ? riskClass : (m.key === 'anomaly_score' && isAlert(n) ? 'risk' : '');
    return `<button class="metric selectable ${riskStyle} ${selectedMetric === m.key ? 'selected' : ''}" onclick="selectMetric('${m.key}')"><span>${m.label}</span><b>${fmt(n[m.key], m.digits)}${m.unit}</b></button>`;
  }).join('');
}

function selectMetric(key) {
  selectedMetric = key;
  renderDrawer(currentNode());
  updateChart(currentTrendPoints);
}
function setSampleLimit(value) {
  sampleLimit = value;
  updateChart(currentTrendPoints);
}

async function fetchNodeDetails(nodeId) {
  lastDetailFetch = Date.now();
  try {
    const [er, tr] = await Promise.all([
      fetch('/api/node/events?node_id=' + nodeId),
      fetch('/api/node/trend?node_id=' + nodeId)
    ]);
    const ed = await er.json();
    const td = await tr.json();
    currentEvents = ed.events || [];
    currentTrendPoints = td.points || [];
    renderEventFilters(currentEvents);
    renderEvents(currentEvents);
    updateChart(currentTrendPoints);
  } catch (e) {
    setError('Detail error: ' + e.message);
  }
}

function eventTypeLabel(e) {
  if (e.type_label === 'BOTH_ALERT') return 'BOTH ALERT: anomaly + risk';
  if (e.type_label === 'ANOMALY_ALERT') return 'ANOMALY ALERT';
  if (e.type_label === 'RISK_ALERT') return 'RISK ALERT';
  return e.type_label || 'EVENT';
}
function renderEventFilters(events) {
  const types = ['ALL', ...Array.from(new Set(events.map(e => e.type_label || 'UNKNOWN')))];
  if (!types.includes(eventFilter)) eventFilter = 'ALL';
  document.getElementById('eventFilters').innerHTML = types.map(t =>
    `<button class="event-filter ${eventFilter === t ? 'active' : ''}" onclick="setEventFilter('${t}')">${t === 'ALL' ? 'All' : eventTypeLabel({ type_label: t })}</button>`
  ).join('');
  document.getElementById('eventCountPill').textContent = events.length + ' events';
}
function setEventFilter(type) {
  eventFilter = type;
  renderEventFilters(currentEvents);
  renderEvents(currentEvents);
}
function renderEvents(events) {
  const ev = document.getElementById('events');
  const filtered = eventFilter === 'ALL' ? events : events.filter(e => e.type_label === eventFilter);
  ev.innerHTML = filtered.length ? filtered.map(e => {
    const both = e.type_label === 'BOTH_ALERT';
    return `<li class="${both ? 'event-both' : ''}">` +
      `<div class="event-top"><span class="event-type ${both ? 'alert' : ''}">${eventTypeLabel(e)}</span><span class="event-status">${e.still_open ? 'active' : 'closed'}</span></div>` +
      `<div>${timeLabel(e.start_timestamp_s)} &rarr; ${e.still_open ? 'active' : timeLabel(e.end_timestamp_s)}</div>` +
      `<div class="event-stats">` +
      `<div class="event-stat"><span>Peak anomaly</span><b>${score(e.peak_anomaly_score)}</b></div>` +
      `<div class="event-stat"><span>Peak risk</span><b>${score(e.peak_risk_score)}</b></div>` +
      `<div class="event-stat"><span>Max temp</span><b>${fmt(e.max_temperature)} C</b></div>` +
      `<div class="event-stat"><span>Min humidity</span><b>${fmt(e.min_humidity)} %</b></div>` +
      `<div class="event-stat"><span>Min gas</span><b>${fmt(e.min_gas_resistance, 0)}</b></div>` +
      `<div class="event-stat"><span>Samples</span><b>${e.sample_count || 0}</b></div>` +
      `</div></li>`;
  }).join('') : '<li>No recent events</li>';
}

function updateChart(points) {
  const wrap = document.getElementById('chartWrap');
  if (!window.Chart) {
    wrap.innerHTML = '<div class="unavailable">Trend chart unavailable</div>';
    return;
  }
  if (!document.getElementById('trendChart')) {
    wrap.innerHTML = '<canvas id="trendChart" height="180"></canvas>';
  }
  const def = metricDef(selectedMetric);
  const visiblePoints = sampleLimit === 'all' ? points : points.slice(-Number(sampleLimit));
  const labels = visiblePoints.map(p => timeLabel(p.timestamp_s));
  const values = visiblePoints.map(p => p[selectedMetric]);
  document.getElementById('trendTitle').textContent = def.label + ' Trend';
  if (!chart) {
    chart = new Chart(document.getElementById('trendChart'), {
      type: 'line',
      data: { labels, datasets: [{ label: def.label, data: values, borderColor: def.color, backgroundColor: 'transparent', tension: .25 }] },
      options: {
        responsive: true,
        animation: false,
        maintainAspectRatio: false,
        scales: {
          x: { ticks: { color: '#667257', maxTicksLimit: 4 }, grid: { color: '#d8dfcb' } },
          y: { ticks: { color: '#667257' }, grid: { color: '#d8dfcb' } }
        },
        plugins: { legend: { labels: { color: '#17220f' } } }
      }
    });
  } else {
    chart.data.labels = labels;
    chart.data.datasets[0].label = def.label;
    chart.data.datasets[0].borderColor = def.color;
    chart.data.datasets[0].data = values;
    chart.update();
  }
}

initMap();
fetchState();
setInterval(fetchState, STATE_REFRESH_MS);
