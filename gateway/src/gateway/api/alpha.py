"""Alpha model API -- computes and serves client alpha scores."""

import logging
from datetime import datetime

from fastapi import APIRouter
from pydantic import BaseModel

router = APIRouter()
logger = logging.getLogger("efx.api.alpha")

_bridge = None


def set_alpha_bridge(bridge):
    global _bridge
    _bridge = bridge


class AlphaScoreResponse(BaseModel):
    client_id: str
    score: float
    toxicity: str
    recommended_tier: str
    trade_count: int
    spread_pnl: float


@router.get("/scores", response_model=list[AlphaScoreResponse])
async def get_alpha_scores():
    """Get current alpha scores for all clients with trade activity."""
    if not _bridge:
        return []

    fills = list(_bridge.state.recent_fills)
    if not fills:
        return []

    # Aggregate per-client stats from recent fills
    client_stats: dict[str, dict] = {}
    for f in fills:
        cid = f.get("client_id", "")
        if not cid or cid.startswith("_"):
            continue
        if cid not in client_stats:
            client_stats[cid] = {
                "trade_count": 0,
                "total_spread_bps": 0.0,
                "total_amount": 0.0,
            }
        cs = client_stats[cid]
        cs["trade_count"] += 1
        cs["total_spread_bps"] += f.get("spread_bps", 0)
        cs["total_amount"] += f.get("amount", 0)

    results = []
    for cid, cs in sorted(client_stats.items(), key=lambda x: -x[1]["total_spread_bps"]):
        avg_spread = cs["total_spread_bps"] / cs["trade_count"] if cs["trade_count"] > 0 else 0
        score = avg_spread * 5.0  # scale to alpha convention
        spread_pnl = cs["total_spread_bps"] * cs["total_amount"] * 0.0001 * 0.5

        if score > 5.0:
            toxicity = "extreme"
            tier = "restricted"
        elif score > 3.0:
            toxicity = "high"
            tier = "bronze"
        elif score > 1.5:
            toxicity = "medium"
            tier = "silver"
        else:
            toxicity = "low"
            tier = "platinum"

        results.append(AlphaScoreResponse(
            client_id=cid,
            score=round(score, 2),
            toxicity=toxicity,
            recommended_tier=tier,
            trade_count=cs["trade_count"],
            spread_pnl=round(spread_pnl, 2),
        ))

    return results


@router.get("/current-session")
async def get_current_session():
    """Get the current active trading session based on UTC time."""
    now = datetime.utcnow()
    hour = now.hour

    if 6 <= hour < 14:
        return {
            "session_id": "london",
            "label": "London",
            "hub_city": "London",
            "next_handoff_utc": "14:00",
            "hours_remaining": 14 - hour - (1 if now.minute > 0 else 0),
        }
    elif 14 <= hour < 21:
        return {
            "session_id": "new_york",
            "label": "New York",
            "hub_city": "New York",
            "next_handoff_utc": "21:00",
            "hours_remaining": 21 - hour - (1 if now.minute > 0 else 0),
        }
    else:
        next_h = 6 if hour >= 21 else 6
        rem = (next_h - hour) % 24
        return {
            "session_id": "singapore",
            "label": "Singapore",
            "hub_city": "Singapore",
            "next_handoff_utc": "06:00",
            "hours_remaining": rem,
        }
