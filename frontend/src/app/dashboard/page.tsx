"use client";

import { useDashboard } from "@/hooks/use-prices";
import { useClients } from "@/hooks/use-clients";
import { DashboardHeader } from "@/components/dashboard/header";
import { PriceTiles } from "@/components/dashboard/price-tiles";
import { PnlPanel } from "@/components/dashboard/pnl-panel";
import { PositionsGrid } from "@/components/dashboard/positions-grid";
import { VenueBoard } from "@/components/dashboard/venue-board";
import { ClientAlphaTable } from "@/components/dashboard/client-alpha-table";
import { AlertFeed } from "@/components/dashboard/alert-feed";
import { FairValueMonitor } from "@/components/dashboard/fair-value-monitor";

export default function DashboardPage() {
  const { pairs, pnl, positions, venues, alerts, connected, engineConnected } =
    useDashboard();
  const { clients, tiers, updateClientTier } = useClients();

  return (
    <div className="flex flex-col h-screen">
      <DashboardHeader
        engineConnected={engineConnected}
        wsConnected={connected}
      />
      <main className="flex-1 overflow-auto p-4 bg-muted/30">
        <div className="grid grid-cols-12 gap-4">
          {/* Left column */}
          <div className="col-span-3 flex flex-col gap-4">
            <PnlPanel pnl={pnl} />
            <PositionsGrid positions={positions} />
          </div>

          {/* Center-left column */}
          <div className="col-span-3 flex flex-col gap-4">
            <PriceTiles prices={pairs} />
          </div>

          {/* Center-right column */}
          <div className="col-span-3 flex flex-col gap-4">
            <FairValueMonitor prices={pairs} />
            <ClientAlphaTable
              clients={clients}
              tiers={tiers}
              onTierChange={(clientId, tier) =>
                updateClientTier(clientId, tier, "Manual adjustment")
              }
            />
          </div>

          {/* Right column */}
          <div className="col-span-3 flex flex-col gap-4">
            <VenueBoard venues={venues} />
            <AlertFeed alerts={alerts} />
          </div>
        </div>
      </main>
    </div>
  );
}
