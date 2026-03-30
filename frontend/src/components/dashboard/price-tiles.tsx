"use client";

import { Card } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import type { FairValue } from "@/types/market";
import { useEffect, useRef } from "react";

const PAIR_ORDER = [
  "EURUSD", "USDJPY", "GBPUSD", "AUDUSD", "USDCHF",
  "USDCAD", "NZDUSD", "EURGBP", "EURJPY", "GBPJPY",
];

function formatPrice(pair: string, price: number): { big: string; small: string; pip: string } {
  const isJpy = pair.includes("JPY");
  const str = price.toFixed(isJpy ? 3 : 5);
  if (isJpy) {
    const parts = str.split(".");
    return {
      big: parts[0] + "." + parts[1].slice(0, 1),
      pip: parts[1].slice(1, 3),
      small: "",
    };
  }
  const parts = str.split(".");
  return {
    big: parts[0] + "." + parts[1].slice(0, 3),
    pip: parts[1].slice(3, 5),
    small: "",
  };
}

function TickDirection({ pair, mid }: { pair: string; mid: number }) {
  const prevRef = useRef<number>(mid);
  const dirRef = useRef<"up" | "down" | "flat">("flat");

  useEffect(() => {
    if (mid > prevRef.current) dirRef.current = "up";
    else if (mid < prevRef.current) dirRef.current = "down";
    prevRef.current = mid;
  }, [mid]);

  const color =
    dirRef.current === "up"
      ? "text-emerald-500"
      : dirRef.current === "down"
        ? "text-red-500"
        : "text-muted-foreground";

  return <span className={`text-xs ${color}`}>{dirRef.current === "up" ? "\u25B2" : dirRef.current === "down" ? "\u25BC" : "\u25C6"}</span>;
}

interface PriceTilesProps {
  prices: Record<string, FairValue>;
}

export function PriceTiles({ prices }: PriceTilesProps) {
  const activePairs = PAIR_ORDER.filter((p) => prices[p]);

  if (activePairs.length === 0) {
    return (
      <Card className="p-4 gap-0">
        <h3 className="text-sm font-medium text-muted-foreground mb-3">Live Prices</h3>
        <p className="text-sm text-muted-foreground">Waiting for engine connection...</p>
      </Card>
    );
  }

  return (
    <Card className="p-4 gap-0">
      <h3 className="text-sm font-medium text-muted-foreground mb-3">Live Fair Values</h3>
      <div className="grid grid-cols-2 gap-2">
        {activePairs.map((pair) => {
          const fv = prices[pair];
          const fmt = formatPrice(pair, fv.mid);
          return (
            <div
              key={pair}
              className="flex items-center justify-between rounded-md border px-3 py-2"
            >
              <div className="flex items-center gap-2">
                <TickDirection pair={pair} mid={fv.mid} />
                <span className="text-sm font-mono font-semibold">
                  {pair.slice(0, 3)}/{pair.slice(3)}
                </span>
              </div>
              <div className="text-right">
                <span className="font-mono text-sm">{fmt.big}</span>
                <span className="font-mono text-lg font-bold">{fmt.pip}</span>
              </div>
              <div className="flex flex-col items-end gap-0.5">
                <Badge
                  variant={fv.confidence > 0.8 ? "default" : fv.confidence > 0.5 ? "secondary" : "destructive"}
                  className="text-[10px] px-1 py-0"
                >
                  {(fv.confidence * 100).toFixed(0)}%
                </Badge>
                <span className="text-[10px] text-muted-foreground font-mono">
                  {fv.num_venues}v
                </span>
              </div>
            </div>
          );
        })}
      </div>
    </Card>
  );
}
