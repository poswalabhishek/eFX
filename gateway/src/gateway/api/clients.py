from typing import Optional

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

router = APIRouter()


class ClientResponse(BaseModel):
    client_id: str
    name: str
    client_type: str
    default_tier: str
    alpha_score: float
    toxicity: str
    is_important: bool
    is_active: bool
    notes: Optional[str] = None


class ClientTierUpdate(BaseModel):
    tier: str
    reason: Optional[str] = None


class ClientPairOverride(BaseModel):
    pair: str
    tier_override: str
    adjustment_pct: float = 0.0


@router.get("/", response_model=list[ClientResponse])
async def list_clients():
    # TODO: wire to PostgreSQL
    return []


@router.get("/{client_id}", response_model=ClientResponse)
async def get_client(client_id: str):
    raise HTTPException(status_code=404, detail="Client not found")


@router.patch("/{client_id}/tier")
async def update_client_tier(client_id: str, update: ClientTierUpdate):
    # TODO: update tier in postgres + log change
    return {"status": "updated", "client_id": client_id, "new_tier": update.tier}


@router.put("/{client_id}/overrides")
async def set_pair_override(client_id: str, override: ClientPairOverride):
    # TODO: upsert pair override in postgres
    return {"status": "updated", "client_id": client_id, "pair": override.pair}
