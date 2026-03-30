"""Aggregated engine state computed from ZMQ stream.

Tracks per-pair fair values, per-venue tick stats, simulated PnL,
positions, and generates alerts. This is the single source of truth
for the dashboard until the full OMS is built.
"""

import time
import random
import math
from dataclasses import dataclass, field


@dataclass
class VenueStats:
    venue: str
    tick_count: int = 0
    last_tick_time: float = 0.0
    volume_usd: float = 0.0
    pnl: float = 0.0
    ranking: int = 0
    market_share: float = 0.0


@dataclass
class PairPosition:
    pair: str
    position: float = 0.0
    avg_entry: float = 0.0
    unrealized_pnl: float = 0.0
    last_mid: float = 0.0


@dataclass
class PnlState:
    spread_pnl: float = 0.0
    position_pnl: float = 0.0
    hedge_cost: float = 0.0
    peak: float = 0.0
    drawdown: float = 0.0

    @property
    def total(self) -> float:
        return self.spread_pnl + self.position_pnl + self.hedge_cost

    def update_drawdown(self):
        t = self.total
        if t > self.peak:
            self.peak = t
        self.drawdown = t - self.peak


@dataclass
class Alert:
    id: str
    severity: str
    message: str
    timestamp: float


class EngineState:
    """Computes aggregated dashboard state from ZMQ tick/fair_value stream."""

    def __init__(self):
        self.venue_stats: dict[str, VenueStats] = {}
        self.positions: dict[str, PairPosition] = {}
        self.pnl = PnlState()
        self.alerts: list[Alert] = []
        self._alert_counter = 0
        self._last_sim_trade_time = 0.0
        self._rng = random.Random(42)

        for v in ("EBS", "Reuters", "Hotspot", "Currenex", "SDP"):
            self.venue_stats[v] = VenueStats(venue=v)

    def on_tick(self, data: dict):
        venue = data.get("venue", "")
        if venue in self.venue_stats:
            vs = self.venue_stats[venue]
            vs.tick_count += 1
            vs.last_tick_time = time.time()

    def on_fair_value(self, data: dict):
        pair = data.get("pair", "")
        mid = data.get("mid", 0.0)
        if not pair or mid <= 0:
            return

        if pair not in self.positions:
            self.positions[pair] = PairPosition(pair=pair)

        pos = self.positions[pair]
        pos.last_mid = mid

        # Simulate occasional trades that generate PnL
        now = time.time()
        if now - self._last_sim_trade_time > 0.5:
            self._simulate_trade_activity(pair, mid)
            self._last_sim_trade_time = now

        # Update unrealized PnL for open positions
        if abs(pos.position) > 0 and pos.avg_entry > 0:
            pos.unrealized_pnl = pos.position * (mid - pos.avg_entry)

        # Aggregate position PnL
        self.pnl.position_pnl = sum(
            p.unrealized_pnl for p in self.positions.values()
        )
        self.pnl.update_drawdown()

        self._check_alerts()

    def _simulate_trade_activity(self, pair: str, mid: float):
        """Generate simulated trade flow for realistic PnL movement."""
        if self._rng.random() > 0.3:
            return

        pos = self.positions[pair]
        is_jpy = "JPY" in pair
        pip = 0.001 if is_jpy else 0.00001

        # Spread capture: we earn the bid-ask spread
        base_spread_pips = 0.3 if is_jpy else 0.2
        spread_capture = base_spread_pips * pip * self._rng.uniform(0.5, 2.0)
        trade_size = self._rng.choice([1e6, 2e6, 5e6, 10e6])

        self.pnl.spread_pnl += spread_capture * trade_size * 0.01

        # Small hedge cost
        hedge_slippage = pip * self._rng.uniform(0.0, 0.3)
        self.pnl.hedge_cost -= hedge_slippage * trade_size * 0.001

        # Adjust position randomly
        direction = self._rng.choice([-1, 1])
        pos.position += direction * trade_size * self._rng.uniform(0.1, 0.5)
        pos.position = max(min(pos.position, 50e6), -50e6)
        if abs(pos.position) > 0:
            pos.avg_entry = mid + self._rng.uniform(-5 * pip, 5 * pip)

        # Update venue volumes
        venue = self._rng.choice(["EBS", "Reuters", "Hotspot", "Currenex", "SDP"])
        if venue in self.venue_stats:
            self.venue_stats[venue].volume_usd += trade_size
            self.venue_stats[venue].pnl += spread_capture * trade_size * 0.002

        self._update_venue_rankings()

    def _update_venue_rankings(self):
        sorted_venues = sorted(
            self.venue_stats.values(),
            key=lambda v: v.volume_usd,
            reverse=True,
        )
        total_vol = sum(v.volume_usd for v in sorted_venues)
        for rank, vs in enumerate(sorted_venues, 1):
            vs.ranking = rank
            vs.market_share = (vs.volume_usd / total_vol * 100) if total_vol > 0 else 0.0

    def _check_alerts(self):
        if self.pnl.drawdown < -50_000 and not self._has_recent_alert("drawdown_warning"):
            self._add_alert("warning", f"PnL drawdown ${self.pnl.drawdown:,.0f} from peak ${self.pnl.peak:,.0f}")

        if self.pnl.drawdown < -100_000 and not self._has_recent_alert("drawdown_critical"):
            self._add_alert("critical", f"CRITICAL drawdown ${self.pnl.drawdown:,.0f}")

        for v_id, vs in self.venue_stats.items():
            if vs.last_tick_time > 0 and time.time() - vs.last_tick_time > 5.0:
                if not self._has_recent_alert(f"stale_{v_id}"):
                    self._add_alert("warning", f"{v_id} stale (no tick > 5s)")

    def _has_recent_alert(self, prefix: str) -> bool:
        cutoff = time.time() - 30
        return any(a.id.startswith(prefix) and a.timestamp > cutoff for a in self.alerts[-20:])

    def _add_alert(self, severity: str, message: str):
        self._alert_counter += 1
        alert = Alert(
            id=f"{severity}_{self._alert_counter}",
            severity=severity,
            message=message,
            timestamp=time.time(),
        )
        self.alerts.append(alert)
        if len(self.alerts) > 100:
            self.alerts = self.alerts[-100:]

    def get_pnl_snapshot(self) -> dict:
        return {
            "type": "pnl",
            "total": round(self.pnl.total, 2),
            "spread_pnl": round(self.pnl.spread_pnl, 2),
            "position_pnl": round(self.pnl.position_pnl, 2),
            "hedge_cost": round(self.pnl.hedge_cost, 2),
            "peak": round(self.pnl.peak, 2),
            "drawdown": round(self.pnl.drawdown, 2),
            "timestamp": time.time(),
        }

    def get_positions(self) -> list[dict]:
        return [
            {
                "pair": p.pair,
                "position": round(p.position),
                "unrealized_pnl": round(p.unrealized_pnl, 2),
            }
            for p in sorted(self.positions.values(), key=lambda x: x.pair)
            if p.last_mid > 0
        ]

    def get_venue_stats(self) -> list[dict]:
        def fmt_vol(v: float) -> str:
            if v >= 1e9:
                return f"${v/1e9:.1f}B"
            if v >= 1e6:
                return f"${v/1e6:.0f}M"
            return f"${v:,.0f}"

        return [
            {
                "venue": vs.venue,
                "volume": fmt_vol(vs.volume_usd),
                "ranking": vs.ranking,
                "pnl": round(vs.pnl, 2),
                "market_share": round(vs.market_share, 1),
            }
            for vs in sorted(self.venue_stats.values(), key=lambda x: -x.volume_usd)
        ]

    def get_recent_alerts(self, limit: int = 10) -> list[dict]:
        return [
            {
                "id": a.id,
                "severity": a.severity,
                "message": a.message,
                "timestamp": a.timestamp,
            }
            for a in reversed(self.alerts[-limit:])
        ]
