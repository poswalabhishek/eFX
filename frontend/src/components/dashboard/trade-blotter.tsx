"use client";

import { Card } from "@/components/ui/card";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Badge } from "@/components/ui/badge";
import type { FillData } from "@/hooks/use-prices";

function formatTime(ts: number): string {
  const d = new Date(ts / 1000); // microseconds -> ms
  return d.toLocaleTimeString("en-GB", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

function formatAmount(v: number): string {
  if (Math.abs(v) >= 1e6) return `${(v / 1e6).toFixed(1)}M`;
  if (Math.abs(v) >= 1e3) return `${(v / 1e3).toFixed(0)}K`;
  return v.toFixed(0);
}

interface TradeBlotterProps {
  fills: FillData[];
}

export function TradeBlotter({ fills }: TradeBlotterProps) {
  return (
    <Card className="p-4 gap-0">
      <div className="flex items-baseline justify-between mb-3">
        <h3 className="text-sm font-medium text-muted-foreground">
          Trade Blotter
        </h3>
        <span className="text-xs text-muted-foreground font-mono">
          {fills.length} recent
        </span>
      </div>

      <ScrollArea className="h-[220px]">
        {fills.length === 0 ? (
          <p className="text-xs text-muted-foreground">No trades yet...</p>
        ) : (
          <div className="space-y-1">
            {fills.map((f, i) => {
              const isBuy = f.side === "BUY";
              const isJpy = f.pair.includes("JPY");
              const dec = isJpy ? 3 : 5;
              return (
                <div
                  key={f.trade_id ?? i}
                  className="flex items-center gap-2 text-xs py-1 border-b border-border/50 last:border-0"
                >
                  <span className="text-muted-foreground font-mono w-16 shrink-0">
                    {formatTime(f.timestamp)}
                  </span>
                  <span className="font-mono font-medium w-14 shrink-0">
                    {f.pair}
                  </span>
                  <Badge
                    variant={isBuy ? "default" : "destructive"}
                    className="text-[10px] w-9 justify-center"
                  >
                    {f.side}
                  </Badge>
                  <span className="font-mono w-12 text-right shrink-0">
                    {formatAmount(f.amount)}
                  </span>
                  <span className="font-mono w-16 text-right shrink-0">
                    {f.price.toFixed(dec)}
                  </span>
                  <span className="text-muted-foreground w-14 text-right shrink-0">
                    {f.venue}
                  </span>
                  <span
                    className={`font-mono w-12 text-right shrink-0 ${
                      f.spread_bps > 0.5
                        ? "text-amber-500"
                        : "text-muted-foreground"
                    }`}
                  >
                    {f.spread_bps.toFixed(2)}bp
                  </span>
                </div>
              );
            })}
          </div>
        )}
      </ScrollArea>
    </Card>
  );
}
