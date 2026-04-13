"""Simulation mode controls for manual market impact testing."""

from fastapi import APIRouter
from pydantic import BaseModel

router = APIRouter()

_bridge = None


def set_simulation_bridge(bridge):
    global _bridge
    _bridge = bridge


class SimulationModeRequest(BaseModel):
    enabled: bool


class SimulationModeResponse(BaseModel):
    manual_mode: bool


@router.get("/status", response_model=SimulationModeResponse)
async def get_status():
    return SimulationModeResponse(
        manual_mode=bool(_bridge and _bridge.manual_mode),
    )


@router.post("/manual-mode", response_model=SimulationModeResponse)
async def set_manual_mode(req: SimulationModeRequest):
    if _bridge:
        _bridge.set_manual_mode(req.enabled)
    return SimulationModeResponse(
        manual_mode=bool(_bridge and _bridge.manual_mode),
    )
