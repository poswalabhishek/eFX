"use client";

import { Card } from "@/components/ui/card";
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from "@/components/ui/table";

interface PositionRow {
  pair: string;
  position: number;
  unrealized_pnl: number;
}

const MOCK_POSITIONS: PositionRow[] = [
  { pair: "EURUSD", position: 2_500_000, unrealized_pnl: 1_200 },
  { pair: "USDJPY", position: -5_000_000, unrealized_pnl: -3_400 },
  { pair: "GBPUSD", position: 1_000_000, unrealized_pnl: 500 },
  { pair: "AUDUSD", position: -750_000, unrealized_pnl: -180 },
  { pair: "USDCHF", position: 0, unrealized_pnl: 0 },
  { pair: "USDCAD", position: 3_200_000, unrealized_pnl: 890 },
];

function formatAmount(value: number): string {
  if (Math.abs(value) >= 1_000_000) {
    return `${(value / 1_000_000).toFixed(1)}M`;
  }
  if (Math.abs(value) >= 1_000) {
    return `${(value / 1_000).toFixed(0)}K`;
  }
  return value.toString();
}

export function PositionsGrid() {
  const netExposure = MOCK_POSITIONS.reduce(
    (sum, p) => sum + p.unrealized_pnl,
    0,
  );

  return (
    <Card className="p-4 gap-0">
      <div className="flex items-baseline justify-between mb-3">
        <h3 className="text-sm font-medium text-muted-foreground">
          Positions
        </h3>
        <span
          className={`text-xs font-mono ${
            netExposure >= 0 ? "text-emerald-500" : "text-red-500"
          }`}
        >
          Net: ${netExposure.toLocaleString()}
        </span>
      </div>

      <Table>
        <TableHeader>
          <TableRow>
            <TableHead className="text-xs">Pair</TableHead>
            <TableHead className="text-xs text-right">Position</TableHead>
            <TableHead className="text-xs text-right">Unrealized</TableHead>
          </TableRow>
        </TableHeader>
        <TableBody>
          {MOCK_POSITIONS.map((p) => (
            <TableRow key={p.pair}>
              <TableCell className="font-mono text-sm font-medium">
                {p.pair}
              </TableCell>
              <TableCell
                className={`text-right font-mono text-sm ${
                  p.position > 0
                    ? "text-emerald-500"
                    : p.position < 0
                      ? "text-red-500"
                      : "text-muted-foreground"
                }`}
              >
                {p.position === 0 ? "FLAT" : formatAmount(p.position)}
              </TableCell>
              <TableCell
                className={`text-right font-mono text-sm ${
                  p.unrealized_pnl > 0
                    ? "text-emerald-500"
                    : p.unrealized_pnl < 0
                      ? "text-red-500"
                      : "text-muted-foreground"
                }`}
              >
                {p.unrealized_pnl === 0
                  ? "-"
                  : `$${p.unrealized_pnl.toLocaleString()}`}
              </TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </Card>
  );
}
