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
import { TradeBlotter } from "@/components/dashboard/trade-blotter";
import { AnalyticsInsights } from "@/components/dashboard/analytics-insights";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";

export default function DashboardPage() {
  const {
    pairs, pnl, positions, venues, alerts, fills, pnlHistory, connected, engineConnected, lastUpdate,
  } = useDashboard();
  const { clients, tiers, updateClientTier } = useClients();

  return (
    <div className="flex flex-col h-screen">
      <DashboardHeader
        engineConnected={engineConnected}
        wsConnected={connected}
      />
      <main className="flex-1 overflow-auto p-4 bg-muted/30">
        <Tabs defaultValue="ops" className="h-full">
          <TabsList>
            <TabsTrigger value="ops">Operations</TabsTrigger>
            <TabsTrigger value="analytics">Analytics</TabsTrigger>
          </TabsList>

          <TabsContent value="ops">
            <div className="grid grid-cols-12 gap-4">
              <div className="col-span-3 flex flex-col gap-4">
                <PnlPanel pnl={pnl} />
                <PositionsGrid positions={positions} />
              </div>

              <div className="col-span-3 flex flex-col gap-4">
                <PriceTiles prices={pairs} />
                <TradeBlotter fills={fills} />
              </div>

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

              <div className="col-span-3 flex flex-col gap-4">
                <VenueBoard venues={venues} />
                <AlertFeed alerts={alerts} />
              </div>
            </div>
          </TabsContent>

          <TabsContent value="analytics">
            <div className="grid grid-cols-12 gap-4">
              <div className="col-span-8">
                <AnalyticsInsights
                  pnl={pnl}
                  pnlHistory={pnlHistory}
                  positions={positions}
                  venues={venues}
                  alerts={alerts}
                  fills={fills}
                  engineConnected={engineConnected}
                  lastUpdate={lastUpdate}
                />
              </div>
              <div className="col-span-4 flex flex-col gap-4">
                <VenueBoard venues={venues} />
                <AlertFeed alerts={alerts} />
              </div>
            </div>
          </TabsContent>
        </Tabs>
      </main>
    </div>
  );
}
