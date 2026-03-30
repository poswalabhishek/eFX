import asyncio
import json
import time
import logging

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from gateway.bridge.zmq_subscriber import ZmqBridge

router = APIRouter()
logger = logging.getLogger("efx.ws")

_bridge: ZmqBridge | None = None


def set_bridge(bridge: ZmqBridge):
    global _bridge
    _bridge = bridge


class ConnectionManager:
    def __init__(self):
        self.price_connections: list[WebSocket] = []
        self.pnl_connections: list[WebSocket] = []
        self.alert_connections: list[WebSocket] = []

    async def connect_prices(self, ws: WebSocket):
        await ws.accept()
        self.price_connections.append(ws)
        logger.info(f"Price client connected. Total: {len(self.price_connections)}")

    async def connect_pnl(self, ws: WebSocket):
        await ws.accept()
        self.pnl_connections.append(ws)

    async def connect_alerts(self, ws: WebSocket):
        await ws.accept()
        self.alert_connections.append(ws)

    def disconnect_prices(self, ws: WebSocket):
        if ws in self.price_connections:
            self.price_connections.remove(ws)

    def disconnect_pnl(self, ws: WebSocket):
        if ws in self.pnl_connections:
            self.pnl_connections.remove(ws)

    def disconnect_alerts(self, ws: WebSocket):
        if ws in self.alert_connections:
            self.alert_connections.remove(ws)

    async def broadcast_prices(self, data: dict):
        msg = json.dumps(data)
        dead = []
        for ws in self.price_connections:
            try:
                await ws.send_text(msg)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.disconnect_prices(ws)

    async def broadcast_pnl(self, data: dict):
        msg = json.dumps(data)
        dead = []
        for ws in self.pnl_connections:
            try:
                await ws.send_text(msg)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.disconnect_pnl(ws)


manager = ConnectionManager()


@router.websocket("/prices")
async def price_stream(websocket: WebSocket):
    """Stream real-time fair value prices from the C++ engine via ZMQ."""
    await manager.connect_prices(websocket)
    try:
        while True:
            if _bridge and _bridge.latest_fair_values:
                snapshot = {
                    "type": "prices",
                    "timestamp": time.time(),
                    "pairs": _bridge.latest_fair_values,
                }
                await websocket.send_json(snapshot)
            else:
                await websocket.send_json({
                    "type": "heartbeat",
                    "timestamp": time.time(),
                    "engine_connected": _bridge is not None and _bridge.message_count > 0,
                })
            await asyncio.sleep(0.1)  # 10 Hz to frontend
    except WebSocketDisconnect:
        manager.disconnect_prices(websocket)


@router.websocket("/pnl")
async def pnl_stream(websocket: WebSocket):
    """Stream real-time PnL updates."""
    await manager.connect_pnl(websocket)
    try:
        while True:
            await websocket.send_json({
                "type": "heartbeat",
                "timestamp": time.time(),
            })
            await asyncio.sleep(1)
    except WebSocketDisconnect:
        manager.disconnect_pnl(websocket)


@router.websocket("/alerts")
async def alert_stream(websocket: WebSocket):
    """Stream real-time alerts."""
    await manager.connect_alerts(websocket)
    try:
        while True:
            await websocket.send_json({
                "type": "heartbeat",
                "timestamp": time.time(),
            })
            await asyncio.sleep(5)
    except WebSocketDisconnect:
        manager.disconnect_alerts(websocket)
