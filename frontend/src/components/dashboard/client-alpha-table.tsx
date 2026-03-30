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
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import type { Client, Tier, TierId, Toxicity } from "@/types/market";

const TOXICITY_COLORS: Record<Toxicity, string> = {
  low: "bg-emerald-500/10 text-emerald-500 border-emerald-500/20",
  medium: "bg-amber-500/10 text-amber-500 border-amber-500/20",
  high: "bg-red-500/10 text-red-500 border-red-500/20",
  extreme: "bg-red-700/10 text-red-700 border-red-700/20",
};

const TIER_SHORT: Record<string, string> = {
  platinum: "Plat",
  gold: "Gold",
  silver: "Slvr",
  bronze: "Brnz",
  restricted: "Rstr",
};

interface ClientAlphaTableProps {
  clients: Client[];
  tiers: Tier[];
  onTierChange: (clientId: string, newTier: string) => void;
}

export function ClientAlphaTable({
  clients,
  tiers,
  onTierChange,
}: ClientAlphaTableProps) {
  const sorted = [...clients].sort(
    (a, b) => b.alpha_score - a.alpha_score,
  );

  return (
    <Card className="p-4 gap-0">
      <h3 className="text-sm font-medium text-muted-foreground mb-3">
        Client Alpha ({clients.length})
      </h3>

      {clients.length === 0 ? (
        <p className="text-sm text-muted-foreground">Loading clients...</p>
      ) : (
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead className="text-xs">Client</TableHead>
              <TableHead className="text-xs text-right">Alpha</TableHead>
              <TableHead className="text-xs text-center">Signal</TableHead>
              <TableHead className="text-xs text-center">Tier</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            {sorted.map((c) => (
              <TableRow key={c.client_id}>
                <TableCell className="text-sm">
                  <div className="flex items-center gap-1.5">
                    {c.is_important && (
                      <span
                        className="text-amber-400 text-xs"
                        title="Important client"
                      >
                        *
                      </span>
                    )}
                    <span className="font-medium truncate max-w-[140px]">
                      {c.name}
                    </span>
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
                    className={`text-xs ${TOXICITY_COLORS[c.toxicity] ?? ""}`}
                  >
                    {c.toxicity}
                  </Badge>
                </TableCell>
                <TableCell className="text-center">
                  <Select
                    value={c.default_tier}
                    onValueChange={(val) => val && onTierChange(c.client_id, val)}
                  >
                    <SelectTrigger className="h-7 w-[75px] text-xs font-mono">
                      <SelectValue>
                        {TIER_SHORT[c.default_tier] ?? c.default_tier}
                      </SelectValue>
                    </SelectTrigger>
                    <SelectContent>
                      {tiers.map((t) => (
                        <SelectItem key={t.tier_id} value={t.tier_id}>
                          {t.label} ({t.multiplier}x)
                        </SelectItem>
                      ))}
                    </SelectContent>
                  </Select>
                </TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      )}
    </Card>
  );
}
