"use client";

import { usePrices } from "@/hooks/use-prices";
import { useClients } from "@/hooks/use-clients";
import { DashboardHeader } from "@/components/dashboard/header";
import { PriceTiles } from "@/components/dashboard/price-tiles";
import { PnlPanel } from "@/components/dashboard/pnl-panel";
import { PositionsGrid } from "@/components/dashboard/positions-grid";
import { VenueBoard } from "@/components/dashboard/venue-board";
import { ClientAlphaTable } from "@/components/dashboard/client-alpha-table";
import { AlertFeed } from "@/components/dashboard/alert-feed";

export default function DashboardPage() {
  const { pairs, connected, engineConnected } = usePrices();
  const { clients, tiers, updateClientTier } = useClients();

  return (
    <div className="flex flex-col h-screen">
      <DashboardHeader
        engineConnected={engineConnected}
        wsConnected={connected}
      />
      <main className="flex-1 overflow-auto p-4 bg-muted/30">
        <div className="grid grid-cols-12 gap-4">
          {/* Left column: PnL + Positions */}
          <div className="col-span-4 flex flex-col gap-4">
            <PnlPanel />
            <PositionsGrid />
          </div>

          {/* Center column: Live Prices + Client Alpha */}
          <div className="col-span-4 flex flex-col gap-4">
            <PriceTiles prices={pairs} />
            <ClientAlphaTable
              clients={clients}
              tiers={tiers}
              onTierChange={(clientId, tier) =>
                updateClientTier(clientId, tier, "Manual adjustment from dashboard")
              }
            />
          </div>

          {/* Right column: Venue Performance + Alerts */}
          <div className="col-span-4 flex flex-col gap-4">
            <VenueBoard />
            <AlertFeed />
          </div>
        </div>
      </main>
    </div>
  );
}
