from __future__ import annotations

from contextlib import asynccontextmanager
from typing import Any

from fastapi import FastAPI, HTTPException, Query, Request
from fastapi.responses import JSONResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates

from config import BASE_DIR, DB_PATH, TREND_DEFAULT_LIMIT
from db import DashboardStore
from mqtt_bus import MqttBus


store = DashboardStore(DB_PATH)
mqtt_bus = MqttBus(store)
templates = Jinja2Templates(directory=str(BASE_DIR / "templates"))


@asynccontextmanager
async def lifespan(app: FastAPI):
    mqtt_bus.start()
    yield
    mqtt_bus.stop()


app = FastAPI(title="WEDS Dashboard", lifespan=lifespan)
app.mount("/static", StaticFiles(directory=str(BASE_DIR / "static")), name="static")


@app.get("/")
async def dashboard(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})


@app.get("/admin")
async def admin(request: Request):
    return templates.TemplateResponse("admin.html", {"request": request})


@app.get("/api/state/all")
async def get_state_all():
    return store.list_nodes()


@app.get("/api/mqtt/status")
async def mqtt_status():
    return mqtt_bus.status()


@app.get("/api/gateway/status")
async def gateway_status():
    return {"mqtt": mqtt_bus.status(), "gateway": store.get_gateway_status()}


@app.get("/api/state")
async def get_state(node_id: int = Query(..., gt=0)):
    node = store.get_node(node_id)
    if node is None:
        raise HTTPException(status_code=404, detail="node_id was not found")
    return node


@app.get("/api/nodes/unlocated")
async def get_unlocated_nodes():
    return store.list_unlocated()


@app.get("/api/commands")
async def get_commands(limit: int = Query(20, ge=1, le=100)):
    return store.list_commands(limit)


@app.get("/api/node/events")
async def get_node_events(node_id: int = Query(..., gt=0)):
    return {"node_id": node_id, "events": store.list_events(node_id)}


@app.get("/api/node/trend")
async def get_node_trend(
    node_id: int = Query(..., gt=0),
    limit: int | None = Query(TREND_DEFAULT_LIMIT, ge=1),
):
    return {"node_id": node_id, "points": store.list_trend(node_id, limit)}


@app.post("/api/admin/setlocation")
async def set_location(body: dict[str, Any]):
    node_id = int(body.get("node_id") or 0)
    latitude = body.get("latitude")
    longitude = body.get("longitude")

    if node_id <= 0:
        raise HTTPException(status_code=400, detail="node_id must be non-zero")

    if latitude is None or longitude is None:
        raise HTTPException(status_code=400, detail="latitude and longitude are required")

    latitude = float(latitude)
    longitude = float(longitude)

    if latitude < -90 or latitude > 90 or longitude < -180 or longitude > 180:
        raise HTTPException(status_code=400, detail="invalid coordinates")

    command = mqtt_bus.publish_command(
        "setLocation",
        node_id=node_id,
        params={"latitude": latitude, "longitude": longitude},
    )

    return {
        "status": "queued",
        "command_id": command["id"],
        "node_id": node_id,
        "location_known": True,
        "latitude": latitude,
        "longitude": longitude,
    }


@app.post("/api/admin/clearconfig")
async def clear_config():
    command = mqtt_bus.publish_command("clearConfig")
    return {"status": "queued", "command_id": command["id"]}


@app.post("/api/admin/cleardb")
async def clear_database():
    store.clear_database()
    return {"status": "cleared", "message": "database_cleared"}


@app.get("/api/export")
async def export_snapshot():
    return JSONResponse(
        store.snapshot(),
        headers={"Content-Disposition": "attachment; filename=weds_snapshot.json"},
    )
