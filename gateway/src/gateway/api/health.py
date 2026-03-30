from fastapi import APIRouter

router = APIRouter()

_bridge = None


def set_health_bridge(bridge):
    global _bridge
    _bridge = bridge


@router.get("/health")
async def health_check():
    engine_connected = False
    zmq_messages = 0
    pairs_count = 0

    if _bridge:
        zmq_messages = _bridge.message_count
        engine_connected = zmq_messages > 0
        pairs_count = len(_bridge.latest_fair_values)

    return {
        "status": "ok",
        "service": "efx-gateway",
        "version": "0.2.0",
        "engine_connected": engine_connected,
        "zmq_messages_received": zmq_messages,
        "active_pairs": pairs_count,
    }
