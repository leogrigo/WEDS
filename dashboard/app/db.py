from __future__ import annotations

import json
import sqlite3
import threading
import time
from pathlib import Path
from typing import Any

from .config import ALERT_STATE


class DashboardStore:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._lock = threading.RLock()
        self._conn = sqlite3.connect(self.path, check_same_thread=False)
        self._conn.row_factory = sqlite3.Row
        self._conn.execute("PRAGMA journal_mode=WAL")
        self._conn.execute("PRAGMA foreign_keys=ON")
        self.init_schema()

    def init_schema(self) -> None:
        with self._lock, self._conn:
            self._conn.executescript(
                """
                CREATE TABLE IF NOT EXISTS nodes (
                    node_id INTEGER PRIMARY KEY,
                    last_seen_timestamp_s INTEGER,
                    last_sequence_id INTEGER,
                    last_msg_type INTEGER,
                    temperature REAL,
                    humidity REAL,
                    pressure REAL,
                    gas_resistance REAL,
                    battery_level REAL,
                    anomaly_state INTEGER,
                    anomaly_score REAL,
                    risk_state INTEGER,
                    risk_score REAL,
                    location_known INTEGER,
                    latitude REAL,
                    longitude REAL,
                    pending_alert_mode INTEGER,
                    streak_open INTEGER,
                    trend_count INTEGER DEFAULT 0,
                    event_count INTEGER DEFAULT 0,
                    updated_at REAL
                );

                CREATE TABLE IF NOT EXISTS telemetry (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    node_id INTEGER NOT NULL,
                    timestamp_s INTEGER,
                    temperature REAL,
                    humidity REAL,
                    pressure REAL,
                    gas_resistance REAL,
                    battery_level REAL,
                    anomaly_score REAL,
                    risk_score REAL,
                    created_at REAL
                );

                CREATE INDEX IF NOT EXISTS idx_telemetry_node_time
                    ON telemetry(node_id, timestamp_s, id);

                CREATE TABLE IF NOT EXISTS events (
                    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    node_id INTEGER NOT NULL,
                    type_label TEXT NOT NULL,
                    start_timestamp_s INTEGER,
                    end_timestamp_s INTEGER,
                    still_open INTEGER NOT NULL,
                    peak_anomaly_score REAL,
                    peak_risk_score REAL,
                    max_temperature REAL,
                    min_humidity REAL,
                    min_gas_resistance REAL,
                    sample_count INTEGER NOT NULL
                );

                CREATE INDEX IF NOT EXISTS idx_events_node_open
                    ON events(node_id, still_open, event_id);

                CREATE TABLE IF NOT EXISTS command_log (
                    id TEXT PRIMARY KEY,
                    method TEXT NOT NULL,
                    node_id INTEGER,
                    params_json TEXT,
                    success INTEGER,
                    message TEXT,
                    created_at REAL,
                    responded_at REAL
                );

                CREATE TABLE IF NOT EXISTS gateway_status (
                    id INTEGER PRIMARY KEY CHECK (id = 1),
                    gateway_id TEXT,
                    online INTEGER,
                    ip TEXT,
                    known_nodes INTEGER,
                    uptime_ms INTEGER,
                    firmware TEXT,
                    published_at_s INTEGER,
                    updated_at REAL
                );
                """
            )
            self._drop_legacy_command_response_flags()

    def _drop_legacy_command_response_flags(self) -> None:
        columns = {
            row["name"]
            for row in self._conn.execute("PRAGMA table_info(command_log)").fetchall()
        }
        legacy_columns = ("accepted", "delivered", "pending")
        if not any(column in columns for column in legacy_columns):
            return

        rows = self._conn.execute(
            """
            SELECT id, method, node_id, params_json, success, message, created_at, responded_at
            FROM command_log
            """
        ).fetchall()
        self._conn.executescript(
            """
            ALTER TABLE command_log RENAME TO command_log_old;
            CREATE TABLE command_log (
                id TEXT PRIMARY KEY,
                method TEXT NOT NULL,
                node_id INTEGER,
                params_json TEXT,
                success INTEGER,
                message TEXT,
                created_at REAL,
                responded_at REAL
            );
            DROP TABLE command_log_old;
            """
        )
        self._conn.executemany(
            """
            INSERT INTO command_log (
                id, method, node_id, params_json, success, message, created_at, responded_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            """,
            [
                (
                    row["id"],
                    row["method"],
                    row["node_id"],
                    row["params_json"],
                    row["success"],
                    row["message"],
                    row["created_at"],
                    row["responded_at"],
                )
                for row in rows
            ],
        )

    def upsert_node_state(self, state: dict[str, Any]) -> None:
        node_id = int(state["node_id"])
        timestamp_s = int(state.get("timestamp_s") or 0)
        now = time.time()

        with self._lock, self._conn:
            previous = self._conn.execute(
                """
                SELECT last_sequence_id, last_msg_type
                FROM nodes
                WHERE node_id = ?
                """,
                (node_id,),
            ).fetchone()
            duplicate_sample = (
                previous is not None and
                previous["last_sequence_id"] == state.get("last_sequence_id") and
                previous["last_msg_type"] == state.get("last_msg_type")
            )

            if not duplicate_sample:
                self._conn.execute(
                    """
                    INSERT INTO telemetry (
                        node_id, timestamp_s, temperature, humidity, pressure,
                        gas_resistance, battery_level, anomaly_score, risk_score,
                        created_at
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        node_id,
                        timestamp_s,
                        state.get("temperature"),
                        state.get("humidity"),
                        state.get("pressure"),
                        state.get("gas_resistance"),
                        state.get("battery_level"),
                        state.get("anomaly_score"),
                        state.get("risk_score"),
                        now,
                    ),
                )
                self._apply_event_state(node_id, timestamp_s, state)

            counts = self._conn.execute(
                """
                SELECT
                    (SELECT COUNT(*) FROM telemetry WHERE node_id = ?) AS trend_count,
                    (SELECT COUNT(*) FROM events WHERE node_id = ?) AS event_count
                """,
                (node_id, node_id),
            ).fetchone()

            self._conn.execute(
                """
                INSERT INTO nodes (
                    node_id, last_seen_timestamp_s, last_sequence_id, last_msg_type,
                    temperature, humidity, pressure, gas_resistance, battery_level,
                    anomaly_state, anomaly_score, risk_state, risk_score,
                    location_known, latitude, longitude, pending_alert_mode,
                    streak_open, trend_count, event_count, updated_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                ON CONFLICT(node_id) DO UPDATE SET
                    last_seen_timestamp_s = excluded.last_seen_timestamp_s,
                    last_sequence_id = excluded.last_sequence_id,
                    last_msg_type = excluded.last_msg_type,
                    temperature = excluded.temperature,
                    humidity = excluded.humidity,
                    pressure = excluded.pressure,
                    gas_resistance = excluded.gas_resistance,
                    battery_level = excluded.battery_level,
                    anomaly_state = excluded.anomaly_state,
                    anomaly_score = excluded.anomaly_score,
                    risk_state = excluded.risk_state,
                    risk_score = excluded.risk_score,
                    location_known = excluded.location_known,
                    latitude = excluded.latitude,
                    longitude = excluded.longitude,
                    pending_alert_mode = excluded.pending_alert_mode,
                    streak_open = excluded.streak_open,
                    trend_count = excluded.trend_count,
                    event_count = excluded.event_count,
                    updated_at = excluded.updated_at
                """,
                (
                    node_id,
                    timestamp_s,
                    state.get("last_sequence_id"),
                    state.get("last_msg_type"),
                    state.get("temperature"),
                    state.get("humidity"),
                    state.get("pressure"),
                    state.get("gas_resistance"),
                    state.get("battery_level"),
                    state.get("anomaly_state"),
                    state.get("anomaly_score"),
                    state.get("risk_state"),
                    state.get("risk_score"),
                    int(bool(state.get("location_known"))),
                    state.get("latitude"),
                    state.get("longitude"),
                    int(bool(state.get("pending_alert_mode"))),
                    int(self._is_alert(state)),
                    counts["trend_count"],
                    counts["event_count"],
                    now,
                ),
            )

    def list_nodes(self) -> list[dict[str, Any]]:
        with self._lock:
            rows = self._conn.execute(
                "SELECT * FROM nodes ORDER BY node_id"
            ).fetchall()
        return [self._node_dict(row) for row in rows]

    def get_node(self, node_id: int) -> dict[str, Any] | None:
        with self._lock:
            row = self._conn.execute(
                "SELECT * FROM nodes WHERE node_id = ?",
                (node_id,),
            ).fetchone()
        return self._node_dict(row) if row else None

    def list_unlocated(self) -> list[dict[str, Any]]:
        with self._lock:
            rows = self._conn.execute(
                """
                SELECT node_id, last_seen_timestamp_s, updated_at
                FROM nodes
                WHERE location_known = 0
                ORDER BY node_id
                """
            ).fetchall()
        return [dict(row) for row in rows]

    def list_events(self, node_id: int) -> list[dict[str, Any]]:
        with self._lock:
            rows = self._conn.execute(
                """
                SELECT * FROM events
                WHERE node_id = ?
                ORDER BY still_open DESC, event_id DESC
                """,
                (node_id,),
            ).fetchall()
        return [self._event_dict(row) for row in rows]

    def list_trend(self, node_id: int, limit: int | None = None) -> list[dict[str, Any]]:
        query = """
            SELECT timestamp_s, temperature, humidity, pressure, gas_resistance,
                   battery_level, anomaly_score, risk_score
            FROM telemetry
            WHERE node_id = ?
            ORDER BY id DESC
        """
        params: tuple[Any, ...]
        if limit:
            query += " LIMIT ?"
            params = (node_id, limit)
        else:
            params = (node_id,)

        with self._lock:
            rows = self._conn.execute(query, params).fetchall()
        return [dict(row) for row in reversed(rows)]

    def set_location_local(self, node_id: int, latitude: float, longitude: float) -> None:
        with self._lock, self._conn:
            self._conn.execute(
                """
                UPDATE nodes
                SET location_known = 1, latitude = ?, longitude = ?, updated_at = ?
                WHERE node_id = ?
                """,
                (latitude, longitude, time.time(), node_id),
            )

    def clear_locations_local(self) -> None:
        with self._lock, self._conn:
            self._conn.execute(
                """
                UPDATE nodes
                SET location_known = 0, latitude = 0, longitude = 0, updated_at = ?
                """,
                (time.time(),),
            )

    def log_command(self, command: dict[str, Any]) -> None:
        with self._lock, self._conn:
            self._conn.execute(
                """
                INSERT OR REPLACE INTO command_log (
                    id, method, node_id, params_json, created_at
                ) VALUES (?, ?, ?, ?, ?)
                """,
                (
                    command["id"],
                    command["method"],
                    command.get("node_id"),
                    json.dumps(command.get("params") or {}),
                    time.time(),
                ),
            )

    def apply_command_response(self, response: dict[str, Any]) -> None:
        with self._lock, self._conn:
            command = self._conn.execute(
                "SELECT method, node_id, params_json FROM command_log WHERE id = ?",
                (response.get("id"),),
            ).fetchone()
            self._conn.execute(
                """
                UPDATE command_log
                SET success = ?, message = ?, responded_at = ?
                WHERE id = ?
                """,
                (
                    int(bool(response.get("success"))),
                    response.get("message"),
                    time.time(),
                    response.get("id"),
                ),
            )
            if bool(response.get("success")) and command is not None:
                self._apply_successful_command_side_effect(command)

    def _apply_successful_command_side_effect(self, command: sqlite3.Row) -> None:
        method = command["method"]
        if method == "setLocation":
            try:
                params = json.loads(command["params_json"] or "{}")
            except json.JSONDecodeError:
                return

            node_id = command["node_id"]
            latitude = params.get("latitude")
            longitude = params.get("longitude")
            if node_id is None or latitude is None or longitude is None:
                return

            self._conn.execute(
                """
                UPDATE nodes
                SET location_known = 1, latitude = ?, longitude = ?, updated_at = ?
                WHERE node_id = ?
                """,
                (float(latitude), float(longitude), time.time(), int(node_id)),
            )
            return

        if method == "clearConfig":
            self._conn.execute(
                """
                UPDATE nodes
                SET location_known = 0, latitude = 0, longitude = 0, updated_at = ?
                """,
                (time.time(),),
            )

    def apply_gateway_status(self, status: dict[str, Any]) -> None:
        with self._lock, self._conn:
            self._conn.execute(
                """
                INSERT INTO gateway_status (
                    id, gateway_id, online, ip, known_nodes, uptime_ms,
                    firmware, published_at_s, updated_at
                ) VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?)
                ON CONFLICT(id) DO UPDATE SET
                    gateway_id = excluded.gateway_id,
                    online = excluded.online,
                    ip = excluded.ip,
                    known_nodes = excluded.known_nodes,
                    uptime_ms = excluded.uptime_ms,
                    firmware = excluded.firmware,
                    published_at_s = excluded.published_at_s,
                    updated_at = excluded.updated_at
                """,
                (
                    status.get("gateway_id"),
                    int(bool(status.get("online"))),
                    status.get("ip"),
                    status.get("known_nodes"),
                    status.get("uptime_ms"),
                    status.get("firmware"),
                    status.get("published_at_s"),
                    time.time(),
                ),
            )

    def get_gateway_status(self) -> dict[str, Any] | None:
        with self._lock:
            row = self._conn.execute(
                "SELECT * FROM gateway_status WHERE id = 1"
            ).fetchone()

        if row is None:
            return None

        item = dict(row)
        item["online"] = bool(item["online"])
        return item

    def list_commands(self, limit: int = 20) -> list[dict[str, Any]]:
        with self._lock:
            rows = self._conn.execute(
                """
                SELECT * FROM command_log
                ORDER BY created_at DESC
                LIMIT ?
                """,
                (limit,),
            ).fetchall()

        commands: list[dict[str, Any]] = []
        now = time.time()
        for row in rows:
            item = dict(row)
            item["success"] = None if item["success"] is None else bool(item["success"])
            try:
                item["params"] = json.loads(item.pop("params_json") or "{}")
            except json.JSONDecodeError:
                item["params"] = {}

            if item["responded_at"] is None:
                item["status"] = "timeout" if now - item["created_at"] > 15 else "queued"
            elif item["success"] is False:
                item["status"] = "failed"
            else:
                item["status"] = "success"

            commands.append(item)

        return commands

    def clear_database(self) -> None:
        with self._lock, self._conn:
            self._conn.execute("DELETE FROM command_log")
            self._conn.execute("DELETE FROM events")
            self._conn.execute("DELETE FROM telemetry")
            self._conn.execute("DELETE FROM nodes")
            self._conn.execute("DELETE FROM gateway_status")
            self._conn.execute(
                """
                DELETE FROM sqlite_sequence
                WHERE name IN ('telemetry', 'events')
                """
            )

    def snapshot(self) -> dict[str, Any]:
        nodes = self.list_nodes()
        return {
            "gateway": self.get_gateway_status(),
            "nodes": [
                {
                    **node,
                    "events": self.list_events(node["node_id"]),
                    "trend": self.list_trend(node["node_id"]),
                }
                for node in nodes
            ],
            "commands": self.list_commands(100),
        }

    def _apply_event_state(self, node_id: int, timestamp_s: int, state: dict[str, Any]) -> None:
        is_alert = self._is_alert(state)
        open_event = self._conn.execute(
            """
            SELECT * FROM events
            WHERE node_id = ? AND still_open = 1
            ORDER BY event_id DESC
            LIMIT 1
            """,
            (node_id,),
        ).fetchone()

        if is_alert and open_event is None:
            self._conn.execute(
                """
                INSERT INTO events (
                    node_id, type_label, start_timestamp_s, end_timestamp_s,
                    still_open, peak_anomaly_score, peak_risk_score,
                    max_temperature, min_humidity, min_gas_resistance,
                    sample_count
                ) VALUES (?, ?, ?, ?, 1, ?, ?, ?, ?, ?, 1)
                """,
                (
                    node_id,
                    self._event_type(state),
                    timestamp_s,
                    timestamp_s,
                    state.get("anomaly_score"),
                    state.get("risk_score"),
                    state.get("temperature"),
                    state.get("humidity"),
                    state.get("gas_resistance"),
                ),
            )
            return

        if is_alert and open_event is not None:
            next_type = self._event_type(state)
            if open_event["type_label"] != next_type:
                next_type = "BOTH_ALERT"

            self._conn.execute(
                """
                UPDATE events
                SET type_label = ?,
                    end_timestamp_s = ?,
                    peak_anomaly_score = MAX(peak_anomaly_score, ?),
                    peak_risk_score = MAX(peak_risk_score, ?),
                    max_temperature = MAX(max_temperature, ?),
                    min_humidity = MIN(min_humidity, ?),
                    min_gas_resistance = MIN(min_gas_resistance, ?),
                    sample_count = sample_count + 1
                WHERE event_id = ?
                """,
                (
                    next_type,
                    timestamp_s,
                    state.get("anomaly_score"),
                    state.get("risk_score"),
                    state.get("temperature"),
                    state.get("humidity"),
                    state.get("gas_resistance"),
                    open_event["event_id"],
                ),
            )
            return

        if not is_alert and open_event is not None:
            self._conn.execute(
                """
                UPDATE events
                SET still_open = 0, end_timestamp_s = ?
                WHERE event_id = ?
                """,
                (timestamp_s, open_event["event_id"]),
            )

    def _is_alert(self, state: dict[str, Any]) -> bool:
        return (
            int(state.get("anomaly_state") or 0) == ALERT_STATE or
            int(state.get("risk_state") or 0) == ALERT_STATE
        )

    def _event_type(self, state: dict[str, Any]) -> str:
        anomaly = int(state.get("anomaly_state") or 0) == ALERT_STATE
        risk = int(state.get("risk_state") or 0) == ALERT_STATE

        if anomaly and risk:
            return "BOTH_ALERT"
        if anomaly:
            return "ANOMALY_ALERT"
        if risk:
            return "RISK_ALERT"
        return "NONE"

    def _node_dict(self, row: sqlite3.Row) -> dict[str, Any]:
        item = dict(row)
        for key in ("location_known", "pending_alert_mode", "streak_open"):
            item[key] = bool(item[key])
        item["anomaly_state_label"] = "ALERT" if item.get("anomaly_state") == ALERT_STATE else "NORMAL"
        item["risk_state_label"] = "ALERT" if item.get("risk_state") == ALERT_STATE else "NORMAL"
        return item

    def _event_dict(self, row: sqlite3.Row) -> dict[str, Any]:
        item = dict(row)
        item["still_open"] = bool(item["still_open"])
        return item
