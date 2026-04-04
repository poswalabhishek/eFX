"""REST endpoints for live pricing data from the engine."""

import logging

from fastapi import APIRouter
from pydantic import BaseModel

router = APIRouter()
logger = logging.getLogger("efx.api.pricing")

_bridge = None


def set_pricing_bridge(bridge):
    global _bridge
    _bridge = bridge


class ClientPriceResponse(BaseModel):
    client_id: str
    pair: str
    bid: float
    ask: float
    spread_bps: float
    tier: str


@router.get("/client-prices/{pair}", response_model=list[ClientPriceResponse])
async def get_client_prices_for_pair(pair: str):
    """Get latest per-client prices for a currency pair."""
    if not _bridge or pair not in _bridge.latest_client_prices:
        return []
    return [
        ClientPriceResponse(**{
            "client_id": data.get("client_id", ""),
            "pair": data.get("pair", pair),
            "bid": data.get("bid", 0),
            "ask": data.get("ask", 0),
            "spread_bps": data.get("spread_bps", 0),
            "tier": data.get("tier", ""),
        })
        for data in _bridge.latest_client_prices[pair].values()
    ]


@router.get("/client-prices", response_model=dict[str, list[ClientPriceResponse]])
async def get_all_client_prices():
    """Get latest per-client prices for all pairs."""
    if not _bridge:
        return {}
    result = {}
    for pair, clients in _bridge.latest_client_prices.items():
        result[pair] = [
            ClientPriceResponse(**{
                "client_id": data.get("client_id", ""),
                "pair": data.get("pair", pair),
                "bid": data.get("bid", 0),
                "ask": data.get("ask", 0),
                "spread_bps": data.get("spread_bps", 0),
                "tier": data.get("tier", ""),
            })
            for data in clients.values()
        ]
    return result


@router.get("/fair-values")
async def get_fair_values():
    """Get latest fair values for all pairs."""
    if not _bridge:
        return {}
    return _bridge.latest_fair_values
