"use client";

import { Card } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import type { FairValue } from "@/types/market";

const PAIR_ORDER = [
  "EURUSD", "USDJPY", "GBPUSD", "AUDUSD", "USDCHF",
  "USDCAD", "NZDUSD", "EURGBP", "EURJPY", "GBPJPY",
];

interface FairValueMonitorProps {
  prices: Record<string, FairValue>;
}

export function FairValueMonitor({ prices }: FairValueMonitorProps) {
  const pairs = PAIR_ORDER.filter((p) => prices[p]);

  if (pairs.length === 0) {
    return (
      <Card className="p-4 gap-0">
        <h3 className="text-sm font-medium text-muted-foreground mb-3">Fair Value Monitor</h3>
        <p className="text-sm text-muted-foreground">Waiting for engine...</p>
      </Card>
    );
  }

  return (
    <Card className="p-4 gap-0">
      <h3 className="text-sm font-medium text-muted-foreground mb-3">
        Fair Value Monitor
      </h3>
      <div className="space-y-1.5">
        {pairs.map((pair) => {
          const fv = prices[pair];
          const isJpy = pair.includes("JPY");
          const decimals = isJpy ? 3 : 5;
          const volPct = (fv.volatility * 100).toFixed(1);
          const confPct = (fv.confidence * 100).toFixed(0);

          return (
            <div key={pair} className="flex items-center gap-3 text-sm">
              <span className="font-mono font-medium w-16 shrink-0">
                {pair}
              </span>
              <span className="font-mono tabular-nums w-20 text-right">
                {fv.mid.toFixed(decimals)}
              </span>
              <div className="flex-1 flex items-center gap-2">
                <div className="flex-1 h-1.5 bg-muted rounded-full overflow-hidden">
                  <div
                    className={`h-full rounded-full ${
                      fv.confidence > 0.8
                        ? "bg-emerald-500"
                        : fv.confidence > 0.5
                          ? "bg-amber-500"
                          : "bg-red-500"
                    }`}
                    style={{ width: `${fv.confidence * 100}%` }}
                  />
                </div>
                <span className="text-xs text-muted-foreground font-mono w-8">
                  {confPct}%
                </span>
              </div>
              <span className="text-xs text-muted-foreground font-mono w-14 text-right">
                {volPct}%vol
              </span>
              <Badge
                variant={fv.num_venues >= 3 ? "default" : fv.num_venues >= 2 ? "secondary" : "destructive"}
                className="text-[10px] w-6 justify-center"
              >
                {fv.num_venues}
              </Badge>
            </div>
          );
        })}
      </div>
    </Card>
  );
}
