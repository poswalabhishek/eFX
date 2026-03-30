"use client";

import { Card } from "@/components/ui/card";
import { ScrollArea } from "@/components/ui/scroll-area";
import type { AlertData } from "@/hooks/use-prices";
import type { AlertSeverity } from "@/types/market";

const SEVERITY_STYLES: Record<AlertSeverity, { icon: string; color: string }> =
  {
    info: { icon: "i", color: "text-blue-400" },
    warning: { icon: "!", color: "text-amber-400" },
    critical: { icon: "!!", color: "text-red-500" },
    emergency: { icon: "!!!", color: "text-red-600 font-bold" },
  };

function formatTime(ts: number): string {
  const d = new Date(ts * 1000);
  return d.toLocaleTimeString("en-GB", { hour: "2-digit", minute: "2-digit" });
}

interface AlertFeedProps {
  alerts: AlertData[];
}

export function AlertFeed({ alerts }: AlertFeedProps) {
  return (
    <Card className="p-4 gap-0">
      <h3 className="text-sm font-medium text-muted-foreground mb-3">
        Alerts
      </h3>

      <ScrollArea className="h-[180px]">
        <div className="space-y-2">
          {alerts.length === 0 ? (
            <p className="text-xs text-muted-foreground">No alerts</p>
          ) : (
            alerts.map((alert) => {
              const style = SEVERITY_STYLES[alert.severity] ?? SEVERITY_STYLES.info;
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
                    {formatTime(alert.timestamp)}
                  </span>
                  <span className="text-sm">{alert.message}</span>
                </div>
              );
            })
          )}
        </div>
      </ScrollArea>
    </Card>
  );
}
