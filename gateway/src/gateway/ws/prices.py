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


@router.websocket("/prices")
async def price_stream(websocket: WebSocket):
    """Stream all dashboard data: prices, PnL, positions, venues, alerts."""
    await websocket.accept()
    logger.info(f"Dashboard client connected. Bridge: {_bridge is not None}")
    try:
        while True:
            if _bridge and _bridge.latest_fair_values:
                snapshot = {
                    "type": "dashboard",
                    "timestamp": time.time(),
                    "prices": _bridge.latest_fair_values,
                    "client_prices": _bridge.latest_client_prices,
                    "pnl": _bridge.state.get_pnl_snapshot(),
                    "positions": _bridge.state.get_positions(),
                    "venues": _bridge.state.get_venue_stats(),
                    "alerts": _bridge.state.get_recent_alerts(10),
                    "fills": _bridge.state.get_recent_fills(15),
                    "engine_connected": True,
                    "manual_mode": _bridge.manual_mode,
                }
                await websocket.send_json(snapshot)
            else:
                await websocket.send_json({
                    "type": "heartbeat",
                    "timestamp": time.time(),
                    "engine_connected": _bridge is not None and _bridge.message_count > 0,
                    "manual_mode": _bridge.manual_mode if _bridge else False,
                })
            await asyncio.sleep(0.1)
    except WebSocketDisconnect:
        logger.info("Dashboard client disconnected")
    except Exception as e:
        logger.error(f"WS error: {e}")


@router.websocket("/pnl")
async def pnl_stream(websocket: WebSocket):
    await websocket.accept()
    try:
        while True:
            if _bridge:
                await websocket.send_json(_bridge.state.get_pnl_snapshot())
            await asyncio.sleep(0.5)
    except WebSocketDisconnect:
        pass


@router.websocket("/alerts")
async def alert_stream(websocket: WebSocket):
    await websocket.accept()
    try:
        while True:
            if _bridge:
                await websocket.send_json({
                    "type": "alerts",
                    "alerts": _bridge.state.get_recent_alerts(20),
                    "timestamp": time.time(),
                })
            await asyncio.sleep(2)
    except WebSocketDisconnect:
        pass
