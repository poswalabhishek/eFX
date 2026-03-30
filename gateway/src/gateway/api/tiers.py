from fastapi import APIRouter
from pydantic import BaseModel

router = APIRouter()


class TierResponse(BaseModel):
    tier_id: str
    label: str
    multiplier: float


@router.get("/", response_model=list[TierResponse])
async def list_tiers():
    # TODO: wire to PostgreSQL
    return [
        TierResponse(tier_id="platinum", label="Platinum", multiplier=1.0),
        TierResponse(tier_id="gold", label="Gold", multiplier=1.5),
        TierResponse(tier_id="silver", label="Silver", multiplier=2.5),
        TierResponse(tier_id="bronze", label="Bronze", multiplier=4.0),
        TierResponse(tier_id="restricted", label="Restricted", multiplier=8.0),
    ]
