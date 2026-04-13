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
        self.total_hedges: int = 0
        self.spread_pnl: float = 0.0
        self.hedge_cost: float = 0.0
        self.avg_hedge_slippage: float = 0.0
        self.peak_pnl: float = 0.0
        self.drawdown: float = 0.0

        # Recent fills (blotter)
        self.recent_fills: deque[dict] = deque(maxlen=100)

        # Alerts
        self.alerts: list[Alert] = []
        self._alert_counter = 0
        self.manual_mode: bool = False
        self.latest_fair_values: dict[str, float] = {}
        self.manual_positions: dict[str, dict] = {}

    def set_manual_mode(self, enabled: bool):
        self.manual_mode = enabled

    def on_tick(self, data: dict):
        venue = data.get("venue", "")
        if venue in self.venue_stats:
            self.venue_stats[venue].tick_count += 1
            self.venue_stats[venue].last_tick_time = time.time()

    def on_fair_value(self, data: dict):
        pair = data.get("pair")
        mid = float(data.get("mid", 0.0))
        if pair and mid > 0:
            self.latest_fair_values[pair] = mid
            self._mark_manual_positions_to_market(pair, mid)
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
        if data.get("venue") == "SDP":
            self._apply_manual_fill_position(data)
        self._update_venue_rankings()

    def on_positions(self, data: dict):
        """Process position snapshot from engine."""
        if self.manual_mode:
            return

        self.positions = data.get("positions", [])
        self.realized_pnl = data.get("total_realized_pnl", 0.0)
        self.unrealized_pnl = data.get("total_unrealized_pnl", 0.0)
        self.total_fills = data.get("total_fills", 0)
        self.total_rejects = data.get("total_rejects", 0)
        self._update_drawdown()

    def on_hedge(self, data: dict):
        """Process a hedge execution from the engine."""
        venue = data.get("venue", "")
        if venue in self.venue_stats:
            self.venue_stats[venue].fill_count += 1

    def on_engine_pnl(self, data: dict):
        """Process decomposed PnL from engine."""
        if self.manual_mode:
            return

        self.spread_pnl = data.get("spread_capture", self.spread_pnl)
        self.realized_pnl = data.get("realized_pnl", self.realized_pnl)
        self.unrealized_pnl = data.get("unrealized_pnl", self.unrealized_pnl)
        self.hedge_cost = data.get("hedge_cost", 0.0)
        self.total_fills = data.get("total_fills", self.total_fills)
        self.total_hedges = data.get("total_hedges", 0)
        self.avg_hedge_slippage = data.get("avg_hedge_slippage_bps", 0.0)
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

    def _apply_manual_fill_position(self, data: dict):
        pair = data.get("pair")
        side = data.get("side")
        if not pair or side not in ("BUY", "SELL"):
            return

        amount = float(data.get("amount", 0.0))
        price = float(data.get("price", 0.0))
        if amount <= 0 or price <= 0:
            return

        signed_qty = amount if side == "BUY" else -amount
        pos = self.manual_positions.get(pair, {
            "pair": pair,
            "position": 0.0,
            "avg_entry": 0.0,
            "realized_pnl": 0.0,
            "unrealized_pnl": 0.0,
        })

        cur_pos = float(pos["position"])
        avg_entry = float(pos["avg_entry"])
        realized = float(pos["realized_pnl"])

        if cur_pos == 0:
            cur_pos = signed_qty
            avg_entry = price
        elif cur_pos * signed_qty > 0:
            new_pos = cur_pos + signed_qty
            avg_entry = (avg_entry * abs(cur_pos) + price * abs(signed_qty)) / abs(new_pos)
            cur_pos = new_pos
        else:
            closing_qty = min(abs(cur_pos), abs(signed_qty))
            if cur_pos > 0:
                realized += (price - avg_entry) * closing_qty
            else:
                realized += (avg_entry - price) * closing_qty

            remaining = cur_pos + signed_qty
            if remaining == 0:
                cur_pos = 0.0
                avg_entry = 0.0
            elif cur_pos * remaining > 0:
                cur_pos = remaining
            else:
                cur_pos = remaining
                avg_entry = price

        pos["position"] = cur_pos
        pos["avg_entry"] = avg_entry
        pos["realized_pnl"] = realized
        mid = self.latest_fair_values.get(pair, price)
        pos["unrealized_pnl"] = (mid - avg_entry) * cur_pos if cur_pos != 0 else 0.0
        self.manual_positions[pair] = pos

        if self.manual_mode:
            self.realized_pnl = sum(p["realized_pnl"] for p in self.manual_positions.values())
            self.unrealized_pnl = sum(p["unrealized_pnl"] for p in self.manual_positions.values())
        self._update_drawdown()

    def _mark_manual_positions_to_market(self, pair: str, mid: float):
        pos = self.manual_positions.get(pair)
        if not pos:
            return
        position = float(pos["position"])
        avg_entry = float(pos["avg_entry"])
        pos["unrealized_pnl"] = (mid - avg_entry) * position if position != 0 else 0.0
        self.manual_positions[pair] = pos
        if self.manual_mode:
            self.unrealized_pnl = sum(p["unrealized_pnl"] for p in self.manual_positions.values())
            self.realized_pnl = sum(p["realized_pnl"] for p in self.manual_positions.values())
        self._update_drawdown()

    # ---- Snapshot methods for WebSocket ----

    def get_pnl_snapshot(self) -> dict:
        manual_realized = sum(p["realized_pnl"] for p in self.manual_positions.values())
        manual_unrealized = sum(p["unrealized_pnl"] for p in self.manual_positions.values())
        realized = self.realized_pnl if self.manual_mode else self.realized_pnl + manual_realized
        unrealized = self.unrealized_pnl if self.manual_mode else self.unrealized_pnl + manual_unrealized
        total = self.spread_pnl + self.hedge_cost + realized + unrealized
        return {
            "type": "pnl",
            "total": round(total, 2),
            "spread_pnl": round(self.spread_pnl, 2),
            "position_pnl": round(unrealized, 2),
            "hedge_cost": round(self.hedge_cost, 2),
            "realized_pnl": round(realized, 2),
            "peak": round(self.peak_pnl, 2),
            "drawdown": round(self.drawdown, 2),
            "total_fills": self.total_fills,
            "total_hedges": self.total_hedges,
            "avg_hedge_slippage_bps": round(self.avg_hedge_slippage, 3),
            "timestamp": time.time(),
        }

    def get_positions(self) -> list[dict]:
        if self.manual_mode:
            return [
                {
                    "pair": p["pair"],
                    "position": round(p["position"]),
                    "unrealized_pnl": round(p["unrealized_pnl"], 2),
                }
                for p in sorted(self.manual_positions.values(), key=lambda x: x["pair"])
            ]

        merged: dict[str, dict] = {}
        for p in self.positions:
            pair = p.get("pair", "")
            if not pair:
                continue
            merged[pair] = {
                "pair": pair,
                "position": float(p.get("position", 0)),
                "unrealized_pnl": float(p.get("unrealized_pnl", 0)),
            }

        for pair, p in self.manual_positions.items():
            row = merged.get(pair, {"pair": pair, "position": 0.0, "unrealized_pnl": 0.0})
            row["position"] += float(p.get("position", 0.0))
            row["unrealized_pnl"] += float(p.get("unrealized_pnl", 0.0))
            merged[pair] = row

        return [
            {
                "pair": p["pair"],
                "position": round(p["position"]),
                "unrealized_pnl": round(p["unrealized_pnl"], 2),
            }
            for p in sorted(merged.values(), key=lambda x: x["pair"])
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
