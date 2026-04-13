"use client";

import { Card } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { useEffect, useState } from "react";
import type {
  AlertData,
  FillData,
  HistoryPoint,
  PnlData,
  PositionData,
  VenueData,
} from "@/hooks/use-prices";

interface AnalyticsInsightsProps {
  pnl: PnlData;
  pnlHistory: HistoryPoint[];
  positions: PositionData[];
  venues: VenueData[];
  alerts: AlertData[];
  fills: FillData[];
  engineConnected: boolean;
  lastUpdate: number;
}

function formatCurrency(value: number): string {
  const sign = value >= 0 ? "+" : "-";
  return `${sign}$${Math.abs(value).toLocaleString(undefined, { maximumFractionDigits: 0 })}`;
}

function formatNotional(value: number): string {
  if (value >= 1_000_000_000) return `$${(value / 1_000_000_000).toFixed(2)}B`;
  if (value >= 1_000_000) return `$${(value / 1_000_000).toFixed(1)}M`;
  if (value >= 1_000) return `$${(value / 1_000).toFixed(0)}K`;
  return `$${value.toFixed(0)}`;
}

function formatAgeMs(ms: number): string {
  if (ms < 1000) return `${ms}ms`;
  return `${(ms / 1000).toFixed(1)}s`;
}

export function AnalyticsInsights({
  pnl,
  pnlHistory,
  positions,
  venues,
  alerts,
  fills,
  engineConnected,
  lastUpdate,
}: AnalyticsInsightsProps) {
  const [clockTs, setClockTs] = useState<number>(() => Date.now());
  useEffect(() => {
    const interval = setInterval(() => setClockTs(Date.now()), 1000);
    return () => clearInterval(interval);
  }, []);

  const stalenessMs =
    lastUpdate > 0 ? clockTs - lastUpdate : Number.POSITIVE_INFINITY;

  const largestRisk = [...positions].sort(
    (a, b) => Math.abs(b.unrealized_pnl) - Math.abs(a.unrealized_pnl),
  )[0];
  const worstPosition = [...positions].sort((a, b) => a.unrealized_pnl - b.unrealized_pnl)[0];

  const topVenueByShare = [...venues].sort((a, b) => b.market_share - a.market_share)[0];
  const worstVenueByPnl = [...venues].sort((a, b) => a.pnl - b.pnl)[0];

  const notionalByClient: Record<string, number> = {};
  const notionalByPair: Record<string, number> = {};
  let grossNotional = 0;
  let wideSpreadTrades = 0;
  let spreadBpsTotal = 0;

  for (const fill of fills) {
    const notional = Math.abs(fill.amount * fill.price);
    grossNotional += notional;
    spreadBpsTotal += fill.spread_bps;
    if (fill.spread_bps >= 1.0) wideSpreadTrades++;
    notionalByClient[fill.client_id] = (notionalByClient[fill.client_id] ?? 0) + notional;
    notionalByPair[fill.pair] = (notionalByPair[fill.pair] ?? 0) + notional;
  }

  const topClientFlow = Object.entries(notionalByClient).sort((a, b) => b[1] - a[1])[0];
  const topPairs = Object.entries(notionalByPair)
    .sort((a, b) => b[1] - a[1])
    .slice(0, 3);

  const avgSpread = fills.length > 0 ? spreadBpsTotal / fills.length : 0;
  const wideSpreadPct = fills.length > 0 ? (wideSpreadTrades / fills.length) * 100 : 0;

  const pnlSwing =
    pnlHistory.length >= 2
      ? pnlHistory[pnlHistory.length - 1].total - pnlHistory[0].total
      : 0;

  const criticalAlerts = alerts.filter((a) => a.severity === "critical" || a.severity === "emergency");
  const warningAlerts = alerts.filter((a) => a.severity === "warning");

  const insights: string[] = [];
  if (!engineConnected) insights.push("Engine stream disconnected; analytics are stale.");
  if (stalenessMs > 5000) insights.push(`No dashboard update for ${formatAgeMs(stalenessMs)}.`);
  if (pnl.drawdown < -50_000) insights.push(`Deep drawdown at ${formatCurrency(pnl.drawdown)} from peak.`);
  if (pnlSwing < -25_000) insights.push(`PnL dropped ${formatCurrency(pnlSwing)} over the recent window.`);
  if (worstPosition && worstPosition.unrealized_pnl < -10_000) {
    insights.push(`${worstPosition.pair} is the largest unrealized drag (${formatCurrency(worstPosition.unrealized_pnl)}).`);
  }
  if (topClientFlow) {
    const concentration = grossNotional > 0 ? (topClientFlow[1] / grossNotional) * 100 : 0;
    if (concentration > 35) {
      insights.push(`Flow is concentrated in ${topClientFlow[0]} (${concentration.toFixed(1)}% of notional).`);
    }
  }
  if (avgSpread > 0.9) insights.push(`Average captured spread is elevated at ${avgSpread.toFixed(2)} bps.`);
  if (criticalAlerts.length > 0) insights.push(`${criticalAlerts.length} critical/emergency alerts need immediate attention.`);
  if (insights.length === 0) insights.push("System looks stable: no immediate risk concentration flags.");

  return (
    <Card className="p-4 gap-0">
      <div className="flex items-center justify-between mb-3">
        <h3 className="text-sm font-medium text-muted-foreground">Market Intelligence</h3>
        <div className="flex items-center gap-1">
          <Badge variant={engineConnected ? "default" : "destructive"} className="text-[10px]">
            {engineConnected ? "engine live" : "engine down"}
          </Badge>
          <Badge variant={stalenessMs < 3000 ? "secondary" : "destructive"} className="text-[10px]">
            {lastUpdate > 0 ? `tick ${formatAgeMs(stalenessMs)} ago` : "no ticks"}
          </Badge>
        </div>
      </div>

      <div className="grid grid-cols-4 gap-2 mb-4">
        <div className="rounded border p-2">
          <p className="text-[11px] text-muted-foreground">Gross Notional</p>
          <p className="font-mono text-sm">{formatNotional(grossNotional)}</p>
        </div>
        <div className="rounded border p-2">
          <p className="text-[11px] text-muted-foreground">Avg Spread</p>
          <p className="font-mono text-sm">{avgSpread.toFixed(2)} bps</p>
        </div>
        <div className="rounded border p-2">
          <p className="text-[11px] text-muted-foreground">Wide Spreads</p>
          <p className="font-mono text-sm">{wideSpreadPct.toFixed(1)}%</p>
        </div>
        <div className="rounded border p-2">
          <p className="text-[11px] text-muted-foreground">Active Alerts</p>
          <p className="font-mono text-sm">
            {criticalAlerts.length}/{warningAlerts.length}/{alerts.length}
          </p>
        </div>
      </div>

      <div className="space-y-1.5 mb-4">
        <p className="text-xs text-muted-foreground">What is happening now</p>
        {insights.map((line, idx) => (
          <p key={`${line}-${idx}`} className="text-sm">
            - {line}
          </p>
        ))}
      </div>

      <div className="grid grid-cols-3 gap-2 text-xs">
        <div className="rounded border p-2">
          <p className="text-muted-foreground mb-1">Top Venue by Share</p>
          <p className="font-mono text-sm">{topVenueByShare?.venue ?? "-"}</p>
          <p className="font-mono text-[11px] text-muted-foreground">
            {topVenueByShare ? `${topVenueByShare.market_share.toFixed(1)}%` : "-"}
          </p>
        </div>
        <div className="rounded border p-2">
          <p className="text-muted-foreground mb-1">Worst Venue PnL</p>
          <p className="font-mono text-sm">{worstVenueByPnl?.venue ?? "-"}</p>
          <p className="font-mono text-[11px] text-red-500">
            {worstVenueByPnl ? formatCurrency(worstVenueByPnl.pnl) : "-"}
          </p>
        </div>
        <div className="rounded border p-2">
          <p className="text-muted-foreground mb-1">Largest Risk Pair</p>
          <p className="font-mono text-sm">{largestRisk?.pair ?? "-"}</p>
          <p className="font-mono text-[11px]">
            {largestRisk ? formatCurrency(largestRisk.unrealized_pnl) : "-"}
          </p>
        </div>
      </div>

      <div className="mt-4">
        <p className="text-xs text-muted-foreground mb-1">Top Flow Pairs</p>
        <div className="grid grid-cols-3 gap-2">
          {topPairs.length === 0 ? (
            <p className="text-xs text-muted-foreground">No fills yet.</p>
          ) : (
            topPairs.map(([pair, notional]) => (
              <div key={pair} className="rounded border p-2">
                <p className="font-mono text-sm">{pair}</p>
                <p className="font-mono text-[11px] text-muted-foreground">
                  {formatNotional(notional)}
                </p>
              </div>
            ))
          )}
        </div>
      </div>
    </Card>
  );
}
