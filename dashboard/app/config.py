from __future__ import annotations

import os
from pathlib import Path


BASE_DIR = Path(__file__).resolve().parents[1]

MQTT_HOST = os.getenv("WEDS_MQTT_HOST", "localhost")
MQTT_PORT = int(os.getenv("WEDS_MQTT_PORT", "1883"))
MQTT_CLIENT_ID = os.getenv("WEDS_MQTT_CLIENT_ID", "weds-dashboard")
DB_PATH = Path(os.getenv("WEDS_DB_PATH", str(BASE_DIR / "weds_dashboard.sqlite3")))

ALERT_STATE = 1
TREND_DEFAULT_LIMIT = 60
