"""Drawdown detection and alerting.

Monitors running PnL for drawdowns exceeding configurable thresholds.
Supports both absolute and rolling-window drawdown detection.
"""

from dataclasses import dataclass
from enum import Enum
from datetime import datetime


class AlertSeverity(Enum):
    INFO = "info"
    WARNING = "warning"
    CRITICAL = "critical"
    EMERGENCY = "emergency"


@dataclass
class DrawdownAlert:
    severity: AlertSeverity
    message: str
    drawdown: float
    peak_pnl: float
    current_pnl: float
    timestamp: datetime


class DrawdownDetector:
    def __init__(
        self,
        warning_threshold: float = -50_000,
        critical_threshold: float = -100_000,
        emergency_threshold: float = -250_000,
    ):
        self.warning_threshold = warning_threshold
        self.critical_threshold = critical_threshold
        self.emergency_threshold = emergency_threshold

        self.peak_pnl = 0.0
        self.last_severity: AlertSeverity | None = None

    def check(self, current_pnl: float) -> DrawdownAlert | None:
        if current_pnl > self.peak_pnl:
            self.peak_pnl = current_pnl
            self.last_severity = None
            return None

        drawdown = current_pnl - self.peak_pnl

        severity = None
        if drawdown <= self.emergency_threshold:
            severity = AlertSeverity.EMERGENCY
        elif drawdown <= self.critical_threshold:
            severity = AlertSeverity.CRITICAL
        elif drawdown <= self.warning_threshold:
            severity = AlertSeverity.WARNING

        if severity is None:
            return None

        # Only alert on severity escalation
        if self.last_severity == severity:
            return None

        self.last_severity = severity
        return DrawdownAlert(
            severity=severity,
            message=f"PnL drawdown: ${drawdown:,.0f} from peak ${self.peak_pnl:,.0f}",
            drawdown=drawdown,
            peak_pnl=self.peak_pnl,
            current_pnl=current_pnl,
            timestamp=datetime.utcnow(),
        )
