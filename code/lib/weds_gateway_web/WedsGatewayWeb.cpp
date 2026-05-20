#include "WedsGatewayWeb.h"

/**
 * @brief PROGMEM string containing the HTML content for the main dashboard page.
 */
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>WEDS Gateway Dashboard</title>
  <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">
  <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.7/dist/chart.umd.min.js"></script>
  <style>
    :root{--evergreen:#002400;--forest:#273b09;--olive:#58641d;--palm:#7b904b;--lavender:#dbd2e0;--bg:#f2eff5;--panel:#fffffb;--panel2:#eef3e4;--panel3:#e3ddea;--line:#c8d0b8;--text:#17220f;--muted:#667257;--green:#177245;--red:#b32636;--blue:#4d7c2e;--amber:#a86e00}
    *{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:Inter,Arial,Helvetica,sans-serif;font-size:14px}button,a{font:inherit}button{border:0;cursor:pointer;color:var(--text)}
    .app-header{background:var(--evergreen);color:#fff;border-bottom:4px solid var(--palm);padding:16px 24px;display:flex;align-items:center;justify-content:space-between;gap:18px}
    .brand{display:flex;align-items:center;gap:12px;min-width:240px}.logo{width:42px;height:42px;border-radius:11px;background:#f05a3b;display:grid;place-items:center;box-shadow:0 8px 24px rgba(0,0,0,.18)}.logo svg{width:25px;height:25px;fill:#fff}
    h1,h2,h3,p{margin:0}h1{font-size:22px;line-height:1.1}.subtitle{color:#d9e6cf;font-size:13px;margin-top:3px}.head-meta{display:flex;align-items:center;justify-content:flex-end;gap:10px;flex-wrap:wrap}.meta{background:rgba(255,255,255,.1);border:1px solid rgba(255,255,255,.2);border-radius:8px;padding:8px 10px;color:#d9e6cf}.meta b{display:block;color:#fff;font-size:16px}.admin-btn{background:var(--palm);color:white;text-decoration:none;border-radius:8px;padding:10px 13px;font-weight:700;transition:box-shadow .14s ease,filter .14s ease}.admin-btn:hover{box-shadow:inset 0 0 0 999px rgba(0,36,0,.12);filter:brightness(.98)}
    main{padding:18px;display:grid;gap:16px}.panel{background:var(--panel);border:1px solid var(--line);border-radius:12px;box-shadow:0 8px 24px rgba(39,59,9,.08)}.map-panel,.nodes-panel{padding:12px}.panel-head{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:10px}.panel-title{font-size:17px;font-weight:800;color:var(--forest)}.map-actions{display:flex;align-items:center;gap:8px;flex-wrap:wrap;justify-content:flex-end}.hint,.summary-line{color:var(--forest);font-size:13px;background:var(--panel3);border:1px solid #c7bdd0;border-radius:999px;padding:6px 10px}.hint b,.summary-line b{color:var(--evergreen)}.fit-map{background:var(--palm);border:1px solid var(--palm);color:#fff;border-radius:8px;padding:10px 13px;font-size:13px;font-weight:700;transition:box-shadow .14s ease,filter .14s ease}.fit-map:hover:not(:disabled){box-shadow:inset 0 0 0 999px rgba(0,36,0,.12);filter:brightness(.98)}.fit-map:disabled{opacity:.45;cursor:not-allowed}.error{color:#6f111b;background:#ffe1e5;border:1px solid #f0a9b1;border-radius:8px;padding:9px 11px;display:none}
    #map{height:520px;border-radius:9px;background:#dbe8d3;border:1px solid var(--line);overflow:hidden;position:relative;z-index:1}.leaflet-container{z-index:1}.map-fallback{height:520px;border-radius:9px;background:var(--panel2);border:1px dashed var(--line);display:none;align-items:center;justify-content:center;color:var(--muted)}
    .node-summary{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;margin-bottom:12px}.node-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:12px}.node-card{background:#fff;border:1px solid var(--line);border-left:5px solid var(--palm);border-radius:10px;padding:13px;text-align:left;transition:transform .12s ease,border-color .12s ease,background .12s ease,box-shadow .12s ease}.node-card:hover{transform:translateY(-2px);background:#fbfdf8;box-shadow:0 10px 22px rgba(39,59,9,.12)}.node-card.alert-card{border-left-color:var(--red)}.node-card.selected{outline:2px solid var(--olive);background:#f6f9ef}.card-top{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-bottom:12px}.node-id{font-size:16px;font-weight:800;color:var(--text)}.badge{display:inline-flex;align-items:center;border-radius:999px;padding:4px 8px;font-size:11px;font-weight:800;letter-spacing:.02em}.badge.ok{background:#dcebd2;color:#174f31}.badge.alert{background:#ffdce0;color:#8e1b29}.card-metrics{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px}.mini{background:linear-gradient(180deg,var(--panel2),#f7faf1);border:1px solid #dbe3cc;border-radius:8px;padding:8px}.mini span,.metric span{display:block;color:var(--muted);font-size:11px}.mini b,.metric b{display:block;margin-top:3px;font-size:14px;color:var(--text)}.flags{display:flex;gap:6px;flex-wrap:wrap;margin-top:10px}.flag{border:1px solid var(--line);border-radius:999px;color:var(--muted);padding:3px 7px;font-size:11px;background:#fff}.flag.hot{border-color:#db8b94;color:#8e1b29}.flag.pending{border-color:#e3b34c;color:#704700;background:#fff8e5}.empty{display:none;color:var(--muted);background:var(--panel);border:1px dashed var(--line);border-radius:10px;padding:22px;text-align:center}
    .marker-node{position:relative;width:21px;height:21px;border:2px solid #fff;box-shadow:0 0 14px rgba(0,0,0,.65);transform:rotate(45deg);border-radius:5px}.marker-node::after{content:'';position:absolute;inset:5px;border-radius:2px;background:rgba(255,255,255,.65)}.marker-green{background:var(--green)}.marker-red{background:var(--red)}.popup-btn{margin-top:7px;background:var(--blue);color:white;border-radius:6px;padding:6px 8px}
    .drawer-backdrop{position:fixed;inset:0;background:rgba(0,36,0,.22);z-index:2000;display:none}.drawer-backdrop.open{display:block}.drawer{position:fixed;top:0;right:0;height:100vh;width:min(50vw,760px);min-width:520px;background:var(--panel);border-left:1px solid var(--line);z-index:2010;transform:translateX(105%);transition:transform .18s ease;box-shadow:-20px 0 50px rgba(39,59,9,.28);display:flex;flex-direction:column}.drawer.open{transform:translateX(0)}.drawer-head{padding:20px 22px;border-bottom:1px solid var(--line);display:grid;grid-template-columns:1fr auto;gap:14px;background:linear-gradient(135deg,var(--evergreen),var(--forest));color:#fff}.drawer-title-row{display:flex;align-items:center;gap:12px;flex-wrap:wrap}.drawer-title-row h2{font-size:24px}.drawer-head .coords{color:#d9e6cf;margin-top:10px}.drawer-meta{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}.drawer-chip{background:rgba(255,255,255,.12);border:1px solid rgba(255,255,255,.22);border-radius:999px;color:#eef7e9;padding:5px 9px;font-size:12px}.close{background:rgba(255,255,255,.1);color:#fff;font-size:30px;line-height:1;width:38px;height:38px;border-radius:10px}.drawer-body{padding:22px;overflow:auto;display:grid;gap:22px;background:linear-gradient(180deg,#fffffb,var(--bg))}.section-title-row{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:10px}.section-title-row h3{margin:0}.section-note-pill{background:var(--panel3);border:1px solid #c7bdd0;color:var(--forest);border-radius:999px;padding:5px 9px;font-size:12px}.sample-select{background:#fff;border:1px solid #c7bdd0;color:var(--forest);border-radius:999px;padding:5px 9px;font-size:12px;outline:none}.coords{color:var(--muted);font-size:13px}.metric-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px}.metric{background:#fff;border:1px solid var(--line);border-radius:9px;padding:10px;text-align:left}.metric.selectable{cursor:pointer}.metric.selectable:hover{border-color:var(--palm);background:#fbfdf8}.metric.selected{outline:2px solid var(--olive);background:#f6f9ef}.metric.risk{border-color:#db8b94;background:#fff1f3}.metric.risk.normal{border-color:var(--line);background:#f3f7ee}.event-tools{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px}.event-filter{border:1px solid var(--line);background:#fff;color:var(--forest);border-radius:999px;padding:5px 9px;font-size:12px}.event-filter.active{background:var(--forest);border-color:var(--forest);color:#fff}.events-box{background:var(--panel2);border:1px solid var(--line);border-radius:10px;padding:10px}.events{list-style:none;margin:0;padding:0;display:grid;gap:10px;max-height:320px;overflow-y:auto;padding-right:4px}.events li{background:#fff;border:1px solid var(--line);border-left:4px solid var(--palm);border-radius:8px;padding:10px;color:var(--muted)}.events li.event-both{border-left-color:var(--red);background:#fff7f8}.events b{color:var(--text)}.event-top{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-bottom:6px}.event-type{font-weight:800;color:var(--forest)}.event-type.alert{color:var(--red)}.event-status{font-size:11px;border-radius:999px;padding:3px 7px;background:var(--panel2);color:var(--forest)}.event-stats{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:8px}.event-stat{background:var(--panel2);border-radius:7px;padding:6px}.event-stat span{display:block;font-size:10px;color:var(--muted)}.event-stat b{display:block;font-size:12px}.chart-wrap{background:#fff;border:1px solid var(--line);border-radius:9px;padding:10px;min-height:260px}.unavailable{color:var(--muted);padding:18px;text-align:center}
    @media(max-width:900px){.drawer{width:100vw;min-width:0}.metric-grid{grid-template-columns:1fr 1fr}}
    @media(max-width:760px){.app-header{align-items:flex-start;display:block}.head-meta{justify-content:flex-start;margin-top:12px}.meta{padding:7px 9px}#map,.map-fallback{height:430px}main{padding:12px}.card-metrics{grid-template-columns:1fr 1fr}}
  </style>
</head>
<body>
  <header class="app-header">
    <div class="brand">
      <div class="logo" aria-hidden="true"><svg viewBox="0 0 24 24"><path d="M13.1 2.4c.7 3.5-.5 5.5-2.2 7.3-1.3 1.4-2.7 2.9-2.7 5.2 0 2.4 1.8 4.4 4.3 4.4 2.8 0 4.9-2.1 4.9-5.1 0-1.8-.7-3.6-2-5.2-.2 1.4-.8 2.4-1.7 3.2.2-2.3-.4-5.3-2.9-8.1-.5-.6-1.1-1.1-1.7-1.7zM12 22C7.8 22 5 19.1 5 15.1c0-3.5 2-5.7 3.5-7.3 1.4-1.5 2.2-2.5 1.5-4.9-.1-.4.1-.8.5-1 .3-.2.8-.2 1.1.1 5.7 4.8 8.4 8.6 8.4 12.3C20 18.8 16.6 22 12 22z"/></svg></div>
      <div><h1>WEDS Gateway Dashboard</h1><p class="subtitle">Wildfire Early Detection System</p></div>
    </div>
    <div class="head-meta">
      <div class="meta">Total Nodes <b id="headTotal">0</b></div>
      <div class="meta">Alert Nodes <b id="headAlert">0</b></div>
      <div class="meta">Last Refresh <b id="headRefresh">--:--:--</b></div>
      <a class="admin-btn" href="/admin">Admin</a>
    </div>
  </header>
  <main>
    <div id="apiError" class="error"></div>
    <section class="panel map-panel">
      <div class="panel-head"><h2 class="panel-title">Live Node Map</h2><div class="map-actions"><span id="mapHint" class="hint">Loading nodes...</span><button id="fitMapBtn" class="fit-map" onclick="fitMapToNodes()">Fit zoom</button></div></div>
      <div id="map"></div>
      <div id="mapFallback" class="map-fallback">Map unavailable: Leaflet failed to load</div>
    </section>
    <section class="panel nodes-panel">
      <div class="node-summary">
        <h2 class="panel-title">Nodes</h2>
        <div id="nodeSummary" class="summary-line">Showing <b>0</b> nodes</div>
      </div>
      <div id="emptyState" class="empty">No nodes received yet.</div>
      <div id="nodeGrid" class="node-grid"></div>
    </section>
  </main>
  <div id="drawerBackdrop" class="drawer-backdrop" onclick="closeDrawer()"></div>
  <aside id="drawer" class="drawer" aria-hidden="true">
    <div class="drawer-head">
      <div>
        <div class="drawer-title-row"><h2 id="drawerTitle">Node</h2><div id="drawerStatus"></div></div>
        <p id="drawerCoords" class="coords"></p>
        <div id="drawerMeta" class="drawer-meta"></div>
      </div>
      <button class="close" onclick="closeDrawer()" aria-label="Close">&times;</button>
    </div>
    <div class="drawer-body">
      <section><div class="section-title-row"><h3>Latest Telemetry</h3><span id="telemetrySeen" class="section-note-pill">Last seen -</span></div><div id="metricGrid" class="metric-grid"></div></section>
      <section><div class="section-title-row"><h3 id="trendTitle">Telemetry Trend</h3><select id="sampleSelect" class="sample-select" onchange="setSampleLimit(this.value)"><option value="10">Last 10 samples</option><option value="20">Last 20 samples</option><option value="40">Last 40 samples</option><option value="all" selected>All samples</option></select></div><div id="chartWrap" class="chart-wrap"><canvas id="trendChart" height="180"></canvas></div></section>
      <section><div class="section-title-row"><h3>Recent Events</h3><span id="eventCountPill" class="section-note-pill">0 events</span></div><div id="eventFilters" class="event-tools"></div><div class="events-box"><ul id="events" class="events"><li>No recent events</li></ul></div></section>
    </div>
  </aside>
  <script>
    const DEFAULT_CENTER=[41.9028,12.4964],STATE_REFRESH_MS=15000,DETAIL_REFRESH_MS=10000;
    let nodes=[],selectedId=null,map=null,markers={},chart=null,firstBoundsFit=false,lastDetailFetch=0;
    let selectedMetric='temperature',currentTrendPoints=[],currentEvents=[],eventFilter='ALL',sampleLimit='all';
    const METRICS=[
      {key:'temperature',label:'Temperature',unit:' C',digits:1,color:'#a86e00'},
      {key:'humidity',label:'Humidity',unit:' %',digits:1,color:'#58641d'},
      {key:'pressure',label:'Pressure',unit:' hPa',digits:1,color:'#4d7c2e'},
      {key:'gas_resistance',label:'Gas resistance',unit:'',digits:0,color:'#273b09'},
      {key:'battery_level',label:'Battery',unit:' %',digits:1,color:'#7b904b'},
      {key:'anomaly_score',label:'Anomaly score',unit:'',digits:2,color:'#b32636'},
      {key:'risk_score',label:'Risk score',unit:'',digits:2,color:'#b32636'}
    ];
    function isAlert(n){return n.anomaly_state===1||n.risk_state===1}
    function fmt(v,d=1){return Number.isFinite(Number(v))?Number(v).toFixed(d):'-'}
    function score(v){return fmt(v,2)}
    function timeLabel(s){const n=Number(s);if(!Number.isFinite(n)||n<=0)return '-';return n>1704067200?new Date(n*1000).toLocaleString():n+'s'}
    function timeRange(a,b){return timeLabel(a)+' - '+timeLabel(b)}
    function statusLabel(n){return isAlert(n)?'ALERT':'NORMAL'}
    function badge(n){return `<span class="badge ${isAlert(n)?'alert':'ok'}">${statusLabel(n)}</span>`}
    function metricDef(key){return METRICS.find(m=>m.key===key)||METRICS[0]}
    function setError(msg){const el=document.getElementById('apiError');el.textContent=msg||'';el.style.display=msg?'block':'none'}
    function initMap(){
      if(!window.L){document.getElementById('map').style.display='none';document.getElementById('mapFallback').style.display='flex';return}
      map=L.map('map',{zoomControl:true}).setView(DEFAULT_CENTER,12);
      L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'&copy; OpenStreetMap'}).addTo(map);
    }
    async function fetchState(){
      try{
        const r=await fetch('/api/state/all');if(!r.ok)throw new Error('state fetch failed');
        nodes=await r.json();setError('');renderHeader();renderMap();renderNodeCards();
        if(selectedId){renderDrawer(currentNode());if(Date.now()-lastDetailFetch>DETAIL_REFRESH_MS)fetchNodeDetails(selectedId)}
      }catch(e){setError('API error: '+e.message)}
    }
    function renderHeader(){
      const total=nodes.length, alerts=nodes.filter(isAlert).length;
      document.getElementById('headTotal').textContent=total;
      document.getElementById('headAlert').textContent=alerts;
      document.getElementById('headRefresh').textContent=new Date().toLocaleTimeString();
      const unlocated=nodes.filter(n=>!n.location_known).length;
      const avg=total?nodes.reduce((s,n)=>s+Number(n.risk_score||0),0)/total:0;
      document.getElementById('nodeSummary').innerHTML=`Showing <b>${total}</b> nodes &nbsp; Unlocated: <b>${unlocated}</b> &nbsp; Average risk: <b>${avg.toFixed(2)}</b>`;
    }
    function markerIcon(alert){
      return L.divIcon({className:'',html:`<div class="marker-node ${alert?'marker-red':'marker-green'}"></div>`,iconSize:[24,24],iconAnchor:[12,12]});
    }
    function renderMap(){
      const located=nodes.filter(n=>n.location_known);
      document.getElementById('mapHint').textContent=located.length?`${located.length} located nodes`:'No located nodes yet';
      document.getElementById('fitMapBtn').disabled=!located.length||!map;
      if(!map)return;
      const active={};
      located.forEach(n=>{
        active[n.node_id]=true;
        const html=`<b>Node ${n.node_id}</b><br>Status: ${statusLabel(n)}<br>Anomaly: ${score(n.anomaly_score)}<br>Risk: ${score(n.risk_score)}<br>Battery: ${fmt(n.battery_level)}%<br><button class="popup-btn" onclick="selectNode(${n.node_id})">Open Details</button>`;
        if(!markers[n.node_id]){
          markers[n.node_id]=L.marker([n.latitude,n.longitude],{icon:markerIcon(isAlert(n))}).addTo(map).on('click',()=>selectNode(n.node_id));
        }
        markers[n.node_id].setLatLng([n.latitude,n.longitude]);
        markers[n.node_id].setIcon(markerIcon(isAlert(n)));
        markers[n.node_id].bindPopup(html);
      });
      Object.keys(markers).forEach(id=>{if(!active[id]){map.removeLayer(markers[id]);delete markers[id];}});
      if(located.length&&!firstBoundsFit){fitMapToNodes();firstBoundsFit=true}
    }
    function fitMapToNodes(){
      if(!map)return;
      const located=nodes.filter(n=>n.location_known);
      if(!located.length)return;
      map.fitBounds(located.map(n=>[n.latitude,n.longitude]),{padding:[12,12],maxZoom:18});
    }
    function renderNodeCards(){
      const grid=document.getElementById('nodeGrid'),empty=document.getElementById('emptyState');grid.innerHTML='';
      empty.style.display=nodes.length?'none':'block';
      nodes.forEach(n=>{
        const card=document.createElement('button');
        card.className=`node-card ${isAlert(n)?'alert-card':''} ${String(selectedId)===String(n.node_id)?'selected':''}`;
        card.onclick=()=>selectNode(n.node_id);
        card.innerHTML=`<div class="card-top"><div class="node-id">Node ${n.node_id}</div>${badge(n)}</div><div class="card-metrics"><div class="mini"><span>Anomaly</span><b>${score(n.anomaly_score)}</b></div><div class="mini"><span>Risk</span><b>${score(n.risk_score)}</b></div><div class="mini"><span>Battery</span><b>${fmt(n.battery_level)}%</b></div></div><div class="flags"><span class="flag">${n.location_known?'Located':'No location'}</span>${n.pending_alert_mode?'<span class="flag pending">Pending command</span>':''}${n.streak_open?'<span class="flag hot">Active event</span>':''}</div>`;
        grid.appendChild(card);
      });
    }
    function currentNode(){return nodes.find(x=>String(x.node_id)===String(selectedId))}
    function selectNode(id){selectedId=id;renderNodeCards();openDrawer();renderDrawer(currentNode());fetchNodeDetails(id)}
    function openDrawer(){document.getElementById('drawer').classList.add('open');document.getElementById('drawer').setAttribute('aria-hidden','false');document.getElementById('drawerBackdrop').classList.add('open')}
    function closeDrawer(){selectedId=null;document.getElementById('drawer').classList.remove('open');document.getElementById('drawer').setAttribute('aria-hidden','true');document.getElementById('drawerBackdrop').classList.remove('open');renderNodeCards()}
    function renderDrawer(n){
      if(!n)return;
      document.getElementById('drawerTitle').textContent='Node '+n.node_id;
      document.getElementById('drawerStatus').innerHTML=badge(n);
      document.getElementById('drawerCoords').textContent=n.location_known?`Location: ${fmt(n.latitude,6)}, ${fmt(n.longitude,6)}`:'Location: not configured';
      document.getElementById('drawerMeta').innerHTML=`<span class="drawer-chip">${n.event_count||0} events</span><span class="drawer-chip">${n.trend_count||0} trend points</span>${n.pending_alert_mode?'<span class="drawer-chip">Pending command</span>':''}`;
      document.getElementById('telemetrySeen').textContent='Last seen '+timeLabel(n.last_seen_timestamp_s);
      const riskClass=isAlert(n)?'risk':'risk normal';
      document.getElementById('metricGrid').innerHTML=METRICS.map(m=>{
        const riskStyle=m.key==='risk_score'?riskClass:(m.key==='anomaly_score'&&isAlert(n)?'risk':'');
        return `<button class="metric selectable ${riskStyle} ${selectedMetric===m.key?'selected':''}" onclick="selectMetric('${m.key}')"><span>${m.label}</span><b>${fmt(n[m.key],m.digits)}${m.unit}</b></button>`;
      }).join('');
    }
    function selectMetric(key){selectedMetric=key;renderDrawer(currentNode());updateChart(currentTrendPoints)}
    function setSampleLimit(value){sampleLimit=value;updateChart(currentTrendPoints)}
    async function fetchNodeDetails(nodeId){
      lastDetailFetch=Date.now();
      try{
        const [er,tr]=await Promise.all([fetch('/api/node/events?node_id='+nodeId),fetch('/api/node/trend?node_id='+nodeId)]);
        const ed=await er.json();const td=await tr.json();
        currentEvents=ed.events||[];
        currentTrendPoints=td.points||[];
        renderEventFilters(currentEvents);
        renderEvents(currentEvents);
        updateChart(currentTrendPoints);
      }catch(e){setError('Detail error: '+e.message);}
    }
    function eventTypeLabel(e){
      if(e.type_label==='BOTH_ALERT')return 'BOTH ALERT: anomaly + risk';
      if(e.type_label==='ANOMALY_ALERT')return 'ANOMALY ALERT';
      if(e.type_label==='RISK_ALERT')return 'RISK ALERT';
      return e.type_label||'EVENT';
    }
    function renderEventFilters(events){
      const types=['ALL',...Array.from(new Set(events.map(e=>e.type_label||'UNKNOWN')))];
      if(!types.includes(eventFilter))eventFilter='ALL';
      document.getElementById('eventFilters').innerHTML=types.map(t=>`<button class="event-filter ${eventFilter===t?'active':''}" onclick="setEventFilter('${t}')">${t==='ALL'?'All':eventTypeLabel({type_label:t})}</button>`).join('');
      document.getElementById('eventCountPill').textContent=events.length+' events';
    }
    function setEventFilter(type){eventFilter=type;renderEventFilters(currentEvents);renderEvents(currentEvents)}
    function renderEvents(events){
      const ev=document.getElementById('events');
      const filtered=eventFilter==='ALL'?events:events.filter(e=>e.type_label===eventFilter);
      ev.innerHTML=filtered.length?filtered.map(e=>{
        const both=e.type_label==='BOTH_ALERT';
        return `<li class="${both?'event-both':''}"><div class="event-top"><span class="event-type ${both?'alert':''}">${eventTypeLabel(e)}</span><span class="event-status">${e.still_open?'active':'closed'}</span></div><div>${timeLabel(e.start_timestamp_s)} &rarr; ${e.still_open?'active':timeLabel(e.end_timestamp_s)}</div><div class="event-stats"><div class="event-stat"><span>Peak anomaly</span><b>${score(e.peak_anomaly_score)}</b></div><div class="event-stat"><span>Peak risk</span><b>${score(e.peak_risk_score)}</b></div><div class="event-stat"><span>Max temp</span><b>${fmt(e.max_temperature)} C</b></div><div class="event-stat"><span>Min humidity</span><b>${fmt(e.min_humidity)} %</b></div><div class="event-stat"><span>Min gas</span><b>${fmt(e.min_gas_resistance,0)}</b></div><div class="event-stat"><span>Samples</span><b>${e.sample_count||0}</b></div></div></li>`;
      }).join(''):'<li>No recent events</li>';
    }
    function updateChart(points){
      const wrap=document.getElementById('chartWrap');
      if(!window.Chart){wrap.innerHTML='<div class="unavailable">Trend chart unavailable</div>';return}
      if(!document.getElementById('trendChart'))wrap.innerHTML='<canvas id="trendChart" height="180"></canvas>';
      const def=metricDef(selectedMetric);
      const visiblePoints=sampleLimit==='all'?points:points.slice(-Number(sampleLimit));
      const labels=visiblePoints.map(p=>timeLabel(p.timestamp_s)), values=visiblePoints.map(p=>p[selectedMetric]);
      document.getElementById('trendTitle').textContent=def.label+' Trend';
      if(!chart){
        chart=new Chart(document.getElementById('trendChart'),{type:'line',data:{labels,datasets:[{label:def.label,data:values,borderColor:def.color,backgroundColor:'transparent',tension:.25}]},options:{responsive:true,animation:false,maintainAspectRatio:false,scales:{x:{ticks:{color:'#667257',maxTicksLimit:4},grid:{color:'#d8dfcb'}},y:{ticks:{color:'#667257'},grid:{color:'#d8dfcb'}}},plugins:{legend:{labels:{color:'#17220f'}}}}});
      }else{chart.data.labels=labels;chart.data.datasets[0].label=def.label;chart.data.datasets[0].borderColor=def.color;chart.data.datasets[0].data=values;chart.update();}
    }
    initMap();fetchState();setInterval(fetchState,STATE_REFRESH_MS);
  </script>
</body>
</html>
)rawliteral";

/**
 * @brief PROGMEM string containing the HTML content for the administration page.
 */
static const char ADMIN_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>WEDS Gateway Admin</title>
  <style>
    :root{--evergreen:#002400;--forest:#273b09;--olive:#58641d;--palm:#7b904b;--lavender:#dbd2e0;--bg:#f2eff5;--panel:#fffffb;--panel2:#eef3e4;--panel3:#e3ddea;--line:#c8d0b8;--text:#17220f;--muted:#667257;--red:#b32636}
    *{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:Inter,Arial,Helvetica,sans-serif;font-size:14px}
    header{padding:18px 24px;background:linear-gradient(135deg,var(--evergreen),var(--forest));border-bottom:4px solid var(--palm);display:flex;justify-content:space-between;align-items:center;gap:16px;color:white}.admin-brand{display:flex;align-items:center;gap:12px}.admin-logo{width:42px;height:42px;border-radius:11px;background:#7f8578;display:grid;place-items:center;box-shadow:0 8px 24px rgba(0,0,0,.18)}.admin-logo svg{width:25px;height:25px;fill:#fff}h1,h2,p{margin:0}h1{font-size:24px}h2{font-size:18px;color:var(--forest);margin-bottom:14px}.sub{color:#d9e6cf;margin-top:4px}.back{background:var(--palm);border:1px solid var(--palm);color:white;text-decoration:none;border-radius:8px;padding:10px 13px;font-weight:700;transition:box-shadow .14s ease,filter .14s ease}.back:hover{box-shadow:inset 0 0 0 999px rgba(0,36,0,.12);filter:brightness(.98)}
    main{padding:18px;display:grid;grid-template-columns:1fr 1fr;gap:16px}.panel{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:16px;box-shadow:0 8px 24px rgba(39,59,9,.08)}
    table{width:100%;border-collapse:separate;border-spacing:0 8px}th,td{padding:10px;text-align:left}th{color:var(--muted);font-size:12px;text-transform:uppercase}tbody tr{background:#fff;border:1px solid var(--line)}tbody td{border-top:1px solid var(--line);border-bottom:1px solid var(--line)}tbody td:first-child{border-left:1px solid var(--line);border-radius:9px 0 0 9px;font-weight:800}tbody td:last-child{border-right:1px solid var(--line);border-radius:0 9px 9px 0}
    label{display:block;color:var(--forest);font-weight:700;margin:12px 0 5px}input{width:100%;padding:11px;border-radius:8px;border:1px solid var(--line);background:#fff;color:var(--text);outline:none}input:focus{border-color:var(--olive);box-shadow:0 0 0 3px rgba(123,144,75,.18)}
    button{background:var(--olive);color:white;border:0;border-radius:8px;padding:9px 12px;cursor:pointer;margin-top:10px;font-weight:700;transition:box-shadow .14s ease,filter .14s ease}button:hover{filter:brightness(.98);box-shadow:inset 0 0 0 999px rgba(0,36,0,.12)}button.danger{background:var(--red)}.msg{min-height:20px;margin-top:10px}.ok{color:#17633c}.err{color:#8e1b29}.section-note{color:var(--muted);margin:-6px 0 12px}.config-box,.export-box{background:var(--panel2);border:1px solid var(--line);border-radius:10px;padding:12px;margin-top:26px}
    @media(max-width:800px){main{grid-template-columns:1fr;padding:12px}header{display:block}.back{display:inline-block;margin-top:12px}}
  </style>
</head>
<body>
  <header>
    <div class="admin-brand">
      <div class="admin-logo" aria-hidden="true"><svg viewBox="0 0 24 24"><path d="M19.4 13.5c.1-.5.1-1 .1-1.5s0-1-.1-1.5l2-1.5-2-3.5-2.4 1a7.8 7.8 0 0 0-2.6-1.5L14 2.5h-4l-.4 2.5A7.8 7.8 0 0 0 7 6.5l-2.4-1-2 3.5 2 1.5A8.6 8.6 0 0 0 4.5 12c0 .5 0 1 .1 1.5l-2 1.5 2 3.5 2.4-1a7.8 7.8 0 0 0 2.6 1.5l.4 2.5h4l.4-2.5a7.8 7.8 0 0 0 2.6-1.5l2.4 1 2-3.5-2-1.5zM12 15.5A3.5 3.5 0 1 1 12 8a3.5 3.5 0 0 1 0 7.5z"/></svg></div>
      <div><h1>WEDS Gateway Admin</h1><p class="sub">Node location and persistent configuration</p></div>
    </div>
    <a class="back" href="/">Back to dashboard</a>
  </header>
  <main>
    <section class="panel">
      <h2>Unlocated Nodes</h2>
      <p class="section-note">Nodes seen by the gateway that still need coordinates.</p>
      <table>
        <thead><tr><th>Node ID</th><th>Last Seen</th><th>Action</th></tr></thead>
        <tbody id="unlocatedRows"></tbody>
      </table>
    </section>
    <section class="panel">
      <h2>Set Location</h2>
      <form id="locationForm">
        <label for="nodeId">node_id</label>
        <input id="nodeId" name="node_id" inputmode="numeric" required>
        <label for="latitude">latitude</label>
        <input id="latitude" name="latitude" inputmode="decimal" required>
        <label for="longitude">longitude</label>
        <input id="longitude" name="longitude" inputmode="decimal" required>
        <button id="useBrowserLocation" type="button">Use Browser Location</button>
        <button type="submit">Save Location</button>
      </form>
      <div id="formMsg" class="msg"></div>
      <div class="export-box">
        <h2>Export RAM State</h2>
        <p class="section-note">Download the current in-memory registry snapshot, including node state, events and trend points.</p>
        <button id="exportState" type="button">Export Current State</button>
        <div id="exportMsg" class="msg"></div>
      </div>
      <div class="config-box">
        <h2>Persistent Config</h2>
        <p class="section-note">Clears only saved node locations. Current RAM state stays active until reboot.</p>
        <button id="clearConfig" class="danger">Clear Persistent Config</button>
        <div id="clearMsg" class="msg"></div>
      </div>
    </section>
  </main>
  <script>
    function setMsg(id,msg,ok){const el=document.getElementById(id);el.textContent=msg;el.className='msg '+(ok?'ok':'err');}
    function timeLabel(s){const n=Number(s);if(!Number.isFinite(n)||n<=0)return '-';return n>1704067200?new Date(n*1000).toLocaleString():n+'s';}
    async function refreshUnlocated(){
      try{
        const r=await fetch('/api/nodes/unlocated');if(!r.ok)throw new Error('failed to load nodes');
        const nodes=await r.json();const rows=document.getElementById('unlocatedRows');rows.innerHTML='';
        if(!nodes.length){rows.innerHTML='<tr><td colspan="3">No unlocated nodes</td></tr>';return;}
        nodes.forEach(n=>{
          const tr=document.createElement('tr');
          tr.innerHTML=`<td>${n.node_id}</td><td>${timeLabel(n.last_seen_timestamp_s)}</td><td><button>Use this node</button></td>`;
          tr.querySelector('button').onclick=()=>{document.getElementById('nodeId').value=n.node_id;};
          rows.appendChild(tr);
        });
      }catch(e){setMsg('formMsg','Load error: '+e.message,false);}
    }
    document.getElementById('locationForm').addEventListener('submit',async e=>{
      e.preventDefault();
      const body={node_id:Number(document.getElementById('nodeId').value),latitude:Number(document.getElementById('latitude').value),longitude:Number(document.getElementById('longitude').value)};
      try{
        const r=await fetch('/api/admin/setlocation',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
        const data=await r.json();if(!r.ok)throw new Error(data.message||'save failed');
        setMsg('formMsg','Location saved for node '+data.node_id,true);refreshUnlocated();
      }catch(e){setMsg('formMsg',e.message,false);}
    });
    document.getElementById('useBrowserLocation').onclick=()=>{
      if(!navigator.geolocation){
        setMsg('formMsg','Browser geolocation is not available',false);
        return;
      }
      if(!window.isSecureContext){
        setMsg('formMsg','Browser location requires HTTPS or localhost. This ESP32 page is served over HTTP, so enter coordinates manually or use a secure proxy.',false);
        return;
      }
      setMsg('formMsg','Requesting browser location...',true);
      navigator.geolocation.getCurrentPosition(
        position=>{
          document.getElementById('latitude').value=position.coords.latitude.toFixed(6);
          document.getElementById('longitude').value=position.coords.longitude.toFixed(6);
          setMsg('formMsg','Browser location copied into the form',true);
        },
        error=>{
          setMsg('formMsg','Location error: '+error.message,false);
        },
        {enableHighAccuracy:true,timeout:10000,maximumAge:30000}
      );
    };
    document.getElementById('clearConfig').onclick=async()=>{
      if(!confirm('Clear persistent node location config? RAM state will stay active.'))return;
      try{
        const r=await fetch('/api/admin/clearconfig',{method:'POST'});const data=await r.json();
        if(!r.ok)throw new Error(data.message||'clear failed');
        setMsg('clearMsg',data.message,true);refreshUnlocated();
      }catch(e){setMsg('clearMsg',e.message,false);}
    };
    document.getElementById('exportState').onclick=async()=>{
      try{
        setMsg('exportMsg','Preparing export...',true);
        const stateResponse=await fetch('/api/state/all');
        if(!stateResponse.ok)throw new Error('failed to load registry state');
        const nodes=await stateResponse.json();
        const enriched=[];
        for(const node of nodes){
          const nodeId=node.node_id;
          let events=[],trend=[];
          try{
            const er=await fetch('/api/node/events?node_id='+nodeId);
            if(er.ok){const data=await er.json();events=data.events||[];}
          }catch(e){}
          try{
            const tr=await fetch('/api/node/trend?node_id='+nodeId);
            if(tr.ok){const data=await tr.json();trend=data.points||[];}
          }catch(e){}
          enriched.push({...node,events,trend});
        }
        const exportedAt=new Date();
        const payload={exported_at:exportedAt.toISOString(),source:'WEDS Gateway RAM registry',node_count:enriched.length,nodes:enriched};
        const blob=new Blob([JSON.stringify(payload,null,2)],{type:'application/json'});
        const url=URL.createObjectURL(blob);
        const a=document.createElement('a');
        const stamp=exportedAt.toISOString().replace(/[:.]/g,'-');
        a.href=url;a.download='weds-registry-state-'+stamp+'.json';
        document.body.appendChild(a);a.click();a.remove();URL.revokeObjectURL(url);
        setMsg('exportMsg','Exported '+enriched.length+' nodes',true);
      }catch(e){setMsg('exportMsg',e.message,false);}
    };
    refreshUnlocated();
  </script>
</body>
</html>
)rawliteral";

const char* WedsGatewayWeb::indexHtml() {
    return INDEX_HTML;
}

const char* WedsGatewayWeb::adminHtml() {
    return ADMIN_HTML;
}
