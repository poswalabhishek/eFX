"use client";

import { Card } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from "@/components/ui/table";
import type { Toxicity, TierId } from "@/types/market";

interface ClientAlphaRow {
  client_id: string;
  name: string;
  alpha_score: number;
  toxicity: Toxicity;
  tier: TierId;
  pnl: number;
  is_important: boolean;
}

const MOCK_CLIENTS: ClientAlphaRow[] = [
  {
    client_id: "toxic_hft_firm",
    name: "Velocity Trading",
    alpha_score: 5.2,
    toxicity: "high",
    tier: "restricted",
    pnl: -28_000,
    is_important: false,
  },
  {
    client_id: "algo_firm_momentum",
    name: "Quant Momentum",
    alpha_score: 3.8,
    toxicity: "high",
    tier: "bronze",
    pnl: -15_200,
    is_important: false,
  },
  {
    client_id: "hedge_fund_alpha",
    name: "Alpha Capital",
    alpha_score: 2.1,
    toxicity: "medium",
    tier: "gold",
    pnl: -4_100,
    is_important: false,
  },
  {
    client_id: "retail_aggregator",
    name: "FX Connect Retail",
    alpha_score: 0.1,
    toxicity: "low",
    tier: "silver",
    pnl: 18_500,
    is_important: false,
  },
  {
    client_id: "sovereign_fund",
    name: "Norges Investment",
    alpha_score: -0.5,
    toxicity: "low",
    tier: "platinum",
    pnl: 42_000,
    is_important: true,
  },
  {
    client_id: "real_money_pension",
    name: "Global Pension Fund",
    alpha_score: -0.8,
    toxicity: "low",
    tier: "platinum",
    pnl: 55_300,
    is_important: true,
  },
];

const TOXICITY_COLORS: Record<Toxicity, string> = {
  low: "bg-emerald-500/10 text-emerald-500 border-emerald-500/20",
  medium: "bg-amber-500/10 text-amber-500 border-amber-500/20",
  high: "bg-red-500/10 text-red-500 border-red-500/20",
  extreme: "bg-red-700/10 text-red-700 border-red-700/20",
};

const TIER_LABELS: Record<TierId, string> = {
  platinum: "Plat",
  gold: "Gold",
  silver: "Slvr",
  bronze: "Brnz",
  restricted: "Rstr",
};

export function ClientAlphaTable() {
  return (
    <Card className="p-4 gap-0">
      <h3 className="text-sm font-medium text-muted-foreground mb-3">
        Client Alpha
      </h3>

      <Table>
        <TableHeader>
          <TableRow>
            <TableHead className="text-xs">Client</TableHead>
            <TableHead className="text-xs text-right">Alpha</TableHead>
            <TableHead className="text-xs text-center">Signal</TableHead>
            <TableHead className="text-xs text-center">Tier</TableHead>
            <TableHead className="text-xs text-right">PnL</TableHead>
          </TableRow>
        </TableHeader>
        <TableBody>
          {MOCK_CLIENTS.map((c) => (
            <TableRow key={c.client_id}>
              <TableCell className="text-sm">
                <div className="flex items-center gap-1.5">
                  {c.is_important && (
                    <span className="text-amber-400 text-xs" title="Important client">
                      *
                    </span>
                  )}
                  <span className="font-medium">{c.name}</span>
                </div>
              </TableCell>
              <TableCell
                className={`text-right font-mono text-sm font-medium ${
                  c.alpha_score > 1.5
                    ? "text-red-500"
                    : c.alpha_score > 0.5
                      ? "text-amber-500"
                      : "text-emerald-500"
                }`}
              >
                {c.alpha_score > 0 ? "+" : ""}
                {c.alpha_score.toFixed(1)}
              </TableCell>
              <TableCell className="text-center">
                <Badge
                  variant="outline"
                  className={`text-xs ${TOXICITY_COLORS[c.toxicity]}`}
                >
                  {c.toxicity}
                </Badge>
              </TableCell>
              <TableCell className="text-center">
                <Badge variant="secondary" className="text-xs font-mono">
                  {TIER_LABELS[c.tier]}
                </Badge>
              </TableCell>
              <TableCell
                className={`text-right font-mono text-sm ${
                  c.pnl >= 0 ? "text-emerald-500" : "text-red-500"
                }`}
              >
                {c.pnl >= 0 ? "+" : ""}${Math.abs(c.pnl).toLocaleString()}
              </TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </Card>
  );
}
