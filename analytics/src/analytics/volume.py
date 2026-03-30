"""Volume tracking and venue ranking analytics.

Tracks our market share and ranking on each platform,
sliceable by pair, venue, and session.
"""

from dataclasses import dataclass, field
from datetime import datetime


@dataclass
class VenueVolumeSnapshot:
    venue: str
    pair: str
    session: str
    our_volume: float
    total_volume: float
    market_share: float
    ranking: int
    timestamp: datetime


class VolumeTracker:
    """Tracks cumulative volume per venue/pair/session."""

    def __init__(self):
        # (venue, pair) -> our cumulative volume
        self._our_volume: dict[tuple[str, str], float] = {}
        # (venue, pair) -> simulated total platform volume
        self._total_volume: dict[tuple[str, str], float] = {}

    def on_fill(self, venue: str, pair: str, amount: float):
        key = (venue, pair)
        self._our_volume[key] = self._our_volume.get(key, 0.0) + abs(amount)

    def on_platform_volume(self, venue: str, pair: str, total: float):
        """Update simulated total platform volume (from simulator)."""
        key = (venue, pair)
        self._total_volume[key] = total

    def get_market_share(self, venue: str, pair: str) -> float:
        key = (venue, pair)
        our = self._our_volume.get(key, 0.0)
        total = self._total_volume.get(key, 0.0)
        if total <= 0:
            return 0.0
        return our / total

    def get_venue_summary(self, venue: str) -> dict:
        total_our = sum(v for (v_id, _), v in self._our_volume.items() if v_id == venue)
        total_platform = sum(v for (v_id, _), v in self._total_volume.items() if v_id == venue)
        return {
            "venue": venue,
            "our_volume": total_our,
            "total_volume": total_platform,
            "market_share": total_our / total_platform if total_platform > 0 else 0.0,
        }

    def reset(self):
        self._our_volume.clear()
        self._total_volume.clear()
