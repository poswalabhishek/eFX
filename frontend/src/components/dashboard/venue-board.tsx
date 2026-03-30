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
import type { VenueData } from "@/hooks/use-prices";

function getRankBadgeVariant(
  rank: number,
): "default" | "secondary" | "outline" | "destructive" {
  if (rank === 0) return "outline";
  if (rank <= 2) return "default";
  if (rank <= 4) return "secondary";
  return "destructive";
}

interface VenueBoardProps {
  venues: VenueData[];
}

export function VenueBoard({ venues }: VenueBoardProps) {
  return (
    <Card className="p-4 gap-0">
      <h3 className="text-sm font-medium text-muted-foreground mb-3">
        Venue Performance
      </h3>

      <Table>
        <TableHeader>
          <TableRow>
            <TableHead className="text-xs">Venue</TableHead>
            <TableHead className="text-xs text-right">Volume</TableHead>
            <TableHead className="text-xs text-center">Rank</TableHead>
            <TableHead className="text-xs text-right">PnL</TableHead>
            <TableHead className="text-xs text-right">Share</TableHead>
          </TableRow>
        </TableHeader>
        <TableBody>
          {venues.length === 0 ? (
            <TableRow>
              <TableCell colSpan={5} className="text-center text-xs text-muted-foreground">
                Waiting for data...
              </TableCell>
            </TableRow>
          ) : (
            venues.map((v) => (
              <TableRow key={v.venue}>
                <TableCell className="font-medium text-sm">{v.venue}</TableCell>
                <TableCell className="text-right font-mono text-sm">
                  {v.volume}
                </TableCell>
                <TableCell className="text-center">
                  <Badge variant={getRankBadgeVariant(v.ranking)} className="text-xs">
                    {v.ranking === 0 ? "-" : `#${v.ranking}`}
                  </Badge>
                </TableCell>
                <TableCell
                  className={`text-right font-mono text-sm ${
                    v.pnl >= 0 ? "text-emerald-500" : "text-red-500"
                  }`}
                >
                  {v.pnl >= 0 ? "+" : ""}${Math.abs(v.pnl).toLocaleString(undefined, { maximumFractionDigits: 0 })}
                </TableCell>
                <TableCell className="text-right font-mono text-sm text-muted-foreground">
                  {v.market_share > 0 ? `${v.market_share}%` : "-"}
                </TableCell>
              </TableRow>
            ))
          )}
        </TableBody>
      </Table>
    </Card>
  );
}
