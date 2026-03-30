import { PnlPanel } from "@/components/dashboard/pnl-panel";
import { PositionsGrid } from "@/components/dashboard/positions-grid";
import { VenueBoard } from "@/components/dashboard/venue-board";
import { ClientAlphaTable } from "@/components/dashboard/client-alpha-table";
import { AlertFeed } from "@/components/dashboard/alert-feed";

export default function DashboardPage() {
  return (
    <div className="grid grid-cols-12 gap-4 h-full">
      {/* Left column: PnL + Positions */}
      <div className="col-span-4 flex flex-col gap-4">
        <PnlPanel />
        <PositionsGrid />
      </div>

      {/* Center column: Client Alpha + Venue Performance */}
      <div className="col-span-4 flex flex-col gap-4">
        <ClientAlphaTable />
        <VenueBoard />
      </div>

      {/* Right column: Alerts */}
      <div className="col-span-4 flex flex-col gap-4">
        <AlertFeed />
      </div>
    </div>
  );
}
