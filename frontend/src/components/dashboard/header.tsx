"use client";

import { SessionIndicator } from "./session-indicator";

export function DashboardHeader() {
  const utcTime = new Date().toISOString().slice(11, 19);

  return (
    <header className="border-b bg-card px-6 py-3">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-4">
          <h1 className="text-lg font-bold tracking-tight">EFX</h1>
          <span className="text-xs text-muted-foreground">
            Desk Dashboard
          </span>
        </div>

        <div className="flex items-center gap-6">
          <SessionIndicator />
          <div className="flex items-center gap-2 text-xs text-muted-foreground font-mono">
            <span>UTC</span>
            <span className="font-medium text-foreground">{utcTime}</span>
          </div>
          <div className="flex items-center gap-1.5">
            <span className="h-2 w-2 rounded-full bg-emerald-500" />
            <span className="text-xs text-muted-foreground">Engine Connected</span>
          </div>
        </div>
      </div>
    </header>
  );
}
