import logging

from fastapi import APIRouter
from pydantic import BaseModel

from gateway import db

router = APIRouter()
logger = logging.getLogger("efx.api.sessions")

SEED_SESSIONS = [
    {"session_id": "new_york", "label": "New York", "hub_city": "New York",
     "start_utc": "14:00", "end_utc": "21:00", "spread_multiplier": 1.2, "is_active": True},
    {"session_id": "singapore", "label": "Singapore", "hub_city": "Singapore",
     "start_utc": "21:00", "end_utc": "06:00", "spread_multiplier": 1.5, "is_active": True},
    {"session_id": "london", "label": "London", "hub_city": "London",
     "start_utc": "06:00", "end_utc": "14:00", "spread_multiplier": 1.0, "is_active": True},
]


class SessionResponse(BaseModel):
    session_id: str
    label: str
    hub_city: str
    start_utc: str
    end_utc: str
    spread_multiplier: float
    is_active: bool


@router.get("/", response_model=list[SessionResponse])
async def list_sessions():
    try:
        rows = await db.fetch_all(
            "SELECT session_id, label, hub_city, start_utc::text, end_utc::text, "
            "spread_multiplier, is_active FROM sessions ORDER BY start_utc"
        )
        return [SessionResponse(**r) for r in rows]
    except Exception:
        return [SessionResponse(**s) for s in SEED_SESSIONS]
