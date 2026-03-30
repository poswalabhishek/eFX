"""Real-time and historical PnL calculation engine.

PnL = Spread PnL + Position PnL + Hedge Cost

Sliceable by: time, pair, client, venue, session, component.
Tracks both per-center (NY/SG/LDN) and global PnL book.
"""

from dataclasses import dataclass, field
from datetime import datetime


@dataclass
class PnlSnapshot:
    timestamp: datetime
    pair: str
    session: str
    spread_pnl: float = 0.0
    position_pnl: float = 0.0
    hedge_cost: float = 0.0
    total_pnl: float = 0.0
    position: float = 0.0
    drawdown: float = 0.0
    peak_pnl: float = 0.0


@dataclass
class PnlTracker:
    """Tracks running PnL across all dimensions."""

    pair_pnl: dict[str, float] = field(default_factory=dict)
    client_pnl: dict[str, float] = field(default_factory=dict)
    venue_pnl: dict[str, float] = field(default_factory=dict)
    session_pnl: dict[str, float] = field(default_factory=dict)

    total_spread_pnl: float = 0.0
    total_position_pnl: float = 0.0
    total_hedge_cost: float = 0.0

    peak_total: float = 0.0
    current_drawdown: float = 0.0

    def on_fill(self, pair: str, client_id: str, venue: str, session: str,
                spread_capture: float, amount: float):
        self.total_spread_pnl += spread_capture

        self.pair_pnl[pair] = self.pair_pnl.get(pair, 0.0) + spread_capture
        self.client_pnl[client_id] = self.client_pnl.get(client_id, 0.0) + spread_capture
        self.venue_pnl[venue] = self.venue_pnl.get(venue, 0.0) + spread_capture
        self.session_pnl[session] = self.session_pnl.get(session, 0.0) + spread_capture

        self._update_drawdown()

    def on_hedge(self, slippage_cost: float):
        self.total_hedge_cost += slippage_cost
        self._update_drawdown()

    def update_position_pnl(self, unrealized_pnl: float):
        self.total_position_pnl = unrealized_pnl
        self._update_drawdown()

    @property
    def total_pnl(self) -> float:
        return self.total_spread_pnl + self.total_position_pnl + self.total_hedge_cost

    def _update_drawdown(self):
        total = self.total_pnl
        if total > self.peak_total:
            self.peak_total = total
        self.current_drawdown = total - self.peak_total
