"""Client alpha model -- measures how much a client's trades predict future price moves.

Alpha(client, horizon) = direction * (mid_{t+horizon} - mid_t) / mid_t

Positive alpha = client is informed (toxic to us).
Negative alpha = client provides liquidity (benign).

The model trains daily on historical fills, computing features per client,
and exports an XGBoost model that predicts forward alpha from trade patterns.
"""

import logging
from dataclasses import dataclass, field
from datetime import datetime

import numpy as np

logger = logging.getLogger("efx.alpha")

HORIZONS_SECONDS = [1, 5, 10, 30, 60, 300]


@dataclass
class ClientAlphaFeatures:
    """Feature vector computed per client for alpha model training."""
    client_id: str
    trade_count: int = 0
    avg_size: float = 0.0
    buy_ratio: float = 0.5
    pair_concentration: float = 0.0  # Herfindahl index
    time_of_day_entropy: float = 0.0
    avg_fill_rate: float = 0.0
    adverse_selection_ratio: float = 0.0  # % trades where price moved against us within 1s

    # Rolling alpha at each horizon
    alpha_1s: float = 0.0
    alpha_5s: float = 0.0
    alpha_10s: float = 0.0
    alpha_30s: float = 0.0
    alpha_60s: float = 0.0
    alpha_300s: float = 0.0

    def feature_vector(self) -> list[float]:
        return [
            self.trade_count,
            self.avg_size,
            self.buy_ratio,
            self.pair_concentration,
            self.time_of_day_entropy,
            self.adverse_selection_ratio,
            self.alpha_1s,
            self.alpha_5s,
            self.alpha_10s,
            self.alpha_30s,
            self.alpha_60s,
            self.alpha_300s,
        ]

    @staticmethod
    def feature_names() -> list[str]:
        return [
            "trade_count", "avg_size", "buy_ratio", "pair_concentration",
            "tod_entropy", "adverse_selection",
            "alpha_1s", "alpha_5s", "alpha_10s", "alpha_30s", "alpha_60s", "alpha_300s",
        ]


@dataclass
class AlphaScore:
    """Output of the alpha model for a single client."""
    client_id: str
    score: float  # continuous alpha score
    toxicity: str  # low/medium/high/extreme
    recommended_tier: str
    confidence: float
    computed_at: str


def classify_toxicity(alpha_score: float) -> str:
    if alpha_score > 5.0:
        return "extreme"
    if alpha_score > 3.0:
        return "high"
    if alpha_score > 1.5:
        return "medium"
    return "low"


def recommend_tier(alpha_score: float, is_important: bool = False) -> str:
    if is_important and alpha_score < 3.0:
        return "gold"
    if alpha_score > 5.0:
        return "restricted"
    if alpha_score > 3.0:
        return "bronze"
    if alpha_score > 1.5:
        return "silver"
    if alpha_score > 0.5:
        return "gold"
    return "platinum"


class AlphaModelTrainer:
    """Trains an XGBoost model to predict client forward alpha.

    In production this runs daily at 22:00 UTC on the last 30 days of fills.
    For the simulation, we provide a simplified version that computes
    alpha directly from fill marks.
    """

    def __init__(self):
        self.model = None
        self.feature_importance: dict[str, float] = {}
        self.last_trained: str | None = None

    def compute_features(self, fills: list[dict], mid_history: dict[str, list[tuple[float, float]]]) -> list[ClientAlphaFeatures]:
        """Compute per-client feature vectors from trade data.

        fills: list of fill dicts with client_id, pair, side, amount, price, mid_at_fill, timestamp
        mid_history: pair -> [(timestamp, mid)] sorted by time
        """
        client_fills: dict[str, list[dict]] = {}
        for f in fills:
            cid = f.get("client_id", "")
            if cid:
                client_fills.setdefault(cid, []).append(f)

        features = []
        for cid, cfs in client_fills.items():
            feat = ClientAlphaFeatures(client_id=cid)
            feat.trade_count = len(cfs)

            amounts = [f.get("amount", 0) for f in cfs]
            feat.avg_size = np.mean(amounts) if amounts else 0

            buys = sum(1 for f in cfs if f.get("side") == "BUY")
            feat.buy_ratio = buys / len(cfs) if cfs else 0.5

            # Pair concentration (Herfindahl)
            pair_counts: dict[str, int] = {}
            for f in cfs:
                p = f.get("pair", "")
                pair_counts[p] = pair_counts.get(p, 0) + 1
            total = sum(pair_counts.values())
            if total > 0:
                shares = [c / total for c in pair_counts.values()]
                feat.pair_concentration = sum(s * s for s in shares)

            # Simplified alpha: use spread_bps as proxy for alpha signal
            # (In production, we'd mark-to-market against future mid prices)
            spread_bps = [f.get("spread_bps", 0) for f in cfs]
            avg_spread = np.mean(spread_bps) if spread_bps else 0
            feat.alpha_1s = avg_spread * 0.3
            feat.alpha_5s = avg_spread * 0.5
            feat.alpha_30s = avg_spread * 0.8

            features.append(feat)

        return features

    def train(self, features: list[ClientAlphaFeatures]) -> list[AlphaScore]:
        """Train the model and return alpha scores.

        For the simulation, we use a simple rule-based scoring since
        we don't have enough historical data for XGBoost yet.
        The infrastructure is here for when we do.
        """
        try:
            import xgboost as xgb
            logger.info("XGBoost available -- model training infrastructure ready")
        except ImportError:
            logger.info("XGBoost not available, using rule-based scoring")

        scores = []
        for feat in features:
            # Weighted combination of alpha signals
            raw_score = (
                feat.alpha_1s * 0.3 +
                feat.alpha_5s * 0.4 +
                feat.alpha_30s * 0.3 +
                feat.adverse_selection_ratio * 2.0
            )

            # Scale to match the scoring convention
            alpha_score = raw_score * 10.0

            scores.append(AlphaScore(
                client_id=feat.client_id,
                score=round(alpha_score, 2),
                toxicity=classify_toxicity(alpha_score),
                recommended_tier=recommend_tier(alpha_score),
                confidence=min(1.0, feat.trade_count / 50.0),
                computed_at=datetime.utcnow().isoformat(),
            ))

        self.last_trained = datetime.utcnow().isoformat()
        logger.info(f"Alpha model scored {len(scores)} clients")
        return scores
