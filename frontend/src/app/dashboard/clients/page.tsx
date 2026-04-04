"use client";

import { useState } from "react";
import { useClients } from "@/hooks/use-clients";
import { useDashboard } from "@/hooks/use-prices";
import { DashboardHeader } from "@/components/dashboard/header";
import { Card } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
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
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from "@/components/ui/dialog";
import type { Client, TierId, Toxicity } from "@/types/market";

const TOXICITY_COLORS: Record<Toxicity, string> = {
  low: "bg-emerald-500/10 text-emerald-500 border-emerald-500/20",
  medium: "bg-amber-500/10 text-amber-500 border-amber-500/20",
  high: "bg-red-500/10 text-red-500 border-red-500/20",
  extreme: "bg-red-700/10 text-red-700 border-red-700/20",
};

function ClientDetailDialog({ client, tiers, onTierChange }: {
  client: Client;
  tiers: { tier_id: string; label: string; multiplier: number }[];
  onTierChange: (tier: string, reason: string) => void;
}) {
  const [reason, setReason] = useState("");
  const [selectedTier, setSelectedTier] = useState<string>(client.default_tier);

  return (
    <DialogContent className="max-w-lg">
      <DialogHeader>
        <DialogTitle>{client.name}</DialogTitle>
      </DialogHeader>
      <div className="space-y-4">
        <div className="grid grid-cols-2 gap-4">
          <div>
            <p className="text-xs text-muted-foreground">Client ID</p>
            <p className="text-sm font-mono">{client.client_id}</p>
          </div>
          <div>
            <p className="text-xs text-muted-foreground">Type</p>
            <p className="text-sm">{client.client_type}</p>
          </div>
          <div>
            <p className="text-xs text-muted-foreground">Alpha Score</p>
            <p className={`text-sm font-mono font-bold ${
              client.alpha_score > 1.5 ? "text-red-500" :
              client.alpha_score > 0.5 ? "text-amber-500" : "text-emerald-500"
            }`}>
              {client.alpha_score > 0 ? "+" : ""}{client.alpha_score.toFixed(1)} bps
            </p>
          </div>
          <div>
            <p className="text-xs text-muted-foreground">Toxicity</p>
            <Badge variant="outline" className={TOXICITY_COLORS[client.toxicity] ?? ""}>
              {client.toxicity}
            </Badge>
          </div>
          <div>
            <p className="text-xs text-muted-foreground">Important</p>
            <p className="text-sm">{client.is_important ? "Yes" : "No"}</p>
          </div>
          <div>
            <p className="text-xs text-muted-foreground">Active</p>
            <p className="text-sm">{client.is_active ? "Yes" : "No"}</p>
          </div>
        </div>

        <div className="border-t pt-4">
          <h4 className="text-sm font-medium mb-3">Change Tier</h4>
          <div className="flex gap-3">
            <Select value={selectedTier} onValueChange={(val) => val && setSelectedTier(val)}>
              <SelectTrigger className="w-[180px]">
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                {tiers.map((t) => (
                  <SelectItem key={t.tier_id} value={t.tier_id}>
                    {t.label} ({t.multiplier}x)
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
            <Input
              placeholder="Reason for change..."
              value={reason}
              onChange={(e) => setReason(e.target.value)}
              className="flex-1"
            />
            <Button
              size="sm"
              disabled={selectedTier === client.default_tier && !reason}
              onClick={() => {
                onTierChange(selectedTier, reason || "Manual adjustment");
                setReason("");
              }}
            >
              Apply
            </Button>
          </div>
        </div>
      </div>
    </DialogContent>
  );
}

export default function ClientsPage() {
  const { clients, tiers, updateClientTier } = useClients();
  const { connected, engineConnected } = useDashboard();
  const [filter, setFilter] = useState("");

  const filtered = clients.filter((c) =>
    c.name.toLowerCase().includes(filter.toLowerCase()) ||
    c.client_id.toLowerCase().includes(filter.toLowerCase()) ||
    c.client_type.toLowerCase().includes(filter.toLowerCase())
  );

  const sorted = [...filtered].sort((a, b) => b.alpha_score - a.alpha_score);

  return (
    <div className="flex flex-col h-screen">
      <DashboardHeader engineConnected={engineConnected} wsConnected={connected} />
      <main className="flex-1 overflow-auto p-6 bg-muted/30">
        <div className="max-w-6xl mx-auto space-y-6">
          <div className="flex items-center justify-between">
            <div>
              <h2 className="text-xl font-bold">Client & Tier Management</h2>
              <p className="text-sm text-muted-foreground mt-1">
                {clients.length} clients configured. Adjust tiers, view alpha scores, manage overrides.
              </p>
            </div>
            <Input
              placeholder="Filter clients..."
              value={filter}
              onChange={(e) => setFilter(e.target.value)}
              className="w-64"
            />
          </div>

          {/* Summary cards */}
          <div className="grid grid-cols-5 gap-4">
            {tiers.map((t) => {
              const count = clients.filter((c) => c.default_tier === t.tier_id).length;
              return (
                <Card key={t.tier_id} className="p-3 gap-0">
                  <p className="text-xs text-muted-foreground">{t.label}</p>
                  <p className="text-2xl font-bold font-mono">{count}</p>
                  <p className="text-xs text-muted-foreground">{t.multiplier}x spread</p>
                </Card>
              );
            })}
          </div>

          {/* Client table */}
          <Card className="p-0 gap-0">
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead>Client</TableHead>
                  <TableHead>Type</TableHead>
                  <TableHead className="text-right">Alpha</TableHead>
                  <TableHead className="text-center">Toxicity</TableHead>
                  <TableHead className="text-center">Tier</TableHead>
                  <TableHead className="text-center">Important</TableHead>
                  <TableHead className="text-center">Actions</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {sorted.map((client) => (
                  <TableRow key={client.client_id}>
                    <TableCell>
                      <div>
                        <p className="font-medium">{client.name}</p>
                        <p className="text-xs text-muted-foreground font-mono">
                          {client.client_id}
                        </p>
                      </div>
                    </TableCell>
                    <TableCell>
                      <Badge variant="outline" className="text-xs">
                        {client.client_type}
                      </Badge>
                    </TableCell>
                    <TableCell className="text-right">
                      <span className={`font-mono font-bold ${
                        client.alpha_score > 1.5 ? "text-red-500" :
                        client.alpha_score > 0.5 ? "text-amber-500" : "text-emerald-500"
                      }`}>
                        {client.alpha_score > 0 ? "+" : ""}
                        {client.alpha_score.toFixed(1)}
                      </span>
                    </TableCell>
                    <TableCell className="text-center">
                      <Badge
                        variant="outline"
                        className={`text-xs ${TOXICITY_COLORS[client.toxicity] ?? ""}`}
                      >
                        {client.toxicity}
                      </Badge>
                    </TableCell>
                    <TableCell className="text-center">
                      <Select
                        value={client.default_tier}
                        onValueChange={(val) =>
                          val && updateClientTier(client.client_id, val, "Quick change from table")
                        }
                      >
                        <SelectTrigger className="h-8 w-[100px] text-xs">
                          <SelectValue />
                        </SelectTrigger>
                        <SelectContent>
                          {tiers.map((t) => (
                            <SelectItem key={t.tier_id} value={t.tier_id}>
                              {t.label}
                            </SelectItem>
                          ))}
                        </SelectContent>
                      </Select>
                    </TableCell>
                    <TableCell className="text-center">
                      {client.is_important ? (
                        <span className="text-amber-400">*</span>
                      ) : (
                        <span className="text-muted-foreground">-</span>
                      )}
                    </TableCell>
                    <TableCell className="text-center">
                      <Dialog>
                        <DialogTrigger>
                          <Button variant="ghost" size="sm" className="text-xs">
                            Details
                          </Button>
                        </DialogTrigger>
                        <ClientDetailDialog
                          client={client}
                          tiers={tiers}
                          onTierChange={(tier, reason) =>
                            updateClientTier(client.client_id, tier, reason)
                          }
                        />
                      </Dialog>
                    </TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>
          </Card>
        </div>
      </main>
    </div>
  );
}
