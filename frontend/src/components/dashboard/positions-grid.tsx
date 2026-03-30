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
import type { PositionData } from "@/hooks/use-prices";

function formatAmount(value: number): string {
  if (Math.abs(value) >= 1_000_000) {
    return `${(value / 1_000_000).toFixed(1)}M`;
  }
  if (Math.abs(value) >= 1_000) {
    return `${(value / 1_000).toFixed(0)}K`;
  }
  return value.toFixed(0);
}

interface PositionsGridProps {
  positions: PositionData[];
}

export function PositionsGrid({ positions }: PositionsGridProps) {
  const netExposure = positions.reduce((sum, p) => sum + p.unrealized_pnl, 0);

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
          Net: ${netExposure.toLocaleString(undefined, { maximumFractionDigits: 0 })}
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
          {positions.length === 0 ? (
            <TableRow>
              <TableCell colSpan={3} className="text-center text-xs text-muted-foreground">
                Waiting for data...
              </TableCell>
            </TableRow>
          ) : (
            positions.map((p) => (
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
                    : `$${p.unrealized_pnl.toLocaleString(undefined, { maximumFractionDigits: 0 })}`}
                </TableCell>
              </TableRow>
            ))
          )}
        </TableBody>
      </Table>
    </Card>
  );
}
