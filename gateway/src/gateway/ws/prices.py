import asyncio
import json
import time

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

router = APIRouter()


class ConnectionManager:
    def __init__(self):
        self.active_connections: list[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)

    async def broadcast(self, message: dict):
        data = json.dumps(message)
        for connection in self.active_connections:
            try:
                await connection.send_text(data)
            except Exception:
                pass


manager = ConnectionManager()


@router.websocket("/prices")
async def price_stream(websocket: WebSocket):
    """Stream real-time price updates to connected clients.

    In production this will subscribe to ZMQ from the C++ engine.
    For now, sends simulated price data for development.
    """
    await manager.connect(websocket)
    try:
        while True:
            # TODO: replace with ZMQ subscriber to C++ engine
            # For now, send a heartbeat
            await websocket.send_json({
                "type": "heartbeat",
                "timestamp": time.time(),
            })
            await asyncio.sleep(1)
    except WebSocketDisconnect:
        manager.disconnect(websocket)


@router.websocket("/pnl")
async def pnl_stream(websocket: WebSocket):
    """Stream real-time PnL updates."""
    await manager.connect(websocket)
    try:
        while True:
            await websocket.send_json({
                "type": "heartbeat",
                "timestamp": time.time(),
            })
            await asyncio.sleep(1)
    except WebSocketDisconnect:
        manager.disconnect(websocket)


@router.websocket("/alerts")
async def alert_stream(websocket: WebSocket):
    """Stream real-time alerts."""
    await manager.connect(websocket)
    try:
        while True:
            await websocket.send_json({
                "type": "heartbeat",
                "timestamp": time.time(),
            })
            await asyncio.sleep(5)
    except WebSocketDisconnect:
        manager.disconnect(websocket)
