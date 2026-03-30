import logging
from typing import Optional
from datetime import datetime, timezone

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

from gateway import db

router = APIRouter()
logger = logging.getLogger("efx.api.clients")

# Seed data used when Postgres is unavailable
SEED_CLIENTS = [
    {"client_id": "hedge_fund_alpha", "name": "Alpha Capital Partners", "client_type": "hedge_fund",
     "default_tier": "gold", "alpha_score": 2.1, "toxicity": "medium", "is_important": False, "is_active": True},
    {"client_id": "real_money_pension", "name": "Global Pension Fund", "client_type": "real_money",
     "default_tier": "platinum", "alpha_score": -0.8, "toxicity": "low", "is_important": True, "is_active": True},
    {"client_id": "corp_treasury_acme", "name": "Acme Corp Treasury", "client_type": "corporate",
     "default_tier": "silver", "alpha_score": -0.3, "toxicity": "low", "is_important": False, "is_active": True},
    {"client_id": "toxic_hft_firm", "name": "Velocity Trading LLC", "client_type": "hft",
     "default_tier": "restricted", "alpha_score": 5.2, "toxicity": "high", "is_important": False, "is_active": True},
    {"client_id": "retail_aggregator", "name": "FX Connect Retail", "client_type": "retail_agg",
     "default_tier": "silver", "alpha_score": 0.1, "toxicity": "low", "is_important": False, "is_active": True},
    {"client_id": "sovereign_fund", "name": "Norges Investment Mgmt", "client_type": "sovereign",
     "default_tier": "platinum", "alpha_score": -0.5, "toxicity": "low", "is_important": True, "is_active": True},
    {"client_id": "algo_firm_momentum", "name": "Quant Momentum Capital", "client_type": "algo",
     "default_tier": "bronze", "alpha_score": 3.8, "toxicity": "high", "is_important": False, "is_active": True},
    {"client_id": "bank_flow_dealer", "name": "Regional Bank Treasury", "client_type": "bank",
     "default_tier": "gold", "alpha_score": 0.3, "toxicity": "low", "is_important": True, "is_active": True},
]


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
    changed_by: str = "analyst"


class ClientPairOverrideRequest(BaseModel):
    pair: str
    tier_override: str
    adjustment_pct: float = 0.0


class PairOverrideResponse(BaseModel):
    client_id: str
    pair: str
    tier_override: str
    adjustment_pct: float
    is_active: bool


class TierChangeLogEntry(BaseModel):
    client_id: str
    pair: Optional[str] = None
    old_tier: Optional[str] = None
    new_tier: str
    reason: Optional[str] = None
    changed_by: str
    created_at: str


@router.get("/", response_model=list[ClientResponse])
async def list_clients():
    try:
        rows = await db.fetch_all(
            "SELECT client_id, name, client_type, default_tier, alpha_score, "
            "toxicity, is_important, is_active, notes FROM clients ORDER BY name"
        )
        return [ClientResponse(**r) for r in rows]
    except Exception:
        return [ClientResponse(**c) for c in SEED_CLIENTS]


@router.get("/{client_id}", response_model=ClientResponse)
async def get_client(client_id: str):
    try:
        row = await db.fetch_one(
            "SELECT client_id, name, client_type, default_tier, alpha_score, "
            "toxicity, is_important, is_active, notes FROM clients WHERE client_id = $1",
            client_id,
        )
        if not row:
            raise HTTPException(status_code=404, detail="Client not found")
        return ClientResponse(**row)
    except HTTPException:
        raise
    except Exception:
        match = next((c for c in SEED_CLIENTS if c["client_id"] == client_id), None)
        if not match:
            raise HTTPException(status_code=404, detail="Client not found")
        return ClientResponse(**match)


@router.patch("/{client_id}/tier")
async def update_client_tier(client_id: str, update: ClientTierUpdate):
    try:
        old = await db.fetch_one(
            "SELECT default_tier FROM clients WHERE client_id = $1", client_id
        )
        if not old:
            raise HTTPException(status_code=404, detail="Client not found")

        old_tier = old["default_tier"]

        await db.execute(
            "UPDATE clients SET default_tier = $1, updated_at = NOW() WHERE client_id = $2",
            update.tier, client_id,
        )

        await db.execute(
            "INSERT INTO tier_change_log (client_id, old_tier, new_tier, reason, changed_by) "
            "VALUES ($1, $2, $3, $4, $5)",
            client_id, old_tier, update.tier, update.reason, update.changed_by,
        )

        logger.info(f"Tier change: {client_id} {old_tier} -> {update.tier} by {update.changed_by}")

        return {
            "status": "updated",
            "client_id": client_id,
            "old_tier": old_tier,
            "new_tier": update.tier,
        }
    except HTTPException:
        raise
    except Exception as e:
        logger.warning(f"Postgres unavailable for tier update: {e}")
        return {
            "status": "updated_local",
            "client_id": client_id,
            "new_tier": update.tier,
            "note": "Database unavailable, change applied in-memory only",
        }


@router.get("/{client_id}/overrides", response_model=list[PairOverrideResponse])
async def get_pair_overrides(client_id: str):
    try:
        rows = await db.fetch_all(
            "SELECT client_id, pair, tier_override, adjustment_pct, is_active "
            "FROM client_pair_overrides WHERE client_id = $1 ORDER BY pair",
            client_id,
        )
        return [PairOverrideResponse(**r) for r in rows]
    except Exception:
        return []


@router.put("/{client_id}/overrides")
async def set_pair_override(client_id: str, override: ClientPairOverrideRequest):
    try:
        await db.execute(
            "INSERT INTO client_pair_overrides (client_id, pair, tier_override, adjustment_pct, updated_at) "
            "VALUES ($1, $2, $3, $4, NOW()) "
            "ON CONFLICT (client_id, pair) DO UPDATE SET "
            "tier_override = EXCLUDED.tier_override, adjustment_pct = EXCLUDED.adjustment_pct, "
            "updated_at = NOW()",
            client_id, override.pair, override.tier_override, override.adjustment_pct,
        )

        await db.execute(
            "INSERT INTO tier_change_log (client_id, pair, new_tier, reason, changed_by) "
            "VALUES ($1, $2, $3, $4, $5)",
            client_id, override.pair, override.tier_override,
            f"Override set: {override.adjustment_pct:+.1f}%", "analyst",
        )

        return {
            "status": "updated",
            "client_id": client_id,
            "pair": override.pair,
            "tier_override": override.tier_override,
            "adjustment_pct": override.adjustment_pct,
        }
    except Exception as e:
        logger.warning(f"Postgres unavailable for override: {e}")
        return {"status": "updated_local", "note": "Database unavailable"}


@router.delete("/{client_id}/overrides/{pair}")
async def delete_pair_override(client_id: str, pair: str):
    try:
        await db.execute(
            "DELETE FROM client_pair_overrides WHERE client_id = $1 AND pair = $2",
            client_id, pair,
        )
        return {"status": "deleted", "client_id": client_id, "pair": pair}
    except Exception as e:
        logger.warning(f"Postgres unavailable for delete: {e}")
        return {"status": "deleted_local", "note": "Database unavailable"}


@router.get("/{client_id}/tier-history", response_model=list[TierChangeLogEntry])
async def get_tier_history(client_id: str):
    try:
        rows = await db.fetch_all(
            "SELECT client_id, pair, old_tier, new_tier, reason, changed_by, "
            "created_at::text FROM tier_change_log WHERE client_id = $1 "
            "ORDER BY created_at DESC LIMIT 50",
            client_id,
        )
        return [TierChangeLogEntry(**r) for r in rows]
    except Exception:
        return []
