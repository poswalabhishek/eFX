"""Aggregated engine state from ZMQ stream.

Tracks real fills, positions, PnL, venue stats, and generates alerts.
"""

import time
from dataclasses import dataclass, field
from collections import deque


@dataclass
class VenueStats:
    venue: str
    tick_count: int = 0
    fill_count: int = 0
    last_tick_time: float = 0.0
    volume_usd: float = 0.0
    pnl: float = 0.0
    ranking: int = 0
    market_share: float = 0.0


@dataclass
class Alert:
    id: str
    severity: str
    message: str
    timestamp: float


class EngineState:
    def __init__(self):
        self.venue_stats: dict[str, VenueStats] = {
            v: VenueStats(venue=v) for v in ("EBS", "Reuters", "Hotspot", "Currenex", "SDP")
        }

        # Real positions from engine
        self.positions: list[dict] = []
        self.realized_pnl: float = 0.0
        self.unrealized_pnl: float = 0.0
        self.total_fills: int = 0
        self.total_rejects: int = 0
        self.spread_pnl: float = 0.0
        self.peak_pnl: float = 0.0
        self.drawdown: float = 0.0

        # Recent fills (blotter)
        self.recent_fills: deque[dict] = deque(maxlen=100)

        # Alerts
        self.alerts: list[Alert] = []
        self._alert_counter = 0

    def on_tick(self, data: dict):
        venue = data.get("venue", "")
        if venue in self.venue_stats:
            self.venue_stats[venue].tick_count += 1
            self.venue_stats[venue].last_tick_time = time.time()

    def on_fair_value(self, data: dict):
        self._check_alerts()

    def on_fill(self, data: dict):
        """Process a real fill from the C++ engine."""
        self.recent_fills.appendleft(data)

        venue = data.get("venue", "")
        amount = data.get("amount", 0)
        spread_bps = data.get("spread_bps", 0)

        if venue in self.venue_stats:
            vs = self.venue_stats[venue]
            vs.fill_count += 1
            vs.volume_usd += abs(amount)
            vs.pnl += spread_bps * abs(amount) * 0.0001 * 0.5

        # Track spread PnL from half-spread capture
        self.spread_pnl += spread_bps * abs(amount) * 0.0001 * 0.5
        self._update_venue_rankings()

    def on_positions(self, data: dict):
        """Process position snapshot from engine."""
        self.positions = data.get("positions", [])
        self.realized_pnl = data.get("total_realized_pnl", 0.0)
        self.unrealized_pnl = data.get("total_unrealized_pnl", 0.0)
        self.total_fills = data.get("total_fills", 0)
        self.total_rejects = data.get("total_rejects", 0)
        self._update_drawdown()

    def on_engine_pnl(self, data: dict):
        """Process PnL update from engine."""
        self.realized_pnl = data.get("realized_pnl", self.realized_pnl)
        self.unrealized_pnl = data.get("unrealized_pnl", self.unrealized_pnl)
        self.total_fills = data.get("total_fills", self.total_fills)
        self._update_drawdown()

    def _update_drawdown(self):
        total = self.realized_pnl + self.unrealized_pnl
        if total > self.peak_pnl:
            self.peak_pnl = total
        self.drawdown = total - self.peak_pnl

    def _update_venue_rankings(self):
        sorted_venues = sorted(
            self.venue_stats.values(), key=lambda v: v.volume_usd, reverse=True
        )
        total_vol = sum(v.volume_usd for v in sorted_venues)
        for rank, vs in enumerate(sorted_venues, 1):
            vs.ranking = rank
            vs.market_share = (vs.volume_usd / total_vol * 100) if total_vol > 0 else 0.0

    def _check_alerts(self):
        if self.drawdown < -50_000 and not self._has_recent("dd_warn"):
            self._add_alert("warning", f"Drawdown ${self.drawdown:,.0f} from peak ${self.peak_pnl:,.0f}")
        if self.drawdown < -100_000 and not self._has_recent("dd_crit"):
            self._add_alert("critical", f"CRITICAL drawdown ${self.drawdown:,.0f}")

        for v_id, vs in self.venue_stats.items():
            if vs.last_tick_time > 0 and time.time() - vs.last_tick_time > 5.0:
                if not self._has_recent(f"stale_{v_id}"):
                    self._add_alert("warning", f"{v_id} stale (no tick >5s)")

    def _has_recent(self, prefix: str) -> bool:
        cutoff = time.time() - 30
        return any(a.id.startswith(prefix) and a.timestamp > cutoff for a in self.alerts[-20:])

    def _add_alert(self, severity: str, message: str):
        self._alert_counter += 1
        self.alerts.append(Alert(
            id=f"{severity}_{self._alert_counter}",
            severity=severity,
            message=message,
            timestamp=time.time(),
        ))
        if len(self.alerts) > 100:
            self.alerts = self.alerts[-100:]

    # ---- Snapshot methods for WebSocket ----

    def get_pnl_snapshot(self) -> dict:
        total = self.realized_pnl + self.unrealized_pnl
        return {
            "type": "pnl",
            "total": round(total, 2),
            "spread_pnl": round(self.spread_pnl, 2),
            "position_pnl": round(self.unrealized_pnl, 2),
            "hedge_cost": round(self.realized_pnl - self.spread_pnl, 2),
            "peak": round(self.peak_pnl, 2),
            "drawdown": round(self.drawdown, 2),
            "total_fills": self.total_fills,
            "total_rejects": self.total_rejects,
            "timestamp": time.time(),
        }

    def get_positions(self) -> list[dict]:
        return [
            {
                "pair": p.get("pair", ""),
                "position": round(p.get("position", 0)),
                "unrealized_pnl": round(p.get("unrealized_pnl", 0), 2),
            }
            for p in sorted(self.positions, key=lambda x: x.get("pair", ""))
        ]

    def get_venue_stats(self) -> list[dict]:
        def fmt_vol(v: float) -> str:
            if v >= 1e9: return f"${v/1e9:.1f}B"
            if v >= 1e6: return f"${v/1e6:.0f}M"
            if v >= 1e3: return f"${v/1e3:.0f}K"
            return f"${v:,.0f}"

        return [
            {
                "venue": vs.venue,
                "volume": fmt_vol(vs.volume_usd),
                "ranking": vs.ranking,
                "pnl": round(vs.pnl, 2),
                "market_share": round(vs.market_share, 1),
                "fill_count": vs.fill_count,
            }
            for vs in sorted(self.venue_stats.values(), key=lambda x: -x.volume_usd)
        ]

    def get_recent_alerts(self, limit: int = 10) -> list[dict]:
        return [
            {"id": a.id, "severity": a.severity, "message": a.message, "timestamp": a.timestamp}
            for a in reversed(self.alerts[-limit:])
        ]

    def get_recent_fills(self, limit: int = 20) -> list[dict]:
        return list(self.recent_fills)[:limit]
