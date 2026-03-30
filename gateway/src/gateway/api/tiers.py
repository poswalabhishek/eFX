import logging

from fastapi import APIRouter
from pydantic import BaseModel

from gateway import db

router = APIRouter()
logger = logging.getLogger("efx.api.tiers")

SEED_TIERS = [
    {"tier_id": "platinum", "label": "Platinum", "multiplier": 1.0, "sort_order": 1},
    {"tier_id": "gold", "label": "Gold", "multiplier": 1.5, "sort_order": 2},
    {"tier_id": "silver", "label": "Silver", "multiplier": 2.5, "sort_order": 3},
    {"tier_id": "bronze", "label": "Bronze", "multiplier": 4.0, "sort_order": 4},
    {"tier_id": "restricted", "label": "Restricted", "multiplier": 8.0, "sort_order": 5},
]


class TierResponse(BaseModel):
    tier_id: str
    label: str
    multiplier: float
    sort_order: int


@router.get("/", response_model=list[TierResponse])
async def list_tiers():
    try:
        rows = await db.fetch_all(
            "SELECT tier_id, label, multiplier, sort_order FROM tiers ORDER BY sort_order"
        )
        return [TierResponse(**r) for r in rows]
    except Exception:
        return [TierResponse(**t) for t in SEED_TIERS]
