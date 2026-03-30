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

interface VenueRow {
  venue: string;
  volume: string;
  ranking: number;
  pnl: number;
  market_share: number;
}

const MOCK_VENUES: VenueRow[] = [
  {
    venue: "EBS",
    volume: "$2.1B",
    ranking: 3,
    pnl: 45_000,
    market_share: 12.4,
  },
  {
    venue: "Reuters",
    volume: "$1.8B",
    ranking: 5,
    pnl: 38_000,
    market_share: 9.8,
  },
  {
    venue: "Hotspot",
    volume: "$0.9B",
    ranking: 7,
    pnl: 22_000,
    market_share: 6.2,
  },
  {
    venue: "Currenex",
    volume: "$0.4B",
    ranking: 12,
    pnl: 12_000,
    market_share: 3.1,
  },
  {
    venue: "SDP",
    volume: "$0.6B",
    ranking: 0,
    pnl: 18_000,
    market_share: 0,
  },
];

function getRankBadgeVariant(
  rank: number,
): "default" | "secondary" | "outline" | "destructive" {
  if (rank === 0) return "outline";
  if (rank <= 3) return "default";
  if (rank <= 10) return "secondary";
  return "destructive";
}

export function VenueBoard() {
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
          {MOCK_VENUES.map((v) => (
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
              <TableCell className="text-right font-mono text-sm text-emerald-500">
                +${v.pnl.toLocaleString()}
              </TableCell>
              <TableCell className="text-right font-mono text-sm text-muted-foreground">
                {v.market_share > 0 ? `${v.market_share}%` : "-"}
              </TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </Card>
  );
}
