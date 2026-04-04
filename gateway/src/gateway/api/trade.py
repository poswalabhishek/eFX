"""Trade submission API for the SDP (Single Dealer Platform).

Clients submit orders via REST; these are forwarded to the engine.
For now, orders are executed locally using the latest ZMQ prices.
"""

import time
import logging
from typing import Optional

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

router = APIRouter()
logger = logging.getLogger("efx.api.trade")

_bridge = None


def set_trade_bridge(bridge):
    global _bridge
    _bridge = bridge


class TradeRequest(BaseModel):
    client_id: str
    pair: str
    side: str  # "BUY" or "SELL"
    amount: float
    venue: str = "SDP"


class TradeResponse(BaseModel):
    status: str
    trade_id: Optional[str] = None
    pair: str
    side: str
    price: float
    amount: float
    spread_bps: float
    venue: str
    timestamp: float
    message: Optional[str] = None


@router.post("/submit", response_model=TradeResponse)
async def submit_trade(req: TradeRequest):
    """Submit a trade from the SDP."""
    if not _bridge or not _bridge.latest_fair_values:
        raise HTTPException(status_code=503, detail="Engine not connected")

    fv = _bridge.latest_fair_values.get(req.pair)
    if not fv:
        raise HTTPException(status_code=400, detail=f"No price for {req.pair}")

    mid = fv.get("mid", 0)
    if mid <= 0:
        raise HTTPException(status_code=400, detail="Invalid price")

    # Get the client's quoted price from the engine
    client_prices = _bridge.latest_client_prices.get(req.pair, {})
    client_price = client_prices.get(req.client_id)

    if client_price:
        price = client_price.get("ask" if req.side == "BUY" else "bid", mid)
        spread_bps = client_price.get("spread_bps", 0)
    else:
        is_jpy = "JPY" in req.pair
        pip = 0.001 if is_jpy else 0.00001
        half_spread = 0.3 * pip
        price = mid + half_spread if req.side == "BUY" else mid - half_spread
        spread_bps = abs(price - mid) / mid * 10000

    trade_id = f"SDP-{int(time.time() * 1000)}"

    # Record the fill in EngineState
    fill_data = {
        "trade_id": trade_id,
        "client_id": req.client_id,
        "pair": req.pair,
        "side": req.side,
        "price": price,
        "amount": req.amount,
        "venue": req.venue,
        "mid_at_fill": mid,
        "spread_bps": spread_bps,
        "timestamp": time.time() * 1e6,
    }
    _bridge.state.on_fill(fill_data)

    logger.info(f"SDP trade: {req.client_id} {req.side} {req.amount:,.0f} {req.pair} @ {price:.5f}")

    return TradeResponse(
        status="FILLED",
        trade_id=trade_id,
        pair=req.pair,
        side=req.side,
        price=price,
        amount=req.amount,
        spread_bps=spread_bps,
        venue=req.venue,
        timestamp=time.time(),
    )
