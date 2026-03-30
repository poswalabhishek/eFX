"use client";

import { Card } from "@/components/ui/card";
import type { PnlData } from "@/hooks/use-prices";

function formatCurrency(value: number): string {
  const prefix = value >= 0 ? "+$" : "-$";
  return `${prefix}${Math.abs(value).toLocaleString(undefined, { maximumFractionDigits: 0 })}`;
}

interface PnlPanelProps {
  pnl: PnlData;
}

export function PnlPanel({ pnl }: PnlPanelProps) {
  const isPositive = pnl.total >= 0;

  return (
    <Card className="p-4 gap-0">
      <div className="flex items-baseline justify-between mb-3">
        <h3 className="text-sm font-medium text-muted-foreground">
          Real-Time PnL
        </h3>
        <span className="text-xs text-muted-foreground">Today</span>
      </div>

      <div className="mb-4">
        <span
          className={`text-3xl font-bold font-mono tabular-nums ${
            isPositive ? "text-emerald-500" : "text-red-500"
          }`}
        >
          {formatCurrency(pnl.total)}
        </span>
      </div>

      <div className="grid grid-cols-3 gap-3 mb-4">
        <div>
          <p className="text-xs text-muted-foreground">Spread</p>
          <p className={`text-sm font-mono tabular-nums ${pnl.spread_pnl >= 0 ? "text-emerald-500" : "text-red-500"}`}>
            {formatCurrency(pnl.spread_pnl)}
          </p>
        </div>
        <div>
          <p className="text-xs text-muted-foreground">Position</p>
          <p
            className={`text-sm font-mono tabular-nums ${
              pnl.position_pnl >= 0 ? "text-emerald-500" : "text-red-500"
            }`}
          >
            {formatCurrency(pnl.position_pnl)}
          </p>
        </div>
        <div>
          <p className="text-xs text-muted-foreground">Hedge Cost</p>
          <p className="text-sm font-mono tabular-nums text-red-500">
            {formatCurrency(pnl.hedge_cost)}
          </p>
        </div>
      </div>

      <div className="flex items-center justify-between border-t pt-3">
        <div>
          <p className="text-xs text-muted-foreground">Peak</p>
          <p className="text-sm font-mono tabular-nums">
            {formatCurrency(pnl.peak)}
          </p>
        </div>
        <div className="text-right">
          <p className="text-xs text-muted-foreground">Drawdown</p>
          <p
            className={`text-sm font-mono tabular-nums ${
              pnl.drawdown < -50_000
                ? "text-red-500"
                : pnl.drawdown < -10_000
                  ? "text-amber-500"
                  : "text-muted-foreground"
            }`}
          >
            {formatCurrency(pnl.drawdown)}
          </p>
        </div>
      </div>
    </Card>
  );
}
