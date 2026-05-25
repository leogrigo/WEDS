function setMsg(id, msg, ok) {
  const el = document.getElementById(id);
  el.textContent = msg;
  el.className = `msg ${ok ? 'ok' : 'err'}`;
}

function timeLabel(s) {
  const n = Number(s);
  if (!Number.isFinite(n) || n <= 0) return '-';
  return n > 1704067200 ? new Date(n * 1000).toLocaleString() : `${n}s`;
}

async function refreshUnlocated() {
  try {
    const r = await fetch('/api/nodes/unlocated');
    if (!r.ok) throw new Error('failed to load nodes');
    const nodes = await r.json();
    const rows = document.getElementById('unlocatedRows');
    rows.innerHTML = '';
    if (!nodes.length) {
      rows.innerHTML = '<tr><td colspan="3">No unlocated nodes</td></tr>';
      return;
    }
    nodes.forEach(n => {
      const tr = document.createElement('tr');
      tr.innerHTML = `<td>${n.node_id}</td><td>${timeLabel(n.last_seen_timestamp_s)}</td><td><button>Use this node</button></td>`;
      tr.querySelector('button').onclick = () => {
        document.getElementById('nodeId').value = n.node_id;
      };
      rows.appendChild(tr);
    });
  } catch (e) {
    setMsg('formMsg', 'Load error: ' + e.message, false);
  }
}

document.getElementById('locationForm').addEventListener('submit', async e => {
  e.preventDefault();
  const body = {
    node_id: Number(document.getElementById('nodeId').value),
    latitude: Number(document.getElementById('latitude').value),
    longitude: Number(document.getElementById('longitude').value)
  };
  try {
    const r = await fetch('/api/admin/setlocation', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    const data = await r.json();
    if (!r.ok) throw new Error(data.detail || data.message || 'save failed');
    setMsg('formMsg', `Location command queued for node ${data.node_id}`, true);
    refreshUnlocated();
  } catch (e) {
    setMsg('formMsg', e.message, false);
  }
});

document.getElementById('useBrowserLocation').onclick = () => {
  if (!navigator.geolocation) {
    setMsg('formMsg', 'Browser geolocation is not available', false);
    return;
  }
  if (!window.isSecureContext) {
    setMsg('formMsg', 'Browser location requires HTTPS or localhost. Enter coordinates manually or use a secure proxy.', false);
    return;
  }
  setMsg('formMsg', 'Requesting browser location...', true);
  navigator.geolocation.getCurrentPosition(
    position => {
      document.getElementById('latitude').value = position.coords.latitude.toFixed(6);
      document.getElementById('longitude').value = position.coords.longitude.toFixed(6);
      setMsg('formMsg', 'Browser location copied into the form', true);
    },
    error => {
      setMsg('formMsg', 'Location error: ' + error.message, false);
    },
    { enableHighAccuracy: true, timeout: 10000, maximumAge: 30000 }
  );
};

document.getElementById('clearConfig').onclick = async () => {
  if (!confirm('Clear persistent node location config?')) return;
  try {
    const r = await fetch('/api/admin/clearconfig', { method: 'POST' });
    const data = await r.json();
    if (!r.ok) throw new Error(data.detail || data.message || 'clear failed');
    setMsg('clearMsg', `Clear config command queued: ${data.command_id}`, true);
    refreshUnlocated();
  } catch (e) {
    setMsg('clearMsg', e.message, false);
  }
};

document.getElementById('exportState').onclick = async () => {
  try {
    setMsg('exportMsg', 'Preparing export...', true);
    const response = await fetch('/api/export');
    if (!response.ok) throw new Error('failed to load dashboard snapshot');
    const payload = await response.json();
    const exportedAt = new Date();
    const blob = new Blob([JSON.stringify({
      exported_at: exportedAt.toISOString(),
      source: 'WEDS Dashboard MQTT/SQLite registry',
      ...payload
    }, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    const stamp = exportedAt.toISOString().replace(/[:.]/g, '-');
    a.href = url;
    a.download = `weds-registry-state-${stamp}.json`;
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
    const nodeCount = Array.isArray(payload.nodes) ? payload.nodes.length : 0;
    setMsg('exportMsg', `Exported ${nodeCount} nodes`, true);
  } catch (e) {
    setMsg('exportMsg', e.message, false);
  }
};

document.getElementById('clearDatabase').onclick = async () => {
  if (!confirm('Clear all local dashboard data from SQLite? Gateway data will arrive again from MQTT when nodes publish.')) return;
  try {
    const r = await fetch('/api/admin/cleardb', { method: 'POST' });
    const data = await r.json();
    if (!r.ok) throw new Error(data.detail || data.message || 'database clear failed');
    setMsg('exportMsg', 'Dashboard database cleared', true);
    refreshUnlocated();
  } catch (e) {
    setMsg('exportMsg', e.message, false);
  }
};

refreshUnlocated();
