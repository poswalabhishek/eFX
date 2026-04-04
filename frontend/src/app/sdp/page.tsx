"use client";

import { useState, useCallback } from "react";
import { useDashboard, type FillData } from "@/hooks/use-prices";
import { Card } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { ScrollArea } from "@/components/ui/scroll-area";
import { SessionIndicator } from "@/components/dashboard/session-indicator";
import type { FairValue } from "@/types/market";

const API_BASE = "http://localhost:8000/api";
const CLIENT_ID = "real_money_pension";

const PAIR_ORDER = [
  "EURUSD", "USDJPY", "GBPUSD", "AUDUSD", "USDCHF",
  "USDCAD", "NZDUSD", "EURGBP", "EURJPY", "GBPJPY",
];

const AMOUNTS = ["500K", "1M", "2M", "5M", "10M", "25M", "50M"];

function parseAmount(s: string): number {
  if (s.endsWith("M")) return parseFloat(s) * 1_000_000;
  if (s.endsWith("K")) return parseFloat(s) * 1_000;
  return parseFloat(s);
}

interface TradeResult {
  status: string;
  trade_id?: string;
  pair: string;
  side: string;
  price: number;
  amount: number;
  spread_bps: number;
  timestamp: number;
}

function PriceTile({
  pair,
  fv,
  amount,
  onTrade,
}: {
  pair: string;
  fv: FairValue;
  amount: string;
  onTrade: (pair: string, side: "BUY" | "SELL") => void;
}) {
  const isJpy = pair.includes("JPY");
  const dec = isJpy ? 3 : 5;
  const pip = isJpy ? 0.001 : 0.00001;
  const halfSpread = 0.2 * pip;

  const bid = fv.mid - halfSpread;
  const ask = fv.mid + halfSpread;

  const bidStr = bid.toFixed(dec);
  const askStr = ask.toFixed(dec);

  const bidBig = isJpy ? bidStr.slice(0, -2) : bidStr.slice(0, -2);
  const bidPip = isJpy ? bidStr.slice(-2) : bidStr.slice(-2);
  const askBig = isJpy ? askStr.slice(0, -2) : askStr.slice(0, -2);
  const askPip = isJpy ? askStr.slice(-2) : askStr.slice(-2);

  return (
    <Card className="p-0 gap-0 overflow-hidden">
      <div className="px-3 py-2 bg-muted/50 border-b flex items-center justify-between">
        <span className="font-mono font-bold text-sm">
          {pair.slice(0, 3)}/{pair.slice(3)}
        </span>
        <span className="text-[10px] text-muted-foreground font-mono">
          {amount}
        </span>
      </div>
      <div className="grid grid-cols-2 divide-x">
        <button
          onClick={() => onTrade(pair, "SELL")}
          className="p-3 text-center hover:bg-red-500/5 transition-colors active:bg-red-500/10 cursor-pointer"
        >
          <p className="text-[10px] text-muted-foreground mb-1">SELL</p>
          <div>
            <span className="font-mono text-sm text-red-500">{bidBig}</span>
            <span className="font-mono text-xl font-bold text-red-500">{bidPip}</span>
          </div>
        </button>
        <button
          onClick={() => onTrade(pair, "BUY")}
          className="p-3 text-center hover:bg-emerald-500/5 transition-colors active:bg-emerald-500/10 cursor-pointer"
        >
          <p className="text-[10px] text-muted-foreground mb-1">BUY</p>
          <div>
            <span className="font-mono text-sm text-emerald-500">{askBig}</span>
            <span className="font-mono text-xl font-bold text-emerald-500">{askPip}</span>
          </div>
        </button>
      </div>
    </Card>
  );
}

export default function SDPPage() {
  const { pairs, connected, engineConnected } = useDashboard();
  const [selectedAmount, setSelectedAmount] = useState("1M");
  const [trades, setTrades] = useState<TradeResult[]>([]);
  const [submitting, setSubmitting] = useState(false);

  const handleTrade = useCallback(
    async (pair: string, side: "BUY" | "SELL") => {
      if (submitting) return;
      setSubmitting(true);
      try {
        const res = await fetch(`${API_BASE}/trade/submit`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            client_id: CLIENT_ID,
            pair,
            side,
            amount: parseAmount(selectedAmount),
            venue: "SDP",
          }),
        });
        if (res.ok) {
          const result: TradeResult = await res.json();
          setTrades((prev) => [result, ...prev].slice(0, 50));
        }
      } catch {
        // ignore
      } finally {
        setSubmitting(false);
      }
    },
    [selectedAmount, submitting],
  );

  const activePairs = PAIR_ORDER.filter((p) => pairs[p]);

  return (
    <div className="flex flex-col h-screen bg-background">
      {/* Header */}
      <header className="border-b bg-card px-6 py-3">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-4">
            <h1 className="text-lg font-bold tracking-tight">EFX</h1>
            <Badge variant="outline" className="text-xs">Single Dealer Platform</Badge>
          </div>
          <div className="flex items-center gap-6">
            <SessionIndicator />
            <div className="flex items-center gap-1.5">
              <span className={`h-2 w-2 rounded-full ${engineConnected ? "bg-emerald-500" : "bg-red-500"}`} />
              <span className="text-xs text-muted-foreground">
                {engineConnected ? "Connected" : "Disconnected"}
              </span>
            </div>
          </div>
        </div>
      </header>

      <main className="flex-1 overflow-auto p-6">
        <div className="max-w-6xl mx-auto space-y-6">
          {/* Amount selector */}
          <div className="flex items-center gap-4">
            <span className="text-sm text-muted-foreground">Trade Amount:</span>
            <div className="flex gap-1">
              {AMOUNTS.map((amt) => (
                <Button
                  key={amt}
                  variant={selectedAmount === amt ? "default" : "outline"}
                  size="sm"
                  className="text-xs font-mono"
                  onClick={() => setSelectedAmount(amt)}
                >
                  {amt}
                </Button>
              ))}
            </div>
            <span className="text-xs text-muted-foreground ml-auto">
              Client: {CLIENT_ID}
            </span>
          </div>

          {/* Price tiles grid */}
          {activePairs.length === 0 ? (
            <Card className="p-8 text-center">
              <p className="text-muted-foreground">Waiting for engine connection...</p>
            </Card>
          ) : (
            <div className="grid grid-cols-5 gap-3">
              {activePairs.map((pair) => (
                <PriceTile
                  key={pair}
                  pair={pair}
                  fv={pairs[pair]}
                  amount={selectedAmount}
                  onTrade={handleTrade}
                />
              ))}
            </div>
          )}

          {/* Trade blotter */}
          <Card className="p-4 gap-0">
            <div className="flex items-baseline justify-between mb-3">
              <h3 className="text-sm font-medium text-muted-foreground">
                My Trades
              </h3>
              <span className="text-xs text-muted-foreground font-mono">
                {trades.length} fills
              </span>
            </div>
            <ScrollArea className="h-[250px]">
              {trades.length === 0 ? (
                <p className="text-sm text-muted-foreground">Click a price to trade</p>
              ) : (
                <div className="space-y-1">
                  {trades.map((t, i) => {
                    const isJpy = t.pair.includes("JPY");
                    const dec = isJpy ? 3 : 5;
                    const isBuy = t.side === "BUY";
                    return (
                      <div
                        key={t.trade_id ?? i}
                        className="flex items-center gap-3 text-sm py-1.5 border-b border-border/50 last:border-0"
                      >
                        <span className="text-xs text-muted-foreground font-mono w-20">
                          {new Date(t.timestamp * 1000).toLocaleTimeString("en-GB")}
                        </span>
                        <span className="font-mono font-medium w-16">{t.pair}</span>
                        <Badge
                          variant={isBuy ? "default" : "destructive"}
                          className="w-10 justify-center text-xs"
                        >
                          {t.side}
                        </Badge>
                        <span className="font-mono w-14 text-right">
                          {t.amount >= 1e6
                            ? `${(t.amount / 1e6).toFixed(1)}M`
                            : `${(t.amount / 1e3).toFixed(0)}K`}
                        </span>
                        <span className="font-mono w-20 text-right">
                          {t.price.toFixed(dec)}
                        </span>
                        <Badge variant="outline" className="text-xs">
                          {t.status}
                        </Badge>
                        <span className="text-xs text-muted-foreground font-mono">
                          {t.spread_bps.toFixed(2)}bp
                        </span>
                      </div>
                    );
                  })}
                </div>
              )}
            </ScrollArea>
          </Card>
        </div>
      </main>
    </div>
  );
}
