"use client";

import { Card } from "@/components/ui/card";
import { ScrollArea } from "@/components/ui/scroll-area";
import type { AlertSeverity } from "@/types/market";

interface AlertItem {
  id: string;
  severity: AlertSeverity;
  message: string;
  timestamp: string;
}

const MOCK_ALERTS: AlertItem[] = [
  {
    id: "1",
    severity: "warning",
    message: "USDJPY drawdown -$8,200 in last 30min",
    timestamp: "14:15",
  },
  {
    id: "2",
    severity: "critical",
    message: "AlgoFirm3 alpha spike: +5.8 (was +2.1 yesterday)",
    timestamp: "14:10",
  },
  {
    id: "3",
    severity: "info",
    message: "Session handoff: Singapore -> London complete",
    timestamp: "14:00",
  },
  {
    id: "4",
    severity: "warning",
    message: "EBS stale for GBPUSD (no tick > 3s)",
    timestamp: "13:55",
  },
  {
    id: "5",
    severity: "info",
    message: "Tier change: Retail Aggregator Silver -> Gold (auto)",
    timestamp: "13:42",
  },
];

const SEVERITY_STYLES: Record<AlertSeverity, { icon: string; color: string }> =
  {
    info: { icon: "i", color: "text-blue-400" },
    warning: { icon: "!", color: "text-amber-400" },
    critical: { icon: "!!", color: "text-red-500" },
    emergency: { icon: "!!!", color: "text-red-600 font-bold" },
  };

export function AlertFeed() {
  return (
    <Card className="p-4 gap-0">
      <h3 className="text-sm font-medium text-muted-foreground mb-3">
        Alerts
      </h3>

      <ScrollArea className="h-[180px]">
        <div className="space-y-2">
          {MOCK_ALERTS.map((alert) => {
            const style = SEVERITY_STYLES[alert.severity];
            return (
              <div
                key={alert.id}
                className="flex items-start gap-2 text-sm py-1"
              >
                <span
                  className={`shrink-0 w-5 text-center font-mono text-xs ${style.color}`}
                >
                  {style.icon}
                </span>
                <span className="text-xs text-muted-foreground font-mono shrink-0">
                  {alert.timestamp}
                </span>
                <span className="text-sm">{alert.message}</span>
              </div>
            );
          })}
        </div>
      </ScrollArea>
    </Card>
  );
}
