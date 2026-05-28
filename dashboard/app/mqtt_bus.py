from __future__ import annotations

import json
import time
import uuid
from typing import Any

import paho.mqtt.client as mqtt

from config import MQTT_CLIENT_ID, MQTT_HOST, MQTT_PORT
from db import DashboardStore


STATE_TOPIC = "weds/#"
COMMAND_TOPIC = "weds/gateway/commands"
RESPONSE_TOPIC = "weds/gateway/command_responses"
GATEWAY_STATUS_TOPIC = "weds/gateway/status"


class MqttBus:
    def __init__(self, store: DashboardStore) -> None:
        self.store = store
        self.client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id=f"{MQTT_CLIENT_ID}-{uuid.uuid4().hex[:8]}",
        )
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message
        self.connected = False
        self.last_message_topic: str | None = None
        self.last_message_at: float | None = None
        self.last_error: str | None = None

    def start(self) -> None:
        self.client.connect_async(MQTT_HOST, MQTT_PORT, keepalive=30)
        self.client.loop_start()

    def stop(self) -> None:
        self.client.loop_stop()
        self.client.disconnect()

    def publish_command(
        self,
        method: str,
        node_id: int | None = None,
        params: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        command: dict[str, Any] = {
            "id": str(uuid.uuid4()),
            "method": method,
            "params": params or {},
        }

        if node_id is not None:
            command["node_id"] = node_id

        self.store.log_command(command)
        self.client.publish(COMMAND_TOPIC, json.dumps(command), qos=0, retain=False)
        return command

    def status(self) -> dict[str, Any]:
        socket_alive = self.client.is_connected()
        connected = self.connected and socket_alive

        if self.connected and not socket_alive:
            self.connected = False
            self.last_error = "mqtt socket is disconnected"

        return {
            "host": MQTT_HOST,
            "port": MQTT_PORT,
            "connected": connected,
            "gateway": self.store.get_gateway_status(),
            "last_message_topic": self.last_message_topic,
            "last_message_at": self.last_message_at,
            "last_error": self.last_error,
        }

    def _subscribe_topics(self) -> None:
        self.client.subscribe(STATE_TOPIC, qos=0)

    def _on_connect(
        self,
        client: mqtt.Client,
        userdata: Any,
        flags: mqtt.ConnectFlags,
        reason_code: mqtt.ReasonCode,
        properties: mqtt.Properties | None,
    ) -> None:
        if reason_code == 0:
            self.connected = True
            self.last_error = None
            self._subscribe_topics()
            return

        self.connected = False
        self.last_error = f"connect failed: {reason_code}"

    def _on_disconnect(
        self,
        client: mqtt.Client,
        userdata: Any,
        disconnect_flags: mqtt.DisconnectFlags,
        reason_code: mqtt.ReasonCode,
        properties: mqtt.Properties | None,
    ) -> None:
        self.connected = False
        self.last_error = f"disconnected: {reason_code}"

    def _on_message(
        self,
        client: mqtt.Client,
        userdata: Any,
        message: mqtt.MQTTMessage,
    ) -> None:
        try:
            payload = json.loads(message.payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self.last_error = f"invalid payload on {message.topic}"
            return

        self.last_message_topic = message.topic
        self.last_message_at = time.time()
        self.last_error = None

        if message.topic.startswith("weds/nodes/") and message.topic.endswith("/state"):
            self.store.upsert_node_state(payload)
            return

        if message.topic == RESPONSE_TOPIC:
            self.store.apply_command_response(payload)
            return

        if message.topic == GATEWAY_STATUS_TOPIC:
            self.store.apply_gateway_status(payload)
